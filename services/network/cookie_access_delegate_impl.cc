// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_access_delegate_impl.h"

#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace network {

CookieAccessDelegateImpl::CookieAccessDelegateImpl(
    mojom::CookieAccessDelegateType type,
    const CookieSettings* cookie_settings)
    : type_(type), cookie_settings_(cookie_settings) {
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

net::CookieScopeSemantics CookieAccessDelegateImpl::GetScopeSemantics(
    const std::string_view domain) const {
  if (!cookie_settings_) {
    return net::CookieScopeSemantics::UNKNOWN;
  }
  return cookie_settings_->GetCookieScopeSemanticsForDomain(domain);
}

bool CookieAccessDelegateImpl::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_level_origin) const {
  if (cookie_settings_) {
    return cookie_settings_->ShouldIgnoreSameSiteRestrictions(
        url, site_for_cookies, top_level_origin);
  }
  return false;
}

}  // namespace network
