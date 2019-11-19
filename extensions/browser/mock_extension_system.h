// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_
#define EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_

#include "base/macros.h"
#include "base/one_shot_event.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"

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
  ~MockExtensionSystem() override;

  content::BrowserContext* browser_context() { return browser_context_; }

  // ExtensionSystem overrides:
  void InitForRegularProfile(bool extensions_enabled) override;
  ExtensionService* extension_service() override;
  RuntimeData* runtime_data() override;
  ManagementPolicy* management_policy() override;
  ServiceWorkerManager* service_worker_manager() override;
  SharedUserScriptMaster* shared_user_script_master() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  scoped_refptr<ValueStoreFactory> store_factory() override;
  InfoMap* info_map() override;
  QuotaService* quota_service() override;
  AppSorting* app_sorting() override;
  const base::OneShotEvent& ready() const override;
  ContentVerifier* content_verifier() override;
  std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const std::string& extension_id,
                     const std::string& public_key,
                     const base::FilePath& temp_dir,
                     bool install_immediately,
                     InstallUpdateCallback install_update_callback) override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;

 private:
  content::BrowserContext* browser_context_;
  base::OneShotEvent ready_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionSystem);
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

  ~MockExtensionSystemFactory() override {}

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new T(context);
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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockExtensionSystemFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOCK_EXTENSION_SYSTEM_H_
