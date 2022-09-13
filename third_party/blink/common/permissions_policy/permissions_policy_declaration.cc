// Copyright 2022 The Chromium Authors. All rights reserved.
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
    const std::vector<url::Origin>& allowed_origins,
    bool matches_all_origins,
    bool matches_opaque_src)
    : feature(feature),
      allowed_origins(allowed_origins),
      matches_all_origins(matches_all_origins),
      matches_opaque_src(matches_opaque_src) {}

ParsedPermissionsPolicyDeclaration::ParsedPermissionsPolicyDeclaration(
    const ParsedPermissionsPolicyDeclaration& rhs) = default;

ParsedPermissionsPolicyDeclaration&
ParsedPermissionsPolicyDeclaration::operator=(
    const ParsedPermissionsPolicyDeclaration& rhs) = default;

bool ParsedPermissionsPolicyDeclaration::Contains(
    const url::Origin& origin) const {
  return matches_all_origins || (matches_opaque_src && origin.opaque()) ||
         base::Contains(allowed_origins, origin);
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
