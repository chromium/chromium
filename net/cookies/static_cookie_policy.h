// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_STATIC_COOKIE_POLICY_H_
#define NET_COOKIES_STATIC_COOKIE_POLICY_H_

#include "net/base/net_export.h"

class GURL;

namespace net {

class SiteForCookies;

// The StaticCookiePolicy class implements a static cookie policy that supports
// three modes: allow all, deny all, or block third-party cookies.
class NET_EXPORT StaticCookiePolicy {
 public:
  // Do not change the order of these types as they are persisted in
  // preferences.
  enum Type {
    // Do not perform any cookie blocking.
    ALLOW_ALL_COOKIES = 0,
    // Block all cookies (third-party or not) from begin set or read.
    BLOCK_ALL_COOKIES,
    // Prevent only third-party cookies from being set or read.
    BLOCK_ALL_THIRD_PARTY_COOKIES
  };

  StaticCookiePolicy() : type_(StaticCookiePolicy::ALLOW_ALL_COOKIES) {}

  explicit StaticCookiePolicy(Type type) : type_(type) {}

  StaticCookiePolicy(const StaticCookiePolicy&) = delete;
  StaticCookiePolicy& operator=(const StaticCookiePolicy&) = delete;

  // Sets the current policy to enforce. This should be called when the user's
  // preferences change.
  void set_type(Type type) { type_ = type; }
  Type type() const { return type_; }

  // Consults the user's third-party cookie blocking preferences to determine
  // whether the URL's cookies can be accessed (i.e., can be get or set).
  int CanAccessCookies(const GURL& url,
                       const net::SiteForCookies& site_for_cookies) const;

 private:
  Type type_;
};

}  // namespace net

#endif  // NET_COOKIES_STATIC_COOKIE_POLICY_H_
