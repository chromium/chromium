// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "base/feature_list.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_source.h"
#include "services/network/public/cpp/cors/origin_access_entry.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

OriginWithPossibleWildcards::OriginWithPossibleWildcards() = default;

OriginWithPossibleWildcards::OriginWithPossibleWildcards(
    const OriginWithPossibleWildcards& rhs) = default;

OriginWithPossibleWildcards& OriginWithPossibleWildcards::operator=(
    const OriginWithPossibleWildcards& rhs) = default;

OriginWithPossibleWildcards::~OriginWithPossibleWildcards() = default;

// static
absl::optional<OriginWithPossibleWildcards>
OriginWithPossibleWildcards::FromOrigin(const url::Origin& origin) {
  // Origins cannot be opaque.
  if (origin.opaque()) {
    return absl::nullopt;
  }
  return Parse(origin.Serialize(), NodeType::kHeader);
}

// static
absl::optional<OriginWithPossibleWildcards>
OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
    const url::Origin& origin,
    bool has_subdomain_wildcard) {
  absl::optional<OriginWithPossibleWildcards> origin_with_possible_wildcards =
      FromOrigin(origin);
  if (origin_with_possible_wildcards.has_value()) {
    // Overwrite wildcard settings.
    origin_with_possible_wildcards->csp_source.is_host_wildcard =
        has_subdomain_wildcard;
  }
  return origin_with_possible_wildcards;
}

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

  // The CSPSource must have a scheme.
  if (origin_with_possible_wildcards.csp_source.scheme.empty()) {
    return absl::nullopt;
  }

  // Attribute policies must not have wildcards in the port, host, or scheme.
  if (type == NodeType::kAttribute &&
      (origin_with_possible_wildcards.csp_source.host.empty() ||
       origin_with_possible_wildcards.csp_source.is_port_wildcard ||
       origin_with_possible_wildcards.csp_source.is_host_wildcard)) {
    return absl::nullopt;
  }

  // The CSPSource may have parsed a path but we should ignore it as permissions
  // policies are origin based, not URL based.
  origin_with_possible_wildcards.csp_source.path = "";

  // If expanded matching is supported we can conclude validation early.
  if (base::FeatureList::IsEnabled(
          blink::features::kCSPWildcardsInPermissionsPolicies)) {
    return origin_with_possible_wildcards;
  }

  // Since extended matching is disabled the CSPSource must have a host and must
  // not have a wildcard port.
  if (origin_with_possible_wildcards.csp_source.host.empty() ||
      origin_with_possible_wildcards.csp_source.is_port_wildcard) {
    return absl::nullopt;
  }

  // The remaining host (after the wildcard) must be at least an eTLD+1.
  if (origin_with_possible_wildcards.csp_source.is_host_wildcard &&
      !net::registry_controlled_domains::HostHasRegistryControlledDomain(
          origin_with_possible_wildcards.csp_source.host,
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return absl::nullopt;
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
