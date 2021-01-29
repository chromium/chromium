// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/test_cookie_access_delegate.h"

#include "net/base/schemeful_site.h"
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

bool TestCookieAccessDelegate::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const SiteForCookies& site_for_cookies) const {
  auto it =
      ignore_samesite_restrictions_schemes_.find(site_for_cookies.scheme());
  if (it == ignore_samesite_restrictions_schemes_.end())
    return false;
  if (it->second)
    return url.SchemeIsCryptographic();
  return true;
}

bool TestCookieAccessDelegate::IsContextSamePartyWithSite(
    const net::SchemefulSite& site,
    const net::SchemefulSite& top_frame_site,
    const std::set<net::SchemefulSite>& party_context) const {
  return false;
}

bool TestCookieAccessDelegate::IsInNontrivialFirstPartySet(
    const net::SchemefulSite& site) const {
  return false;
}

base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>
TestCookieAccessDelegate::RetrieveFirstPartySets() const {
  return first_party_sets_;
}

void TestCookieAccessDelegate::SetExpectationForCookieDomain(
    const std::string& cookie_domain,
    CookieAccessSemantics access_semantics) {
  expectations_[GetKeyForDomainValue(cookie_domain)] = access_semantics;
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

void TestCookieAccessDelegate::SetFirstPartySets(
    const base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>&
        sets) {
  first_party_sets_ = sets;
}

}  // namespace net
