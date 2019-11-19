// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/ns_http_system_cookie_store.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/test/task_environment.h"
#include "ios/net/cookies/system_cookie_store_unittest_template.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

// Test class that conforms to SystemCookieStoreTestDelegate to exercise
// NSHTTPSystemCookieStore created with
// |[NSHTTPCookieStorage sharedHTTPCookieStorage]|.
class NSHTTPSystemCookieStoreTestDelegate {
 public:
  NSHTTPSystemCookieStoreTestDelegate()
      : scoped_cookie_store_ios_client_(
            std::make_unique<TestCookieStoreIOSClient>()),
        shared_store_([NSHTTPCookieStorage sharedHTTPCookieStorage]),
        store_(std::make_unique<net::NSHTTPSystemCookieStore>(shared_store_)) {}

  bool IsCookieSet(NSHTTPCookie* system_cookie, NSURL* url) {
    // Verify that cookie is set in system storage.
    NSHTTPCookie* result_cookie = nil;

    for (NSHTTPCookie* cookie in [shared_store_ cookiesForURL:url]) {
      if ([cookie.name isEqualToString:system_cookie.name]) {
        result_cookie = cookie;
        break;
      }
    }
    return [result_cookie.value isEqualToString:system_cookie.value];
  }

  void ClearCookies() {
    // The default cookie accept policy is allow, The test needs to make sure
    // that policy is set to default so the test can set cookies.
    [shared_store_ setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
    for (NSHTTPCookie* cookie in shared_store_.cookies)
      [shared_store_ deleteCookie:cookie];
    EXPECT_EQ(0u, shared_store_.cookies.count);
  }

  int CookiesCount() { return shared_store_.cookies.count; }

  SystemCookieStore* GetCookieStore() { return store_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
  ScopedTestingCookieStoreIOSClient scoped_cookie_store_ios_client_;
  NSHTTPCookieStorage* shared_store_;
  std::unique_ptr<net::NSHTTPSystemCookieStore> store_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(NSHTTPSystemCookieStore,
                               SystemCookieStoreTest,
                               NSHTTPSystemCookieStoreTestDelegate);

}  // namespace net
