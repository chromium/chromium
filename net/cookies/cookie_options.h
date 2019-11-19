// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_OPTIONS_H_
#define NET_COOKIES_COOKIE_OPTIONS_H_

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"
#include "url/gurl.h"

namespace net {

class NET_EXPORT CookieOptions {
 public:
  // Mask indicating insecure site-for-cookies and secure request/response.
  static const int kToSecureMask = 1 << 5;
  // Mask indicating secure site-for-cookies and insecure request/response.
  static const int kToInsecureMask = kToSecureMask << 1;

  // Relation between the cookie and the navigational environment.
  // CROSS_SITE to SAME_SITE_STRICT are ordered from least to most trusted
  // environment. The remaining values are reiterations with additional bits for
  // cross-scheme contexts. Don't renumber, used in histograms.
  enum class SameSiteCookieContext {
    CROSS_SITE = 0,
    // Same rules as lax but the http method is unsafe.
    SAME_SITE_LAX_METHOD_UNSAFE = 1,
    SAME_SITE_LAX = 2,
    SAME_SITE_STRICT = 3,
    // The CROSS_SCHEME enums are for when the url and site_for_cookies
    // differ in their schemes (http vs https). Their values are chosen such
    // that the CROSS_SCHEME flag can be bitmasked out.
    // SECURE_URL indicates either a request to a secure url or a response from
    // a secure url, similarly for INSECURE.
    SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_SECURE_URL =
        SAME_SITE_LAX_METHOD_UNSAFE | kToSecureMask,
    SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL = SAME_SITE_LAX | kToSecureMask,
    SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL = SAME_SITE_STRICT | kToSecureMask,
    SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_INSECURE_URL =
        SAME_SITE_LAX_METHOD_UNSAFE | kToInsecureMask,
    SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL = SAME_SITE_LAX | kToInsecureMask,
    SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL =
        SAME_SITE_STRICT | kToInsecureMask,

    // Keep last, used for histograms.
    COUNT
  };

  // Creates a CookieOptions object which:
  //
  // * Excludes HttpOnly cookies
  // * Excludes SameSite cookies
  // * Updates last-accessed time.
  // * Does not report excluded cookies in APIs that can do so.
  //
  // These settings can be altered by calling:
  //
  // * |set_{include,exclude}_httponly()|
  // * |set_same_site_cookie_context(
  //        CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT)|
  // * |set_do_not_update_access_time()|
  CookieOptions();

  void set_exclude_httponly() { exclude_httponly_ = true; }
  void set_include_httponly() { exclude_httponly_ = false; }
  bool exclude_httponly() const { return exclude_httponly_; }

  // How trusted is the current browser environment when it comes to accessing
  // SameSite cookies. Default is not trusted, e.g. CROSS_SITE.
  void set_same_site_cookie_context(SameSiteCookieContext context) {
    same_site_cookie_context_ = context;
  }

  // Strips off the cross-scheme bits to only return the same-site context.
  SameSiteCookieContext same_site_cookie_context() const {
    return RemoveCrossSchemeBitmask(same_site_cookie_context_);
  }

  SameSiteCookieContext same_site_cookie_context_full() const {
    return same_site_cookie_context_;
  }

  static SameSiteCookieContext ApplyCrossSchemeBitmask(
      SameSiteCookieContext context,
      int mask) {
    int return_value = static_cast<int>(context);
    return_value = return_value | mask;
    return static_cast<CookieOptions::SameSiteCookieContext>(return_value);
  }

  static SameSiteCookieContext RemoveCrossSchemeBitmask(
      SameSiteCookieContext context) {
    int return_value = static_cast<int>(context);
    return_value = return_value & ~(kToSecureMask | kToInsecureMask);
    return static_cast<CookieOptions::SameSiteCookieContext>(return_value);
  }

  bool IsDifferentScheme() const {
    return static_cast<int>(same_site_cookie_context_) &
           (kToSecureMask | kToInsecureMask);
  }

  void set_update_access_time() { update_access_time_ = true; }
  void set_do_not_update_access_time() { update_access_time_ = false; }
  bool update_access_time() const { return update_access_time_; }

  void set_return_excluded_cookies() { return_excluded_cookies_ = true; }
  void unset_return_excluded_cookies() { return_excluded_cookies_ = false; }
  bool return_excluded_cookies() const { return return_excluded_cookies_; }

  // Convenience method for where you need a CookieOptions that will
  // work for getting/setting all types of cookies, including HttpOnly and
  // SameSite cookies. Also specifies not to update the access time, because
  // usually this is done to get all the cookies to check that they are correct,
  // including the creation time. This basically makes a CookieOptions that is
  // the opposite of the default CookieOptions.
  static CookieOptions MakeAllInclusive();

 private:
  bool exclude_httponly_;
  SameSiteCookieContext same_site_cookie_context_;
  bool update_access_time_;
  bool return_excluded_cookies_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_OPTIONS_H_
