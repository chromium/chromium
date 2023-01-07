// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_early_page_script_provider.h"

#import <WebKit/WebKit.h>
#include <memory>

#include "ios/web/public/test/fakes/fake_browser_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web_view/internal/web_view_web_client.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class WebViewEarlyPageScriptProviderTest : public TestWithLocaleAndResources {
 protected:
  WebViewEarlyPageScriptProviderTest()
      : web_client_(std::make_unique<WebViewWebClient>()) {}

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  web::FakeBrowserState browser_state_;

  // Whether or not |string| appears in any of the configured WKUserScripts.
  bool UserScriptsContainString(NSString* string) {
    web::WKWebViewConfigurationProvider& config_provider =
        web::WKWebViewConfigurationProvider::FromBrowserState(&browser_state_);
    WKWebViewConfiguration* configuration =
        config_provider.GetWebViewConfiguration();

    for (WKUserScript* script in configuration.userContentController
             .userScripts) {
      if ([script.source containsString:string]) {
        return true;
      }
    }
    return false;
  }
};

// Test WebViewEarlyPageScriptProvder::SetScript properly updates the underlying
// WKUserContentController.
TEST_F(WebViewEarlyPageScriptProviderTest, SetScript) {
  EXPECT_FALSE(UserScriptsContainString(@"WebViewEarlyPageScriptProvider"));

  WebViewEarlyPageScriptProvider::FromBrowserState(&browser_state_)
      .SetScript(@"WebViewEarlyPageScriptProvider");
  EXPECT_TRUE(UserScriptsContainString(@"WebViewEarlyPageScriptProvider"));
}

}  // namespace ios_web_view
