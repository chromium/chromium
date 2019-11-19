// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_
#define EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/events/lazy_event_dispatch_util.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/update_observer.h"
#include "extensions/common/api/runtime.h"

namespace base {
class Version;
}

namespace content {
class BrowserContext;
}

class PrefRegistrySimple;

namespace extensions {

namespace api {
namespace runtime {
struct PlatformInfo;
}
}

class Extension;
class ExtensionRegistry;

// Runtime API dispatches onStartup, onInstalled, and similar events to
// extensions. There is one instance shared between a browser context and
// its related incognito instance.
class RuntimeAPI : public BrowserContextKeyedAPI,
                   public ExtensionRegistryObserver,
                   public UpdateObserver,
                   public ProcessManagerObserver,
                   public LazyEventDispatchUtil::Observer {
 public:
  // The status of the restartAfterDelay request.
  enum class RestartAfterDelayStatus {
    // The request was made by a different extension other than the first one to
    // invoke the restartAfterDelay runtime API.
    FAILED_NOT_FIRST_EXTENSION,

    // The request came too soon after a previous restart induced by the
    // restartAfterDelay API. It failed to be scheduled as requested, and was
    // instead throttled.
    FAILED_THROTTLED,

    // Any previously scheduled restart was successfully canceled.
    SUCCESS_RESTART_CANCELED,

    // A restart was successfully scheduled.
    SUCCESS_RESTART_SCHEDULED,
  };

  static BrowserContextKeyedAPIFactory<RuntimeAPI>* GetFactoryInstance();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit RuntimeAPI(content::BrowserContext* context);
  ~RuntimeAPI() override;

  void ReloadExtension(const std::string& extension_id);
  bool CheckForUpdates(const std::string& extension_id,
                       const RuntimeAPIDelegate::UpdateCheckCallback& callback);
  void OpenURL(const GURL& uninstall_url);
  bool GetPlatformInfo(api::runtime::PlatformInfo* info);
  bool RestartDevice(std::string* error_message);

  RestartAfterDelayStatus RestartDeviceAfterDelay(
      const std::string& extension_id,
      int seconds_from_now);

  bool OpenOptionsPage(const Extension* extension,
                       content::BrowserContext* browser_context);

 private:
  friend class BrowserContextKeyedAPIFactory<RuntimeAPI>;
  friend class RestartAfterDelayApiTest;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  // LazyEventDispatchUtil::Observer:
  void OnExtensionInstalledAndLoaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Version& previous_version) override;

  // Cancels any previously scheduled restart request.
  void MaybeCancelRunningDelayedRestartTimer();

  // Handler for the signal from ExtensionSystem::ready().
  void OnExtensionsReady();

  RestartAfterDelayStatus ScheduleDelayedRestart(const base::Time& now,
                                                 int seconds_from_now);

  // Called when the delayed restart timer times out so that it attempts to
  // execute the restart request scheduled earlier.
  void OnDelayedRestartTimerTimeout();

  // BrowserContextKeyedAPI implementation:
  static const char* service_name() { return "RuntimeAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;
  void Shutdown() override;

  // extensions::UpdateObserver overrides:
  void OnAppUpdateAvailable(const Extension* extension) override;
  void OnChromeUpdateAvailable() override;

  // ProcessManagerObserver implementation:
  void OnBackgroundHostStartup(const Extension* extension) override;

  void AllowNonKioskAppsInRestartAfterDelayForTesting();

  void set_min_duration_between_restarts_for_testing(base::TimeDelta delta) {
    minimum_duration_between_restarts_ = delta;
  }

  std::unique_ptr<RuntimeAPIDelegate> delegate_;

  content::BrowserContext* browser_context_;

  content::NotificationRegistrar registrar_;

  // Listen to extension notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_{this};

  // The ID of the first extension to call the restartAfterDelay API. Any other
  // extensions to call this API after that will fail.
  std::string schedule_restart_first_extension_id_;

  // The timer that will trigger a device restart when it times out.
  base::OneShotTimer restart_after_delay_timer_;

  // The minimum allowed duration between two successive restarts caused by
  // restartAfterDelay calls.
  base::TimeDelta minimum_duration_between_restarts_;

  // The last restart time which was a result of a successful call to
  // chrome.runtime.restartAfterDelay().
  base::Time last_delayed_restart_time_;

  // True if we should dispatch the chrome.runtime.onInstalled event with
  // reason "chrome_update" upon loading each extension.
  bool dispatch_chrome_updated_event_;

  bool did_read_delayed_restart_preferences_;
  bool was_last_restart_due_to_delayed_restart_api_;

  base::WeakPtrFactory<RuntimeAPI> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RuntimeAPI);
};

template <>
void BrowserContextKeyedAPIFactory<RuntimeAPI>::DeclareFactoryDependencies();

class RuntimeEventRouter {
 public:
  // Dispatches the onStartup event to all currently-loaded extensions.
  static void DispatchOnStartupEvent(content::BrowserContext* context,
                                     const std::string& extension_id);

  // Dispatches the onInstalled event to the given extension.
  static void DispatchOnInstalledEvent(content::BrowserContext* context,
                                       const std::string& extension_id,
                                       const base::Version& old_version,
                                       bool chrome_updated);

  // Dispatches the onUpdateAvailable event to the given extension.
  static void DispatchOnUpdateAvailableEvent(
      content::BrowserContext* context,
      const std::string& extension_id,
      const base::DictionaryValue* manifest);

  // Dispatches the onBrowserUpdateAvailable event to all extensions.
  static void DispatchOnBrowserUpdateAvailableEvent(
      content::BrowserContext* context);

  // Dispatches the onRestartRequired event to the given app.
  static void DispatchOnRestartRequiredEvent(
      content::BrowserContext* context,
      const std::string& app_id,
      api::runtime::OnRestartRequiredReason reason);

  // Does any work needed at extension uninstall (e.g. load uninstall url).
  static void OnExtensionUninstalled(content::BrowserContext* context,
                                     const std::string& extension_id,
                                     UninstallReason reason);
};

class RuntimeGetBackgroundPageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getBackgroundPage",
                             RUNTIME_GETBACKGROUNDPAGE)

 protected:
  ~RuntimeGetBackgroundPageFunction() override {}
  ResponseAction Run() override;

 private:
  void OnPageLoaded(
      std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info);
};

class RuntimeOpenOptionsPageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.openOptionsPage", RUNTIME_OPENOPTIONSPAGE)

 protected:
  ~RuntimeOpenOptionsPageFunction() override {}
  ResponseAction Run() override;
};

class RuntimeSetUninstallURLFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.setUninstallURL", RUNTIME_SETUNINSTALLURL)

 protected:
  ~RuntimeSetUninstallURLFunction() override {}
  ResponseAction Run() override;
};

class RuntimeReloadFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.reload", RUNTIME_RELOAD)

 protected:
  ~RuntimeReloadFunction() override {}
  ResponseAction Run() override;
};

class RuntimeRequestUpdateCheckFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.requestUpdateCheck",
                             RUNTIME_REQUESTUPDATECHECK)

 protected:
  ~RuntimeRequestUpdateCheckFunction() override {}
  ResponseAction Run() override;

 private:
  void CheckComplete(const RuntimeAPIDelegate::UpdateCheckResult& result);
};

class RuntimeRestartFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.restart", RUNTIME_RESTART)

 protected:
  ~RuntimeRestartFunction() override {}
  ResponseAction Run() override;
};

class RuntimeRestartAfterDelayFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.restartAfterDelay",
                             RUNTIME_RESTARTAFTERDELAY)

 protected:
  ~RuntimeRestartAfterDelayFunction() override {}
  ResponseAction Run() override;
};

class RuntimeGetPlatformInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getPlatformInfo", RUNTIME_GETPLATFORMINFO)

 protected:
  ~RuntimeGetPlatformInfoFunction() override {}
  ResponseAction Run() override;
};

class RuntimeGetPackageDirectoryEntryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getPackageDirectoryEntry",
                             RUNTIME_GETPACKAGEDIRECTORYENTRY)

 protected:
  ~RuntimeGetPackageDirectoryEntryFunction() override {}
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_
