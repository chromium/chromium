// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_SYSTEM_COOKIE_UTIL_H_
#define IOS_NET_COOKIES_SYSTEM_COOKIE_UTIL_H_

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "net/cookies/canonical_cookie.h"

@class NSHTTPCookie;

namespace base {
class Time;
}

namespace net {

// Converts NSHTTPCookie to net::CanonicalCookie.
net::CanonicalCookie CanonicalCookieFromSystemCookie(
    NSHTTPCookie* cookie,
    const base::Time& ceation_time);

// Converts net::CanonicalCookie to NSHTTPCookie.
NSHTTPCookie* SystemCookieFromCanonicalCookie(
    const net::CanonicalCookie& cookie);

// Converts net::CookieList to NSArray<NSHTTPCookie*>.
NSArray<NSHTTPCookie*>* SystemCookiesFromCanonicalCookieList(
    const net::CookieList& cookie_list);

enum CookieEvent {
  COOKIES_READ,                     // Cookies have been read from disk.
  COOKIES_APPLICATION_FOREGROUNDED  // The application has been foregrounded.
};

// Enum for the IOS.Cookies.GetCookiesForURLCallStoreType UMA histogram to
// report the type of the backing system cookie store, when GetCookiesForURL
// method is called.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SystemCookieStoreType {
  kWKHTTPSystemCookieStore = 0,
  kNSHTTPSystemCookieStore = 1,
  kCookieMonster = 2,
  kMaxValue = kCookieMonster,
};

// Enum for the IOS.Cookies.GetCookiesForURLCallResult UMA histogram to report
// if the call found cookies or no cookies were found on a specific system
// cookie store type.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetCookiesForURLCallResult {
  kCookiesFoundOnWKHTTPSystemCookieStore = 0,
  kNoCookiesOnWKHTTPSystemCookieStore = 1,
  kCookiesFoundOnNSHTTPSystemCookieStore = 2,
  kNoCookiesOnNSHTTPSystemCookieStore = 3,
  kCookiesFoundOnCookieMonster = 4,
  kNoCookiesOnCookieMonster = 5,
  kMaxValue = kNoCookiesOnCookieMonster,
};

// Reports metrics to indicate if call to GetCookiesForURL found cookies or no
// cookies were found on a specific system cookie store type.
void ReportGetCookiesForURLResult(SystemCookieStoreType store_type,
                                  bool has_cookies);

// Reports metrics to indicate that GetCookiesForURL was called from cookie
// store with type |store_type|.
void ReportGetCookiesForURLCall(SystemCookieStoreType store_type);

// Report metrics if the number of cookies drops unexpectedly.
void CheckForCookieLoss(size_t cookie_count, CookieEvent event);

// Reset the cookie count internally used by the CheckForCookieLoss() function.
void ResetCookieCountMetrics();

}  // namespace net

#endif  // IOS_NET_COOKIES_SYSTEM_COOKIE_UTIL_H_
