// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/cookie_util.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/net/cookies/cookie_store_ios_test_util.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_store.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForCookiesTimeout;

namespace {

// Date of the last cookie deletion.
NSString* const kLastCookieDeletionDate = @"LastCookieDeletionDate";

class CookieUtilTest : public PlatformTest {
 public:
  CookieUtilTest()
      : ns_http_cookie_store_([NSHTTPCookieStorage sharedHTTPCookieStorage]) {}

  ~CookieUtilTest() override {
    // Make sure NSHTTPCookieStorage is empty.
    [ns_http_cookie_store_ removeCookiesSinceDate:[NSDate distantPast]];
  }

 protected:
  NSHTTPCookieStorage* ns_http_cookie_store_;
};

TEST_F(CookieUtilTest, ShouldClearSessionCookies) {
  time_t start_test_time;
  time(&start_test_time);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  // Delete cookies if the key is not present.
  [defaults removeObjectForKey:kLastCookieDeletionDate];
  EXPECT_TRUE(cookie_util::ShouldClearSessionCookies());
  // The deletion time should be created.
  time_t deletion_time = [defaults integerForKey:kLastCookieDeletionDate];
  time_t now;
  time(&now);
  EXPECT_LE(start_test_time, deletion_time);
  EXPECT_LE(deletion_time, now);
  // Cookies are not deleted again.
  EXPECT_FALSE(cookie_util::ShouldClearSessionCookies());

  // Set the deletion time before the machine was started.
  // Sometime in year 1980.
  [defaults setInteger:328697227 forKey:kLastCookieDeletionDate];
  EXPECT_TRUE(cookie_util::ShouldClearSessionCookies());
  EXPECT_LE(now, [defaults integerForKey:kLastCookieDeletionDate]);
}

// Tests that CreateCookieStore returns the correct type of net::CookieStore
// based on the given parameters.
TEST_F(CookieUtilTest, CreateCookieStore) {
  web::WebTaskEnvironment task_environment;
  net::ScopedTestingCookieStoreIOSClient scoped_cookie_store_ios_client(
      std::make_unique<net::TestCookieStoreIOSClient>());

  GURL test_url("http://foo.google.com/bar");
  NSString* cookie_name = @"cookie_name";
  NSString* cookie_value = @"cookie_value";
  std::unique_ptr<net::SystemCookieStore> system_cookie_store =
      std::make_unique<net::NSHTTPSystemCookieStore>(ns_http_cookie_store_);
  net::SystemCookieStore* ns_cookie_store = system_cookie_store.get();
  cookie_util::CookieStoreConfig config(
      base::FilePath(),
      cookie_util::CookieStoreConfig::EPHEMERAL_SESSION_COOKIES,
      cookie_util::CookieStoreConfig::CookieStoreType::COOKIE_STORE_IOS,
      nullptr);
  std::unique_ptr<net::CookieStore> cookie_store =
      cookie_util::CreateCookieStore(config, std::move(system_cookie_store),
                                     nullptr /* net_log */);

  net::CookieOptions options;
  options.set_include_httponly();
  std::string cookie_line = base::SysNSStringToUTF8(cookie_name) + "=" +
                            base::SysNSStringToUTF8(cookie_value);
  auto canonical_cookie =
      net::CanonicalCookie::Create(test_url, cookie_line, base::Time::Now(),
                                   base::nullopt /* server_time */);
  cookie_store->SetCanonicalCookieAsync(std::move(canonical_cookie),
                                        test_url.scheme(), options,
                                        net::CookieStore::SetCookiesCallback());

  __block NSArray<NSHTTPCookie*>* result_cookies = nil;
  __block bool callback_called = false;
  ns_cookie_store->GetCookiesForURLAsync(
      test_url, base::BindOnce(^(NSArray<NSHTTPCookie*>* cookies) {
        callback_called = true;
        result_cookies = [cookies copy];
      }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));

  // The cookie should be set directly in the backing SystemCookieStore.
  EXPECT_EQ(1U, result_cookies.count);
  EXPECT_NSEQ(cookie_name, result_cookies[0].name);
  EXPECT_NSEQ(cookie_value, result_cookies[0].value);

  // Clear cookies that was set in the test.
  __block bool cookies_cleared = false;
  cookie_store->DeleteAllAsync(base::BindOnce(^(unsigned int) {
    cookies_cleared = true;
  }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return cookies_cleared;
  }));
}

}  // namespace
