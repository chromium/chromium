// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/system_cookie_util.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kCookieDomain[] = "domain";
const char kCookieName[] = "name";
const char kCookiePath[] = "path/";
const char kCookieValue[] = "value";
const char kCookieValueInvalidUtf8[] = "\x81r\xe4\xbd\xa0\xe5\xa5\xbd";

void CheckSystemCookie(const base::Time& expires, bool secure, bool httponly) {
  net::CookieSameSite same_site = net::CookieSameSite::NO_RESTRICTION;
  if (@available(iOS 13, *)) {
    // SamesitePolicy property of NSHTTPCookieStore is available on iOS 13+.
    same_site = net::CookieSameSite::LAX_MODE;
  }
  // Generate a canonical cookie.
  std::unique_ptr<net::CanonicalCookie> canonical_cookie =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          kCookieName, kCookieValue, kCookieDomain, kCookiePath,
          base::Time(),  // creation
          expires,
          base::Time(),  // last_access
          base::Time(),  // last_update
          secure, httponly, same_site, net::COOKIE_PRIORITY_DEFAULT);
  // Convert it to system cookie.
  NSHTTPCookie* system_cookie =
      SystemCookieFromCanonicalCookie(*canonical_cookie);

  // Check the attributes.
  EXPECT_TRUE(system_cookie);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kCookieName), [system_cookie name]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kCookieValue), [system_cookie value]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kCookieDomain), [system_cookie domain]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kCookiePath), [system_cookie path]);
  EXPECT_EQ(secure, [system_cookie isSecure]);
  EXPECT_EQ(httponly, [system_cookie isHTTPOnly]);
  EXPECT_EQ(expires.is_null(), [system_cookie isSessionOnly]);

  if (@available(iOS 13, *)) {
    EXPECT_NSEQ(NSHTTPCookieSameSiteLax, [system_cookie sameSitePolicy]);
  }
  // Allow 1 second difference as iOS rounds expiry time to the nearest second.
  base::Time system_cookie_expire_date = base::Time::FromSecondsSinceUnixEpoch(
      [[system_cookie expiresDate] timeIntervalSince1970]);
  EXPECT_LE(expires - base::Seconds(1), system_cookie_expire_date);
  EXPECT_GE(expires + base::Seconds(1), system_cookie_expire_date);
}

}  // namespace

using CookieUtil = PlatformTest;

TEST_F(CookieUtil, CanonicalCookieFromSystemCookie) {
  base::Time creation_time = base::Time::Now();
  base::Time expire_date = creation_time + base::Hours(2);
  NSDate* system_expire_date = [NSDate
      dateWithTimeIntervalSince1970:expire_date.InSecondsFSinceUnixEpoch()];
  NSMutableDictionary* properties =
      [NSMutableDictionary dictionaryWithDictionary:@{
        NSHTTPCookieDomain : @"foo",
        NSHTTPCookieName : @"a",
        NSHTTPCookiePath : @"/",
        NSHTTPCookieValue : @"b",
        NSHTTPCookieExpires : system_expire_date,
        @"HttpOnly" : @YES,
      }];
  if (@available(iOS 13, *)) {
    // sameSitePolicy is only available on iOS 13+.
    properties[NSHTTPCookieSameSitePolicy] = NSHTTPCookieSameSiteStrict;
  }

  NSHTTPCookie* system_cookie =
      [[NSHTTPCookie alloc] initWithProperties:properties];

  ASSERT_TRUE(system_cookie);
  std::unique_ptr<net::CanonicalCookie> chrome_cookie =
      CanonicalCookieFromSystemCookie(system_cookie, creation_time);
  EXPECT_EQ("a", chrome_cookie->Name());
  EXPECT_EQ("b", chrome_cookie->Value());
  EXPECT_EQ("foo", chrome_cookie->Domain());
  EXPECT_EQ("/", chrome_cookie->Path());
  EXPECT_EQ(creation_time, chrome_cookie->CreationDate());
  EXPECT_TRUE(chrome_cookie->LastAccessDate().is_null());
  EXPECT_TRUE(chrome_cookie->IsPersistent());
  // Allow 1 second difference as iOS rounds expiry time to the nearest second.
  EXPECT_LE(expire_date - base::Seconds(1), chrome_cookie->ExpiryDate());
  EXPECT_GE(expire_date + base::Seconds(1), chrome_cookie->ExpiryDate());
  EXPECT_FALSE(chrome_cookie->SecureAttribute());
  EXPECT_TRUE(chrome_cookie->IsHttpOnly());
  EXPECT_EQ(net::COOKIE_PRIORITY_DEFAULT, chrome_cookie->Priority());
  if (@available(iOS 13, *)) {
    EXPECT_EQ(net::CookieSameSite::STRICT_MODE, chrome_cookie->SameSite());
  }

  // Test session and secure cookie.
  system_cookie = [[NSHTTPCookie alloc] initWithProperties:@{
    NSHTTPCookieDomain : @"foo",
    NSHTTPCookieName : @"a",
    NSHTTPCookiePath : @"/",
    NSHTTPCookieValue : @"b",
    NSHTTPCookieSecure : @"Y",
  }];
  ASSERT_TRUE(system_cookie);
  chrome_cookie = CanonicalCookieFromSystemCookie(system_cookie, creation_time);
  EXPECT_FALSE(chrome_cookie->IsPersistent());
  EXPECT_TRUE(chrome_cookie->SecureAttribute());

  // Test a non-Canonical cookie does not cause a crash.
  system_cookie = [[NSHTTPCookie alloc] initWithProperties:@{
    NSHTTPCookieDomain : @"foo",
    // Malformed name will make the resulting cookie non-canonical.
    NSHTTPCookieName : @"A=",
    NSHTTPCookiePath : @"/",
    NSHTTPCookieValue : @"b",
  }];
  EXPECT_FALSE(CanonicalCookieFromSystemCookie(system_cookie, creation_time));
}

TEST_F(CookieUtil, SystemCookieFromCanonicalCookie) {
  base::Time expire_date = base::Time::Now() + base::Hours(2);

  // Test various combinations of session, secure and httponly attributes.
  CheckSystemCookie(expire_date, false, false);
  CheckSystemCookie(base::Time(), true, false);
  CheckSystemCookie(expire_date, false, true);
  CheckSystemCookie(base::Time(), true, true);
}

TEST_F(CookieUtil, SystemCookieFromBadCanonicalCookie) {
  // Generate a bad canonical cookie (value is invalid utf8).
  std::unique_ptr<net::CanonicalCookie> bad_canonical_cookie =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          kCookieName, kCookieValueInvalidUtf8, kCookieDomain, kCookiePath,
          base::Time(),  // creation
          base::Time(),  // expires
          base::Time(),  // last_access
          base::Time(),  // last_update
          false,         // secure
          false,         // httponly
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  // Convert it to system cookie.
  NSHTTPCookie* system_cookie =
      SystemCookieFromCanonicalCookie(*bad_canonical_cookie);
  EXPECT_TRUE(system_cookie == nil);
}

TEST_F(CookieUtil, SystemCookiesFromCanonicalCookieList) {
  base::Time expire_date = base::Time::Now() + base::Hours(2);
  net::CookieList cookie_list = {
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "name1", "value1", "domain1", "path1/",
          base::Time(),  // creation
          expire_date,
          base::Time(),  // last_access
          base::Time(),  // last_update
          false,         // secure
          false,         // httponly
          net::CookieSameSite::UNSPECIFIED, net::COOKIE_PRIORITY_DEFAULT),
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "name2", "value2", "domain2", "path2/",
          base::Time(),  // creation
          expire_date,
          base::Time(),  // last_access
          base::Time(),  // last_update
          false,         // secure
          false,         // httponly
          net::CookieSameSite::UNSPECIFIED, net::COOKIE_PRIORITY_DEFAULT),
  };

  NSArray<NSHTTPCookie*>* system_cookies =
      SystemCookiesFromCanonicalCookieList(cookie_list);

  ASSERT_EQ(2UL, system_cookies.count);
  EXPECT_NSEQ(@"name1", system_cookies[0].name);
  EXPECT_NSEQ(@"value1", system_cookies[0].value);
  EXPECT_NSEQ(@"domain1", system_cookies[0].domain);
  EXPECT_NSEQ(@"path1/", system_cookies[0].path);
  EXPECT_NSEQ(@"name2", system_cookies[1].name);
  EXPECT_NSEQ(@"value2", system_cookies[1].value);
  EXPECT_NSEQ(@"domain2", system_cookies[1].domain);
  EXPECT_NSEQ(@"path2/", system_cookies[1].path);
}

}  // namespace net
