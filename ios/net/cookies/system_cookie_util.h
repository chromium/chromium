// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_SYSTEM_COOKIE_UTIL_H_
#define IOS_NET_COOKIES_SYSTEM_COOKIE_UTIL_H_

#import <Foundation/Foundation.h>
#include <stddef.h>

#include <memory>

#include "net/cookies/canonical_cookie.h"

@class NSHTTPCookie;

namespace base {
class Time;
}

namespace net {

// Converts NSHTTPCookie to net::CanonicalCookie.
std::unique_ptr<net::CanonicalCookie> CanonicalCookieFromSystemCookie(
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

}  // namespace net

#endif  // IOS_NET_COOKIES_SYSTEM_COOKIE_UTIL_H_
