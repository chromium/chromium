// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/web_view_v5_search_hashes_cache_factory.h"

#import <memory>

#import "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_web_client.h"
#import "ios/web_view/test/test_with_locale_and_resources.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace ios_web_view {

class WebViewV5SearchHashesCacheFactoryTest
    : public TestWithLocaleAndResources {
 protected:
  WebViewV5SearchHashesCacheFactoryTest()
      : web_client_(std::make_unique<WebViewWebClient>()) {
    WebViewV5SearchHashesCacheFactory::GetInstance();
    browser_state_ =
        std::make_unique<WebViewBrowserState>(/*off_the_record=*/false);
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<WebViewBrowserState> browser_state_;
};

// Checks that WebViewV5SearchHashesCacheFactory returns a non-null instance
// for a regular browser state.
TEST_F(WebViewV5SearchHashesCacheFactoryTest, EnabledForRegularMode) {
  EXPECT_TRUE(WebViewV5SearchHashesCacheFactory::GetForBrowserState(
      browser_state_.get()));
}

// Checks that WebViewV5SearchHashesCacheFactory returns a non-null and
// distinct instance for an off-the-record browser state.
TEST_F(WebViewV5SearchHashesCacheFactoryTest, EnabledForIncognitoMode) {
  std::unique_ptr<WebViewBrowserState> off_the_record_browser_state =
      std::make_unique<WebViewBrowserState>(/*off_the_record=*/true,
                                            browser_state_.get());
  EXPECT_TRUE(WebViewV5SearchHashesCacheFactory::GetForBrowserState(
      off_the_record_browser_state.get()));
  EXPECT_NE(WebViewV5SearchHashesCacheFactory::GetForBrowserState(
                browser_state_.get()),
            WebViewV5SearchHashesCacheFactory::GetForBrowserState(
                off_the_record_browser_state.get()));
}

}  // namespace ios_web_view
