// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/schemeful_site.h"

#include <string_view>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace net {

namespace {

// When `a_is_site` is true, `a` is actually a SchemefulSite internal
// `site_as_origin_`.
bool IsSameSiteInternal(const url::Origin& a,
                        const url::Origin& b,
                        bool a_is_site) {
  if (a.opaque() || b.opaque()) {
    return a == b;
  }

  if (a.scheme() != b.scheme()) {
    return false;
  }

  // The remaining code largely matches what `SameDomainOrHost()` would do, with
  // one exception: we consider equal-but-empty hosts to be same-site.

  // Host equality covers two cases:
  // 1. Non-network schemes where origins are passed through unchanged.
  // 2. Network schemes where equal hosts will have equal sites (and site
  //    computation is idempotent in cases where `a` is already a site).
  if (a.host() == b.host()) {
    return true;
  }

  // If host equality is false, then the only way the origins can be same site
  // is if we have a standard scheme with a network host, which is the only case
  // when SchemefulSite looks at the registerable domain.
  if (!IsStandardSchemeWithNetworkHost(a.scheme())) {
    return false;
  }

  std::string_view b_site = GetDomainAndRegistryAsStringPiece(
      b, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  // If either `a_site` or `b_site` is empty, their associated SchemefulSites
  // will have origins passed through without modification, and the positive
  // result would be covered in the host check above.
  if (b_site.empty()) {
    return false;
  }

  // Avoid re-calculating the site for `a` if it has already been done.
  std::string_view a_site =
      a_is_site
          ? a.host()
          : GetDomainAndRegistryAsStringPiece(
                a,
                net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return a_site == b_site;
}

}  // namespace

struct SchemefulSite::ObtainASiteResult {
  // This is only set if the supplied origin differs from calculated one.
  std::optional<url::Origin> origin;
  bool used_registerable_domain;
};

// Return a tuple containing:
// * a new origin using the registerable domain of `origin` if possible and
//   a port of 0; otherwise, the passed-in origin.
// * a bool indicating whether `origin` had a non-null registerable domain.
//   (False if `origin` was opaque.)
//
// Follows steps specified in
// https://html.spec.whatwg.org/multipage/origin.html#obtain-a-site
SchemefulSite::ObtainASiteResult SchemefulSite::ObtainASite(
    const url::Origin& origin) {
  // 1. If origin is an opaque origin, then return origin.
  if (origin.opaque()) {
    return {std::nullopt, false /* used_registerable_domain */};
  }

  int port = url::DefaultPortForScheme(origin.scheme());

  // Provide a default port of 0 for non-standard schemes.
  if (port == url::PORT_UNSPECIFIED) {
    port = 0;
  }

  std::string_view registerable_domain;

  // Non-normative step.
  // We only lookup the registerable domain for schemes with network hosts, this
  // is non-normative. Other schemes for non-opaque origins do not
  // meaningfully have a registerable domain for their host, so they are
  // skipped.
  if (IsStandardSchemeWithNetworkHost(origin.scheme())) {
    registerable_domain = GetDomainAndRegistryAsStringPiece(
        origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (!registerable_domain.empty() &&
        registerable_domain.size() == origin.host().size() &&
        origin.port() == port) {
      return {std::nullopt, /* used_registerable_domain */ true};
    }
  }

  // If origin's host's registrable domain is null, then return (origin's
  // scheme, origin's host).
  //
  // `GetDomainAndRegistry()` returns an empty string for IP literals and
  // effective TLDs.
  //
  // Note that `registerable_domain` could still end up empty, since the
  // `origin` might have a scheme that permits empty hostnames, such as "file".
  bool used_registerable_domain = !registerable_domain.empty();
  if (!used_registerable_domain)
    registerable_domain = origin.host();

  return {url::Origin::CreateFromNormalizedTuple(
              origin.scheme(), std::string(registerable_domain), port),
          used_registerable_domain};
}

SchemefulSite::SchemefulSite(ObtainASiteResult result,
                             const url::Origin& origin) {
  if (result.origin) {
    site_as_origin_ = std::move(*(result.origin));
  } else {
    site_as_origin_ = origin;
  }
}

SchemefulSite::SchemefulSite(const url::Origin& origin)
    : SchemefulSite(ObtainASite(origin), origin) {}

SchemefulSite::SchemefulSite(const GURL& url)
    : SchemefulSite(url::Origin::Create(url)) {}

SchemefulSite::SchemefulSite(const SchemefulSite& other) = default;
SchemefulSite::SchemefulSite(SchemefulSite&& other) noexcept = default;

SchemefulSite& SchemefulSite::operator=(const SchemefulSite& other) = default;
SchemefulSite& SchemefulSite::operator=(SchemefulSite&& other) noexcept =
    default;

// static
bool SchemefulSite::IsSameSite(const url::Origin& a, const url::Origin& b) {
  bool same_site = IsSameSiteInternal(a, b, /*a_is_site=*/false);
  DCHECK_EQ(same_site, SchemefulSite(a) == SchemefulSite(b));
  return same_site;
}

bool SchemefulSite::IsSameSiteWith(const url::Origin& other) const {
  bool same_site =
      IsSameSiteInternal(internal_value(), other, /*a_is_site=*/true);
  DCHECK_EQ(same_site, *this == SchemefulSite(other));
  return same_site;
}

// TODO(csharrison): Consider augmenting the below SameSite methods to avoid
// creating intermediate Origins. For now, we sacrifice some performance for
// simplicity as GURL --> Origin conversion can be quite subtle.
// We could likely fast-path simple cases (e.g. valid http/https GURLs).

// static
bool SchemefulSite::IsSameSite(const GURL& a, const GURL& b) {
  bool same_site = IsSameSiteInternal(
      url::Origin::Create(a), url::Origin::Create(b), /*a_is_site=*/false);
  DCHECK_EQ(same_site, SchemefulSite(a) == SchemefulSite(b));
  return same_site;
}

bool SchemefulSite::IsSameSiteWith(const GURL& other) const {
  bool same_site = IsSameSiteInternal(
      internal_value(), url::Origin::Create(other), /*a_is_site=*/true);
  DCHECK_EQ(same_site, *this == SchemefulSite(other));
  return same_site;
}

// static
bool SchemefulSite::FromWire(const url::Origin& site_as_origin,
                             SchemefulSite* out) {
  // The origin passed into this constructor may not match the
  // `site_as_origin_` used as the internal representation of the schemeful
  // site. However, a valid SchemefulSite's internal origin should result in a
  // match if used to construct another SchemefulSite. Thus, if there is a
  // mismatch here, we must indicate a failure.
  SchemefulSite candidate(site_as_origin);
  if (candidate.site_as_origin_ != site_as_origin)
    return false;

  *out = std::move(candidate);
  return true;
}

std::optional<SchemefulSite> SchemefulSite::CreateIfHasRegisterableDomain(
    const url::Origin& origin) {
  ObtainASiteResult result = ObtainASite(origin);
  if (!result.used_registerable_domain) {
    return std::nullopt;
  }
  return SchemefulSite(std::move(result), origin);
}

void SchemefulSite::ConvertWebSocketToHttp() {
  if (site_as_origin_.scheme() == url::kWsScheme ||
      site_as_origin_.scheme() == url::kWssScheme) {
    site_as_origin_ = url::Origin::Create(
        ChangeWebSocketSchemeToHttpScheme(site_as_origin_.GetURL()));
  }
}

// static
SchemefulSite SchemefulSite::Deserialize(std::string_view value) {
  return SchemefulSite(GURL(value));
}

std::string SchemefulSite::Serialize() const {
  return site_as_origin_.Serialize();
}

std::string SchemefulSite::SerializeFileSiteWithHost() const {
  DCHECK_EQ(url::kFileScheme, site_as_origin_.scheme());
  return site_as_origin_.GetTupleOrPrecursorTupleIfOpaque().Serialize();
}

std::string SchemefulSite::GetDebugString() const {
  return site_as_origin_.GetDebugString();
}

GURL SchemefulSite::GetURL() const {
  return site_as_origin_.GetURL();
}

const url::Origin& SchemefulSite::GetInternalOriginForTesting() const {
  return site_as_origin_;
}

size_t SchemefulSite::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(site_as_origin_);
}

// static
std::optional<SchemefulSite> SchemefulSite::DeserializeWithNonce(
    base::PassKey<NetworkAnonymizationKey>,
    std::string_view value) {
  return DeserializeWithNonce(value);
}

// static
std::optional<SchemefulSite> SchemefulSite::DeserializeWithNonce(
    std::string_view value) {
  std::optional<url::Origin> result = url::Origin::Deserialize(value);
  if (!result)
    return std::nullopt;
  return SchemefulSite(result.value());
}

std::optional<std::string> SchemefulSite::SerializeWithNonce(
    base::PassKey<NetworkAnonymizationKey>) {
  return SerializeWithNonce();
}

std::optional<std::string> SchemefulSite::SerializeWithNonce() {
  return site_as_origin_.SerializeWithNonceAndInitIfNeeded();
}

bool SchemefulSite::SchemelesslyEqual(const SchemefulSite& other) const {
  return site_as_origin_.host() == other.site_as_origin_.host();
}

std::ostream& operator<<(std::ostream& os, const SchemefulSite& ss) {
  os << ss.Serialize();
  return os;
}

}  // namespace net
