// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "url/origin.h"

namespace blink {

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

bool IsClientHintSentByDefault(network::mojom::WebClientHintsType type) {
  switch (type) {
    case network::mojom::WebClientHintsType::kSaveData:
    case network::mojom::WebClientHintsType::kUA:
    case network::mojom::WebClientHintsType::kUAMobile:
    case network::mojom::WebClientHintsType::kUAPlatform:
      return true;
    default:
      return false;
  }
}

// Add a list of Client Hints headers to be removed to the output vector, based
// on PermissionsPolicy and the url's origin.
void FindClientHintsToRemove(const PermissionsPolicy* permissions_policy,
                             const GURL& url,
                             std::vector<std::string>* removed_headers) {
  DCHECK(removed_headers);
  url::Origin origin = url::Origin::Create(url);
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& type = elem.first;
    const auto& header = elem.second;
    // Remove the hint if any is true:
    // * Permissions policy is null (we're in a sync XHR case) and the hint is
    // not sent by default.
    // * Permissions policy exists and doesn't allow for the hint.
    if ((!permissions_policy && !IsClientHintSentByDefault(type)) ||
        (permissions_policy &&
         !permissions_policy->IsFeatureEnabledForOrigin(
             blink::GetClientHintToPolicyFeatureMap().at(type), origin))) {
      removed_headers->push_back(header);
    }
  }
}

}  // namespace blink
