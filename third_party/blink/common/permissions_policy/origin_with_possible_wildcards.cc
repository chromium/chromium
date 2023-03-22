// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/cors/origin_access_entry.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

OriginWithPossibleWildcards::OriginWithPossibleWildcards() = default;

OriginWithPossibleWildcards::OriginWithPossibleWildcards(
    const url::Origin& origin,
    bool has_subdomain_wildcard)
    : origin(origin), has_subdomain_wildcard(has_subdomain_wildcard) {
  // Origins cannot be opaque.
  DCHECK(!origin.opaque());
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
  DCHECK(!origin.opaque());
  auto wildcard_pos = std::string::npos;
  auto serialized_origin = origin.Serialize();
  if (has_subdomain_wildcard &&
      (wildcard_pos = serialized_origin.find("://")) != std::string::npos) {
    // Restore the missing wildcard (`*.`) to the front of the host so this
    // permissions policy element is inspectable. Before subdomain wildcard
    // support this would have been parsed into a `%2A.`.
    serialized_origin.insert(wildcard_pos + 3, "*.");
  }
  return serialized_origin;
}

bool OriginWithPossibleWildcards::DoesMatchOrigin(
    const url::Origin& match_origin) const {
  DCHECK(!origin.opaque());
  if (has_subdomain_wildcard) {
    // This function won't match https://*.foo.com with https://foo.com.
    if (origin == match_origin) {
      return false;
    }
    // Scheme and port must match.
    if (match_origin.scheme() != origin.scheme() ||
        match_origin.port() != origin.port()) {
      return false;
    }
    // The tested host must be a subdomain of the policy host.
    if (!network::cors::IsSubdomainOfHost(match_origin.host(), origin.host())) {
      return false;
    }
    return true;
  } else {
    // If there is no wildcard test normal match.
    return origin == match_origin;
  }
}

bool operator==(const OriginWithPossibleWildcards& lhs,
                const OriginWithPossibleWildcards& rhs) {
  return std::tie(lhs.origin, lhs.has_subdomain_wildcard) ==
         std::tie(rhs.origin, rhs.has_subdomain_wildcard);
}

bool operator!=(const OriginWithPossibleWildcards& lhs,
                const OriginWithPossibleWildcards& rhs) {
  return std::tie(lhs.origin, lhs.has_subdomain_wildcard) !=
         std::tie(rhs.origin, rhs.has_subdomain_wildcard);
}

bool operator<(const OriginWithPossibleWildcards& lhs,
               const OriginWithPossibleWildcards& rhs) {
  return std::tie(lhs.origin, lhs.has_subdomain_wildcard) <
         std::tie(rhs.origin, rhs.has_subdomain_wildcard);
}

}  // namespace blink
