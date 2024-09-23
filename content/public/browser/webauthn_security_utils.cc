// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webauthn_security_utils.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/url_util.h"

namespace content {

blink::mojom::AuthenticatorStatus OriginAllowedToMakeWebAuthnRequests(
    url::Origin caller_origin) {
  if (caller_origin.opaque()) {
    return blink::mojom::AuthenticatorStatus::OPAQUE_DOMAIN;
  }

  // The scheme is required to be HTTP(S).  Given the
  // |network::IsUrlPotentiallyTrustworthy| check below, HTTP is effectively
  // restricted to just "localhost".
  if (caller_origin.scheme() != url::kHttpScheme &&
      caller_origin.scheme() != url::kHttpsScheme) {
    return blink::mojom::AuthenticatorStatus::INVALID_PROTOCOL;
  }

  // TODO(crbug.com/40161236): Use IsOriginPotentiallyTrustworthy?
  if (url::HostIsIPAddress(caller_origin.host()) ||
      !network::IsUrlPotentiallyTrustworthy(caller_origin.GetURL())) {
    return blink::mojom::AuthenticatorStatus::INVALID_DOMAIN;
  }

  return blink::mojom::AuthenticatorStatus::SUCCESS;
}

bool OriginIsAllowedToClaimRelyingPartyId(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin) {
  // `OriginAllowedToMakeWebAuthnRequests()` must have been called before.
  DCHECK_EQ(OriginAllowedToMakeWebAuthnRequests(caller_origin),
            blink::mojom::AuthenticatorStatus::SUCCESS);

  if (claimed_relying_party_id.empty()) {
    return false;
  }

  if (caller_origin.host() == claimed_relying_party_id) {
    return true;
  }

  if (!caller_origin.DomainIs(claimed_relying_party_id)) {
    return false;
  }

  if (!net::registry_controlled_domains::HostHasRegistryControlledDomain(
          caller_origin.host(),
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES) ||
      !net::registry_controlled_domains::HostHasRegistryControlledDomain(
          claimed_relying_party_id,
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // This prevents "https://login.awesomecompany" from claiming
    // "awesomecompany", which is allowed by the spec but disallowed by
    // chromium. It is a potential footgun if a company uses an internal label
    // that later gets added to the PSL.
    return false;
  }

  return true;
}

}  // namespace content
