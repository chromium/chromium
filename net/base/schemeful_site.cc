// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/schemeful_site.h"

#include "base/check.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace net {

namespace {

// Return a new origin using the registerable domain of `origin` if possible and
// a port of 0. Otherwise, returns the passed in origin. Follows steps specified
// in https://html.spec.whatwg.org/multipage/origin.html#obtain-a-site
url::Origin SwitchToRegistrableDomainAndRemovePort(const url::Origin& origin) {
  // There is currently no reason for getting the schemeful site of a web
  // socket, so disallow passing in websocket origins.
  DCHECK_NE(origin.scheme(), url::kWsScheme);
  DCHECK_NE(origin.scheme(), url::kWssScheme);

  // 1. If origin is an opaque origin, then return origin.
  if (origin.opaque())
    return origin;

  std::string registerable_domain;

  // Non-normative step.
  // We only lookup the registerable domain for HTTP/HTTPS schemes, this is
  // non-normative. Other schemes for non-opaque origins like "file" do not
  // meaningfully have a registerable domain for their host, so they are
  // skipped.
  if (origin.scheme() == url::kHttpsScheme ||
      origin.scheme() == url::kHttpScheme) {
    registerable_domain = GetDomainAndRegistry(
        origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  }

  // If origin's host's registrable domain is null, then return (origin's
  // scheme, origin's host).
  //
  // `GetDomainAndRegistry()` returns an empty string for IP literals and
  // effective TLDs.
  if (registerable_domain.empty())
    registerable_domain = origin.host();

  DCHECK(!registerable_domain.empty());

  int port = url::DefaultPortForScheme(origin.scheme().c_str(),
                                       origin.scheme().length());

  // Provide a default port of 0 for non-standard schemes.
  if (port == url::PORT_UNSPECIFIED)
    port = 0;

  // We tack on a port of 0, as a port is not included in the result of running
  // the above algorithm.
  return url::Origin::CreateFromNormalizedTuple(origin.scheme(),
                                                registerable_domain, port);
}

}  // namespace

SchemefulSite::SchemefulSite(const url::Origin& origin)
    : origin_(SwitchToRegistrableDomainAndRemovePort(origin)) {}

SchemefulSite::SchemefulSite(const GURL& url)
    : origin_(
          SwitchToRegistrableDomainAndRemovePort(url::Origin::Create(url))) {}

SchemefulSite::SchemefulSite(const SchemefulSite& other) = default;
SchemefulSite::SchemefulSite(SchemefulSite&& other) = default;

SchemefulSite& SchemefulSite::operator=(const SchemefulSite& other) = default;
SchemefulSite& SchemefulSite::operator=(SchemefulSite&& other) = default;

// static
SchemefulSite SchemefulSite::Deserialize(const std::string& value) {
  return SchemefulSite(GURL(value));
}

std::string SchemefulSite::Serialize() const {
  return origin_.Serialize();
}

std::string SchemefulSite::GetDebugString() const {
  return origin_.GetDebugString();
}

const url::Origin& SchemefulSite::GetInternalOriginForTesting() const {
  return origin_;
}

bool SchemefulSite::operator==(const SchemefulSite& other) const {
  return origin_ == other.origin_;
}

bool SchemefulSite::operator!=(const SchemefulSite& other) const {
  return !(*this == other);
}

// Allows SchemefulSite to be used as a key in STL containers (for example, a
// std::set or std::map).
bool SchemefulSite::operator<(const SchemefulSite& other) const {
  return origin_ < other.origin_;
}

// static
base::Optional<SchemefulSite> SchemefulSite::DeserializeWithNonce(
    const std::string& value) {
  base::Optional<url::Origin> result = url::Origin::Deserialize(value);
  if (!result)
    return base::nullopt;
  return SchemefulSite(result.value());
}

base::Optional<std::string> SchemefulSite::SerializeWithNonce() {
  return origin_.SerializeWithNonceAndInitIfNeeded();
}

}  // namespace net
