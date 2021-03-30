// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_access_delegate_impl.h"

#include "net/cookies/cookie_util.h"
#include "services/network/first_party_sets/first_party_sets.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace network {

CookieAccessDelegateImpl::CookieAccessDelegateImpl(
    mojom::CookieAccessDelegateType type,
    const FirstPartySets* first_party_sets,
    const CookieSettings* cookie_settings)
    : type_(type),
      cookie_settings_(cookie_settings),
      first_party_sets_(first_party_sets) {
  if (type == mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS) {
    DCHECK(cookie_settings);
  }
}

CookieAccessDelegateImpl::~CookieAccessDelegateImpl() = default;

bool CookieAccessDelegateImpl::ShouldTreatUrlAsTrustworthy(
    const GURL& url) const {
  return IsUrlPotentiallyTrustworthy(url);
}

net::CookieAccessSemantics CookieAccessDelegateImpl::GetAccessSemantics(
    const net::CanonicalCookie& cookie) const {
  switch (type_) {
    case mojom::CookieAccessDelegateType::ALWAYS_LEGACY:
      return net::CookieAccessSemantics::LEGACY;
    case mojom::CookieAccessDelegateType::ALWAYS_NONLEGACY:
      return net::CookieAccessSemantics::NONLEGACY;
    case mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS:
      return cookie_settings_->GetCookieAccessSemanticsForDomain(
          cookie.Domain());
  }
}

bool CookieAccessDelegateImpl::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  if (cookie_settings_) {
    return cookie_settings_->ShouldIgnoreSameSiteRestrictions(
        url, site_for_cookies.RepresentativeUrl());
  }
  return false;
}

bool CookieAccessDelegateImpl::IsContextSamePartyWithSite(
    const net::SchemefulSite& site,
    const net::SchemefulSite& top_frame_site,
    const std::set<net::SchemefulSite>& party_context) const {
  return first_party_sets_ && first_party_sets_->IsContextSamePartyWithSite(
                                  site, top_frame_site, party_context);
}

bool CookieAccessDelegateImpl::IsInNontrivialFirstPartySet(
    const net::SchemefulSite& site) const {
  return first_party_sets_ &&
         first_party_sets_->IsInNontrivialFirstPartySet(site);
}

base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>
CookieAccessDelegateImpl::RetrieveFirstPartySets() const {
  if (!first_party_sets_)
    return {};
  return first_party_sets_->Sets();
}

}  // namespace network
