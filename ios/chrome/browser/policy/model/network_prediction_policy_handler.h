// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_NETWORK_PREDICTION_POLICY_HANDLER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_NETWORK_PREDICTION_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {
class PolicyMap;
}  // namespace policy

// Policy handler for the `NetworkPredictionOptions` enterprise policy on iOS.
// Validates integer values in `[0, 2]` and translates them to the inverted
// `prerender_prefs::NetworkPredictionSetting` enum stored in
// `prefs::kNetworkPredictionSetting`.
class NetworkPredictionPolicyHandler
    : public policy::IntRangePolicyHandlerBase {
 public:
  NetworkPredictionPolicyHandler();
  NetworkPredictionPolicyHandler(const NetworkPredictionPolicyHandler&) =
      delete;
  NetworkPredictionPolicyHandler& operator=(
      const NetworkPredictionPolicyHandler&) = delete;
  ~NetworkPredictionPolicyHandler() override;

  // policy::IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_NETWORK_PREDICTION_POLICY_HANDLER_H_
