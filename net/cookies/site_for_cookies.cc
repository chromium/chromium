// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/site_for_cookies.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_util.h"

namespace net {

SiteForCookies::SiteForCookies() = default;

SiteForCookies::SiteForCookies(const SchemefulSite& site)
    : site_(site), schemefully_same_(!site.opaque()) {
  site_.ConvertWebSocketToHttp();
}

SiteForCookies::SiteForCookies(const SiteForCookies& other) = default;
SiteForCookies::SiteForCookies(SiteForCookies&& other) = default;

SiteForCookies::~SiteForCookies() = default;

SiteForCookies& SiteForCookies::operator=(const SiteForCookies& other) =
    default;
SiteForCookies& SiteForCookies::operator=(SiteForCookies&& site_for_cookies) =
    default;

// static
bool SiteForCookies::FromWire(const SchemefulSite& site,
                              bool schemefully_same,
                              SiteForCookies* out) {
  SiteForCookies candidate(site);
  if (site != candidate.site_)
    return false;

  candidate.schemefully_same_ = schemefully_same;

  *out = std::move(candidate);
  return true;
}

// static
SiteForCookies SiteForCookies::FromOrigin(const url::Origin& origin) {
  return SiteForCookies(SchemefulSite(origin));
}

// static
SiteForCookies SiteForCookies::FromUrl(const GURL& url) {
  return SiteForCookies::FromOrigin(url::Origin::Create(url));
}

std::string SiteForCookies::ToDebugString() const {
  std::string same_scheme_string = schemefully_same_ ? "true" : "false";
  return base::StrCat({"SiteForCookies: {site=", site_.Serialize(),
                       "; schemefully_same=", same_scheme_string, "}"});
}

bool SiteForCookies::IsFirstParty(const GURL& url) const {
  return IsFirstPartyWithSchemefulMode(
      url, cookie_util::IsSchemefulSameSiteEnabled());
}

bool SiteForCookies::IsFirstPartyWithSchemefulMode(
    const GURL& url,
    bool compute_schemefully) const {
  if (compute_schemefully)
    return IsSchemefullyFirstParty(url);

  return IsSchemelesslyFirstParty(url);
}

bool SiteForCookies::IsEquivalent(const SiteForCookies& other) const {
  if (IsNull() || other.IsNull()) {
    // We need to check if `other.IsNull()` explicitly in order to catch if
    // `other.schemefully_same_` is false when "Schemeful Same-Site" is enabled.
    return IsNull() && other.IsNull();
  }

  // In the case where the site has no registrable domain or host, the scheme
  // cannot be ws(s) or http(s), so equality of sites implies actual equality of
  // schemes (not just modulo ws-http and wss-https compatibility).
  if (cookie_util::IsSchemefulSameSiteEnabled() ||
      !site_.has_registrable_domain_or_host()) {
    return site_ == other.site_;
  }

  return site_.SchemelesslyEqual(other.site_);
}

bool SiteForCookies::CompareWithFrameTreeSiteAndRevise(
    const SchemefulSite& other) {
  // Two opaque SFC are considered equivalent.
  if (site_.opaque() && other.opaque())
    return true;

  // But if only one is opaque we should return false.
  if (site_.opaque())
    return false;

  // Nullify `this` if the `other` is opaque
  if (other.opaque()) {
    site_ = SchemefulSite();
    return false;
  }

  bool nullify = site_.has_registrable_domain_or_host()
                     ? !site_.SchemelesslyEqual(other)
                     : site_ != other;

  if (nullify) {
    // We should only nullify this SFC if the registrable domains (or the entire
    // site for cases without an RD) don't match. We *should not* nullify if
    // only the schemes mismatch (unless there is no RD) because cookies may be
    // processed with LEGACY semantics which only use the RDs. Eventually, when
    // schemeful same-site can no longer be disabled, we can revisit this.
    site_ = SchemefulSite();
    return false;
  }

  MarkIfCrossScheme(other);

  return true;
}

bool SiteForCookies::CompareWithFrameTreeOriginAndRevise(
    const url::Origin& other) {
  return CompareWithFrameTreeSiteAndRevise(SchemefulSite(other));
}

GURL SiteForCookies::RepresentativeUrl() const {
  if (IsNull())
    return GURL();
  // Cannot use url::Origin::GetURL() because it loses the hostname for file:
  // scheme origins.
  GURL result(base::StrCat({scheme(), "://", registrable_domain(), "/"}));
  DCHECK(result.is_valid());
  return result;
}

bool SiteForCookies::IsNull() const {
  if (cookie_util::IsSchemefulSameSiteEnabled())
    return site_.opaque() || !schemefully_same_;

  return site_.opaque();
}

bool SiteForCookies::IsSchemefullyFirstParty(const GURL& url) const {
  // Can't use IsNull() as we want the same behavior regardless of
  // SchemefulSameSite feature status.
  if (site_.opaque() || !schemefully_same_ || !url.is_valid())
    return false;

  SchemefulSite other_site(url);
  other_site.ConvertWebSocketToHttp();
  return site_ == other_site;
}

bool SiteForCookies::IsSchemelesslyFirstParty(const GURL& url) const {
  // Can't use IsNull() as we want the same behavior regardless of
  // SchemefulSameSite feature status.
  if (site_.opaque() || !url.is_valid())
    return false;

  // We don't need to bother changing WebSocket schemes to http, because if
  // there is no registrable domain or host, the scheme cannot be ws(s) or
  // http(s), and the latter comparison is schemeless anyway.
  SchemefulSite other_site(url);
  if (!site_.has_registrable_domain_or_host())
    return site_ == other_site;

  return site_.SchemelesslyEqual(other_site);
}

void SiteForCookies::MarkIfCrossScheme(const SchemefulSite& other) {
  // If `this` is IsNull() then `this` doesn't match anything which means that
  // the scheme check is pointless. Also exit early if schemefully_same_ is
  // already false.
  if (IsNull() || !schemefully_same_)
    return;

  // Mark if `other` is opaque. Opaque origins shouldn't match.
  if (other.opaque()) {
    schemefully_same_ = false;
    return;
  }

  // Conversion to http/https should have occurred during construction.
  DCHECK_NE(url::kWsScheme, scheme());
  DCHECK_NE(url::kWssScheme, scheme());

  // If the schemes are equal, modulo ws-http and wss-https, don't mark.
  if (scheme() == other.site_as_origin_.scheme() ||
      (scheme() == url::kHttpsScheme &&
       other.site_as_origin_.scheme() == url::kWssScheme) ||
      (scheme() == url::kHttpScheme &&
       other.site_as_origin_.scheme() == url::kWsScheme)) {
    return;
  }

  // Mark that the two are cross-scheme to each other.
  schemefully_same_ = false;
}

bool operator<(const SiteForCookies& lhs, const SiteForCookies& rhs) {
  // Similar to IsEquivalent(), if they're both null then they're equivalent
  // and therefore `lhs` is not < `rhs`.
  if (lhs.IsNull() && rhs.IsNull())
    return false;

  // If only `lhs` is null then it's always < `rhs`.
  if (lhs.IsNull())
    return true;

  // If only `rhs` is null then `lhs` is not < `rhs`.
  if (rhs.IsNull())
    return false;

  // Otherwise neither are null and we need to compare the `site_`s.
  return lhs.site_ < rhs.site_;
}

}  // namespace net
