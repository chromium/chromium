// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/test_cookie_access_delegate.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"

namespace net {

TestCookieAccessDelegate::TestCookieAccessDelegate() = default;

TestCookieAccessDelegate::~TestCookieAccessDelegate() = default;

CookieAccessSemantics TestCookieAccessDelegate::GetAccessSemantics(
    const CanonicalCookie& cookie) const {
  auto it = expectations_.find(GetKeyForDomainValue(cookie.Domain()));
  if (it != expectations_.end())
    return it->second;
  return CookieAccessSemantics::UNKNOWN;
}

CookieScopeSemantics TestCookieAccessDelegate::GetScopeSemantics(
    const std::string_view domain) const {
  GURL cookie_domain_url = net::cookie_util::CookieOriginToURL(
      std::string(domain), /*is_https=*/false);
  auto it = expectations_scoped_.find(SchemefulSite(cookie_domain_url));
  if (it != expectations_scoped_.end()) {
    return it->second;
  }
  return CookieScopeSemantics::UNKNOWN;
}

bool TestCookieAccessDelegate::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const SiteForCookies& site_for_cookies,
    const url::Origin& top_level_origin) const {
  auto it =
      ignore_samesite_restrictions_schemes_.find(site_for_cookies.scheme());
  if (it == ignore_samesite_restrictions_schemes_.end())
    return false;
  if (it->second)
    return url.SchemeIsCryptographic();
  return true;
}

// Returns true if `url` has the same scheme://eTLD+1 as `trustworthy_site_`.
bool TestCookieAccessDelegate::ShouldTreatUrlAsTrustworthy(
    const GURL& url) const {
  return trustworthy_site_.IsSameSiteWith(url);
}

void TestCookieAccessDelegate::SetExpectationForCookieDomain(
    const std::string& cookie_domain,
    CookieAccessSemantics access_semantics) {
  expectations_[GetKeyForDomainValue(cookie_domain)] = access_semantics;
}

void TestCookieAccessDelegate::SetExpectationForCookieScope(
    const std::string_view& cookie_domain,
    CookieScopeSemantics scoped_semantics) {
  GURL cookie_domain_url = net::cookie_util::CookieOriginToURL(
      std::string(cookie_domain), /*is_https=*/false);
  expectations_scoped_[SchemefulSite(cookie_domain_url)] = scoped_semantics;
}

void TestCookieAccessDelegate::SetIgnoreSameSiteRestrictionsScheme(
    const std::string& site_for_cookies_scheme,
    bool require_secure_origin) {
  ignore_samesite_restrictions_schemes_[site_for_cookies_scheme] =
      require_secure_origin;
}

std::string TestCookieAccessDelegate::GetKeyForDomainValue(
    const std::string& domain) const {
  DCHECK(!domain.empty());
  return cookie_util::CookieDomainAsHost(domain);
}

}  // namespace net
