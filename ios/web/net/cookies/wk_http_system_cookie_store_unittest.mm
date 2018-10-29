// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_http_system_cookie_store.h"

#import <Foundation/Foundation.h>
#import <WebKit/Webkit.h>

#include <memory>

#import "base/test/ios/wait_util.h"
#include "ios/net/cookies/system_cookie_store_unittest_template.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

// Test class that conforms to net::SystemCookieStoreTestDelegate to exercise
// WKHTTPSystemCookieStore.
class API_AVAILABLE(ios(11.0)) WKHTTPSystemCookieStoreTestDelegate {
 public:
  WKHTTPSystemCookieStoreTestDelegate() {
    if (@available(iOS 11, *)) {
      shared_store_ =
          [WKWebsiteDataStore nonPersistentDataStore].httpCookieStore;
      store_ = std::make_unique<web::WKHTTPSystemCookieStore>(shared_store_);
    }
  }

  bool IsTestEnabled() {
    if (@available(iOS 11, *))
      return true;
    return false;
  }

  bool IsCookieSet(NSHTTPCookie* system_cookie, NSURL* url) {
    // Verify that cookie is set in system storage.
    __block bool is_set = false;
    __block bool verification_done = false;
    [shared_store_ getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
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
    bool callback_success = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForCookiesTimeout, ^bool {
          return verification_done;
        });
    EXPECT_TRUE(callback_success);
    return is_set;
  }

  void ClearCookies() {
    __block int cookies_found = -1;
    __block int cookies_deleted = 0;
    [shared_store_ getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
      cookies_found = cookies.count;
      for (NSHTTPCookie* cookie in cookies) {
        [shared_store_ deleteCookie:cookie
                  completionHandler:^{
                    cookies_deleted++;
                  }];
      }
    }];
    bool callback_success = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForCookiesTimeout, ^bool {
          return cookies_found == cookies_deleted;
        });
    EXPECT_TRUE(callback_success);
  }

  int CookiesCount() {
    __block int cookies_count = -1;
    [shared_store_ getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
      cookies_count = cookies.count;
    }];
    bool callback_success = base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForCookiesTimeout, ^bool {
          return cookies_count > -1;
        });
    EXPECT_TRUE(callback_success);
    return cookies_count;
  }

  SystemCookieStore* GetCookieStore() { return store_.get(); }

 private:
  web::TestWebThreadBundle web_thread_;
  WKHTTPCookieStore* shared_store_;
  std::unique_ptr<web::WKHTTPSystemCookieStore> store_;
};

API_AVAILABLE(ios(11.0))
INSTANTIATE_TYPED_TEST_CASE_P(WKHTTPSystemCookieStore,
                              SystemCookieStoreTest,
                              WKHTTPSystemCookieStoreTestDelegate);

}  // namespace net
