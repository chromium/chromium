// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_activation_report_utils.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

namespace {

PreloadActivationBeaconFeatureState GetActivationBeaconFeatureState(
    const base::Feature& feature,
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers) {
  auto override_state = base::FeatureList::GetStateIfOverridden(feature);
  if (override_state.has_value() && !override_state.value()) {
    // Explicitly disabled by kill switch.
    return PreloadActivationBeaconFeatureState::kDisabledByFeatureFlag;
  }
  if (base::FeatureList::IsEnabled(feature)) {
    // Enabled by Finch or default.
    return PreloadActivationBeaconFeatureState::kEnabled;
  }
  // Otherwise, check if origin trial is enabled via response headers.
  if (response_headers &&
      blink::TrialTokenValidator().RequestEnablesFeature(
          request_url, response_headers, "PrefetchAndPrerenderActivationBeacon",
          base::Time::Now())) {
    return PreloadActivationBeaconFeatureState::kEnabled;
  }
  return PreloadActivationBeaconFeatureState::kNoOriginTrialToken;
}

}  // namespace

PreloadActivationBeaconFeatureState GetPrefetchActivationBeaconFeatureState(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers) {
  return GetActivationBeaconFeatureState(features::kPrefetchActivationBeacon,
                                         request_url, response_headers);
}

PreloadActivationBeaconFeatureState GetPrerenderActivationBeaconFeatureState(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers) {
  return GetActivationBeaconFeatureState(features::kPrerenderActivationBeacon,
                                         request_url, response_headers);
}

}  // namespace content
