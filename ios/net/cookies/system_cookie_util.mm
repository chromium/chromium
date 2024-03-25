// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/system_cookie_util.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "net/cookies/cookie_constants.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace net {

namespace {

// Undocumented property of NSHTTPCookie.
NSString* const kNSHTTPCookieHttpOnly = @"HttpOnly";

// Possible value for SameSite policy. WebKit doesn't use the value "none" or
// any other value, it uses the empty value to represent none, and any value
// that is not "strict" or "lax" will be considered as none.
NSString* const kNSHTTPCookieSameSiteNone = @"none";

}  // namespace

// Converts NSHTTPCookie to net::CanonicalCookie.
std::unique_ptr<net::CanonicalCookie> CanonicalCookieFromSystemCookie(
    NSHTTPCookie* cookie,
    const base::Time& ceation_time) {
  net::CookieSameSite same_site = net::CookieSameSite::NO_RESTRICTION;
  if (@available(iOS 13, *)) {
    same_site = net::CookieSameSite::UNSPECIFIED;
    if ([cookie.sameSitePolicy isEqual:NSHTTPCookieSameSiteLax])
      same_site = net::CookieSameSite::LAX_MODE;

    if ([cookie.sameSitePolicy isEqual:NSHTTPCookieSameSiteStrict])
      same_site = net::CookieSameSite::STRICT_MODE;

    if ([[cookie.sameSitePolicy lowercaseString]
            isEqual:kNSHTTPCookieSameSiteNone])
      same_site = net::CookieSameSite::NO_RESTRICTION;
  }

  return net::CanonicalCookie::FromStorage(
      base::SysNSStringToUTF8([cookie name]),
      base::SysNSStringToUTF8([cookie value]),
      base::SysNSStringToUTF8([cookie domain]),
      base::SysNSStringToUTF8([cookie path]), ceation_time,
      base::Time::FromSecondsSinceUnixEpoch(
          [[cookie expiresDate] timeIntervalSince1970]),
      base::Time(), base::Time(), [cookie isSecure], [cookie isHTTPOnly],
      same_site,
      // When iOS begins to support the 'Priority' attribute, pass it through
      // here.
      net::COOKIE_PRIORITY_DEFAULT, std::nullopt /* partition_key */,
      net::CookieSourceScheme::kUnset, url::PORT_UNSPECIFIED,
      net::CookieSourceType::kOther);
}

// Converts net::CanonicalCookie to NSHTTPCookie.
NSHTTPCookie* SystemCookieFromCanonicalCookie(
    const net::CanonicalCookie& cookie) {
  NSString* cookie_domain = base::SysUTF8ToNSString(cookie.Domain());
  NSString* cookie_name = base::SysUTF8ToNSString(cookie.Name());
  NSString* cookie_path = base::SysUTF8ToNSString(cookie.Path());
  NSString* cookie_value = base::SysUTF8ToNSString(cookie.Value());
  if (!cookie_domain || !cookie_name || !cookie_path || !cookie_value) {
    DLOG(ERROR) << "Cannot create system cookie: " << cookie.DebugString();
    return nil;
  }
  NSMutableDictionary* properties =
      [NSMutableDictionary dictionaryWithDictionary:@{
        NSHTTPCookieDomain : cookie_domain,
        NSHTTPCookieName : cookie_name,
        NSHTTPCookiePath : cookie_path,
        NSHTTPCookieValue : cookie_value,
      }];
  if (cookie.IsPersistent()) {
    NSDate* expiry =
        [NSDate dateWithTimeIntervalSince1970:cookie.ExpiryDate()
                                                  .InSecondsFSinceUnixEpoch()];
    [properties setObject:expiry forKey:NSHTTPCookieExpires];
  }

  if (@available(iOS 13, *)) {
    // In iOS 13 sameSite property in NSHTTPCookie is used to specify the
    // samesite policy.
    NSString* same_site = @"";
    switch (cookie.SameSite()) {
      case net::CookieSameSite::LAX_MODE:
        same_site = NSHTTPCookieSameSiteLax;
        break;
      case net::CookieSameSite::STRICT_MODE:
        same_site = NSHTTPCookieSameSiteStrict;
        break;
      case net::CookieSameSite::NO_RESTRICTION:
        same_site = kNSHTTPCookieSameSiteNone;
        break;
      case net::CookieSameSite::UNSPECIFIED:
        // All other values of same site policy will be treated as no value .
        break;
    }
    properties[NSHTTPCookieSameSitePolicy] = same_site;
  }

  if (cookie.SecureAttribute()) {
    [properties setObject:@"Y" forKey:NSHTTPCookieSecure];
  }
  if (cookie.IsHttpOnly())
    [properties setObject:@YES forKey:kNSHTTPCookieHttpOnly];
  NSHTTPCookie* system_cookie = [NSHTTPCookie cookieWithProperties:properties];
  DCHECK(system_cookie);
  return system_cookie;
}

NSArray<NSHTTPCookie*>* SystemCookiesFromCanonicalCookieList(
    const net::CookieList& cookie_list) {
  NSMutableArray<NSHTTPCookie*>* cookies = [[NSMutableArray alloc] init];
  for (const net::CanonicalCookie& cookie : cookie_list) {
    [cookies addObject:net::SystemCookieFromCanonicalCookie(cookie)];
  }
  return [cookies copy];
}

}  // namespace net
