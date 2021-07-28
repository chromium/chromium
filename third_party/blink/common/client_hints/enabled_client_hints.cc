// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {

using ::network::mojom::WebClientHintsType;

bool IsDisabledByFeature(const WebClientHintsType type) {
  switch (type) {
    case WebClientHintsType::kLang:
      if (!base::FeatureList::IsEnabled(features::kLangClientHintHeader))
        return true;
      break;
    case WebClientHintsType::kUA:
    case WebClientHintsType::kUAArch:
    case WebClientHintsType::kUAPlatform:
    case WebClientHintsType::kUAPlatformVersion:
    case WebClientHintsType::kUAModel:
    case WebClientHintsType::kUAMobile:
    case WebClientHintsType::kUAFullVersion:
    case WebClientHintsType::kUABitness:
      if (!base::FeatureList::IsEnabled(features::kUserAgentClientHint))
        return true;
      break;
    case WebClientHintsType::kPrefersColorScheme:
      if (!base::FeatureList::IsEnabled(
              features::kPrefersColorSchemeClientHintHeader))
        return true;
      break;
    default:
      break;
  }
  return false;
}

}  // namespace

bool EnabledClientHints::IsEnabled(const WebClientHintsType type) const {
  if (IsDisabledByFeature(type)) {
    return false;
  }
  return enabled_types_[static_cast<int>(type)];
}

void EnabledClientHints::SetIsEnabled(const WebClientHintsType type,
                                      const bool should_send) {
  enabled_types_[static_cast<int>(type)] = should_send;
}

}  // namespace blink
