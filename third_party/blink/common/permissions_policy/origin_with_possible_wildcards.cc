// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "base/feature_list.h"
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
    const auto& tested_host = match_origin.host();
    const auto& policy_host = origin.host();
    // The tested host must be at least 2 char longer than the policy host
    // to be a subdomain of it.
    if (tested_host.length() < (policy_host.length() + 2)) {
      return false;
    }
    // The tested host must end with the policy host.
    if (!base::EndsWith(tested_host, policy_host)) {
      return false;
    }
    // The tested host without the policy host must end with a ".".
    if (tested_host[tested_host.length() - policy_host.length() - 1] != '.') {
      return false;
    }
    // If anything but the host doesn't match they can't match.
    if (origin != url::Origin::CreateFromNormalizedTuple(match_origin.scheme(),
                                                         policy_host,
                                                         match_origin.port())) {
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
