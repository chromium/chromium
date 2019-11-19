// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_http_system_cookie_store.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include <memory>

#import "base/test/ios/wait_util.h"
#include "ios/net/cookies/system_cookie_store_unittest_template.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

// Test class that conforms to net::SystemCookieStoreTestDelegate to exercise
// WKHTTPSystemCookieStore.
class WKHTTPSystemCookieStoreTestDelegate {
 public:
  WKHTTPSystemCookieStoreTestDelegate() {
    // Using off the record browser state so it will use non-persistent
    // datastore.
    browser_state_.SetOffTheRecord(true);
    web::WKWebViewConfigurationProvider& config_provider =
        web::WKWebViewConfigurationProvider::FromBrowserState(&browser_state_);
    shared_store_ = config_provider.GetWebViewConfiguration()
                        .websiteDataStore.httpCookieStore;
    store_ = std::make_unique<web::WKHTTPSystemCookieStore>(&config_provider);
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
  web::WebTaskEnvironment task_environment_;
  web::TestBrowserState browser_state_;
  WKHTTPCookieStore* shared_store_ = nil;
  std::unique_ptr<web::WKHTTPSystemCookieStore> store_;
};

API_AVAILABLE(ios(11.0))
INSTANTIATE_TYPED_TEST_SUITE_P(WKHTTPSystemCookieStore,
                               SystemCookieStoreTest,
                               WKHTTPSystemCookieStoreTestDelegate);

}  // namespace net
