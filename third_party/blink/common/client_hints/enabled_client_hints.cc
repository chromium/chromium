// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

#include "base/feature_list.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {

using ::network::mojom::WebClientHintsType;

bool IsDisabledByFeature(const WebClientHintsType type) {
  switch (type) {
    case WebClientHintsType::kUA:
    case WebClientHintsType::kUAArch:
    case WebClientHintsType::kUAPlatform:
    case WebClientHintsType::kUAPlatformVersion:
    case WebClientHintsType::kUAModel:
    case WebClientHintsType::kUAMobile:
    case WebClientHintsType::kUAFullVersion:
    case WebClientHintsType::kUAFullVersionList:
    case WebClientHintsType::kUABitness:
    case WebClientHintsType::kUAWoW64:
      if (!base::FeatureList::IsEnabled(features::kUserAgentClientHint))
        return true;
      break;
    case WebClientHintsType::kUAFormFactor:
      if (!base::FeatureList::IsEnabled(features::kUserAgentClientHint) ||
          !base::FeatureList::IsEnabled(features::kClientHintsFormFactor)) {
        return true;
      }
      break;
    case WebClientHintsType::kPrefersColorScheme:
      break;
    case WebClientHintsType::kViewportHeight:
      if (!base::FeatureList::IsEnabled(
              features::kViewportHeightClientHintHeader)) {
        return true;
      }
      break;
    case WebClientHintsType::kDeviceMemory:
      if (!base::FeatureList::IsEnabled(features::kClientHintsDeviceMemory))
        return true;
      break;
    case WebClientHintsType::kDpr:
      if (!base::FeatureList::IsEnabled(features::kClientHintsDPR))
        return true;
      break;
    case WebClientHintsType::kResourceWidth:
      if (!base::FeatureList::IsEnabled(features::kClientHintsResourceWidth))
        return true;
      break;
    case WebClientHintsType::kViewportWidth:
      if (!base::FeatureList::IsEnabled(features::kClientHintsViewportWidth))
        return true;
      break;
    case WebClientHintsType::kDeviceMemory_DEPRECATED:
      if (!base::FeatureList::IsEnabled(
              features::kClientHintsDeviceMemory_DEPRECATED)) {
        return true;
      }
      break;
    case WebClientHintsType::kDpr_DEPRECATED:
      if (!base::FeatureList::IsEnabled(features::kClientHintsDPR_DEPRECATED))
        return true;
      break;
    case WebClientHintsType::kResourceWidth_DEPRECATED:
      if (!base::FeatureList::IsEnabled(
              features::kClientHintsResourceWidth_DEPRECATED)) {
        return true;
      }
      break;
    case WebClientHintsType::kViewportWidth_DEPRECATED:
      if (!base::FeatureList::IsEnabled(
              features::kClientHintsViewportWidth_DEPRECATED)) {
        return true;
      }
      break;
    case WebClientHintsType::kSaveData:
      if (!base::FeatureList::IsEnabled(features::kClientHintsSaveData))
        return true;
      break;
    case WebClientHintsType::kPrefersReducedMotion:
      break;
    case WebClientHintsType::kPrefersReducedTransparency:
      return !base::FeatureList::IsEnabled(
          features::kClientHintsPrefersReducedTransparency);
    default:
      break;
  }
  return false;
}

}  // namespace

bool EnabledClientHints::IsEnabled(const WebClientHintsType type) const {
  return enabled_types_[static_cast<int>(type)];
}

void EnabledClientHints::SetIsEnabled(const WebClientHintsType type,
                                      const bool should_send) {
  enabled_types_[static_cast<int>(type)] =
      IsDisabledByFeature(type) ? false : should_send;
}

std::vector<WebClientHintsType> EnabledClientHints::GetEnabledHints() const {
  std::vector<WebClientHintsType> hints;
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& type = elem.first;
    if (IsEnabled(type))
      hints.push_back(type);
  }
  return hints;
}

}  // namespace blink
