// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/cookie_util/cookie_util.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/components/cookie_util/cookie_constants.h"
#import "ios/net/cookies/cookie_store_ios_test_util.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForCookiesTimeout;

namespace {

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
  std::unique_ptr<TestingPrefServiceSimple> pref_service =
      std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterTimePref(kLastCookieDeletionDate,
                                             base::Time());
  base::Time start_test_time = base::Time::Now();
  // Delete cookies if the key is not present.
  pref_service->ClearPref(kLastCookieDeletionDate);
  EXPECT_TRUE(cookie_util::ShouldClearSessionCookies(pref_service.get()));
  // The deletion time should be created.
  base::Time deletion_time = pref_service->GetTime(kLastCookieDeletionDate);
  base::Time now = base::Time::Now();
  EXPECT_LE(start_test_time, deletion_time);
  EXPECT_LE(deletion_time, now);
  // Cookies are not deleted again.
  EXPECT_FALSE(cookie_util::ShouldClearSessionCookies(pref_service.get()));

  // Set the deletion time before the machine was started.
  // Sometime in year 1980.
  pref_service->SetTime(kLastCookieDeletionDate,
                        base::Time::FromTimeT(328697227));
  EXPECT_TRUE(cookie_util::ShouldClearSessionCookies(pref_service.get()));
  EXPECT_LE(now, pref_service->GetTime(kLastCookieDeletionDate));
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
      cookie_util::CookieStoreConfig::CookieStoreType::COOKIE_STORE_IOS);
  std::unique_ptr<net::CookieStore> cookie_store =
      cookie_util::CreateCookieStore(config, std::move(system_cookie_store),
                                     nullptr /* net_log */);

  net::CookieOptions options;
  options.set_include_httponly();
  std::string cookie_line = base::SysNSStringToUTF8(cookie_name) + "=" +
                            base::SysNSStringToUTF8(cookie_value);
  auto canonical_cookie = net::CanonicalCookie::CreateForTesting(
      test_url, cookie_line, base::Time::Now());
  cookie_store->SetCanonicalCookieAsync(std::move(canonical_cookie), test_url,
                                        options,
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
