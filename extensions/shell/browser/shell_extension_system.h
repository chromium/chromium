// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

class ShellExtensionLoader;

// A simplified version of ExtensionSystem for app_shell. Allows
// app_shell to skip initialization of services it doesn't need.
class ShellExtensionSystem : public ExtensionSystem {
 public:
  using InstallUpdateCallback = ExtensionSystem::InstallUpdateCallback;
  explicit ShellExtensionSystem(content::BrowserContext* browser_context);

  ShellExtensionSystem(const ShellExtensionSystem&) = delete;
  ShellExtensionSystem& operator=(const ShellExtensionSystem&) = delete;

  ~ShellExtensionSystem() override;

  // Loads an unpacked extension from a directory. Returns the extension on
  // success, or nullptr otherwise.
  const Extension* LoadExtension(const base::FilePath& extension_dir);

  // Loads an unpacked platform app from a directory. Returns the extension on
  // success, or nullptr otherwise.
  // Currently this just calls LoadExtension, as apps are not loaded differently
  // than other extensions. Use LaunchApp() to actually launch the loaded app.
  const Extension* LoadApp(const base::FilePath& app_dir);

  // Finish initialization for the shell extension system.
  void FinishInitialization();

  // Launch the app with id |extension_id|.
  void LaunchApp(const ExtensionId& extension_id);

  // Reloads the extension with id |extension_id|.
  void ReloadExtension(const ExtensionId& extension_id);

  // KeyedService implementation:
  void Shutdown() override;

  // ExtensionSystem implementation:
  void InitForRegularProfile(bool extensions_enabled) override;
  ExtensionService* extension_service() override;
  ManagementPolicy* management_policy() override;
  ServiceWorkerManager* service_worker_manager() override;
  UserScriptManager* user_script_manager() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  StateStore* dynamic_user_scripts_store() override;
  scoped_refptr<value_store::ValueStoreFactory> store_factory() override;
  QuotaService* quota_service() override;
  AppSorting* app_sorting() override;
  const base::OneShotEvent& ready() const override;
  bool is_ready() const override;
  ContentVerifier* content_verifier() override;
  std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const ExtensionId& extension_id,
                     const std::string& public_key,
                     const base::FilePath& temp_dir,
                     bool install_immediately,
                     InstallUpdateCallback install_update_callback) override;
  void PerformActionBasedOnOmahaAttributes(
      const ExtensionId& extension_id,
      const base::Value::Dict& attributes) override;
  bool FinishDelayedInstallationIfReady(const ExtensionId& extension_id,
                                        bool install_immediately) override;

 private:
  raw_ptr<content::BrowserContext> browser_context_;  // Not owned.

  std::unique_ptr<ServiceWorkerManager> service_worker_manager_;
  std::unique_ptr<QuotaService> quota_service_;
  std::unique_ptr<AppSorting> app_sorting_;
  std::unique_ptr<UserScriptManager> user_script_manager_;

  std::unique_ptr<ShellExtensionLoader> extension_loader_;

  scoped_refptr<value_store::ValueStoreFactory> store_factory_;

  // Signaled when the extension system has completed its startup tasks.
  base::OneShotEvent ready_;

  base::WeakPtrFactory<ShellExtensionSystem> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_
