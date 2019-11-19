// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/power_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/power.h"
#include "extensions/common/extension.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace extensions {

namespace {

const char kWakeLockDescription[] = "extension";

device::mojom::WakeLockType LevelToWakeLockType(api::power::Level level) {
  switch (level) {
    case api::power::LEVEL_SYSTEM:
      return device::mojom::WakeLockType::kPreventAppSuspension;
    case api::power::LEVEL_DISPLAY:  // fallthrough
    case api::power::LEVEL_NONE:
      return device::mojom::WakeLockType::kPreventDisplaySleep;
  }
  NOTREACHED() << "Unhandled level " << level;
  return device::mojom::WakeLockType::kPreventDisplaySleep;
}

base::LazyInstance<BrowserContextKeyedAPIFactory<PowerAPI>>::DestructorAtExit
    g_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ExtensionFunction::ResponseAction PowerRequestKeepAwakeFunction::Run() {
  std::unique_ptr<api::power::RequestKeepAwake::Params> params(
      api::power::RequestKeepAwake::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  PowerAPI::Get(browser_context())->AddRequest(extension_id(), params->level);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction PowerReleaseKeepAwakeFunction::Run() {
  PowerAPI::Get(browser_context())->RemoveRequest(extension_id());
  return RespondNow(NoArguments());
}

// static
PowerAPI* PowerAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<PowerAPI>::Get(context);
}

// static
BrowserContextKeyedAPIFactory<PowerAPI>* PowerAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void PowerAPI::AddRequest(const std::string& extension_id,
                          api::power::Level level) {
  extension_levels_[extension_id] = level;
  UpdateWakeLock();
}

void PowerAPI::RemoveRequest(const std::string& extension_id) {
  extension_levels_.erase(extension_id);
  UpdateWakeLock();
}

void PowerAPI::SetWakeLockFunctionsForTesting(
    const ActivateWakeLockFunction& activate_function,
    const CancelWakeLockFunction& cancel_function) {
  activate_wake_lock_function_ =
      !activate_function.is_null()
          ? activate_function
          : base::Bind(&PowerAPI::ActivateWakeLock, base::Unretained(this));
  cancel_wake_lock_function_ =
      !cancel_function.is_null()
          ? cancel_function
          : base::Bind(&PowerAPI::CancelWakeLock, base::Unretained(this));
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
          base::Bind(&PowerAPI::ActivateWakeLock, base::Unretained(this))),
      cancel_wake_lock_function_(
          base::Bind(&PowerAPI::CancelWakeLock, base::Unretained(this))),
      is_wake_lock_active_(false),
      current_level_(api::power::LEVEL_SYSTEM) {
  ExtensionRegistry::Get(browser_context_)->AddObserver(this);
}

PowerAPI::~PowerAPI() {
}

void PowerAPI::UpdateWakeLock() {
  if (extension_levels_.empty()) {
    cancel_wake_lock_function_.Run();
    return;
  }

  api::power::Level new_level = api::power::LEVEL_SYSTEM;
  for (ExtensionLevelMap::const_iterator it = extension_levels_.begin();
       it != extension_levels_.end(); ++it) {
    if (it->second == api::power::LEVEL_DISPLAY)
      new_level = it->second;
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
  if (wake_lock_)
    return wake_lock_.get();

  auto* connector = content::GetSystemConnector();
  DCHECK(connector);
  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  connector->Connect(device::mojom::kServiceName,
                     wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      LevelToWakeLockType(current_level_),
      device::mojom::WakeLockReason::kOther, kWakeLockDescription,
      wake_lock_.BindNewPipeAndPassReceiver());
  return wake_lock_.get();
}

}  // namespace extensions
