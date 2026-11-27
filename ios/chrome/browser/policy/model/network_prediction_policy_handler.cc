// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/network_prediction_policy_handler.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "ios/chrome/browser/prerender/model/prerender_pref.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

// Mirrored from `NetworkPrediction{Always,WifiOnly,Never}` in
// components/policy/resources/templates/policy_definitions/Miscellaneous/NetworkPredictionOptions.yaml.
constexpr int kNetworkPredictionAlways = 0;
constexpr int kNetworkPredictionWifiOnly = 1;
constexpr int kNetworkPredictionNever = 2;

// Translates a validated `NetworkPredictionOptions` policy integer in `[0, 2]`
// to the corresponding `prerender_prefs::NetworkPredictionSetting` enum value.
prerender_prefs::NetworkPredictionSetting NetworkPredictionSettingFromPolicy(
    int policy_value) {
  switch (policy_value) {
    case kNetworkPredictionAlways:
      return prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular;
    case kNetworkPredictionWifiOnly:
      return prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly;
    case kNetworkPredictionNever:
      return prerender_prefs::NetworkPredictionSetting::kDisabled;
    default:
      NOTREACHED();
  }
}

}  // namespace

NetworkPredictionPolicyHandler::NetworkPredictionPolicyHandler()
    : policy::IntRangePolicyHandlerBase(policy::key::kNetworkPredictionOptions,
                                        /*min=*/kNetworkPredictionAlways,
                                        /*max=*/kNetworkPredictionNever,
                                        /*clamp=*/false) {}

NetworkPredictionPolicyHandler::~NetworkPredictionPolicyHandler() = default;

void NetworkPredictionPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  CHECK(prefs);
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int value_in_range = 0;
  if (!value || !EnsureInRange(value, &value_in_range, nullptr)) {
    return;
  }
  const prerender_prefs::NetworkPredictionSetting setting =
      NetworkPredictionSettingFromPolicy(value_in_range);
  prefs->SetInteger(prefs::kNetworkPredictionSetting,
                    std::to_underlying(setting));
}
