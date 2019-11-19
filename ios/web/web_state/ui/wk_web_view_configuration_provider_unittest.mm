// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#import <WebKit/WebKit.h>

#include "base/memory/ptr_util.h"
#import "ios/web/js_messaging/crw_wk_script_message_router.h"
#import "ios/web/js_messaging/page_script_util.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/web_client.h"
#import "ios/web/test/fakes/fake_wk_configuration_provider_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

class WKWebViewConfigurationProviderTest : public PlatformTest {
 public:
  WKWebViewConfigurationProviderTest()
      : web_client_(base::WrapUnique(new web::WebClient)) {}

 protected:
  // Returns WKWebViewConfigurationProvider associated with |browser_state_|.
  WKWebViewConfigurationProvider& GetProvider() {
    return GetProvider(&browser_state_);
  }
  // Returns WKWebViewConfigurationProvider for given |browser_state|.
  WKWebViewConfigurationProvider& GetProvider(
      BrowserState* browser_state) const {
    return WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  }
  // BrowserState required for WKWebViewConfigurationProvider creation.
  web::ScopedTestingWebClient web_client_;
  TestBrowserState browser_state_;
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
  TestBrowserState other_browser_state;
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
  TestBrowserState other_browser_state;
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

// Tests that script message router is bound to correct user content controller.
TEST_F(WKWebViewConfigurationProviderTest, ScriptMessageRouter) {
  ASSERT_TRUE(GetProvider().GetWebViewConfiguration().userContentController);
  EXPECT_EQ(GetProvider().GetWebViewConfiguration().userContentController,
            GetProvider().GetScriptMessageRouter().userContentController);
}

// Tests that both configuration and script message router are deallocated after
// |Purge| call.
TEST_F(WKWebViewConfigurationProviderTest, Purge) {
  __weak id config;
  __weak id router;
  @autoreleasepool {  // Make sure that resulting copy is deallocated.
    id strong_config = GetProvider().GetWebViewConfiguration();
    config = strong_config;
    router = GetProvider().GetScriptMessageRouter();
    ASSERT_TRUE(config);
    ASSERT_TRUE(router);
  }

  // No configuration and router after |Purge| call.
  GetProvider().Purge();
  EXPECT_FALSE(config);
  EXPECT_FALSE(router);
}

// Tests that configuration's userContentController has only one script with the
// same content as web::GetDocumentStartScriptForMainFrame() returns.
TEST_F(WKWebViewConfigurationProviderTest, UserScript) {
  WKWebViewConfiguration* config = GetProvider().GetWebViewConfiguration();
  NSArray* scripts = config.userContentController.userScripts;
  ASSERT_EQ(4U, scripts.count);
  EXPECT_FALSE(((WKUserScript*)[scripts objectAtIndex:0]).isForMainFrameOnly);
  EXPECT_TRUE(((WKUserScript*)[scripts objectAtIndex:1]).isForMainFrameOnly);
  EXPECT_FALSE(((WKUserScript*)[scripts objectAtIndex:2]).isForMainFrameOnly);
  EXPECT_TRUE(((WKUserScript*)[scripts objectAtIndex:3]).isForMainFrameOnly);
  NSString* early_all_frames_script =
      GetDocumentStartScriptForAllFrames(&browser_state_);
  NSString* main_frame_script =
      GetDocumentStartScriptForMainFrame(&browser_state_);
  NSString* late_all_frames_script =
      GetDocumentEndScriptForAllFrames(&browser_state_);
  NSString* late_main_frame_script =
      GetDocumentEndScriptForMainFrame(&browser_state_);
  // The scripts in |userScrips| are wrapped with a "if (!injected)" check to
  // avoid double injections, so a substring check is necessary.
  EXPECT_LT(0U,
            [[scripts[0] source] rangeOfString:early_all_frames_script].length);
  EXPECT_LT(0U, [[scripts[1] source] rangeOfString:main_frame_script].length);
  EXPECT_LT(0U,
            [[scripts[2] source] rangeOfString:late_all_frames_script].length);
  EXPECT_LT(0U,
            [[scripts[3] source] rangeOfString:late_main_frame_script].length);
}

// Tests that observers methods are correctly triggered when observing the
// WKWebViewConfigurationProvider
TEST_F(WKWebViewConfigurationProviderTest, Observers) {
  std::unique_ptr<TestBrowserState> browser_state =
      std::make_unique<TestBrowserState>();
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
// initialization logic to the |config| that passed into that method
TEST_F(WKWebViewConfigurationProviderTest, ResetConfiguration) {
  std::unique_ptr<TestBrowserState> browser_state =
      std::make_unique<TestBrowserState>();
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

  // To check Chrome's initialization logic has been applied to |actual|,
  // where the |actual.allowsInlineMediaPlayback| should be overwriten by YES.
  EXPECT_EQ(NO, config.allowsInlineMediaPlayback);
  EXPECT_EQ(YES, actual.allowsInlineMediaPlayback);

  // Compares the POINTERS to make sure the |config| has been shallow cloned
  // inside the |provider|.
  EXPECT_NE(config, actual);
}

}  // namespace
}  // namespace web
