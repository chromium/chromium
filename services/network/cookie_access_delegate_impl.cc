// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_access_delegate_impl.h"

#include <optional>
#include <set>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace network {

CookieAccessDelegateImpl::CookieAccessDelegateImpl(
    mojom::CookieAccessDelegateType type,
    FirstPartySetsAccessDelegate* const first_party_sets_access_delegate,
    const CookieSettings* cookie_settings)
    : type_(type),
      cookie_settings_(cookie_settings),
      first_party_sets_access_delegate_(first_party_sets_access_delegate) {
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
    return cookie_settings_->ShouldIgnoreSameSiteRestrictions(url,
                                                              site_for_cookies);
  }
  return false;
}

std::optional<std::pair<net::FirstPartySetMetadata,
                        net::FirstPartySetsCacheFilter::MatchInfo>>
CookieAccessDelegateImpl::ComputeFirstPartySetMetadataMaybeAsync(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata,
                            net::FirstPartySetsCacheFilter::MatchInfo)>
        callback) const {
  if (!first_party_sets_access_delegate_) {
    return std::make_pair(net::FirstPartySetMetadata(),
                          net::FirstPartySetsCacheFilter::MatchInfo());
  }
  return first_party_sets_access_delegate_->ComputeMetadata(
      site, top_frame_site, std::move(callback));
}

std::optional<FirstPartySetsAccessDelegate::EntriesResult>
CookieAccessDelegateImpl::FindFirstPartySetEntries(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySetsAccessDelegate::EntriesResult)>
        callback) const {
  if (!first_party_sets_access_delegate_)
    return FirstPartySetsAccessDelegate::EntriesResult();
  return first_party_sets_access_delegate_->FindEntries(sites,
                                                        std::move(callback));
}

}  // namespace network
