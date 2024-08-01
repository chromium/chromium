// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/power_api.h"

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "content/public/browser/device_service.h"
#include "extensions/browser/api/power/activity_reporter_delegate.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/power.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace extensions {

namespace {

const char kWakeLockDescription[] = "extension";

device::mojom::WakeLockType LevelToWakeLockType(api::power::Level level) {
  switch (level) {
    case api::power::Level::kSystem:
      return device::mojom::WakeLockType::kPreventAppSuspension;
    case api::power::Level::kDisplay:  // fallthrough
    case api::power::Level::kNone:
      return device::mojom::WakeLockType::kPreventDisplaySleep;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unhandled power level: " << api::power::ToString(level);
  return device::mojom::WakeLockType::kPreventDisplaySleep;
}

base::LazyInstance<BrowserContextKeyedAPIFactory<PowerAPI>>::DestructorAtExit
    g_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ExtensionFunction::ResponseAction PowerRequestKeepAwakeFunction::Run() {
  std::optional<api::power::RequestKeepAwake::Params> params =
      api::power::RequestKeepAwake::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  PowerAPI::Get(browser_context())->AddRequest(extension_id(), params->level);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction PowerReleaseKeepAwakeFunction::Run() {
  PowerAPI::Get(browser_context())->RemoveRequest(extension_id());
  return RespondNow(NoArguments());
}

#if BUILDFLAG(IS_CHROMEOS)
ExtensionFunction::ResponseAction PowerReportActivityFunction::Run() {
  std::optional<std::string> error =
      extensions::ActivityReporterDelegate::GetDelegate()->ReportActivity();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
  return RespondNow(NoArguments());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// static
PowerAPI* PowerAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<PowerAPI>::Get(context);
}

// static
BrowserContextKeyedAPIFactory<PowerAPI>* PowerAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void PowerAPI::AddRequest(const ExtensionId& extension_id,
                          api::power::Level level) {
  extension_levels_[extension_id] = level;
  UpdateWakeLock();
}

void PowerAPI::RemoveRequest(const ExtensionId& extension_id) {
  extension_levels_.erase(extension_id);
  UpdateWakeLock();
}

void PowerAPI::SetWakeLockFunctionsForTesting(
    ActivateWakeLockFunction activate_function,
    CancelWakeLockFunction cancel_function) {
  activate_wake_lock_function_ =
      !activate_function.is_null()
          ? std::move(activate_function)
          : base::BindRepeating(&PowerAPI::ActivateWakeLock,
                                base::Unretained(this));
  cancel_wake_lock_function_ =
      !cancel_function.is_null()
          ? std::move(cancel_function)
          : base::BindRepeating(&PowerAPI::CancelWakeLock,
                                base::Unretained(this));
}

void PowerAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionReason reason) {
  RemoveRequest(extension->id());
  UpdateWakeLock();
}

PowerAPI::PowerAPI(content::BrowserContext* context)
    : browser_context_(context),
      activate_wake_lock_function_(
          base::BindRepeating(&PowerAPI::ActivateWakeLock,
                              base::Unretained(this))),
      cancel_wake_lock_function_(base::BindRepeating(&PowerAPI::CancelWakeLock,
                                                     base::Unretained(this))),
      is_wake_lock_active_(false),
      current_level_(api::power::Level::kSystem) {
  ExtensionRegistry::Get(browser_context_)->AddObserver(this);
}

PowerAPI::~PowerAPI() = default;

void PowerAPI::UpdateWakeLock() {
  if (extension_levels_.empty()) {
    cancel_wake_lock_function_.Run();
    return;
  }

  api::power::Level new_level = api::power::Level::kSystem;
  for (ExtensionLevelMap::const_iterator it = extension_levels_.begin();
       it != extension_levels_.end(); ++it) {
    if (it->second == api::power::Level::kDisplay) {
      new_level = it->second;
    }
  }

  if (!is_wake_lock_active_ || new_level != current_level_) {
    device::mojom::WakeLockType type = LevelToWakeLockType(new_level);
    activate_wake_lock_function_.Run(type);
    current_level_ = new_level;
  }
}

void PowerAPI::Shutdown() {
  // Unregister here rather than in the d'tor; otherwise this call will recreate
  // the already-deleted ExtensionRegistry.
  ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
  cancel_wake_lock_function_.Run();
}

void PowerAPI::ActivateWakeLock(device::mojom::WakeLockType type) {
  GetWakeLock()->ChangeType(type, base::DoNothing());
  if (!is_wake_lock_active_) {
    GetWakeLock()->RequestWakeLock();
    is_wake_lock_active_ = true;
  }
}

void PowerAPI::CancelWakeLock() {
  if (is_wake_lock_active_) {
    GetWakeLock()->CancelWakeLock();
    is_wake_lock_active_ = false;
  }
}

device::mojom::WakeLock* PowerAPI::GetWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (wake_lock_) {
    return wake_lock_.get();
  }

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  content::GetDeviceService().BindWakeLockProvider(
      wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      LevelToWakeLockType(current_level_),
      device::mojom::WakeLockReason::kOther, kWakeLockDescription,
      wake_lock_.BindNewPipeAndPassReceiver());
  return wake_lock_.get();
}

}  // namespace extensions
