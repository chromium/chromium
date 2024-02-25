// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <WebKit/WebKit.h>

#import "base/memory/ptr_util.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/web_client.h"
#import "ios/web/test/fakes/fake_wk_configuration_provider_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {
namespace {

class WKWebViewConfigurationProviderTest : public PlatformTest {
 public:
  WKWebViewConfigurationProviderTest()
      : web_client_(std::make_unique<FakeWebClient>()) {}

 protected:
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
};

// Tests that each WKWebViewConfigurationProvider has own, non-nil
// configuration and configurations returned by the same provider will always
// have the same process pool.
TEST_F(WKWebViewConfigurationProviderTest, ConfigurationOwnerhip) {
  // Configuration is not nil.
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
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
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
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
  WKWebViewConfigurationProvider& provider = GetProvider(&browser_state_);
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

// Tests that configuration's userContentController has additional scripts
// injected for JavaScriptFeatures configured through the WebClient.
TEST_F(WKWebViewConfigurationProviderTest, JavaScriptFeatureInjection) {
  FakeWebClient* client = GetWebClient();

  WKUserContentController* user_content_controller =
      GetProvider().GetWebViewConfiguration().userContentController;
  unsigned long original_script_count =
      [user_content_controller.userScripts count];

  std::vector<const web::JavaScriptFeature::FeatureScript> feature_scripts = {
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "java_script_feature_test_inject_once",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

  std::unique_ptr<web::JavaScriptFeature> feature =
      std::make_unique<web::JavaScriptFeature>(
          web::ContentWorld::kPageContentWorld, feature_scripts);

  client->SetJavaScriptFeatures({feature.get()});
  GetProvider().UpdateScripts();

  EXPECT_GT([user_content_controller.userScripts count], original_script_count);
}

// Tests that observers methods are correctly triggered when observing the
// WKWebViewConfigurationProvider
TEST_F(WKWebViewConfigurationProviderTest, Observers) {
  auto browser_state = std::make_unique<FakeBrowserState>();
  WKWebViewConfigurationProvider* provider = &GetProvider(browser_state.get());

  FakeWKConfigurationProviderObserver observer(provider);
  EXPECT_FALSE(observer.GetLastCreatedWKConfiguration());
  WKWebViewConfiguration* config = provider->GetWebViewConfiguration();
  EXPECT_NSEQ(config.preferences,
              observer.GetLastCreatedWKConfiguration().preferences);
  observer.ResetLastCreatedWKConfig();
  config = provider->GetWebViewConfiguration();
  EXPECT_FALSE(observer.GetLastCreatedWKConfiguration());
}

// Tests that if -[ResetWithWebViewConfiguration:] copies and applies Chrome's
// initialization logic to the `config` that passed into that method
TEST_F(WKWebViewConfigurationProviderTest, ResetConfiguration) {
  auto browser_state = std::make_unique<FakeBrowserState>();
  WKWebViewConfigurationProvider* provider = &GetProvider(browser_state.get());

  FakeWKConfigurationProviderObserver observer(provider);
  ASSERT_FALSE(observer.GetLastCreatedWKConfiguration());

  WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
  config.allowsInlineMediaPlayback = NO;
  provider->ResetWithWebViewConfiguration(config);
  WKWebViewConfiguration* actual = observer.GetLastCreatedWKConfiguration();
  ASSERT_TRUE(actual);

  // To check the configuration inside is reset.
  EXPECT_EQ(config.preferences, actual.preferences);

  // To check Chrome's initialization logic has been applied to `actual`,
  // where the `actual.allowsInlineMediaPlayback` should be overwriten by YES.
  EXPECT_EQ(NO, config.allowsInlineMediaPlayback);
  EXPECT_EQ(YES, actual.allowsInlineMediaPlayback);

  // Compares the POINTERS to make sure the `config` has been shallow cloned
  // inside the `provider`.
  EXPECT_NE(config, actual);
}

TEST_F(WKWebViewConfigurationProviderTest, GetContentRuleListProvider) {
  auto browser_state = std::make_unique<FakeBrowserState>();
  WKWebViewConfigurationProvider& provider = GetProvider(browser_state.get());

  EXPECT_NE(nil, provider.GetContentRuleListProvider());
}

}  // namespace
}  // namespace web
