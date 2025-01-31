// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"

#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "url/origin.h"

namespace blink {

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration() =
    default;

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration(
    mojom::PermissionsPolicyFeature feature)
    : feature(feature) {}

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration(
    mojom::PermissionsPolicyFeature feature,
    const std::vector<blink::OriginWithPossibleWildcards>& allowed_origins,
    const std::optional<url::Origin>& self_if_matches,
    bool matches_all_origins,
    bool matches_opaque_src)
    : feature(feature),
      allowed_origins(std::move(allowed_origins)),
      self_if_matches(std::move(self_if_matches)),
      matches_all_origins(matches_all_origins),
      matches_opaque_src(matches_opaque_src) {}

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration(
    const ParsedPermissionsPolicyDeclaration& rhs) = default;

ParsedPermissionsPolicyDeclaration&
ParsedPermissionsPolicyDeclaration::operator=(
    const ParsedPermissionsPolicyDeclaration& rhs) = default;

bool ParsedPermissionsPolicyDeclaration::Contains(
    const url::Origin& origin) const {
  if (matches_all_origins || (matches_opaque_src && origin.opaque())) {
    return true;
  }
  if (origin == self_if_matches) {
    return true;
  }
  for (const auto& origin_with_possible_wildcards : allowed_origins) {
    if (origin_with_possible_wildcards.DoesMatchOrigin(origin)) {
      return true;
    }
  }
  return false;
}

ParsedPermissionsPolicyDeclaration::~ParsedPermissionsPolicyDeclaration() =
    default;

bool operator==(const ParsedPermissionsPolicyDeclaration& lhs,
                const ParsedPermissionsPolicyDeclaration& rhs) {
  return std::tie(lhs.feature, lhs.matches_all_origins, lhs.matches_opaque_src,
                  lhs.allowed_origins) ==
         std::tie(rhs.feature, rhs.matches_all_origins, rhs.matches_opaque_src,
                  rhs.allowed_origins);
}

}  // namespace blink
