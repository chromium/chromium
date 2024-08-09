// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_SYSTEM_H_
#define EXTENSIONS_BROWSER_EXTENSION_SYSTEM_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS) && \
    !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#error "Extensions must be enabled"
#endif

namespace base {
class OneShotEvent;
}

namespace content {
class BrowserContext;
}

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

class AppSorting;
class ContentVerifier;
class Extension;
class ExtensionService;
class ExtensionSet;
class ManagementPolicy;
class QuotaService;
class ServiceWorkerManager;
class StateStore;
class UserScriptManager;
enum class UnloadedExtensionReason;

// ExtensionSystem manages the lifetime of many of the services used by the
// extensions and apps system, and it handles startup and shutdown as needed.
// Eventually, we'd like to make more of these services into KeyedServices in
// their own right.
class ExtensionSystem : public KeyedService {
 public:
  // A callback to be executed when InstallUpdate finishes.
  using InstallUpdateCallback =
      base::OnceCallback<void(const std::optional<CrxInstallError>& result)>;

  ExtensionSystem();
  ~ExtensionSystem() override;

  // Returns the instance for the given browser context, or NULL if none.
  static ExtensionSystem* Get(content::BrowserContext* context);

  // Initializes extensions machinery.
  // Component extensions are always enabled, external and user extensions are
  // controlled (for both incognito and non-incognito profiles) by the
  // |extensions_enabled| flag passed to non-incognito initialization.
  // These calls should occur after the profile IO data is initialized,
  // as extensions initialization depends on that.
  virtual void InitForRegularProfile(bool extensions_enabled) = 0;

  // The ExtensionService is created at startup. ExtensionService is only
  // defined in Chrome.
  virtual ExtensionService* extension_service() = 0;

  // The class controlling whether users are permitted to perform certain
  // actions on extensions (install, uninstall, disable, etc.).
  // The ManagementPolicy is created at startup.
  virtual ManagementPolicy* management_policy() = 0;

  // The ServiceWorkerManager is created at startup.
  virtual ServiceWorkerManager* service_worker_manager() = 0;

  // The UserScriptManager is created at startup.
  virtual UserScriptManager* user_script_manager() = 0;

  // The StateStore is created at startup.
  virtual StateStore* state_store() = 0;

  // The rules store is created at startup.
  virtual StateStore* rules_store() = 0;

  // The dynamic user scripts store is created at startup.
  virtual StateStore* dynamic_user_scripts_store() = 0;

  // Returns the |ValueStore| factory created at startup.
  virtual scoped_refptr<value_store::ValueStoreFactory> store_factory() = 0;

  // Returns the QuotaService that limits calls to certain extension functions.
  // Lives on the UI thread. Created at startup.
  virtual QuotaService* quota_service() = 0;

  // Returns the AppSorting which provides an ordering for all installed apps.
  virtual AppSorting* app_sorting() = 0;

  // Signaled when the extension system has completed its startup tasks.
  virtual const base::OneShotEvent& ready() const = 0;

  // Whether the extension system is ready.
  virtual bool is_ready() const = 0;

  // Returns the content verifier, if any.
  virtual ContentVerifier* content_verifier() = 0;

  // Get a set of extensions that depend on the given extension.
  // TODO(elijahtaylor): Move SharedModuleService out of chrome/browser
  // so it can be retrieved from ExtensionSystem directly.
  virtual std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) = 0;

  // Install an updated version of |extension_id| with the version given in
  // |unpacked_dir|. If |install_immediately| is true, the system will install
  // the given extension immediately instead of waiting until idle. Ownership
  // of |unpacked_dir| in the filesystem is transferred and implementors of
  // this function are responsible for cleaning it up on errors, etc.
  virtual void InstallUpdate(const ExtensionId& extension_id,
                             const std::string& public_key,
                             const base::FilePath& unpacked_dir,
                             bool install_immediately,
                             InstallUpdateCallback install_update_callback) = 0;

  // Perform various actions depending on the Omaga attributes on the extension.
  virtual void PerformActionBasedOnOmahaAttributes(
      const ExtensionId& extension_id,
      const base::Value::Dict& attributes) = 0;

  // Attempts finishing installation of an update for an extension with the
  // specified id, when installation of that extension was previously delayed.
  // |install_immediately| - Install the extension should be installed if it is
  // currently in use.
  // Returns whether the extension installation was finished.
  virtual bool FinishDelayedInstallationIfReady(const ExtensionId& extension_id,
                                                bool install_immediately) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_SYSTEM_H_
