// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/browsing_data/browsing_data_removing_util.h"

#import <WebKit/WebKit.h>

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForCookiesTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Makes sure that the DataStore is created, otherwise cookies can't be set in
// some cases.
[[nodiscard]] bool FetchCookieStore() {
  __block bool fetch_done = false;

  NSSet* data_types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
  [[WKWebsiteDataStore defaultDataStore]
      fetchDataRecordsOfTypes:data_types
            completionHandler:^(NSArray<WKWebsiteDataRecord*>* records) {
              fetch_done = true;
            }];
  return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    return fetch_done;
  });
}

// Adds cookies to the default data store. Returns whether it succeed to add
// cookies.
[[nodiscard]] bool AddCookie() {
  NSHTTPCookie* cookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : @"path",
    NSHTTPCookieName : @"cookieName",
    NSHTTPCookieValue : @"value",
    NSHTTPCookieOriginURL : @"http://chromium.org"
  }];

  __block bool cookie_set = false;
  [[WKWebsiteDataStore defaultDataStore].httpCookieStore setCookie:cookie
                                                 completionHandler:^{
                                                   cookie_set = true;
                                                 }];

  return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    return cookie_set;
  });
}

// Checks that the cookies data store has cookies or not, depending on
// `should_have_cookies`.
[[nodiscard]] bool HasCookies(bool should_have_cookies) {
  __block bool has_cookies = false;
  __block bool completion_called = false;

  [[WKWebsiteDataStore defaultDataStore].httpCookieStore
      getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
        has_cookies = cookies.count > 0;
        completion_called = true;
      }];

  bool completed = WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    return completion_called;
  });

  return completed && (has_cookies == should_have_cookies);
}

// Removes the `types` from the data remover associated with `browser_state`.
// Returns whether the completion block of the clear browsing data has been
// called.
[[nodiscard]] bool RemoveCookies(web::BrowserState* browser_state,
                                 web::ClearBrowsingDataMask types) {
  __block bool closure_called = false;
  base::OnceClosure closure = base::BindOnce(^{
    closure_called = true;
  });
  web::ClearBrowsingData(browser_state, types,
                         base::Time::Now() - base::Minutes(1),
                         std::move(closure));

  return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    return closure_called;
  });
}

}  // namespace

namespace web {

class ClearBrowsingDataTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    original_root_view_controller_ = GetAnyKeyWindow().rootViewController;

    UIViewController* controller = [[UIViewController alloc] init];
    GetAnyKeyWindow().rootViewController = controller;
    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    WKWebView* web_view = [[WKWebView alloc] initWithFrame:CGRectZero
                                             configuration:config];
    [controller.view addSubview:web_view];

    ASSERT_TRUE(FetchCookieStore());
    __block bool clear_done = false;
    bool clear_success = false;
    // Clear store from cookies.
    [[WKWebsiteDataStore defaultDataStore]
        removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeCookies]
            modifiedSince:[NSDate distantPast]
        completionHandler:^{
          clear_done = true;
        }];
    clear_success = WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
      return clear_done;
    });
    ASSERT_TRUE(clear_success);
  }

  void TearDown() override {
    if (original_root_view_controller_) {
      GetAnyKeyWindow().rootViewController = original_root_view_controller_;
      original_root_view_controller_ = nil;
    }
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  // The key window's original root view controller, to be  restored at the end
  // of the test.
  UIViewController* original_root_view_controller_;
};

// Tests that removing the cookies remove them from the cookie store.
TEST_F(ClearBrowsingDataTest, RemoveCookie) {
  ASSERT_TRUE(AddCookie());
  ASSERT_TRUE(HasCookies(true));

  // Remove the cookies.
  EXPECT_TRUE(
      RemoveCookies(&browser_state_, ClearBrowsingDataMask::kRemoveCookies));

  EXPECT_TRUE(HasCookies(false));
}

// Tests that removing the anything but cookies leave the cookies in the store.
TEST_F(ClearBrowsingDataTest, RemoveNothing) {
  ASSERT_TRUE(AddCookie());
  ASSERT_TRUE(HasCookies(true));

  // Remove other things than cookies.
  EXPECT_TRUE(RemoveCookies(&browser_state_,
                            ClearBrowsingDataMask::kRemoveAppCache |
                                ClearBrowsingDataMask::kRemoveIndexedDB));

  EXPECT_TRUE(HasCookies(true));
}

// Tests that removing nothing still call the closure.
TEST_F(ClearBrowsingDataTest, KeepCookie) {
  ASSERT_TRUE(AddCookie());

  // Don't remove anything but check that the closure is still called.
  EXPECT_TRUE(
      RemoveCookies(&browser_state_, ClearBrowsingDataMask::kRemoveNothing));
}

}  // namespace web
