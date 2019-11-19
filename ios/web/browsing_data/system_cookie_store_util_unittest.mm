// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browsing_data/system_cookie_store_util.h"

#import <WebKit/WebKit.h>

#include "base/bind.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "ios/net/cookies/cookie_store_ios_test_util.h"
#include "ios/net/cookies/system_cookie_store.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForCookiesTimeout;

namespace web {

namespace {

// Checks if |system_cookie| was set in WKHTTPCookieStore |cookie_store|.
bool IsCookieSetInWKCookieStore(NSHTTPCookie* system_cookie,
                                NSURL* url,
                                WKHTTPCookieStore* cookie_store)
    API_AVAILABLE(ios(11.0)) {
  __block bool is_set = false;
  __block bool verification_done = false;
  [cookie_store getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
    NSHTTPCookie* result_cookie = nil;
    for (NSHTTPCookie* cookie in cookies) {
      if ([cookie.path isEqualToString:url.path] &&
          [cookie.domain isEqualToString:url.host] &&
          [cookie.name isEqualToString:system_cookie.name]) {
        result_cookie = cookie;
        break;
      }
    }
    is_set = [result_cookie.value isEqualToString:system_cookie.value];
    verification_done = true;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    return verification_done;
  }));
  return is_set;
}

// Sets |cookie| in SystemCookieStore |store| , and wait until set callback
// is finished.
bool SetCookieInCookieStore(NSHTTPCookie* cookie,
                            net::SystemCookieStore* store) {
  __block bool cookie_was_set = false;
  store->SetCookieAsync(cookie, nullptr, base::BindOnce(^{
                          cookie_was_set = true;
                        }));
  return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return cookie_was_set;
  });
}
}  // namespace

using SystemCookieStoreUtilTest = PlatformTest;

// Tests that web::CreateSystemCookieStore returns a SystemCookieStore object
// that is backed by the correct internal cookiestore based on the iOS version.
TEST_F(SystemCookieStoreUtilTest, CreateSystemCookieStore) {
  web::WebTaskEnvironment task_environment;
  net::ScopedTestingCookieStoreIOSClient scoped_cookie_store_ios_client(
      std::make_unique<net::TestCookieStoreIOSClient>());

  web::TestBrowserState browser_state;
  browser_state.SetOffTheRecord(true);
  NSURL* test_cookie_url = [NSURL URLWithString:@"http://foo.google.com/bar"];
  NSHTTPCookie* test_cookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : test_cookie_url.path,
    NSHTTPCookieName : @"x",
    NSHTTPCookieValue : @"d",
    NSHTTPCookieDomain : test_cookie_url.host,
  }];
  auto system_cookie_store = web::CreateSystemCookieStore(&browser_state);

  WKHTTPCookieStore* wk_cookie_store =
      web::WKCookieStoreForBrowserState(&browser_state);
  EXPECT_FALSE(IsCookieSetInWKCookieStore(test_cookie, test_cookie_url,
                                          wk_cookie_store));
  EXPECT_TRUE(SetCookieInCookieStore(test_cookie, system_cookie_store.get()));
  EXPECT_TRUE(IsCookieSetInWKCookieStore(test_cookie, test_cookie_url,
                                         wk_cookie_store));

  // Clear cookies that was set in the test.
  __block bool cookies_cleared = false;
  system_cookie_store->ClearStoreAsync(base::BindOnce(^{
    cookies_cleared = true;
  }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return cookies_cleared;
  }));
}

}  // namespace web
