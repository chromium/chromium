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
    case WebClientHintsType::kUAFormFactors:
    case WebClientHintsType::kPrefersColorScheme:
    case WebClientHintsType::kViewportHeight:
    case WebClientHintsType::kDeviceMemory:
    case WebClientHintsType::kDpr:
    case WebClientHintsType::kResourceWidth:
    case WebClientHintsType::kViewportWidth:
    case WebClientHintsType::kSaveData:
    case WebClientHintsType::kPrefersReducedMotion:
    case WebClientHintsType::kPrefersReducedTransparency:
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
