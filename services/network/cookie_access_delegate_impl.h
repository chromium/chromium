// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_
#define SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_constants.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "services/network/cookie_settings.h"
#include "services/network/first_party_sets/first_party_sets_access_delegate.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace net {
class CanonicalCookie;
class SchemefulSite;
}  // namespace net

namespace network {

// This class acts as a delegate for the CookieStore to query the
// CookieManager's CookieSettings for instructions on how to handle a given
// cookie with respect to SameSite, and to apply developer preferences on
// trusted sites for purpose of secure cookies.
class COMPONENT_EXPORT(NETWORK_SERVICE) CookieAccessDelegateImpl
    : public net::CookieAccessDelegate {
 public:
  // If |type| is USE_CONTENT_SETTINGS, a non-null |cookie_settings| is
  // expected. |cookie_settings| contains the set of content settings that
  // describes which cookies should be subject to legacy access rules.
  // If non-null, |cookie_settings| is expected to outlive this class. If
  // non-null, `first_party_sets_access_delegate` must outlive `this`.
  CookieAccessDelegateImpl(
      mojom::CookieAccessDelegateType type,
      FirstPartySetsAccessDelegate* const first_party_sets_access_delegate,
      const CookieSettings* cookie_settings = nullptr);

  ~CookieAccessDelegateImpl() override;

  // net::CookieAccessDelegate implementation:
  bool ShouldTreatUrlAsTrustworthy(const GURL& url) const override;
  net::CookieAccessSemantics GetAccessSemantics(
      const net::CanonicalCookie& cookie) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override;
  [[nodiscard]] std::optional<
      std::pair<net::FirstPartySetMetadata,
                net::FirstPartySetsCacheFilter::MatchInfo>>
  ComputeFirstPartySetMetadataMaybeAsync(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      base::OnceCallback<void(net::FirstPartySetMetadata,
                              net::FirstPartySetsCacheFilter::MatchInfo)>
          callback) const override;
  [[nodiscard]] std::optional<FirstPartySetsAccessDelegate::EntriesResult>
  FindFirstPartySetEntries(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<void(FirstPartySetsAccessDelegate::EntriesResult)>
          callback) const override;

 private:
  const mojom::CookieAccessDelegateType type_;
  const raw_ptr<const CookieSettings> cookie_settings_;
  const raw_ptr<FirstPartySetsAccessDelegate> first_party_sets_access_delegate_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_
