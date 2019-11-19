// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_
#define NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_

#include <map>

#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_constants.h"

namespace net {

// CookieAccessDelegate for testing. You can set the return value for a given
// cookie_domain (modulo any leading dot). Calling GetAccessSemantics() will
// then return the given value, or UNKNOWN if you haven't set one.
class TestCookieAccessDelegate : public CookieAccessDelegate {
 public:
  TestCookieAccessDelegate();
  ~TestCookieAccessDelegate() override;

  // CookieAccessDelegate implementation:
  CookieAccessSemantics GetAccessSemantics(
      const CanonicalCookie& cookie) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const GURL& site_for_cookies) const override;

  // Sets the expected return value for any cookie whose Domain
  // matches |cookie_domain|. Pass the value of |cookie.Domain()| and any
  // leading dot will be discarded.
  void SetExpectationForCookieDomain(const std::string& cookie_domain,
                                     CookieAccessSemantics access_semantics);

  // Sets the expected return value for ShouldAlwaysAttachSameSiteCookies.
  // Can set schemes that always attach SameSite cookies, or schemes that always
  // attach SameSite cookies if the request URL is secure.
  void SetIgnoreSameSiteRestrictionsScheme(
      const std::string& site_for_cookies_scheme,
      bool require_secure_origin);

 private:
  // Discard any leading dot in the domain string.
  std::string GetKeyForDomainValue(const std::string& domain) const;

  std::map<std::string, CookieAccessSemantics> expectations_;
  std::map<std::string, bool> ignore_samesite_restrictions_schemes_;

  DISALLOW_COPY_AND_ASSIGN(TestCookieAccessDelegate);
};

}  // namespace net

#endif  // NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_
