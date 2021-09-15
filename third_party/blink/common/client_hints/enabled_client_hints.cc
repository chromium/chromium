// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/gurl.h"

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
    case WebClientHintsType::kUABitness:
      if (!base::FeatureList::IsEnabled(features::kUserAgentClientHint))
        return true;
      break;
    case WebClientHintsType::kPrefersColorScheme:
      if (!base::FeatureList::IsEnabled(
              features::kPrefersColorSchemeClientHintHeader))
        return true;
      break;
    case WebClientHintsType::kViewportHeight:
      if (!base::FeatureList::IsEnabled(
              features::kViewportHeightClientHintHeader))
        return true;
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

void EnabledClientHints::SetIsEnabled(
    const GURL& url,
    const net::HttpResponseHeaders* response_headers,
    const network::mojom::WebClientHintsType type,
    const bool should_send) {
  bool enabled = should_send;
  if (type == WebClientHintsType::kUAReduced) {
    enabled &= blink::TrialTokenValidator().RequestEnablesFeature(
        url, response_headers, "UserAgentReduction", base::Time::Now());
  }
  SetIsEnabled(type, enabled);
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
