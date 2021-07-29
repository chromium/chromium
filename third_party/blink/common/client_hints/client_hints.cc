// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <utility>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "url/origin.h"

namespace blink {

const char* const kClientHintsHeaderMapping[] = {"device-memory",
                                                 "dpr",
                                                 "width",
                                                 "viewport-width",
                                                 "rtt",
                                                 "downlink",
                                                 "ect",
                                                 "sec-ch-lang",
                                                 "sec-ch-ua",
                                                 "sec-ch-ua-arch",
                                                 "sec-ch-ua-platform",
                                                 "sec-ch-ua-model",
                                                 "sec-ch-ua-mobile",
                                                 "sec-ch-ua-full-version",
                                                 "sec-ch-ua-platform-version",
                                                 "sec-ch-prefers-color-scheme",
                                                 "sec-ch-ua-bitness",
                                                 "sec-ch-ua-reduced"};

const unsigned kClientHintsNumberOfLegacyHints = 4;

const mojom::PermissionsPolicyFeature kClientHintsPermissionsPolicyMapping[] = {
    // Legacy Hints that are sent cross-origin regardless of Permissions Policy
    // when kAllowClientHintsToThirdParty is enabled.
    mojom::PermissionsPolicyFeature::kClientHintDeviceMemory,
    mojom::PermissionsPolicyFeature::kClientHintDPR,
    mojom::PermissionsPolicyFeature::kClientHintWidth,
    mojom::PermissionsPolicyFeature::kClientHintViewportWidth,
    // End of legacy hints.
    mojom::PermissionsPolicyFeature::kClientHintRTT,
    mojom::PermissionsPolicyFeature::kClientHintDownlink,
    mojom::PermissionsPolicyFeature::kClientHintECT,
    mojom::PermissionsPolicyFeature::kClientHintLang,
    mojom::PermissionsPolicyFeature::kClientHintUA,
    mojom::PermissionsPolicyFeature::kClientHintUAArch,
    mojom::PermissionsPolicyFeature::kClientHintUAPlatform,
    mojom::PermissionsPolicyFeature::kClientHintUAModel,
    mojom::PermissionsPolicyFeature::kClientHintUAMobile,
    mojom::PermissionsPolicyFeature::kClientHintUAFullVersion,
    mojom::PermissionsPolicyFeature::kClientHintUAPlatformVersion,
    mojom::PermissionsPolicyFeature::kClientHintPrefersColorScheme,
    mojom::PermissionsPolicyFeature::kClientHintUABitness,
    mojom::PermissionsPolicyFeature::kClientHintUAReduced,
};

const size_t kClientHintsMappingsCount = base::size(kClientHintsHeaderMapping);

static_assert(
    base::size(kClientHintsHeaderMapping) ==
        (static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) + 1),
    "Client Hint name table size must match network::mojom::WebClientHintsType "
    "range");

static_assert(base::size(kClientHintsPermissionsPolicyMapping) ==
                  kClientHintsMappingsCount,
              "Client Hint table sizes must be identical between names and "
              "feature policies");

const char* const kWebEffectiveConnectionTypeMapping[] = {
    "4g" /* Unknown */, "4g" /* Offline */, "slow-2g" /* Slow 2G */,
    "2g" /* 2G */,      "3g" /* 3G */,      "4g" /* 4G */
};

const size_t kWebEffectiveConnectionTypeMappingCount =
    base::size(kWebEffectiveConnectionTypeMapping);

std::string SerializeLangClientHint(const std::string& raw_language_list) {
  base::StringTokenizer t(raw_language_list, ",");
  std::string result;
  while (t.GetNext()) {
    if (!result.empty())
      result.append(", ");
    base::StrAppend(&result, {"\"", t.token_piece(), "\""});
  }
  return result;
}

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
  size_t startHint = 0;
  if (base::FeatureList::IsEnabled(features::kAllowClientHintsToThirdParty)) {
    // Do not remove any legacy Client Hints
    startHint = kClientHintsNumberOfLegacyHints;
  }
  for (size_t i = startHint; i < blink::kClientHintsMappingsCount; ++i) {
    // Remove the hint if any is true:
    // * Permissions policy is null (we're in a sync XHR case) and the hint is
    // not sent by default.
    // * Permissions policy exists and doesn't allow for the hint.
    if ((!permissions_policy &&
         !IsClientHintSentByDefault(
             static_cast<network::mojom::WebClientHintsType>(i))) ||
        (permissions_policy &&
         !permissions_policy->IsFeatureEnabledForOrigin(
             blink::kClientHintsPermissionsPolicyMapping[i], origin))) {
      removed_headers->push_back(blink::kClientHintsHeaderMapping[i]);
    }
  }
}

}  // namespace blink
