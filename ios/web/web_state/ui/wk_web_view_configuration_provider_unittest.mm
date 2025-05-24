// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <WebKit/WebKit.h>

#import <vector>

#import "base/memory/ptr_util.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "base/uuid.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {
namespace {

// Constants for Content Rule List Tests
NSString* const kTestScriptBlockingListIdentifier = @"script-blocking-list";
NSString* const kTestBlockLocalListIdentifier = @"block-local";
NSString* const kTestMixedContentAutoupgradeListIdentifier =
    @"mixed-content-autoupgrade";

// Helper functions for Content Rule List Tests
NSString* CreateValidScriptBlockingJSONRules() {
  // A simple rule that blocks everything (for testing presence)
  return @"[{\"trigger\":{\"url-filter\":\".*\"},\"action\":{\"type\":"
         @"\"block\"}}]";
}

NSString* CreateInvalidScriptBlockingJSONRules() {
  // Malformed JSON (missing closing brace/bracket)
  return @"[{\"trigger\":{\"url-filter\":\".*\"},\"action\":{\"type\":"
         @"\"block\"";
}

// ContentRuleList test helper functions

// Helper to clear a specific rule list from the WKContentRuleListStore and
// wait.
void ClearRuleListFromStoreAndWait(NSString* identifier) {
  base::RunLoop run_loop;

  // Look up the content rule list.
  void (^lookup_completion_block)(WKContentRuleList*, NSError*) =
      base::CallbackToBlock(base::BindLambdaForTesting(
          [&](WKContentRuleList* ruleList, NSError* lookup_error) {
            if (lookup_error) {
              // An error occurred during lookup.
              // Don't log if the error is
              // WKErrorContentRuleListStoreLookUpFailed (Code 8), as this
              // simply means the list isn't there, so no removal is needed.
              if (!([lookup_error.domain isEqualToString:WKErrorDomain] &&
                    lookup_error.code ==
                        WKErrorContentRuleListStoreLookUpFailed)) {
                NSLog(@"Error looking up list '%@' for potential removal: %@",
                      identifier, lookup_error);
              }
              run_loop.Quit();  // Stop if lookup failed or list confirmed not
                                // found via error.
              return;
            }

            if (ruleList) {
              // Lookup was successful and the ruleList exists. Proceed to
              // remove it.
              void (^removal_completion_block)(NSError*) =
                  base::CallbackToBlock(
                      base::BindLambdaForTesting([&](NSError* removal_error) {
                        // Handle errors during removal.
                        if (removal_error) {
                          NSLog(@"Unexpected error removing list '%@' after "
                                @"successful lookup: %@",
                                identifier, removal_error);
                        }
                        run_loop.Quit();  // Quit after the removal attempt
                      }));

              [WKContentRuleListStore.defaultStore
                  removeContentRuleListForIdentifier:identifier
                                   completionHandler:removal_completion_block];
            } else {
              // Lookup was successful (no error), but ruleList is nil.
              // No removal needed.
              run_loop.Quit();
            }
          }));

  [WKContentRuleListStore.defaultStore
      lookUpContentRuleListForIdentifier:identifier
                       completionHandler:lookup_completion_block];

  run_loop.Run();
}

// Helper to check rule list presence in the store (asynchronously)
bool CheckStoreForRuleListAndWait(NSString* identifier) {
  bool found = false;
  base::RunLoop run_loop;
  // Use base::CallbackToBlock to convert the base::RepeatingCallback to an
  // Objective-C block.
  void (^completion_block)(WKContentRuleList*, NSError*) =
      base::CallbackToBlock(base::BindLambdaForTesting(
          [&](WKContentRuleList* ruleList, NSError* error) {
            found = (ruleList != nil);
            run_loop.Quit();
          }));

  [WKContentRuleListStore.defaultStore
      lookUpContentRuleListForIdentifier:identifier
                       completionHandler:completion_block];
  run_loop.Run();
  return found;
}

// Struct to hold results from UpdateScriptBlockingRuleList callback
struct UpdateResult {
  bool success = false;
  NSError* error = nil;
};

UpdateResult CallUpdateScriptBlockingRuleListAndWait(
    WKContentRuleListProvider* rule_list_provider,
    NSString* json_rules) {
  UpdateResult result;
  base::RunLoop run_loop;
  rule_list_provider->UpdateScriptBlockingRuleList(
      json_rules, base::BindLambdaForTesting([&](bool success, NSError* error) {
        result.success = success;
        result.error = error;  // ARC handles ownership
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

class WKWebViewConfigurationProviderTest : public PlatformTest {
 public:
  WKWebViewConfigurationProviderTest()
      : web_client_(std::make_unique<FakeWebClient>()) {}

 protected:
  void TearDown() override {
    // Clean up by removing lists from the store. This is relevant for
    // Content Rule List tests to avoid test leakage.
    ClearRuleListFromStoreAndWait(kTestScriptBlockingListIdentifier);
    ClearRuleListFromStoreAndWait(kTestBlockLocalListIdentifier);
    ClearRuleListFromStoreAndWait(kTestMixedContentAutoupgradeListIdentifier);

    PlatformTest::TearDown();
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

  // BrowserState required for WKWebViewConfigurationProvider creation.
  web::ScopedTestingWebClient web_client_;
  FakeBrowserState browser_state_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that each WKWebViewConfigurationProvider has own, non-nil
// configuration and configurations returned by the same provider will always
// have the same process pool.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationOwnerhip) {
  // Configuration is not nil.
  WKWebViewConfigurationProvider& provider = GetProvider();
  ASSERT_TRUE(provider.GetWebViewConfiguration());

  // Same non-nil WKProcessPool for the same provider.
  ASSERT_TRUE(provider.GetWebViewConfiguration().processPool);
  EXPECT_EQ(provider.GetWebViewConfiguration().processPool,
            provider.GetWebViewConfiguration().processPool);

  // Different WKProcessPools for different providers.
  FakeBrowserState other_browser_state;
  WKWebViewConfigurationProvider& other_provider =
      GetProvider(&other_browser_state);
  EXPECT_NE(provider.GetWebViewConfiguration().processPool,
            other_provider.GetWebViewConfiguration().processPool);
}

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
  WKProcessPool* pool = [config processPool];
  WKPreferences* prefs = [config preferences];
  WKUserContentController* userContentController =
      [config userContentController];

  // Change the properties of returned configuration object.
  FakeBrowserState other_browser_state;
  WKWebViewConfiguration* other_wk_web_view_configuration =
      GetProvider(&other_browser_state).GetWebViewConfiguration();
  ASSERT_TRUE(other_wk_web_view_configuration);
  config.processPool = other_wk_web_view_configuration.processPool;
  config.preferences = other_wk_web_view_configuration.preferences;
  config.userContentController =
      other_wk_web_view_configuration.userContentController;

  // Make sure that the properties of internal configuration were not changed.
  EXPECT_TRUE(provider.GetWebViewConfiguration().processPool);
  EXPECT_EQ(pool, provider.GetWebViewConfiguration().processPool);
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

// Tests that the content rule list provider is returned.
TEST_F(WKWebViewConfigurationProviderTest, GetContentRuleListProvider) {
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  // Getting the WKContentRuleListProvider may trigger asynchronous compilation.
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  EXPECT_NE(nullptr, rule_list_provider);
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

// --- WKContentRuleListProvider Specific Tests ---

// Tests that the static block-local list is eventually compiled and installed.
TEST_F(WKWebViewConfigurationProviderTest, StaticBlockLocalListInstalled) {
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  ASSERT_NE(nullptr, rule_list_provider);

  base::RunLoop run_loop;
  // Assume rule_list_provider now has this method.
  rule_list_provider->SetOnAllStaticListsCompiledCallback(
      run_loop.QuitClosure());
  run_loop.Run();  // Wait for the signal from the provider.

  EXPECT_TRUE(CheckStoreForRuleListAndWait(kTestBlockLocalListIdentifier))
      << "Block-local list (" << kTestBlockLocalListIdentifier
      << ") should be present in store after provider signals completion.";
}

// Tests that the static mixed-content list is eventually compiled and
// installed.
TEST_F(WKWebViewConfigurationProviderTest, StaticMixedContentListInstalled) {
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  ASSERT_NE(nullptr, rule_list_provider);

  base::RunLoop run_loop;
  // Assume rule_list_provider now has this method.
  rule_list_provider->SetOnAllStaticListsCompiledCallback(
      run_loop.QuitClosure());
  run_loop.Run();  // Wait for the signal from the provider.

  EXPECT_TRUE(
      CheckStoreForRuleListAndWait(kTestMixedContentAutoupgradeListIdentifier))
      << "Mixed-content list (" << kTestMixedContentAutoupgradeListIdentifier
      << ") should be present in store after provider signals completion.";
}

// Tests adding a valid script blocking rule list.
TEST_F(WKWebViewConfigurationProviderTest, AddValidScriptBlockingRules) {
  // Get the WKWebViewConfigurationProvider for the fixture's browser_state.
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  ASSERT_NE(nullptr, rule_list_provider);

  ASSERT_FALSE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Pre-condition: Script blocking list should not be in store.";

  NSString* valid_json = CreateValidScriptBlockingJSONRules();
  // Pass the obtained rule_list_provider to the helper.
  UpdateResult result =
      CallUpdateScriptBlockingRuleListAndWait(rule_list_provider, valid_json);

  // Check callback results
  EXPECT_TRUE(result.success)
      << "Callback should report success for valid JSON.";
  EXPECT_EQ(nil, result.error)
      << "Callback should report nil error for valid JSON.";

  // Check side effects
  EXPECT_TRUE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Script blocking list should be present in store after adding valid "
         "rules.";
}

// Tests attempting to add an invalid script blocking rule list.
TEST_F(WKWebViewConfigurationProviderTest, AddInvalidScriptBlockingRules) {
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  ASSERT_NE(nullptr, rule_list_provider);

  ASSERT_FALSE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Pre-condition: Script blocking list should not be in store.";

  NSString* invalid_json = CreateInvalidScriptBlockingJSONRules();
  UpdateResult result =
      CallUpdateScriptBlockingRuleListAndWait(rule_list_provider, invalid_json);

  // Check callback results
  EXPECT_FALSE(result.success)
      << "Callback should report failure for invalid JSON.";
  ASSERT_NE(nil, result.error)
      << "Callback should report non-nil error for invalid JSON.";
  // Assuming WKErrorDomain and WKErrorContentRuleListStoreCompileFailed are
  // available/correct.
  EXPECT_NSEQ(WKErrorDomain, result.error.domain)
      << "Error domain should be WKErrorDomain for compilation issues.";
  EXPECT_EQ(WKErrorContentRuleListStoreCompileFailed, result.error.code)
      << "Error code should indicate compilation failure.";

  // Check side effects (list should not be installed, state unchanged)
  EXPECT_FALSE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Script blocking list should NOT be present in store after attempting "
         "to add invalid rules.";
}

// Tests attempting to update with nil when a list already exists.
TEST_F(WKWebViewConfigurationProviderTest, UpdateWithNilWhenListExists) {
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  ASSERT_NE(nullptr, rule_list_provider);

  // First, add a list to ensure there's something to remove.
  UpdateResult add_result = CallUpdateScriptBlockingRuleListAndWait(
      rule_list_provider, CreateValidScriptBlockingJSONRules());
  ASSERT_TRUE(add_result.success);
  ASSERT_TRUE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Setup: List should be in store before update attempt.";

  // Now, attempt to "update" by passing nil. This should clear the list.
  UpdateResult update_result =
      CallUpdateScriptBlockingRuleListAndWait(rule_list_provider, nil);

  // Expect success because clearing is a valid operation.
  EXPECT_TRUE(update_result.success)
      << "Callback should report success for nil input (clear operation).";
  // Expect no error for a successful clear.
  EXPECT_EQ(nil, update_result.error)
      << "Callback should report nil error for successful clear.";

  // The list should now be removed from the store.
  EXPECT_FALSE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Script blocking list SHOULD NOT be present in store after clearing "
         "with nil.";
}

// Tests attempting to update with an empty string when a list already exists.
TEST_F(WKWebViewConfigurationProviderTest,
       UpdateWithEmptyStringWhenListExists) {
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  ASSERT_NE(nullptr, rule_list_provider);

  // First, add a list.
  UpdateResult add_result = CallUpdateScriptBlockingRuleListAndWait(
      rule_list_provider, CreateValidScriptBlockingJSONRules());
  ASSERT_TRUE(add_result.success);
  ASSERT_TRUE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Setup: List should be in store before update attempt.";

  // Now, attempt to update by passing an empty string
  UpdateResult update_result =
      CallUpdateScriptBlockingRuleListAndWait(rule_list_provider, @"");

  // Check callback results (compilation fails for empty string)
  EXPECT_FALSE(update_result.success)
      << "Callback should report failure for empty string input.";
  ASSERT_NE(nil, update_result.error)
      << "Callback should report non-nil error for empty string input.";
  EXPECT_NSEQ(WKErrorDomain, update_result.error.domain);
  EXPECT_EQ(WKErrorContentRuleListStoreCompileFailed, update_result.error.code);

  // Check side effects (list should NOT be removed, should remain active in
  // store)
  EXPECT_TRUE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Script blocking list SHOULD remain present in store after failed "
         "update via empty string.";
}

// Tests updating an existing script blocking rule list with new valid rules.
TEST_F(WKWebViewConfigurationProviderTest, UpdateExistingScriptBlockingRules) {
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  ASSERT_NE(nullptr, rule_list_provider);

  // 1. Add an initial list
  UpdateResult initial_add_result = CallUpdateScriptBlockingRuleListAndWait(
      rule_list_provider, CreateValidScriptBlockingJSONRules());
  ASSERT_TRUE(initial_add_result.success);
  ASSERT_TRUE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Setup: List should be in store after initial add.";

  // 2. Update with new rules
  NSString* updated_json =
      @"[{\"trigger\":{\"url-filter\":\"example\\\\.com\"},\"action\":{"
      @"\"type\":\"block\"}}]";  // A different rule
  UpdateResult update_result =
      CallUpdateScriptBlockingRuleListAndWait(rule_list_provider, updated_json);

  // Check callback results
  EXPECT_TRUE(update_result.success)
      << "Callback should report success for valid update.";
  EXPECT_EQ(nil, update_result.error)
      << "Callback should report nil error for valid update.";

  // Check side effects
  EXPECT_TRUE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Script blocking list should still be present in store after update.";
}

// Tests attempting to update with nil when no list exists.
TEST_F(WKWebViewConfigurationProviderTest, UpdateWithNilWhenNoListExists) {
  WKWebViewConfigurationProvider& config_provider = GetProvider();
  WKContentRuleListProvider* rule_list_provider =
      config_provider.GetContentRuleListProvider();
  ASSERT_NE(nullptr, rule_list_provider);

  ASSERT_FALSE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "Pre-condition: List should not be present in store.";

  UpdateResult result =
      CallUpdateScriptBlockingRuleListAndWait(rule_list_provider, nil);

  // Expect success because clearing is a valid operation, even if list wasn't
  // there.
  EXPECT_TRUE(result.success)
      << "Callback should report success for nil input (clear operation). "
      << "Error: "
      << (result.error ? result.error.description.UTF8String : "none");
  // Expect no error for a successful clear.
  EXPECT_EQ(nil, result.error)
      << "Callback should report nil error for successful clear.";

  EXPECT_FALSE(CheckStoreForRuleListAndWait(kTestScriptBlockingListIdentifier))
      << "List should remain not present in store.";
}

// --- End of WKContentRuleListProvider Specific Tests ---

}  // namespace
}  // namespace web
