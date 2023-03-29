// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/content_security_policy/csp_source.h"
#include "services/network/public/cpp/cors/origin_access_entry.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

OriginWithPossibleWildcards::OriginWithPossibleWildcards() = default;

OriginWithPossibleWildcards::OriginWithPossibleWildcards(
    const url::Origin& origin,
    bool has_subdomain_wildcard) {
  // Origins cannot be opaque.
  DCHECK(!origin.opaque());
  csp_source.scheme = origin.scheme();
  csp_source.host = origin.host();
  csp_source.port = origin.port() ?: url::PORT_UNSPECIFIED;
  csp_source.is_host_wildcard = has_subdomain_wildcard;
}

OriginWithPossibleWildcards::OriginWithPossibleWildcards(
    const OriginWithPossibleWildcards& rhs) = default;

OriginWithPossibleWildcards& OriginWithPossibleWildcards::operator=(
    const OriginWithPossibleWildcards& rhs) = default;

OriginWithPossibleWildcards::~OriginWithPossibleWildcards() = default;

// static
absl::optional<OriginWithPossibleWildcards> OriginWithPossibleWildcards::Parse(
    const std::string& allowlist_entry,
    const NodeType type) {
  // TODO(crbug.com/1418009): Add a way for CSP parsing to support eTLD+1 check
  // and prevent upgrading %2A into * as permissions don't allow this.
  auto wildcard_pos = std::string::npos;
  // If there's a subdomain wildcard in the `allowlist_entry` of a permissions
  // policy, then we can parse it out and validate the origin. We know there's a
  // subdomain wildcard if there is a exactly one `*` and it's after the scheme
  // and before the rest of the host. Invalid origins return an instance of
  // OriginWithPossibleWildcards with an opaque origin member.
  if (type == NodeType::kHeader &&
      (wildcard_pos = allowlist_entry.find("://*.")) != std::string::npos &&
      allowlist_entry.find('*') == allowlist_entry.rfind('*')) {
    // We need a copy as erase modifies the original.
    auto allowlist_entry_copy(allowlist_entry);
    allowlist_entry_copy.erase(wildcard_pos + 3, 2);
    const auto parsed_origin = url::Origin::Create(GURL(allowlist_entry_copy));
    // The origin must not be opaque and its host must be registrable.
    if (parsed_origin.opaque()) {
      // We early return here assuming even with the `*.` the origin parses
      // opaque.
      return absl::nullopt;
    } else if (
        net::registry_controlled_domains::HostHasRegistryControlledDomain(
            parsed_origin.host(),
            net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      return OriginWithPossibleWildcards(parsed_origin,
                                         /*has_subdomain_wildcard=*/true);
    }
  }
  // Otherwise, parse the origin string and verify that the result is
  // valid. Invalid strings will produce an opaque origin.
  const auto parsed_origin = url::Origin::Create(GURL(allowlist_entry));
  if (parsed_origin.opaque()) {
    return absl::nullopt;
  } else {
    return OriginWithPossibleWildcards(parsed_origin,
                                       /*has_subdomain_wildcard=*/false);
  }
}

std::string OriginWithPossibleWildcards::Serialize() const {
  // TODO(crbug.com/1418009): Add way to prevent CSP serialization from
  // printing default ports (might be as simple as fixing parsing TODO above).
  network::mojom::CSPSource csp_source_for_serialization = csp_source;
  if (csp_source.port == 80 && (csp_source.scheme == url::kHttpScheme ||
                                csp_source.scheme == url::kWsScheme)) {
    csp_source_for_serialization.port = url::PORT_UNSPECIFIED;
  } else if (csp_source.port == 443 &&
             (csp_source.scheme == url::kHttpsScheme ||
              csp_source.scheme == url::kWssScheme)) {
    csp_source_for_serialization.port = url::PORT_UNSPECIFIED;
  }
  return network::ToString(csp_source_for_serialization);
}

bool OriginWithPossibleWildcards::DoesMatchOrigin(
    const url::Origin& match_origin) const {
  // TODO(crbug.com/1418009): Add way to prevent CSP matching from allowing
  // upgrades (http -> https) as permissions don't allow this.
  return network::CheckCSPSource(csp_source, match_origin.GetURL(),
                                 csp_source) &&
         csp_source.scheme == match_origin.scheme() &&
         csp_source.port == (match_origin.port() ?: url::PORT_UNSPECIFIED);
}

bool operator==(const OriginWithPossibleWildcards& lhs,
                const OriginWithPossibleWildcards& rhs) {
  return lhs.csp_source == rhs.csp_source;
}

bool operator!=(const OriginWithPossibleWildcards& lhs,
                const OriginWithPossibleWildcards& rhs) {
  return lhs.csp_source != rhs.csp_source;
}

bool operator<(const OriginWithPossibleWildcards& lhs,
               const OriginWithPossibleWildcards& rhs) {
  return lhs.csp_source < rhs.csp_source;
}

}  // namespace blink
