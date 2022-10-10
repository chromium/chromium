// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "base/feature_list.h"
#include "services/network/public/cpp/cors/origin_access_entry.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace blink {

OriginWithPossibleWildcards::OriginWithPossibleWildcards() = default;

OriginWithPossibleWildcards::OriginWithPossibleWildcards(
    const url::Origin& origin,
    bool has_subdomain_wildcard)
    : origin(origin), has_subdomain_wildcard(has_subdomain_wildcard) {
  // Origins that do have wildcards cannot be opaque.
  DCHECK(!origin.opaque() || !has_subdomain_wildcard);
}

OriginWithPossibleWildcards::OriginWithPossibleWildcards(
    const OriginWithPossibleWildcards& rhs) = default;

OriginWithPossibleWildcards& OriginWithPossibleWildcards::operator=(
    const OriginWithPossibleWildcards& rhs) = default;

OriginWithPossibleWildcards::~OriginWithPossibleWildcards() = default;

bool OriginWithPossibleWildcards::DoesMatchOrigin(
    const url::Origin& match_origin) const {
  // TODO(crbug.com/1345994): Merge logic with IsSubdomainOfHost where possible.
  if (has_subdomain_wildcard) {
    // Only try to match at all if wildcard matching is enabled.
    if (!base::FeatureList::IsEnabled(
            features::kWildcardSubdomainsInPermissionsPolicy)) {
      return false;
    }
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
