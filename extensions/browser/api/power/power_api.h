// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_POWER_POWER_API_H_
#define EXTENSIONS_BROWSER_API_POWER_POWER_API_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/power.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Implementation of the chrome.power.requestKeepAwake API.
class PowerRequestKeepAwakeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("power.requestKeepAwake", POWER_REQUESTKEEPAWAKE)

 protected:
  ~PowerRequestKeepAwakeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Implementation of the chrome.power.releaseKeepAwake API.
class PowerReleaseKeepAwakeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("power.releaseKeepAwake", POWER_RELEASEKEEPAWAKE)

 protected:
  ~PowerReleaseKeepAwakeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Handles calls made via the chrome.power API. There is a separate instance of
// this class for each profile, as requests are tracked by extension ID, but a
// regular and incognito profile will share the same instance.
class PowerAPI : public BrowserContextKeyedAPI,
                 public extensions::ExtensionRegistryObserver {
 public:
  using ActivateWakeLockFunction =
      base::RepeatingCallback<void(device::mojom::WakeLockType)>;
  using CancelWakeLockFunction = base::RepeatingCallback<void()>;

  static PowerAPI* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PowerAPI>* GetFactoryInstance();

  // Map from extension ID to the corresponding level for each extension
  // that has an outstanding request.
  using ExtensionLevelMap = std::map<std::string, api::power::Level>;
  const ExtensionLevelMap& extension_levels() const {
    return extension_levels_;
  }

  // Adds an extension lock at |level| for |extension_id|, replacing the
  // extension's existing lock, if any.
  void AddRequest(const std::string& extension_id, api::power::Level level);

  // Removes an extension lock for an extension. Calling this for an
  // extension id without a lock will do nothing.
  void RemoveRequest(const std::string& extension_id);

  // Replaces the functions that will be called to activate and cancel the wake
  // lock. Passing empty callbacks will revert to the default.
  void SetWakeLockFunctionsForTesting(
      ActivateWakeLockFunction activate_function,
      CancelWakeLockFunction cancel_function);

  // Overridden from extensions::ExtensionRegistryObserver.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

 private:
  friend class BrowserContextKeyedAPIFactory<PowerAPI>;

  explicit PowerAPI(content::BrowserContext* context);
  ~PowerAPI() override;

  // Updates wake lock status and |current_level_| after iterating
  // over |extension_levels_|.
  void UpdateWakeLock();

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "PowerAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsCreatedWithBrowserContext = false;
  void Shutdown() override;

  // Activates the wake lock with the type. |is_wake_lock_active_| is set true.
  void ActivateWakeLock(device::mojom::WakeLockType type);

  // Cancels the current wake lock if it is in active state.
  // |is_wake_lock_active_| is set false.
  void CancelWakeLock();

  // Returns the raw pointer of the bound |wake_lock_|. This function is used
  // only inside ActivateWakeLock() and CancelWakeLock() to perform the wake
  // lock mojo calls. The |wake_lock_| is bound and the wake lock mojo pipe is
  // created only once at the first time the GetWakeLock() is called.
  device::mojom::WakeLock* GetWakeLock();

  content::BrowserContext* browser_context_;

  // Functions that should be called to activate and cancel the wake lock.
  // Tests can change this to record what would've been done instead of
  // actually changing the system power-saving settings.
  ActivateWakeLockFunction activate_wake_lock_function_;
  CancelWakeLockFunction cancel_wake_lock_function_;

  mojo::Remote<device::mojom::WakeLock> wake_lock_;
  bool is_wake_lock_active_;

  // Current level used by wake lock.
  api::power::Level current_level_;

  // Outstanding requests.
  ExtensionLevelMap extension_levels_;

  DISALLOW_COPY_AND_ASSIGN(PowerAPI);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_POWER_POWER_API_H_
