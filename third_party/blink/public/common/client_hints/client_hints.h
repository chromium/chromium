// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CLIENT_HINTS_CLIENT_HINTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CLIENT_HINTS_CLIENT_HINTS_H_

#include <stddef.h>

#include <optional>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

class GURL;

namespace blink {

class PermissionsPolicy;

using ClientHintToPolicyFeatureMap =
    base::flat_map<network::mojom::WebClientHintsType,
                   mojom::PermissionsPolicyFeature>;

using PolicyFeatureToClientHintMap =
    base::flat_map<mojom::PermissionsPolicyFeature,
                   std::set<network::mojom::WebClientHintsType>>;

// Mapping from WebClientHintsType to the corresponding Permissions-Policy (e.g.
// kDpr => kClientHintsDPR). The order matches the header mapping and the enum
// order in services/network/public/mojom/web_client_hints_types.mojom
BLINK_COMMON_EXPORT const ClientHintToPolicyFeatureMap&
GetClientHintToPolicyFeatureMap();

// Mapping from Permissions-Policy to the corresponding WebClientHintsType(s)
// (e.g. kClientHintsDPR => {kDpr, kDpr_DEPRECATED}).
BLINK_COMMON_EXPORT const PolicyFeatureToClientHintMap&
GetPolicyFeatureToClientHintMap();

// Indicates that a hint is sent by default, regardless of an opt-in.
BLINK_COMMON_EXPORT
bool IsClientHintSentByDefault(network::mojom::WebClientHintsType type);

// Add a list of Client Hints headers to be removed to the output vector, based
// on Permissions Policy and the url's origin.
BLINK_COMMON_EXPORT void FindClientHintsToRemove(
    const PermissionsPolicy* permissions_policy,
    const GURL& url,
    std::vector<std::string>* removed_headers);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CLIENT_HINTS_CLIENT_HINTS_H_
