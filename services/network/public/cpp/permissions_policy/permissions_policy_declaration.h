// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_DECLARATION_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_DECLARATION_H_

#include <vector>

#include "base/component_export.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "url/origin.h"

namespace network {

// This struct holds permissions policy allowlist data that needs to be
// replicated between a RenderFrame and any of its associated
// RenderFrameProxies. A list of these form a ParsedPermissionsPolicy. NOTE:
// These types are used for replication frame state between processes.
struct COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM)
    ParsedPermissionsPolicyDeclaration final {
  ParsedPermissionsPolicyDeclaration();
  explicit ParsedPermissionsPolicyDeclaration(
      network::mojom::PermissionsPolicyFeature feature);
  ParsedPermissionsPolicyDeclaration(
      network::mojom::PermissionsPolicyFeature feature,
      const std::vector<network::OriginWithPossibleWildcards>& allowed_origins,
      const std::optional<url::Origin>& self_if_matches,
      bool matches_all_origins,
      bool matches_opaque_src);
  ParsedPermissionsPolicyDeclaration(
      const ParsedPermissionsPolicyDeclaration& rhs);
  ParsedPermissionsPolicyDeclaration& operator=(
      const ParsedPermissionsPolicyDeclaration& rhs);
  ParsedPermissionsPolicyDeclaration(
      ParsedPermissionsPolicyDeclaration&&) noexcept;
  ParsedPermissionsPolicyDeclaration& operator=(
      ParsedPermissionsPolicyDeclaration&&) noexcept;
  ~ParsedPermissionsPolicyDeclaration();

  // Prefer querying a PermissionsPolicy::Allowlist directly if possible. This
  // method is provided for cases when either an Allowlist is not available or
  // creating one is not desirable, e.g. when looking specifically at the
  // contents of an allow attribute or header value, rather than the active
  // policy on a document
  bool Contains(const url::Origin& origin) const;

  network::mojom::PermissionsPolicyFeature feature;

  // An list of all the origins/wildcards allowed (none can be opaque).
  std::vector<network::OriginWithPossibleWildcards> allowed_origins;
  // An origin that matches self if 'self' is in the allowlist.
  std::optional<url::Origin> self_if_matches;
  // Fallback value is used when feature is enabled for all or disabled for all.
  bool matches_all_origins = false;
  // This flag is set true for a declared policy on an <iframe sandbox>
  // container, for a feature which is supposed to be allowed in the sandboxed
  // document. Usually, the 'src' keyword in a declaration will cause the origin
  // of the iframe to be present in |origins|, but for sandboxed iframes, this
  // flag is set instead.
  bool matches_opaque_src = false;

  std::optional<std::string> reporting_endpoint;

  friend bool operator==(const ParsedPermissionsPolicyDeclaration& lhs,
                         const ParsedPermissionsPolicyDeclaration& rhs) =
      default;
};

using ParsedPermissionsPolicy = std::vector<ParsedPermissionsPolicyDeclaration>;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_DECLARATION_H_
