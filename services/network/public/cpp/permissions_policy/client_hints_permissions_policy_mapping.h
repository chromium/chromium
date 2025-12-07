// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_CLIENT_HINTS_PERMISSIONS_POLICY_MAPPING_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_CLIENT_HINTS_PERMISSIONS_POLICY_MAPPING_H_

#include <set>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"

namespace network {

using ClientHintToPolicyFeatureMap =
    base::flat_map<network::mojom::WebClientHintsType,
                   network::mojom::PermissionsPolicyFeature>;

using PolicyFeatureToClientHintMap =
    base::flat_map<network::mojom::PermissionsPolicyFeature,
                   std::set<network::mojom::WebClientHintsType>>;

// Mapping from WebClientHintsType to the corresponding Permissions-Policy (e.g.
// kDpr => kClientHintsDPR). The order matches the header mapping and the enum
// order in services/network/public/mojom/web_client_hints_types.mojom
COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM)
const ClientHintToPolicyFeatureMap& GetClientHintToPolicyFeatureMap();

// Mapping from Permissions-Policy to the corresponding WebClientHintsType(s)
// (e.g. kClientHintsDPR => {kDpr, kDpr_DEPRECATED}).
COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM)
const PolicyFeatureToClientHintMap& GetPolicyFeatureToClientHintMap();

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_CLIENT_HINTS_PERMISSIONS_POLICY_MAPPING_H_
