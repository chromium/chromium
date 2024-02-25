// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_DECLARATION_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_DECLARATION_H_

#include <vector>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "url/origin.h"

namespace blink {

// This struct holds permissions policy allowlist data that needs to be
// replicated between a RenderFrame and any of its associated
// RenderFrameProxies. A list of these form a ParsedPermissionsPolicy. NOTE:
// These types are used for replication frame state between processes.
struct BLINK_COMMON_EXPORT ParsedPermissionsPolicyDeclaration {
  ParsedPermissionsPolicyDeclaration();
  explicit ParsedPermissionsPolicyDeclaration(
      mojom::PermissionsPolicyFeature feature);
  ParsedPermissionsPolicyDeclaration(
      mojom::PermissionsPolicyFeature feature,
      const std::vector<OriginWithPossibleWildcards>& allowed_origins,
      const std::optional<url::Origin>& self_if_matches,
      bool matches_all_origins,
      bool matches_opaque_src);
  ParsedPermissionsPolicyDeclaration(
      const ParsedPermissionsPolicyDeclaration& rhs);
  ParsedPermissionsPolicyDeclaration& operator=(
      const ParsedPermissionsPolicyDeclaration& rhs);
  ~ParsedPermissionsPolicyDeclaration();

  // Prefer querying a PermissionsPolicy::Allowlist directly if possible. This
  // method is provided for cases when either an Allowlist is not available or
  // creating one is not desirable, e.g. when looking specifically at the
  // contents of an allow attribute or header value, rather than the active
  // policy on a document
  bool Contains(const url::Origin& origin) const;

  mojom::PermissionsPolicyFeature feature;

  // An list of all the origins/wildcards allowed (none can be opaque).
  std::vector<OriginWithPossibleWildcards> allowed_origins;
  // An origin that matches self if 'self' is in the allowlist.
  std::optional<url::Origin> self_if_matches;
  // Fallback value is used when feature is enabled for all or disabled for all.
  bool matches_all_origins{false};
  // This flag is set true for a declared policy on an <iframe sandbox>
  // container, for a feature which is supposed to be allowed in the sandboxed
  // document. Usually, the 'src' keyword in a declaration will cause the origin
  // of the iframe to be present in |origins|, but for sandboxed iframes, this
  // flag is set instead.
  bool matches_opaque_src{false};

  std::optional<std::string> reporting_endpoint;

  // Indicates that the parsed policy is deprecated.
  // The feature specified here will be tracked via
  // Deprecation::CountDeprecation once the document is installed.
  std::optional<mojom::WebFeature> deprecated_feature;
};

using ParsedPermissionsPolicy = std::vector<ParsedPermissionsPolicyDeclaration>;

bool BLINK_COMMON_EXPORT
operator==(const ParsedPermissionsPolicyDeclaration& lhs,
           const ParsedPermissionsPolicyDeclaration& rhs);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_DECLARATION_H_
