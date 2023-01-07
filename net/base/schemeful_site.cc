// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/schemeful_site.h"

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace net {

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
  if (origin.opaque())
    return {origin, false /* used_registerable_domain */};

  std::string registerable_domain;

  // Non-normative step.
  // We only lookup the registerable domain for schemes with network hosts, this
  // is non-normative. Other schemes for non-opaque origins do not
  // meaningfully have a registerable domain for their host, so they are
  // skipped.
  if (IsStandardSchemeWithNetworkHost(origin.scheme())) {
    registerable_domain = GetDomainAndRegistry(
        origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
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

  int port = url::DefaultPortForScheme(origin.scheme().c_str(),
                                       origin.scheme().length());

  // Provide a default port of 0 for non-standard schemes.
  if (port == url::PORT_UNSPECIFIED)
    port = 0;

  return {url::Origin::CreateFromNormalizedTuple(origin.scheme(),
                                                 registerable_domain, port),
          used_registerable_domain};
}

SchemefulSite::SchemefulSite(ObtainASiteResult result)
    : site_as_origin_(std::move(result.origin)) {}

SchemefulSite::SchemefulSite(const url::Origin& origin)
    : SchemefulSite(ObtainASite(origin)) {}

SchemefulSite::SchemefulSite(const GURL& url)
    : SchemefulSite(url::Origin::Create(url)) {}

SchemefulSite::SchemefulSite(const SchemefulSite& other) = default;
SchemefulSite::SchemefulSite(SchemefulSite&& other) noexcept = default;

SchemefulSite& SchemefulSite::operator=(const SchemefulSite& other) = default;
SchemefulSite& SchemefulSite::operator=(SchemefulSite&& other) noexcept =
    default;

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

absl::optional<SchemefulSite> SchemefulSite::CreateIfHasRegisterableDomain(
    const url::Origin& origin) {
  ObtainASiteResult result = ObtainASite(origin);
  if (!result.used_registerable_domain)
    return absl::nullopt;
  return SchemefulSite(std::move(result));
}

void SchemefulSite::ConvertWebSocketToHttp() {
  if (site_as_origin_.scheme() == url::kWsScheme ||
      site_as_origin_.scheme() == url::kWssScheme) {
    site_as_origin_ = url::Origin::Create(
        ChangeWebSocketSchemeToHttpScheme(site_as_origin_.GetURL()));
  }
}

// static
SchemefulSite SchemefulSite::Deserialize(const std::string& value) {
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

bool SchemefulSite::operator==(const SchemefulSite& other) const {
  return site_as_origin_ == other.site_as_origin_;
}

bool SchemefulSite::operator!=(const SchemefulSite& other) const {
  return !(*this == other);
}

// Allows SchemefulSite to be used as a key in STL containers (for example, a
// std::set or std::map).
bool SchemefulSite::operator<(const SchemefulSite& other) const {
  return site_as_origin_ < other.site_as_origin_;
}

// static
absl::optional<SchemefulSite> SchemefulSite::DeserializeWithNonce(
    const std::string& value) {
  absl::optional<url::Origin> result = url::Origin::Deserialize(value);
  if (!result)
    return absl::nullopt;
  return SchemefulSite(result.value());
}

absl::optional<std::string> SchemefulSite::SerializeWithNonce() {
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
