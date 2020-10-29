// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_access_delegate_impl.h"

#include "net/cookies/cookie_util.h"
#include "services/network/first_party_sets/preloaded_first_party_sets.h"

namespace network {

CookieAccessDelegateImpl::CookieAccessDelegateImpl(
    mojom::CookieAccessDelegateType type,
    const PreloadedFirstPartySets* preloaded_first_party_sets,
    const CookieSettings* cookie_settings)
    : type_(type), cookie_settings_(cookie_settings) {
  // TODO(crbug.com/1143756): Save and use the PreloadedFirstPartySets.
  if (type == mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS) {
    DCHECK(cookie_settings);
  }
}

CookieAccessDelegateImpl::~CookieAccessDelegateImpl() = default;

net::CookieAccessSemantics CookieAccessDelegateImpl::GetAccessSemantics(
    const net::CanonicalCookie& cookie) const {
  if (type_ == mojom::CookieAccessDelegateType::ALWAYS_LEGACY)
    return net::CookieAccessSemantics::LEGACY;
  return cookie_settings_->GetCookieAccessSemanticsForDomain(cookie.Domain());
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

}  // namespace network
