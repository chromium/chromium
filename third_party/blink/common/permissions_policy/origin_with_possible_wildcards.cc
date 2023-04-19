// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
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
  // Prevent url::Origin from writing the default port into the CSPSource
  // as the normal parsing route doesn't do this.
  if (csp_source.port == 80 && (csp_source.scheme == url::kHttpScheme ||
                                csp_source.scheme == url::kWsScheme)) {
    csp_source.port = url::PORT_UNSPECIFIED;
  } else if (csp_source.port == 443 &&
             (csp_source.scheme == url::kHttpsScheme ||
              csp_source.scheme == url::kWssScheme)) {
    csp_source.port = url::PORT_UNSPECIFIED;
  }
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
  // First we use the csp parser to extract the CSPSource struct.
  OriginWithPossibleWildcards origin_with_possible_wildcards;
  std::vector<std::string> parsing_errors;
  bool success = network::ParseSource(
      network::mojom::CSPDirectiveName::Unknown, allowlist_entry,
      &origin_with_possible_wildcards.csp_source, parsing_errors);
  if (!success) {
    return absl::nullopt;
  }

  // The CSPSource must have a scheme/host and must not have a wildcard port.
  if (origin_with_possible_wildcards.csp_source.scheme.empty() ||
      origin_with_possible_wildcards.csp_source.host.empty() ||
      origin_with_possible_wildcards.csp_source.is_port_wildcard) {
    return absl::nullopt;
  }

  // The CSPSource may have parsed a path but we should ignore it as permissions
  // policies are origin based, not URL based.
  origin_with_possible_wildcards.csp_source.path = "";

  // Next we need to deal with the host wildcard.
  if (origin_with_possible_wildcards.csp_source.is_host_wildcard) {
    if (type == NodeType::kAttribute) {
      // iframe allow attribute policies don't permit wildcards.
      return absl::nullopt;
    } else if (
        !net::registry_controlled_domains::HostHasRegistryControlledDomain(
            origin_with_possible_wildcards.csp_source.host,
            net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      // The remaining host (after the wildcard) must be at least an eTLD+1.
      return absl::nullopt;
    }
  }

  // The CSPSource is valid so we can return it.
  return origin_with_possible_wildcards;
}

std::string OriginWithPossibleWildcards::Serialize() const {
  return network::ToString(csp_source);
}

bool OriginWithPossibleWildcards::DoesMatchOrigin(
    const url::Origin& match_origin) const {
  return network::CheckCSPSource(csp_source, match_origin.GetURL(), csp_source,
                                 network::CSPSourceContext::PermissionsPolicy);
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
