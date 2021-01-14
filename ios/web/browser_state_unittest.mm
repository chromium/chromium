// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browser_state.h"

#import <WebKit/WebKit.h>

#include "base/supports_user_data.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/browsing_data/cookie_blocking_mode.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace {
class TestSupportsUserData : public base::SupportsUserData {
 public:
  TestSupportsUserData() {}
  ~TestSupportsUserData() override {}
};
}  // namespace

using BrowserStateTest = PlatformTest;

TEST_F(BrowserStateTest, FromSupportsUserData_NullPointer) {
  DCHECK_EQ(static_cast<web::BrowserState*>(nullptr),
            web::BrowserState::FromSupportsUserData(nullptr));
}

TEST_F(BrowserStateTest, FromSupportsUserData_NonBrowserState) {
  TestSupportsUserData supports_user_data;
  DCHECK_EQ(static_cast<web::BrowserState*>(nullptr),
            web::BrowserState::FromSupportsUserData(&supports_user_data));
}

TEST_F(BrowserStateTest, FromSupportsUserData) {
  web::FakeBrowserState browser_state;
  DCHECK_EQ(&browser_state,
            web::BrowserState::FromSupportsUserData(&browser_state));
}

// Tests that changing the cookie blocking mode causes the injected Javascript
// to change.
TEST_F(BrowserStateTest, SetCookieBlockingMode) {
  web::FakeBrowserState browser_state;
  __block bool success = false;
  browser_state.SetCookieBlockingMode(web::CookieBlockingMode::kAllow,
                                      base::BindOnce(^{
                                        success = true;
                                      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  web::WKWebViewConfigurationProvider& config_provider =
      web::WKWebViewConfigurationProvider::FromBrowserState(&browser_state);
  NSArray* wkscripts = config_provider.GetWebViewConfiguration()
                           .userContentController.userScripts;
  ASSERT_GT(wkscripts.count, 0U);

  NSArray<WKUserScript*>* original_scripts =
      [[NSArray alloc] initWithArray:wkscripts copyItems:NO];
  // Make sure that the WKUserScripts are the same across multiple fetches if
  // no changes have occured.
  ASSERT_TRUE(
      [original_scripts isEqualToArray:config_provider.GetWebViewConfiguration()
                                           .userContentController.userScripts]);

  success = false;
  browser_state.SetCookieBlockingMode(web::CookieBlockingMode::kBlock,
                                      base::BindOnce(^{
                                        success = true;
                                      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  NSArray<WKUserScript*>* updated_scripts =
      [[NSArray alloc] initWithArray:wkscripts copyItems:NO];

  EXPECT_FALSE([original_scripts isEqualToArray:updated_scripts]);
}
