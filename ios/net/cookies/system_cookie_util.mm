// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/system_cookie_util.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "net/cookies/cookie_constants.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

namespace {

// Used to report cookie loss events in UMA.
enum CookieLossType {
  LOSS,
  NOT_ENOUGH_COOKIES,  // Not enough cookies, error checking is disabled.
  NO_LOSS,
  COOKIE_LOSS_ENUM_COUNT
};

// Undocumented property of NSHTTPCookie.
NSString* const kNSHTTPCookieHttpOnly = @"HttpOnly";

// Possible value for SameSite policy. WebKit doesn't use the value "none" or
// any other value, it uses the empty value to represent none, and any value
// that is not "strict" or "lax" will be considered as none.
NSString* const kNSHTTPCookieSameSiteNone = @"none";

// Key in NSUserDefaults telling wether a low cookie count must be reported as
// an error.
NSString* const kCheckCookieLossKey = @"CookieUtilIOSCheckCookieLoss";

// When the cookie count reaches |kCookieThresholdHigh|, and then drops below
// |kCookieThresholdLow|, an error is reported.
// Cookies count immediately after installation may fluctuate due to session
// cookies that can expire quickly. In order to catch true cookie loss, it is
// better to wait till a sufficiently large number of cookies have been
// accumulated before checking for sudden drop in cookies count.
const size_t kCookieThresholdHigh = 100;
const size_t kCookieThresholdLow = 30;

// Updates the cookie loss histograms.
// |event| determines which histogram is updated, and |loss| is the value to add
// to this histogram.
void ReportUMACookieLoss(CookieLossType loss, CookieEvent event) {
  // Histogram macros require constant histogram names. Use the string constants
  // explicitly instead of using a variable.
  switch (event) {
    case COOKIES_READ:
      UMA_HISTOGRAM_ENUMERATION("CookieStoreIOS.LossOnRead", loss,
                                COOKIE_LOSS_ENUM_COUNT);
      break;
    case COOKIES_APPLICATION_FOREGROUNDED:
      UMA_HISTOGRAM_ENUMERATION("CookieStoreIOS.LossOnForegrounding", loss,
                                COOKIE_LOSS_ENUM_COUNT);
      break;
  }
}

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
      base::Time::FromDoubleT([[cookie expiresDate] timeIntervalSince1970]),
      base::Time(), [cookie isSecure], [cookie isHTTPOnly], same_site,
      // When iOS begins to support 'Priority' and 'SameParty' attributes, pass
      // them through here.
      net::COOKIE_PRIORITY_DEFAULT, false /* SameParty */,
      net::CookieSourceScheme::kUnset, url::PORT_UNSPECIFIED);
}

void ReportGetCookiesForURLResult(SystemCookieStoreType store_type,
                                  bool has_cookies) {
  GetCookiesForURLCallResult call_result =
      GetCookiesForURLCallResult::kCookiesFoundOnWKHTTPSystemCookieStore;
  switch (store_type) {
    case SystemCookieStoreType::kWKHTTPSystemCookieStore:
      call_result =
          has_cookies
              ? GetCookiesForURLCallResult::
                    kCookiesFoundOnWKHTTPSystemCookieStore
              : GetCookiesForURLCallResult::kNoCookiesOnWKHTTPSystemCookieStore;
      break;
    case SystemCookieStoreType::kNSHTTPSystemCookieStore:
      call_result =
          has_cookies
              ? GetCookiesForURLCallResult::
                    kCookiesFoundOnNSHTTPSystemCookieStore
              : GetCookiesForURLCallResult::kNoCookiesOnNSHTTPSystemCookieStore;

      break;
    case SystemCookieStoreType::kCookieMonster:
      call_result =
          has_cookies ? GetCookiesForURLCallResult::kCookiesFoundOnCookieMonster
                      : GetCookiesForURLCallResult::kNoCookiesOnCookieMonster;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("IOS.Cookies.GetCookiesForURLCallResult",
                            call_result);
}

void ReportGetCookiesForURLCall(SystemCookieStoreType store_type) {
  UMA_HISTOGRAM_ENUMERATION("IOS.Cookies.GetCookiesForURLCallStoreType",
                            store_type);
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
        [NSDate dateWithTimeIntervalSince1970:cookie.ExpiryDate().ToDoubleT()];
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

  if (cookie.IsSecure())
    [properties setObject:@"Y" forKey:NSHTTPCookieSecure];
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

void CheckForCookieLoss(size_t cookie_count, CookieEvent event) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  BOOL check_cookie_loss = [defaults boolForKey:kCheckCookieLossKey];

  // Start reporting errors if the cookie count goes above kCookieThresholdHigh.
  if (!check_cookie_loss && cookie_count >= kCookieThresholdHigh) {
    [defaults setBool:YES forKey:kCheckCookieLossKey];
    [defaults synchronize];
    return;
  }

  if (!check_cookie_loss) {
    // Error reporting is disabled.
    ReportUMACookieLoss(NOT_ENOUGH_COOKIES, event);
    return;
  }

  if (cookie_count > kCookieThresholdLow) {
    // No cookie loss.
    ReportUMACookieLoss(NO_LOSS, event);
    return;
  }

#if 0
  // TODO(ios): [Merge 277884]: crbug.com/386074 ERROR_REPORT is no longer
  // supported.
  LOG(ERROR_REPORT) << "Possible cookie issue (crbug/370024)\nCookie count: "
                    << cookie_count;
#endif

  ReportUMACookieLoss(LOSS, event);

  // Disable reporting until the cookie count rises above kCookieThresholdHigh
  // again.
  ResetCookieCountMetrics();
}

void ResetCookieCountMetrics() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:NO forKey:kCheckCookieLossKey];
  [defaults synchronize];
}

}  // namespace net
