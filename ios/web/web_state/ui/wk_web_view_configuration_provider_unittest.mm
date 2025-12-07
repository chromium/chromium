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

  // Returns WKWebViewConfigurationProvider associated with `browser_state_`.
  WKWebViewConfigurationProvider& GetProvider() {
    return GetProvider(&browser_state_);
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
  FakeBrowserState browser_state_;
  base::test::TaskEnvironment task_environment_;
};

namespace {

// Tests Non-OffTheRecord configuration.
TEST_F(WKWebViewConfigurationProviderTest, NoneOffTheRecordConfiguration) {
  browser_state_.SetOffTheRecord(false);
  WKWebViewConfigurationProvider& provider = GetProvider();
  EXPECT_TRUE(provider.GetWebViewConfiguration().websiteDataStore.persistent);
}

// Tests OffTheRecord configuration.
TEST_F(WKWebViewConfigurationProviderTest, OffTheRecordConfiguration) {
  browser_state_.SetOffTheRecord(true);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  ASSERT_TRUE(config);
  EXPECT_FALSE(config.websiteDataStore.persistent);
}

// Tests that internal configuration object can not be changed by clients.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationProtection) {
  WKWebViewConfigurationProvider& provider = GetProvider();
  WKWebViewConfiguration* config = provider.GetWebViewConfiguration();
  WKPreferences* prefs = [config preferences];
  WKUserContentController* userContentController =
      [config userContentController];

  // Change the properties of returned configuration object.
  FakeBrowserState other_browser_state;
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
  __weak id config;
  @autoreleasepool {  // Make sure that resulting copy is deallocated.
    id strong_config = GetProvider().GetWebViewConfiguration();
    config = strong_config;
    ASSERT_TRUE(config);
  }

  // No configuration after `Purge` call.
  GetProvider().Purge();
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
  auto rule_list_provider =
      std::make_unique<testing::StrictMock<MockWKContentRuleListProvider>>(
          browser_state_.GetStatePath());
  EXPECT_CALL(
      *rule_list_provider,
      UpdateRuleList(kBlockLocalResourcesRuleListKey, testing::_, testing::_));
  EXPECT_CALL(
      *rule_list_provider,
      UpdateRuleList(kMixedContentUpgradeRuleListKey, testing::_, testing::_));

  auto provider = CreateProviderWithMockRuleListProvider(
      &browser_state_, std::move(rule_list_provider));
}

// Tests that configuration's userContentController has additional scripts
// injected for JavaScriptFeatures configured through the WebClient.
TEST_F(WKWebViewConfigurationProviderTest, JavaScriptFeatureInjection) {
  FakeWebClient* client = GetWebClient();

  WKUserContentController* user_content_controller =
      GetProvider().GetWebViewConfiguration().userContentController;
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
  GetProvider().UpdateScripts();

  // Re-fetch userContentController to ensure we're checking the updated state.
  user_content_controller =
      GetProvider().GetWebViewConfiguration().userContentController;
  ASSERT_NE(nil, user_content_controller)
      << "UserContentController should not be nil after update.";
  EXPECT_GT([user_content_controller.userScripts count], original_script_count);
}

// Tests that observers methods are correctly triggered when observing the
// WKWebViewConfigurationProvider
TEST_F(WKWebViewConfigurationProviderTest, Observers) {
  // Use a separate browser_state for this test to avoid interference if
  // GetProvider() has side effects
  auto browser_state = std::make_unique<FakeBrowserState>();
  WKWebViewConfigurationProvider* provider = &GetProvider(browser_state.get());

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
  auto browser_state = std::make_unique<FakeBrowserState>();
  WKWebViewConfigurationProvider* provider = &GetProvider(browser_state.get());

  WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
  config.allowsInlineMediaPlayback = NO;
  provider->ResetWithWebViewConfiguration(config);

  WKWebViewConfiguration* recorded_configuration =
      provider->GetWebViewConfiguration();
  ASSERT_TRUE(recorded_configuration);

  // To check the configuration inside is reset.
  EXPECT_EQ(config.preferences, recorded_configuration.preferences);

  // To check Chrome's initialization logic has been applied to `actual`,
  // where the `actual.allowsInlineMediaPlayback` should be overwriten by YES.
  EXPECT_EQ(NO, config.allowsInlineMediaPlayback);
  EXPECT_EQ(YES, recorded_configuration.allowsInlineMediaPlayback);

  // Compares the POINTERS to make sure the `config` has been shallow cloned
  // inside the `provider`.
  EXPECT_NE(config, recorded_configuration);
}

// Tests that WKWebViewConfiguration has a different data store if browser state
// returns a different storage ID.
TEST_F(WKWebViewConfigurationProviderTest, DifferentDataStore) {
  // Create a configuration with an identifier.
  auto browser_state1 = std::make_unique<FakeBrowserState>();
  browser_state1->SetWebKitStorageID(base::Uuid::GenerateRandomV4());
  WKWebViewConfigurationProvider* provider1 =
      &GetProvider(browser_state1.get());
  WKWebViewConfiguration* config1 = provider1->GetWebViewConfiguration();
  EXPECT_TRUE(config1.websiteDataStore);
  EXPECT_TRUE(config1.websiteDataStore.persistent);

  // Create another configuration with another identifier.
  auto browser_state2 = std::make_unique<FakeBrowserState>();
  browser_state2->SetWebKitStorageID(base::Uuid::GenerateRandomV4());
  WKWebViewConfigurationProvider* provider2 =
      &GetProvider(browser_state2.get());
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
  auto browser_state1 = std::make_unique<FakeBrowserState>();
  browser_state1->SetWebKitStorageID(uuid);
  WKWebViewConfigurationProvider* provider1 =
      &GetProvider(browser_state1.get());
  WKWebViewConfiguration* config1 = provider1->GetWebViewConfiguration();
  EXPECT_TRUE(config1.websiteDataStore);
  EXPECT_TRUE(config1.websiteDataStore.persistent);

  // Create another configuration with the same identifier.
  auto browser_state2 = std::make_unique<FakeBrowserState>();
  browser_state2->SetWebKitStorageID(uuid);
  WKWebViewConfigurationProvider* provider2 =
      &GetProvider(browser_state2.get());
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
  browser_state_.SetOffTheRecord(false);
  WKWebViewConfigurationProvider& provider = GetProvider();
  WKWebsiteDataStore* data_store = provider.GetWebsiteDataStore();
  EXPECT_TRUE(data_store.isPersistent);
  // Default data store shares same pointer.
  EXPECT_NSEQ(WKWebsiteDataStore.defaultDataStore, data_store);
}

// Tests `GetWebSiteDataStore()` for OffTheRecord browser.
TEST_F(WKWebViewConfigurationProviderTest, GetWebSiteDataStore_OffTheRecord) {
  browser_state_.SetOffTheRecord(true);
  WKWebViewConfigurationProvider& provider = GetProvider();
  WKWebsiteDataStore* data_store = provider.GetWebsiteDataStore();
  ASSERT_TRUE(data_store);
  EXPECT_FALSE(data_store.isPersistent);
}

// Tests `GetWebSiteDataStore()` returns different data store if browser state
// returns a different storage ID.
TEST_F(WKWebViewConfigurationProviderTest,
       GetWebSiteDataStore_DifferentDataStore) {
  // Create a data store with an identifier.
  auto browser_state1 = std::make_unique<FakeBrowserState>();
  browser_state1->SetWebKitStorageID(base::Uuid::GenerateRandomV4());
  WKWebViewConfigurationProvider* provider1 =
      &GetProvider(browser_state1.get());
  WKWebsiteDataStore* data_store1 = provider1->GetWebsiteDataStore();
  EXPECT_TRUE(data_store1);
  EXPECT_TRUE(data_store1.isPersistent);

  // Create another data store with another identifier.
  auto browser_state2 = std::make_unique<FakeBrowserState>();
  browser_state2->SetWebKitStorageID(base::Uuid::GenerateRandomV4());
  WKWebViewConfigurationProvider* provider2 =
      &GetProvider(browser_state2.get());
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
  auto browser_state1 = std::make_unique<FakeBrowserState>();
  browser_state1->SetWebKitStorageID(uuid);
  WKWebViewConfigurationProvider* provider1 =
      &GetProvider(browser_state1.get());
  WKWebsiteDataStore* data_store1 = provider1->GetWebsiteDataStore();
  EXPECT_TRUE(data_store1);
  EXPECT_TRUE(data_store1.persistent);

  // Create another data store with the same identifier.
  auto browser_state2 = std::make_unique<FakeBrowserState>();
  browser_state2->SetWebKitStorageID(uuid);
  WKWebViewConfigurationProvider* provider2 =
      &GetProvider(browser_state2.get());
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
  WKWebViewConfigurationProvider& provider = GetProvider();

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
  WKWebViewConfigurationProvider& provider = GetProvider();

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
  browser_state_.SetOffTheRecord(true);
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);

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

}  // namespace
}  // namespace web
