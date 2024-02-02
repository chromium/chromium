// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_
#define EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_

#include "base/memory/raw_ptr.h"
#include "base/one_shot_event.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// An empty ExtensionSystem for testing. Tests that need only specific
// parts of ExtensionSystem should derive from this class and override
// functions as needed. To use this, use
// TestExtensionsBrowserClient::set_extension_system_factory
// with the MockExtensionSystemFactory below.
class MockExtensionSystem : public ExtensionSystem {
 public:
  using InstallUpdateCallback = ExtensionSystem::InstallUpdateCallback;

  explicit MockExtensionSystem(content::BrowserContext* context);

  MockExtensionSystem(const MockExtensionSystem&) = delete;
  MockExtensionSystem& operator=(const MockExtensionSystem&) = delete;

  ~MockExtensionSystem() override;

  content::BrowserContext* browser_context() { return browser_context_; }

  void SetReady();

  // ExtensionSystem overrides:
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
  raw_ptr<content::BrowserContext> browser_context_;
  base::OneShotEvent ready_;
};

// A factory to create a MockExtensionSystem. Sample use:
//
// MockExtensionSystemFactory<MockExtensionSystemSubclass> factory;
// TestExtensionsBrowserClient::set_extension_system_factory(factory);
template <typename T>
class MockExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  MockExtensionSystemFactory()
      : ExtensionSystemProvider(
            "MockExtensionSystem",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(ExtensionRegistryFactory::GetInstance());
  }

  MockExtensionSystemFactory(const MockExtensionSystemFactory&) = delete;
  MockExtensionSystemFactory& operator=(const MockExtensionSystemFactory&) =
      delete;

  ~MockExtensionSystemFactory() override = default;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    return std::make_unique<T>(context);
  }
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    // Separate instance in incognito.
    return context;
  }

  // ExtensionSystemProvider overrides:
  ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) override {
    return static_cast<ExtensionSystem*>(
        GetServiceForBrowserContext(context, true));
  }
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_
