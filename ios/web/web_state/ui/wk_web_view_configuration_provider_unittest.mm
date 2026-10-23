// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <WebKit/WebKit.h>

#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/uuid.h"
#import "ios/web/extension/extension_controller_impl.h"
#import "ios/web/public/extension/extension_controller.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"
#import "ios/web/web_state/ui/wk_content_rule_list_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::TestFuture;

namespace web {

class WKWebViewConfigurationProviderTest : public PlatformTest {
 public:
  WKWebViewConfigurationProviderTest()
      : web_client_(std::make_unique<FakeWebClient>()) {}

 protected:
  // Helper to create a WKWebViewConfigurationProvider with a mock rule list
  // provider for testing. This can call the private constructor because
  // WKWebViewConfigurationProvider declares this test class as a friend.
  std::unique_ptr<WKWebViewConfigurationProvider>
  CreateProviderWithMockRuleListProvider(
      BrowserState* browser_state,
      std::unique_ptr<WKContentRuleListProvider> rule_list_provider) {
    return base::WrapUnique(new WKWebViewConfigurationProvider(
        browser_state, std::move(rule_list_provider)));
  }

  // Returns WKWebViewConfigurationProvider for given `browser_state`.
  WKWebViewConfigurationProvider& GetProvider(
      BrowserState* browser_state) const {
    return WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  }

  FakeWebClient* GetWebClient() {
    return static_cast<FakeWebClient*>(web_client_.Get());
  }

  web::ScopedTestingWebClient web_client_;
  base::test::TaskEnvironment task_environment_;
};

namespace {

// Tests Non-OffTheRecord configuration.
TEST_F(WKWebViewConfigurationProviderTest, NoneOffTheRecordConfiguration) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  browser_state.SetOffTheRecord(false);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);
  EXPECT_TRUE(provider.GetWebViewConfiguration().websiteDataStore.persistent);
}

// Tests OffTheRecord configuration.
TEST_F(WKWebViewConfigurationProviderTest, OffTheRecordConfiguration) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  browser_state.SetOffTheRecord(true);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  ASSERT_TRUE(config);
  EXPECT_FALSE(config.websiteDataStore.persistent);
}

// Tests that SetUniversalOptOutEnabled updates defaultWebpagePreferences for
// iOS 27+, including on configurations copied before Universal Opt-Out is
// enabled.
#if defined(__IPHONE_27_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_0
TEST_F(WKWebViewConfigurationProviderTest, UniversalOptOutConfiguration)
API_AVAILABLE(ios(27)) {
  if (@available(iOS 27, *)) {
    FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
    GetWebClient()->SetUniversalOptOutState(UniversalOptOutState::kEligible);
    WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);
    WKWebViewConfiguration* existing_config =
        provider.GetWebViewConfiguration();
    provider.SetUniversalOptOutEnabled(false);
    EXPECT_FALSE(
        existing_config.defaultWebpagePreferences.globalPrivacyControlEnabled);
    EXPECT_FALSE(provider.GetWebViewConfiguration()
                     .defaultWebpagePreferences.globalPrivacyControlEnabled);

    provider.SetUniversalOptOutEnabled(true);
    EXPECT_TRUE(
        existing_config.defaultWebpagePreferences.globalPrivacyControlEnabled);
    EXPECT_TRUE(provider.GetWebViewConfiguration()
                    .defaultWebpagePreferences.globalPrivacyControlEnabled);
  }
}

// Tests that ResetWithWebViewConfiguration updates defaultWebpagePreferences
// based on GetUniversalOptOutState for iOS 27+.
TEST_F(WKWebViewConfigurationProviderTest,
       ResetWithWebViewConfigurationUniversalOptOut)
API_AVAILABLE(ios(27)) {
  if (@available(iOS 27, *)) {
    FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
    GetWebClient()->SetUniversalOptOutState(UniversalOptOutState::kNotEligible);
    WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);
    provider.ResetWithWebViewConfiguration(nil);
    EXPECT_FALSE(provider.GetWebViewConfiguration()
                     .defaultWebpagePreferences.globalPrivacyControlEnabled);

    GetWebClient()->SetUniversalOptOutState(UniversalOptOutState::kEligible);
    provider.ResetWithWebViewConfiguration(nil);
    EXPECT_FALSE(provider.GetWebViewConfiguration()
                     .defaultWebpagePreferences.globalPrivacyControlEnabled);

    GetWebClient()->SetUniversalOptOutState(UniversalOptOutState::kEnabled);
    provider.ResetWithWebViewConfiguration(nil);
    EXPECT_TRUE(provider.GetWebViewConfiguration()
                    .defaultWebpagePreferences.globalPrivacyControlEnabled);
  }
}
#endif

// Tests that internal configuration object can not be changed by clients.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationProtection) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/false);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  WKPreferences* prefs = [config preferences];
  WKUserContentController* userContentController =
      [config userContentController];

  // Change the properties of returned configuration object.
  FakeBrowserState other_browser_state(/*use_unique_storage_uuid=*/false);
  WKWebViewConfiguration* other_wk_web_view_configuration =
      GetProvider(&other_browser_state).GetWebViewConfiguration();
  ASSERT_TRUE(other_wk_web_view_configuration);
  config.preferences = other_wk_web_view_configuration.preferences;
  config.userContentController =
      other_wk_web_view_configuration.userContentController;

  // Make sure that the properties of internal configuration were not changed.
  EXPECT_TRUE(provider.GetWebViewConfiguration().preferences);
  EXPECT_EQ(prefs, provider.GetWebViewConfiguration().preferences);
  EXPECT_TRUE(provider.GetWebViewConfiguration().userContentController);
  EXPECT_EQ(userContentController,
            provider.GetWebViewConfiguration().userContentController);
}

// Tests that the configuration are deallocated after `Purge` call.
TEST_F(WKWebViewConfigurationProviderTest, Purge) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);

  __weak id config;
  @autoreleasepool {  // Make sure that resulting copy is deallocated.
    id strong_config = GetProvider(&browser_state).GetWebViewConfiguration();
    config = strong_config;
    ASSERT_TRUE(config);
  }

  // No configuration after `Purge` call.
  GetProvider(&browser_state).Purge();
  EXPECT_FALSE(config);
}

class MockWKContentRuleListProvider : public WKContentRuleListProvider {
 public:
  explicit MockWKContentRuleListProvider(const base::FilePath& path)
      : WKContentRuleListProvider(path) {}
  ~MockWKContentRuleListProvider() override = default;

  MOCK_METHOD(void,
              UpdateRuleList,
              (RuleListKey key,
               std::string json_rules,
               OperationCallback callback),
              (override));
  MOCK_METHOD(void,
              RemoveRuleList,
              (RuleListKey key, OperationCallback callback),
              (override));
};

// Tests that the static content rule lists are created on initialization.
TEST_F(WKWebViewConfigurationProviderTest, StaticContentRuleListsAreCreated) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  auto rule_list_provider =
      std::make_unique<testing::StrictMock<MockWKContentRuleListProvider>>(
          browser_state.GetStatePath());
  EXPECT_CALL(
      *rule_list_provider,
      UpdateRuleList(kBlockLocalResourcesRuleListKey, testing::_, testing::_));
  EXPECT_CALL(
      *rule_list_provider,
      UpdateRuleList(kMixedContentUpgradeRuleListKey, testing::_, testing::_));

  auto provider = CreateProviderWithMockRuleListProvider(
      &browser_state, std::move(rule_list_provider));
}

// Tests that configuration's userContentController has additional scripts
// injected for JavaScriptFeatures configured through the WebClient.
TEST_F(WKWebViewConfigurationProviderTest, JavaScriptFeatureInjection) {
  FakeWebClient* client = GetWebClient();

  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  WKUserContentController* user_content_controller =
      GetProvider(&browser_state)
          .GetWebViewConfiguration()
          .userContentController;
  ASSERT_NE(nil, user_content_controller)
      << "UserContentController should not be nil initially.";
  unsigned long original_script_count =
      [user_content_controller.userScripts count];

  const std::vector<web::JavaScriptFeature::FeatureScript> feature_scripts = {
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "java_script_feature_test_inject_once",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

  std::unique_ptr<web::JavaScriptFeature> feature =
      std::make_unique<web::JavaScriptFeature>(
          web::ContentWorld::kPageContentWorld, feature_scripts);

  client->SetJavaScriptFeatures({feature.get()});
  GetProvider(&browser_state).UpdateScripts();

  // Re-fetch userContentController to ensure we're checking the updated state.
  user_content_controller = GetProvider(&browser_state)
                                .GetWebViewConfiguration()
                                .userContentController;
  ASSERT_NE(nil, user_content_controller)
      << "UserContentController should not be nil after update.";
  EXPECT_GT([user_content_controller.userScripts count], original_script_count);
}

// Tests that observers methods are correctly triggered when observing the
// WKWebViewConfigurationProvider
TEST_F(WKWebViewConfigurationProviderTest, Observers) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  WKWebViewConfigurationProvider* provider = &GetProvider(&browser_state);

  // Register a callback to be notified when website data store is
  // updated and check that it is not invoked as part of the registration.
  __block WKWebsiteDataStore* recorded_data_store = nil;
  base::CallbackListSubscription subscription =
      provider->RegisterWebSiteDataStoreUpdatedCallback(
          base::BindRepeating(^(WKWebsiteDataStore* new_data_store) {
            recorded_data_store = new_data_store;
          }));
  ASSERT_FALSE(recorded_data_store);

  // Check that accessing the WebViewConfiguration for the first time
  // creates a new website date store object.
  WKWebViewConfiguration* config = provider->GetWebViewConfiguration();
  EXPECT_NSEQ(config.websiteDataStore, recorded_data_store);
  // Check that the website data store getter returns the same object with the
  // newly updated data store.
  WKWebsiteDataStore* data_store = provider->GetWebsiteDataStore();
  EXPECT_NSEQ(data_store, recorded_data_store);

  // Check that accessing the WebViewConfiguration again does not create
  // a new website data store object and thus does not invoked the registered
  // callback.
  recorded_data_store = nil;
  config = provider->GetWebViewConfiguration();
  EXPECT_FALSE(recorded_data_store);

  // Check that accessing the WebsiteDataStore again does not create
  // a new website data store object and thus does not invoked the registered
  // callback.
  data_store = provider->GetWebsiteDataStore();
  EXPECT_FALSE(recorded_data_store);
}

// Tests that if -[ResetWithWebViewConfiguration:] copies and applies Chrome's
// initialization logic to the `config` that passed into that method
TEST_F(WKWebViewConfigurationProviderTest, ResetConfiguration) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  WKWebViewConfigurationProvider* provider = &GetProvider(&browser_state);

  WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
  config.allowsInlineMediaPlayback = NO;
  config.preferences.minimumFontSize = 42;
  config.applicationNameForUserAgent = @"TestUserAgent";

  provider->ResetWithWebViewConfiguration(config);

  WKWebViewConfiguration* recorded_configuration =
      provider->GetWebViewConfiguration();
  ASSERT_TRUE(recorded_configuration);

  // Check that preferences are reset/applied using config.
  EXPECT_EQ(config.preferences.minimumFontSize,
            recorded_configuration.preferences.minimumFontSize);

  // Check that top-level configuration properties are preserved.
  EXPECT_NSEQ(recorded_configuration.applicationNameForUserAgent,
              config.applicationNameForUserAgent);

  // Check Chrome's initialization logic has been applied to
  // `recorded_configuration`, where
  // `recorded_configuration.allowsInlineMediaPlayback` should be overwritten by
  // YES.
  EXPECT_EQ(NO, config.allowsInlineMediaPlayback);
  EXPECT_EQ(YES, recorded_configuration.allowsInlineMediaPlayback);
}

// Tests that WKWebViewConfiguration has a different data store if browser state
// returns a different storage ID.
TEST_F(WKWebViewConfigurationProviderTest, DifferentDataStore) {
  // Create a configuration with unique storage identifier.
  FakeBrowserState browser_state1(/*use_unique_storage_uuid=*/true);
  WKWebViewConfigurationProvider* provider1 = &GetProvider(&browser_state1);
  WKWebViewConfiguration* config1 = provider1->GetWebViewConfiguration();
  EXPECT_TRUE(config1.websiteDataStore);
  EXPECT_TRUE(config1.websiteDataStore.persistent);

  // Create another configuration with a different unique storage identifier.
  FakeBrowserState browser_state2(/*use_unique_storage_uuid=*/true);
  WKWebViewConfigurationProvider* provider2 = &GetProvider(&browser_state2);
  WKWebViewConfiguration* config2 = provider2->GetWebViewConfiguration();
  EXPECT_TRUE(config2.websiteDataStore);
  EXPECT_TRUE(config2.websiteDataStore.persistent);

  if (@available(iOS 17.0, *)) {
    // `dataStoreForIdentifier:` is available after iOS 17.
    // Check if the data store is different.
    EXPECT_NE(config1.websiteDataStore, config2.websiteDataStore);
    EXPECT_NE(config1.websiteDataStore.httpCookieStore,
              config2.websiteDataStore.httpCookieStore);
  } else {
    // Otherwise, the default data store should be used.
    EXPECT_EQ(config1.websiteDataStore, config2.websiteDataStore);
    EXPECT_EQ(config1.websiteDataStore.httpCookieStore,
              config2.websiteDataStore.httpCookieStore);
  }
}

// Tests that WKWebViewConfiguration has the same data store if browser state
// returns the same storage ID.
TEST_F(WKWebViewConfigurationProviderTest, SameDataStoreForSameID) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();

  // Create a configuration with an identifier.
  FakeBrowserState browser_state1(/*use_unique_storage_uuid=*/false);
  browser_state1.SetWebKitStorageID(uuid);
  WKWebViewConfigurationProvider* provider1 = &GetProvider(&browser_state1);
  WKWebViewConfiguration* config1 = provider1->GetWebViewConfiguration();
  EXPECT_TRUE(config1.websiteDataStore);
  EXPECT_TRUE(config1.websiteDataStore.persistent);

  // Create another configuration with the same identifier.
  FakeBrowserState browser_state2(/*use_unique_storage_uuid=*/false);
  browser_state2.SetWebKitStorageID(uuid);
  WKWebViewConfigurationProvider* provider2 = &GetProvider(&browser_state2);
  WKWebViewConfiguration* config2 = provider2->GetWebViewConfiguration();
  EXPECT_TRUE(config2.websiteDataStore);
  EXPECT_TRUE(config2.websiteDataStore.persistent);

  // The data store should be the same.
  EXPECT_EQ(config1.websiteDataStore, config2.websiteDataStore);
  EXPECT_EQ(config1.websiteDataStore.httpCookieStore,
            config2.websiteDataStore.httpCookieStore);
}

// Tests `GetWebSiteDataStore()` for Non-OffTheRecord browser.
TEST_F(WKWebViewConfigurationProviderTest,
       GetWebSiteDataStore_NoneOffTheRecord) {
  // Explicitly use default data store.
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/false);

  browser_state.SetOffTheRecord(false);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);
  WKWebsiteDataStore* data_store = provider.GetWebsiteDataStore();
  EXPECT_TRUE(data_store.isPersistent);
  // Default data store shares same pointer.
  EXPECT_NSEQ(WKWebsiteDataStore.defaultDataStore, data_store);
}

// Tests `GetWebSiteDataStore()` for OffTheRecord browser.
TEST_F(WKWebViewConfigurationProviderTest, GetWebSiteDataStore_OffTheRecord) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  browser_state.SetOffTheRecord(true);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);
  WKWebsiteDataStore* data_store = provider.GetWebsiteDataStore();
  ASSERT_TRUE(data_store);
  EXPECT_FALSE(data_store.isPersistent);
}

// Tests `GetWebSiteDataStore()` returns different data store if browser state
// returns a different storage ID.
TEST_F(WKWebViewConfigurationProviderTest,
       GetWebSiteDataStore_DifferentDataStore) {
  // Create a data store with unique storage identifier.
  FakeBrowserState browser_state1(/*use_unique_storage_uuid=*/true);
  WKWebViewConfigurationProvider* provider1 = &GetProvider(&browser_state1);
  WKWebsiteDataStore* data_store1 = provider1->GetWebsiteDataStore();
  EXPECT_TRUE(data_store1);
  EXPECT_TRUE(data_store1.isPersistent);

  // Create another data store with another identifier.
  FakeBrowserState browser_state2(/*use_unique_storage_uuid=*/true);
  WKWebViewConfigurationProvider* provider2 = &GetProvider(&browser_state2);
  WKWebsiteDataStore* data_store2 = provider2->GetWebsiteDataStore();
  EXPECT_TRUE(data_store2);
  EXPECT_TRUE(data_store2.isPersistent);

  if (@available(iOS 17.0, *)) {
    // `dataStoreForIdentifier:` is available after iOS 17.
    // Check if the data store is different.
    EXPECT_NSNE(data_store1, data_store2);
    EXPECT_NSNE(data_store1.httpCookieStore, data_store2.httpCookieStore);
  } else {
    // Otherwise, the default data store should be used.
    EXPECT_NSEQ(data_store1, data_store2);
    EXPECT_NSEQ(data_store1.httpCookieStore, data_store2.httpCookieStore);
  }
}

// Tests `GetWebSiteDataStore()` returns the same data store if browser state
// returns the same storage ID.
TEST_F(WKWebViewConfigurationProviderTest,
       GetWebSiteDataStore_SameDataStoreForSameID) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();

  // Create a data store with an identifier.
  FakeBrowserState browser_state1(/*use_unique_storage_uuid=*/false);
  browser_state1.SetWebKitStorageID(uuid);
  WKWebViewConfigurationProvider* provider1 = &GetProvider(&browser_state1);
  WKWebsiteDataStore* data_store1 = provider1->GetWebsiteDataStore();
  EXPECT_TRUE(data_store1);
  EXPECT_TRUE(data_store1.persistent);

  // Create another data store with the same identifier.
  FakeBrowserState browser_state2(/*use_unique_storage_uuid=*/false);
  browser_state2.SetWebKitStorageID(uuid);
  WKWebViewConfigurationProvider* provider2 = &GetProvider(&browser_state2);
  WKWebsiteDataStore* data_store2 = provider2->GetWebsiteDataStore();
  EXPECT_TRUE(data_store2);
  EXPECT_TRUE(data_store2.persistent);

  // The data store should be the same.
  EXPECT_NSEQ(data_store1, data_store2);
  EXPECT_NSEQ(data_store1.httpCookieStore, data_store2.httpCookieStore);
}

// Tests `GetWebSiteDataStore()` returns same data store with the data store
// configuration used.
TEST_F(WKWebViewConfigurationProviderTest,
       GetWebSiteDataStore_SameConfigurationDataStore) {
  // Explicitly use default data store.
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/false);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);

  WKWebsiteDataStore* data_store = provider.GetWebsiteDataStore();
  EXPECT_TRUE(data_store.isPersistent);
  EXPECT_NSEQ(WKWebsiteDataStore.defaultDataStore, data_store);

  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  WKWebsiteDataStore* config_data_store = config.websiteDataStore;
  EXPECT_NSEQ(data_store, config_data_store);
}

// Tests data store is reset correctly when configuration is reset.
TEST_F(WKWebViewConfigurationProviderTest,
       GetWebSiteDataStore_ResetConfiguration) {
  // Explicitly use default data store.
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/false);

  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);

  WKWebsiteDataStore* data_store = provider.GetWebsiteDataStore();
  EXPECT_TRUE(data_store.isPersistent);
  EXPECT_NSEQ(WKWebsiteDataStore.defaultDataStore, data_store);

  // Register a callback to be notified when new configuration object are
  // created and check that it is not invoked as part of the registration.
  __block WKWebsiteDataStore* recorded_data_store;
  base::CallbackListSubscription subscription =
      provider.RegisterWebSiteDataStoreUpdatedCallback(
          base::BindRepeating(^(WKWebsiteDataStore* new_data_store) {
            recorded_data_store = new_data_store;
          }));
  ASSERT_FALSE(recorded_data_store);

  // Check that the data store is not updated when the configuration is reset
  // for the same `//ios/web` managed browser.
  provider.ResetWithWebViewConfiguration(nil);
  ASSERT_FALSE(recorded_data_store);

  WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
  WKWebsiteDataStore* config_data_store =
      WKWebsiteDataStore.nonPersistentDataStore;
  config.websiteDataStore = config_data_store;
  provider.ResetWithWebViewConfiguration(config);
  EXPECT_NSEQ(config_data_store, recorded_data_store);
  // Ensure data store is reset for the configuration not originated from the
  // `//ios/web`.
  EXPECT_NSNE(data_store, recorded_data_store);

  provider.ResetWithWebViewConfiguration(nil);
  EXPECT_NSEQ(data_store, recorded_data_store);
  // Ensure data store is reset again for `//ios/web` managed browser.
  EXPECT_NSNE(config_data_store, recorded_data_store);
}

// Tests data store is reset correctly when configuration is reset for
// OffTheRecord browser.
TEST_F(WKWebViewConfigurationProviderTest,
       GetWebSiteDataStore_OffTheRecordResetConfiguration) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  browser_state.SetOffTheRecord(true);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);

  WKWebsiteDataStore* data_store = provider.GetWebsiteDataStore();
  EXPECT_FALSE(data_store.isPersistent);

  // Register a callback to be notified when new configuration object are
  // created and check that it is not invoked as part of the registration.
  __block WKWebsiteDataStore* recorded_data_store;
  base::CallbackListSubscription subscription =
      provider.RegisterWebSiteDataStoreUpdatedCallback(
          base::BindRepeating(^(WKWebsiteDataStore* new_data_store) {
            recorded_data_store = new_data_store;
          }));
  ASSERT_FALSE(recorded_data_store);

  // Check that the data store is not updated when the configuration is reset
  // for the same `//ios/web` managed OffTheRecord browser.
  provider.ResetWithWebViewConfiguration(nil);
  ASSERT_FALSE(recorded_data_store);

  WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
  WKWebsiteDataStore* config_data_store =
      WKWebsiteDataStore.nonPersistentDataStore;
  config.websiteDataStore = config_data_store;
  provider.ResetWithWebViewConfiguration(config);
  EXPECT_NSEQ(config_data_store, recorded_data_store);
  // Ensure data store is reset for the configuration not originated from the
  // `//ios/web`.
  EXPECT_NSNE(data_store, recorded_data_store);

  provider.ResetWithWebViewConfiguration(nil);
  // `WKWebsiteDataStore.nonPersistentDataStore` will always return a new
  // instance, so the data store should be different.
  EXPECT_NSNE(data_store, recorded_data_store);
  // Ensure data store is reset again for `//ios/web` managed browser.
  EXPECT_NSNE(config_data_store, recorded_data_store);
}

// Tests that webExtensionController is set when WebClient provides an
// ExtensionController, and nil when it does not.
TEST_F(WKWebViewConfigurationProviderTest, WebExtensionControllerConfigured)
API_AVAILABLE(ios(18.4)) {
  FakeBrowserState browser_state(/*use_unique_storage_uuid=*/true);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state);

  // ExtensionController is not provided: webExtensionController is not
  // configured.
  GetWebClient()->SetExtensionController(nullptr);
  provider.ResetWithWebViewConfiguration(nil);
  EXPECT_NSEQ(nil, provider.GetWebViewConfiguration().webExtensionController);

  std::unique_ptr<ExtensionController> controller =
      ExtensionController::Create();
  ASSERT_TRUE(controller);
  auto* impl = static_cast<ExtensionControllerImpl*>(controller.get());
  GetWebClient()->SetExtensionController(controller.get());

  // ExtensionController is provided: webExtensionController is configured.
  provider.ResetWithWebViewConfiguration(nil);
  EXPECT_NSEQ(impl->GetWKWebExtensionController(),
              provider.GetWebViewConfiguration().webExtensionController);

  // ExtensionController is reset: webExtensionController is cleared.
  GetWebClient()->SetExtensionController(nullptr);
  provider.ResetWithWebViewConfiguration(nil);
  EXPECT_NSEQ(nil, provider.GetWebViewConfiguration().webExtensionController);
}

}  // namespace
}  // namespace web
