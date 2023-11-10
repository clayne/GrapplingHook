#include "Utilities.h"
#include "MathUtils.h"
#define HAVOKtoFO4 69.99124f
using namespace RE;
using std::unordered_map;

PlayerCharacter* p;
PlayerCamera* pcam;
BGSProjectile* hookProj;
BGSBendableSpline* rope;

TESObjectREFR* lastProj;
TESObjectREFR* temp;
bool pulling = false;

unordered_map<uint64_t, uint64_t> currentHook; //Actor Handle, Projectile Handle
unordered_map<uint64_t, uint64_t> hookRope; //Actor Handle, Spline Handle
unordered_map<uint64_t, bool> beingPulled; //Actor Handle, true/false

class HookProjectile : public Projectile {
public:
	typedef bool (HookProjectile::* FnProcessImpacts)();
	typedef void (HookProjectile::* FnUpdate)(float);
	typedef void (HookProjectile::* FnInitialize)();

	bool HookedProcessImpacts() {
		BGSProjectile* proj = (BGSProjectile*)this->GetObjectReference();
		if (proj == hookProj) {
			Projectile::ImpactData& ipct = this->impacts[0];
			if (this->impacts.size() > 0 &&
				!ipct.processed) {
				Actor* a = (Actor*)this->shooter.get().get();
				if (a && a->GetFormType() == ENUM_FORM_ID::kACHR) {
					float dist = Length(ipct.location - a->data.location);
					if (dist < 2500) {
						_MESSAGE("Dist %f", dist);
						pulling = true;
					}
					else {
						this->MarkAsDeleted();
						temp->MarkAsDeleted();
					}
				}
			}
		}
		FnProcessImpacts fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)() : false;
	}

	void HookedUpdate(float a_delta) {
		BGSProjectile* proj = (BGSProjectile*)this->GetObjectReference();
		if (proj == hookProj) {
			Projectile::ImpactData& ipct = this->impacts[0];
			if (this->impacts.size() == 0 ||
				!ipct.processed) {
			}

			if (temp) {
				Actor* a = (Actor*)this->shooter.get().get();
				if (a && a->GetFormType() == ENUM_FORM_ID::kACHR && a->Get3D()) {
					float dist = Length(this->data.location - a->data.location);
					if (dist > 3000) {
						temp->MarkAsDeleted();
					}
					NiNode* endPoint = (NiNode*)a->Get3D()->GetObjectByName("ProjectileNode");
					if (!endPoint) {
						endPoint = (NiNode*)a->Get3D()->GetObjectByName("Weapon");
					}
					NiPoint3 newPos = endPoint->world.translate;
					if (a->Get3D(true) == a->Get3D()) {
						NiNode* camera = (NiNode*)a->Get3D()->GetObjectByName("Camera");
						newPos = newPos + pcam->cameraRoot->world.translate - camera->world.translate;
					}
					MoveBendableSpline(temp, newPos, this->data.location, a->parentCell, a->parentCell->worldSpace);


					if (pulling) {
						if (dist < 150) {
							pulling = false;
							this->MarkAsDeleted();
							temp->MarkAsDeleted();
						}
						else {
							if (a->GetFormType() == ENUM_FORM_ID::kACHR) {
								HMODULE hAVF = GetModuleHandleA("ActorVelocityFramework.dll");
								if (!hAVF) {
									logger::warn("Actor Velocity Framework not installed or loaded!");
								}
								else {
									if (a->currentProcess && a->currentProcess->middleHigh) {
										NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
										if (con.get()) {
											NiPoint3 dir = this->data.location - a->data.location - NiPoint3(0, 0, 80.f);
											dir.z = max(0, dir.z);
											NiPoint3 dirNorm = Normalize(dir);
											NiPoint3 velMod = con->initialVelocity;
											float curSpeed = DotProduct(dirNorm, velMod);
											float maxSpeed = 500.f;
											typedef void (*AddVelocity)(std::monostate, Actor*, float, float, float);
											NiPoint3 finalVel = NiPoint3();
											AddVelocity fnAddVelocity = (AddVelocity)GetProcAddress(hAVF, "AddVelocity");
											if (curSpeed < maxSpeed) {
												float accel = min(maxSpeed - curSpeed, 300.f);
												if (velMod.z < 5) {
													finalVel = finalVel + NiPoint3(0, 0, dirNorm.z * accel);
												}
												finalVel = finalVel + NiPoint3(dirNorm.x * accel, dirNorm.y * accel, 0);
											}
											if (Length(finalVel) > 0) {
												fnAddVelocity(std::monostate{}, a, finalVel.x, finalVel.y, finalVel.z);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		FnUpdate fn = fnHash2.at(*(uint64_t*)this);
		if (fn) {
			(this->*fn)(a_delta);
		}
	}

	void HookedInitialize() {
		BGSProjectile* proj = (BGSProjectile*)this->GetObjectReference();
		if (proj == hookProj) {
			if (lastProj && lastProj != this) {
				if (!lastProj->IsDeleted()) {
					lastProj->MarkAsDeleted();
				}
				if (temp && !temp->IsDeleted()) {
					temp->MarkAsDeleted();
				}
				pulling = false;
			}
			lastProj = this;
			Actor* a = (Actor*)this->shooter.get().get();
			GameVM* gvm = GameVM::GetSingleton();
			if (a && a->GetFormType() == ENUM_FORM_ID::kACHR && a->Get3D() && gvm) {
				temp = F4::PlaceAtMe_Native(gvm->GetVM().get(), 0, &lastProj, rope, 1, true, false, false);
				if (temp) {
					
					NiNode* endPoint = (NiNode*)a->Get3D()->GetObjectByName("ProjectileNode");
					if (!endPoint) {
						endPoint = (NiNode*)a->Get3D()->GetObjectByName("Weapon");
					}
					NiPoint3 newPos = endPoint->world.translate;
					if (a->Get3D(true) == a->Get3D()) {
						NiNode* camera = (NiNode*)a->Get3D()->GetObjectByName("Camera");
						newPos = newPos + pcam->cameraRoot->world.translate - camera->world.translate;
					}

					float thickness = 3.0f;
					float slack = 0.01f;
					temp->extraList->SetBendableSplineInfo(&thickness, &slack);

					MoveBendableSpline(temp, newPos, this->data.location, a->parentCell, a->parentCell->worldSpace);
				}
			}
		}
		FnInitialize fn = fnHash3.at(*(uint64_t*)this);
		if (fn) {
			(this->*fn)();
		}
	}

	static void HookProcessImpacts(uint64_t addr) {
		FnProcessImpacts fn = SafeWrite64Function(addr + 0x680, &HookProjectile::HookedProcessImpacts);
		fnHash.insert(std::pair<uint64_t, FnProcessImpacts>(addr, fn));
	}

	static void HookUpdate(uint64_t addr) {
		FnUpdate fn = SafeWrite64Function(addr + 0x678, &HookProjectile::HookedUpdate);
		fnHash2.insert(std::pair<uint64_t, FnUpdate>(addr, fn));
	}

	static void HookInitialize(uint64_t addr) {
		FnInitialize fn = SafeWrite64Function(addr + 0x710, &HookProjectile::HookedInitialize);
		fnHash3.insert(std::pair<uint64_t, FnInitialize>(addr, fn));
	}

protected:
	static unordered_map<uint64_t, FnProcessImpacts> fnHash;
	static unordered_map<uint64_t, FnUpdate> fnHash2;
	static unordered_map<uint64_t, FnInitialize> fnHash3;
};

unordered_map<uint64_t, HookProjectile::FnProcessImpacts> HookProjectile::fnHash;
unordered_map<uint64_t, HookProjectile::FnUpdate> HookProjectile::fnHash2;
unordered_map<uint64_t, HookProjectile::FnInitialize> HookProjectile::fnHash3;

void HookProjectileFunctions() {
	uint64_t addr = ArrowProjectile::VTABLE[0].address();
	_MESSAGE("Patching ArrowProjectile %llx", addr);
	HookProjectile::HookProcessImpacts(addr);
	HookProjectile::HookUpdate(addr);
	HookProjectile::HookInitialize(addr);
}

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	pcam = PlayerCamera::GetSingleton();
	//hookProj = (BGSProjectile*)GetFormFromMod("GrapplingHook.esm", 0x800);
	hookProj = (BGSProjectile*)TESForm::GetFormByID(0x3010B7F);
	_MESSAGE("Hook Projectile %llx", hookProj);
	rope = (BGSBendableSpline*)TESForm::GetFormByID(0x1D971);
	_MESSAGE("Spline %llx", rope);
	lastProj = nullptr;
	temp = nullptr;
	HookProjectileFunctions();
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor"sv);
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical(FMT_STRING("unsupported runtime v{}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		//게임 데이터가 모두 로드된 시점 (메인화면이 나오기 직전)
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
		}
	});
	return true;
}
