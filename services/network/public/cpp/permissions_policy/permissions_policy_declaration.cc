// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"

#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "url/origin.h"

namespace network {

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration() =
    default;

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration(
    network::mojom::PermissionsPolicyFeature feature)
    : feature(feature) {}

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration(
    network::mojom::PermissionsPolicyFeature feature,
    const std::vector<network::OriginWithPossibleWildcards>& allowed_origins,
    const std::optional<url::Origin>& self_if_matches,
    bool matches_all_origins,
    bool matches_opaque_src)
    : feature(feature),
      allowed_origins(allowed_origins),
      self_if_matches(self_if_matches),
      matches_all_origins(matches_all_origins),
      matches_opaque_src(matches_opaque_src) {}

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration(
    const ParsedPermissionsPolicyDeclaration& rhs) = default;

ParsedPermissionsPolicyDeclaration&
ParsedPermissionsPolicyDeclaration::operator=(
    const ParsedPermissionsPolicyDeclaration& rhs) = default;

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration(
    ParsedPermissionsPolicyDeclaration&&) noexcept = default;

ParsedPermissionsPolicyDeclaration&
ParsedPermissionsPolicyDeclaration::operator=(
    ParsedPermissionsPolicyDeclaration&&) noexcept = default;

bool ParsedPermissionsPolicyDeclaration::Contains(
    const url::Origin& origin) const {
  if (matches_all_origins || (matches_opaque_src && origin.opaque())) {
    return true;
  }
  if (origin == self_if_matches) {
    return true;
  }
  return std::ranges::any_of(
      allowed_origins, [&origin](const auto& origin_with_possible_wildcards) {
        return origin_with_possible_wildcards.DoesMatchOrigin(origin);
      });
}

ParsedPermissionsPolicyDeclaration::~ParsedPermissionsPolicyDeclaration() =
    default;

}  // namespace network
