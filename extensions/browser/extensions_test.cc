// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_test.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "content/public/common/content_client.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/test/test_content_utility_client.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"

namespace {

std::unique_ptr<content::TestBrowserContext> CreateTestIncognitoContext() {
  std::unique_ptr<content::TestBrowserContext> incognito_context =
      std::make_unique<content::TestBrowserContext>();
  incognito_context->set_is_off_the_record(true);
  return incognito_context;
}

class ExtensionTestBrowserContext : public content::TestBrowserContext {
 private:
  void SetCorsOriginAccessListForOrigin(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure) override {
    // This method is called for setting up Extensions, but can be ignored
    // unless actual network requests need to be handled.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(closure));
  }
};

}  // namespace

namespace extensions {

ExtensionsTest::ExtensionsTest(
    std::unique_ptr<content::BrowserTaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)),
      rvh_test_enabler_(
          std::make_unique<content::RenderViewHostTestEnabler>()) {}

ExtensionsTest::~ExtensionsTest() {
  // Destroy the task runners before nulling the browser/utility clients, as
  // posted tasks may use them.
  rvh_test_enabler_.reset();
  task_environment_.reset();
  content::SetUtilityClientForTesting(nullptr);
}

void ExtensionsTest::SetExtensionsBrowserClient(
    std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client) {
  DCHECK(extensions_browser_client);
  DCHECK(!extensions_browser_client_);
  extensions_browser_client_ = std::move(extensions_browser_client);
}

void ExtensionsTest::SetUp() {
  content::ForceInProcessNetworkService(true);
  content_utility_client_ = std::make_unique<TestContentUtilityClient>();
  browser_context_ = std::make_unique<ExtensionTestBrowserContext>();
  incognito_context_ = CreateTestIncognitoContext();

  if (!extensions_browser_client_) {
    extensions_browser_client_ =
        std::make_unique<TestExtensionsBrowserClient>();
  }
  extensions_browser_client_->SetMainContext(browser_context_.get());

  content::SetUtilityClientForTesting(content_utility_client_.get());
  ExtensionsBrowserClient::Set(extensions_browser_client_.get());
  extensions_browser_client_->set_extension_system_factory(
      &extension_system_factory_);
  extensions_browser_client_->SetIncognitoContext(incognito_context_.get());

  // Set up all the dependencies of ExtensionPrefs.
  extension_pref_value_map_.reset(new ExtensionPrefValueMap());
  PrefServiceFactory factory;
  factory.set_user_prefs(new TestingPrefStore());
  factory.set_extension_prefs(new TestingPrefStore());
  user_prefs::PrefRegistrySyncable* pref_registry =
      new user_prefs::PrefRegistrySyncable();
  // Prefs should be registered before the PrefService is created.
  ExtensionPrefs::RegisterProfilePrefs(pref_registry);
  pref_service_ = factory.Create(pref_registry);

  std::unique_ptr<ExtensionPrefs> extension_prefs(ExtensionPrefs::Create(
      browser_context(), pref_service_.get(),
      browser_context()->GetPath().AppendASCII("Extensions"),
      extension_pref_value_map_.get(), false /* extensions_disabled */,
      std::vector<EarlyExtensionPrefsObserver*>()));

  ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(
      browser_context(), std::move(extension_prefs));

  // Crashing here? Don't use this class in Chrome's unit_tests. See header.
  BrowserContextDependencyManager::GetInstance()
      ->CreateBrowserContextServicesForTest(browser_context_.get());
  BrowserContextDependencyManager::GetInstance()
      ->CreateBrowserContextServicesForTest(incognito_context_.get());
}

void ExtensionsTest::TearDown() {
  // Allows individual tests to have BrowserContextKeyedServiceFactory objects
  // as member variables instead of singletons. The individual services will be
  // cleaned up before the factories are destroyed.
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      incognito_context_.get());
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context_.get());

  extensions_browser_client_.reset();
  ExtensionsBrowserClient::Set(nullptr);

  incognito_context_.reset();
  browser_context_.reset();
  pref_service_.reset();
}

}  // namespace extensions
