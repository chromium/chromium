// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/site_for_cookies.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_util.h"

namespace net {

namespace {

std::string RegistrableDomainOrHost(const std::string& host) {
  std::string domain = registry_controlled_domains::GetDomainAndRegistry(
      host, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? host : domain;
}

}  // namespace

SiteForCookies::SiteForCookies() : schemefully_same_(false) {}

SiteForCookies::SiteForCookies(const SiteForCookies& other) = default;
SiteForCookies::SiteForCookies(SiteForCookies&& other) = default;

SiteForCookies::~SiteForCookies() = default;

SiteForCookies& SiteForCookies::operator=(const SiteForCookies& other) =
    default;
SiteForCookies& SiteForCookies::operator=(SiteForCookies&& site_for_cookies) =
    default;

// static
bool SiteForCookies::FromWire(const std::string& scheme,
                              const std::string& registrable_domain,
                              bool schemefully_same,
                              SiteForCookies* out) {
  // Make sure scheme meets precondition of methods like
  // GURL::SchemeIsCryptographic.
  if (!base::IsStringASCII(scheme) || base::ToLowerASCII(scheme) != scheme)
    return false;

  // registrable_domain_ should also be canonicalized.
  SiteForCookies candidate(scheme, registrable_domain);
  if (registrable_domain != candidate.registrable_domain_)
    return false;

  candidate.schemefully_same_ = schemefully_same;

  *out = std::move(candidate);
  return true;
}

// static
SiteForCookies SiteForCookies::FromOrigin(const url::Origin& origin) {
  // Opaque origins are not first-party to anything.
  if (origin.opaque())
    return SiteForCookies();

  return SiteForCookies(origin.scheme(), origin.host());
}

// static
SiteForCookies SiteForCookies::FromUrl(const GURL& url) {
  return SiteForCookies::FromOrigin(url::Origin::Create(url));
}

std::string SiteForCookies::ToDebugString() const {
  std::string same_scheme_string = schemefully_same_ ? "true" : "false";
  return base::StrCat({"SiteForCookies: {scheme=", scheme_,
                       "; registrable_domain=", registrable_domain_,
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
  if (IsNull())
    return other.IsNull();

  if (cookie_util::IsSchemefulSameSiteEnabled() &&
      !CompatibleScheme(other.scheme())) {
    return false;
  }

  if (registrable_domain_.empty())
    return other.registrable_domain_.empty() && (scheme_ == other.scheme_);

  return registrable_domain_ == other.registrable_domain_;
}

void SiteForCookies::MarkIfCrossScheme(const url::Origin& other) {
  // If |this| is IsNull() then |this| doesn't match anything which means that
  // the scheme check is pointless. Also exit early if schemefully_same_ is
  // already false.
  if (IsNull() || !schemefully_same_)
    return;

  // Mark if |other| is opaque. Opaque origins shouldn't match.
  if (other.opaque()) {
    schemefully_same_ = false;
    return;
  }

  if (CompatibleScheme(other.scheme()))
    return;

  // The two are cross-scheme to each other.
  schemefully_same_ = false;
}

GURL SiteForCookies::RepresentativeUrl() const {
  if (IsNull())
    return GURL();
  GURL result(base::StrCat({scheme_, "://", registrable_domain_, "/"}));
  DCHECK(result.is_valid());
  return result;
}

bool SiteForCookies::IsNull() const {
  if (cookie_util::IsSchemefulSameSiteEnabled())
    return scheme_.empty() || !schemefully_same_;

  return scheme_.empty();
}

SiteForCookies::SiteForCookies(const net::SchemefulSite& schemeful_site)
    : scheme_(schemeful_site.site_as_origin_.scheme()),
      registrable_domain_(schemeful_site.site_as_origin_.host()),
      schemefully_same_(!scheme_.empty()) {}

SiteForCookies::SiteForCookies(const std::string& scheme,
                               const std::string& host)
    : scheme_(scheme),
      registrable_domain_(RegistrableDomainOrHost(host)),
      schemefully_same_(!scheme.empty()) {}

bool SiteForCookies::CompatibleScheme(const std::string& other_scheme) const {
  DCHECK(base::IsStringASCII(other_scheme));
  DCHECK(base::ToLowerASCII(other_scheme) == other_scheme);

  // Exact match case.
  if (scheme_ == other_scheme)
    return true;

  // ["https", "wss"] case.
  if ((scheme_ == url::kHttpsScheme || scheme_ == url::kWssScheme) &&
      (other_scheme == url::kHttpsScheme || other_scheme == url::kWssScheme)) {
    return true;
  }

  // ["http", "ws"] case.
  if ((scheme_ == url::kHttpScheme || scheme_ == url::kWsScheme) &&
      (other_scheme == url::kHttpScheme || other_scheme == url::kWsScheme)) {
    return true;
  }

  return false;
}

bool SiteForCookies::IsSchemefullyFirstParty(const GURL& url) const {
  // Can't use IsNull() as we want the same behavior regardless of
  // SchemefulSameSite feature status.
  if (scheme_.empty() || !schemefully_same_ || !url.is_valid())
    return false;

  return CompatibleScheme(url.scheme()) && IsSchemelesslyFirstParty(url);
}

bool SiteForCookies::IsSchemelesslyFirstParty(const GURL& url) const {
  // Can't use IsNull() as we want the same behavior regardless of
  // SchemefulSameSite feature status.
  if (scheme_.empty() || !url.is_valid())
    return false;

  std::string other_registrable_domain = RegistrableDomainOrHost(url.host());

  if (registrable_domain_.empty())
    return other_registrable_domain.empty() && (scheme_ == url.scheme());

  return registrable_domain_ == other_registrable_domain;
}

}  // namespace net
