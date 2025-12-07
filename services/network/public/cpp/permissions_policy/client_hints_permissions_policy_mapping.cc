// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/client_hints_permissions_policy_mapping.h"

#include "base/no_destructor.h"
#include "services/network/public/cpp/client_hints.h"

namespace network {

ClientHintToPolicyFeatureMap MakeClientHintToPolicyFeatureMap() {
  return {
      {network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
       network::mojom::PermissionsPolicyFeature::kClientHintDeviceMemory},
      {network::mojom::WebClientHintsType::kDpr_DEPRECATED,
       network::mojom::PermissionsPolicyFeature::kClientHintDPR},
      {network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED,
       network::mojom::PermissionsPolicyFeature::kClientHintWidth},
      {network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
       network::mojom::PermissionsPolicyFeature::kClientHintViewportWidth},
      {network::mojom::WebClientHintsType::kRtt_DEPRECATED,
       network::mojom::PermissionsPolicyFeature::kClientHintRTT},
      {network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
       network::mojom::PermissionsPolicyFeature::kClientHintDownlink},
      {network::mojom::WebClientHintsType::kEct_DEPRECATED,
       network::mojom::PermissionsPolicyFeature::kClientHintECT},
      {network::mojom::WebClientHintsType::kUA,
       network::mojom::PermissionsPolicyFeature::kClientHintUA},
      {network::mojom::WebClientHintsType::kUAArch,
       network::mojom::PermissionsPolicyFeature::kClientHintUAArch},
      {network::mojom::WebClientHintsType::kUAPlatform,
       network::mojom::PermissionsPolicyFeature::kClientHintUAPlatform},
      {network::mojom::WebClientHintsType::kUAModel,
       network::mojom::PermissionsPolicyFeature::kClientHintUAModel},
      {network::mojom::WebClientHintsType::kUAMobile,
       network::mojom::PermissionsPolicyFeature::kClientHintUAMobile},
      {network::mojom::WebClientHintsType::kUAFullVersion,
       network::mojom::PermissionsPolicyFeature::kClientHintUAFullVersion},
      {network::mojom::WebClientHintsType::kUAPlatformVersion,
       network::mojom::PermissionsPolicyFeature::kClientHintUAPlatformVersion},
      {network::mojom::WebClientHintsType::kPrefersColorScheme,
       network::mojom::PermissionsPolicyFeature::kClientHintPrefersColorScheme},
      {network::mojom::WebClientHintsType::kUABitness,
       network::mojom::PermissionsPolicyFeature::kClientHintUABitness},
      {network::mojom::WebClientHintsType::kViewportHeight,
       network::mojom::PermissionsPolicyFeature::kClientHintViewportHeight},
      {network::mojom::WebClientHintsType::kDeviceMemory,
       network::mojom::PermissionsPolicyFeature::kClientHintDeviceMemory},
      {network::mojom::WebClientHintsType::kDpr,
       network::mojom::PermissionsPolicyFeature::kClientHintDPR},
      {network::mojom::WebClientHintsType::kResourceWidth,
       network::mojom::PermissionsPolicyFeature::kClientHintWidth},
      {network::mojom::WebClientHintsType::kViewportWidth,
       network::mojom::PermissionsPolicyFeature::kClientHintViewportWidth},
      {network::mojom::WebClientHintsType::kUAFullVersionList,
       network::mojom::PermissionsPolicyFeature::kClientHintUAFullVersionList},
      {network::mojom::WebClientHintsType::kUAWoW64,
       network::mojom::PermissionsPolicyFeature::kClientHintUAWoW64},
      {network::mojom::WebClientHintsType::kSaveData,
       network::mojom::PermissionsPolicyFeature::kClientHintSaveData},
      {network::mojom::WebClientHintsType::kPrefersReducedMotion,
       network::mojom::PermissionsPolicyFeature::
           kClientHintPrefersReducedMotion},
      {network::mojom::WebClientHintsType::kUAFormFactors,
       network::mojom::PermissionsPolicyFeature::kClientHintUAFormFactors},
      {network::mojom::WebClientHintsType::kPrefersReducedTransparency,
       network::mojom::PermissionsPolicyFeature::
           kClientHintPrefersReducedTransparency},
  };
}

const ClientHintToPolicyFeatureMap& GetClientHintToPolicyFeatureMap() {
  DCHECK_EQ(network::GetClientHintToNameMap().size(),
            MakeClientHintToPolicyFeatureMap().size());
  static const base::NoDestructor<ClientHintToPolicyFeatureMap> map(
      MakeClientHintToPolicyFeatureMap());
  return *map;
}

PolicyFeatureToClientHintMap MakePolicyFeatureToClientHintMap() {
  PolicyFeatureToClientHintMap map;
  for (const auto& pair : GetClientHintToPolicyFeatureMap()) {
    if (map.contains(pair.second)) {
      map[pair.second].insert(pair.first);
    } else {
      map[pair.second] = {pair.first};
    }
  }
  return map;
}

const PolicyFeatureToClientHintMap& GetPolicyFeatureToClientHintMap() {
  static const base::NoDestructor<PolicyFeatureToClientHintMap> map(
      MakePolicyFeatureToClientHintMap());
  return *map;
}

}  // namespace network
