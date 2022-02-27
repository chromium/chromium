// Copyright 2017 The Chromium Authors. All rights reserved.
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

const int kClientHintsNumberOfLegacyHints = 4;

ClientHintToPolicyFeatureMap MakeClientHintToPolicyFeatureMap() {
  return {
      // Legacy Hints that are sent cross-origin regardless of Permissions
      // Policy when kAllowClientHintsToThirdParty is enabled.
      {network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
       mojom::PermissionsPolicyFeature::kClientHintDeviceMemory},
      {network::mojom::WebClientHintsType::kDpr_DEPRECATED,
       mojom::PermissionsPolicyFeature::kClientHintDPR},
      {network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED,
       mojom::PermissionsPolicyFeature::kClientHintWidth},
      {network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
       mojom::PermissionsPolicyFeature::kClientHintViewportWidth},
      // End of legacy hints.
      {network::mojom::WebClientHintsType::kRtt_DEPRECATED,
       mojom::PermissionsPolicyFeature::kClientHintRTT},
      {network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
       mojom::PermissionsPolicyFeature::kClientHintDownlink},
      {network::mojom::WebClientHintsType::kEct_DEPRECATED,
       mojom::PermissionsPolicyFeature::kClientHintECT},
      {network::mojom::WebClientHintsType::kUA,
       mojom::PermissionsPolicyFeature::kClientHintUA},
      {network::mojom::WebClientHintsType::kUAArch,
       mojom::PermissionsPolicyFeature::kClientHintUAArch},
      {network::mojom::WebClientHintsType::kUAPlatform,
       mojom::PermissionsPolicyFeature::kClientHintUAPlatform},
      {network::mojom::WebClientHintsType::kUAModel,
       mojom::PermissionsPolicyFeature::kClientHintUAModel},
      {network::mojom::WebClientHintsType::kUAMobile,
       mojom::PermissionsPolicyFeature::kClientHintUAMobile},
      {network::mojom::WebClientHintsType::kUAFullVersion,
       mojom::PermissionsPolicyFeature::kClientHintUAFullVersion},
      {network::mojom::WebClientHintsType::kUAPlatformVersion,
       mojom::PermissionsPolicyFeature::kClientHintUAPlatformVersion},
      {network::mojom::WebClientHintsType::kPrefersColorScheme,
       mojom::PermissionsPolicyFeature::kClientHintPrefersColorScheme},
      {network::mojom::WebClientHintsType::kUABitness,
       mojom::PermissionsPolicyFeature::kClientHintUABitness},
      {network::mojom::WebClientHintsType::kUAReduced,
       mojom::PermissionsPolicyFeature::kClientHintUAReduced},
      {network::mojom::WebClientHintsType::kViewportHeight,
       mojom::PermissionsPolicyFeature::kClientHintViewportHeight},
      {network::mojom::WebClientHintsType::kDeviceMemory,
       mojom::PermissionsPolicyFeature::kClientHintDeviceMemory},
      {network::mojom::WebClientHintsType::kDpr,
       mojom::PermissionsPolicyFeature::kClientHintDPR},
      {network::mojom::WebClientHintsType::kResourceWidth,
       mojom::PermissionsPolicyFeature::kClientHintWidth},
      {network::mojom::WebClientHintsType::kViewportWidth,
       mojom::PermissionsPolicyFeature::kClientHintViewportWidth},
      {network::mojom::WebClientHintsType::kUAFullVersionList,
       mojom::PermissionsPolicyFeature::kClientHintUAFullVersionList},
      {network::mojom::WebClientHintsType::kFullUserAgent,
       mojom::PermissionsPolicyFeature::kClientHintUAFull},
      {network::mojom::WebClientHintsType::kUAWoW64,
       mojom::PermissionsPolicyFeature::kClientHintUAWoW64},
      {network::mojom::WebClientHintsType::kPartitionedCookies,
       mojom::PermissionsPolicyFeature::kClientHintPartitionedCookies},
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

const char* const kWebEffectiveConnectionTypeMapping[] = {
    "4g" /* Unknown */, "4g" /* Offline */, "slow-2g" /* Slow 2G */,
    "2g" /* 2G */,      "3g" /* 3G */,      "4g" /* 4G */
};

const size_t kWebEffectiveConnectionTypeMappingCount =
    std::size(kWebEffectiveConnectionTypeMapping);

bool IsClientHintSentByDefault(network::mojom::WebClientHintsType type) {
  switch (type) {
    case network::mojom::WebClientHintsType::kUA:
    case network::mojom::WebClientHintsType::kUAMobile:
      return true;
    case network::mojom::WebClientHintsType::kUAPlatform:
      return base::FeatureList::IsEnabled(
          features::kUACHPlatformEnabledByDefault);
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
  int startHint = 0;
  if (base::FeatureList::IsEnabled(features::kAllowClientHintsToThirdParty)) {
    // Do not remove any legacy Client Hints
    startHint = kClientHintsNumberOfLegacyHints;
  }
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& type = elem.first;
    const auto& header = elem.second;
    if (static_cast<int>(type) < startHint)
      continue;
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
