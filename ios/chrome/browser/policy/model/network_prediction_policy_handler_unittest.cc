// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/network_prediction_policy_handler.h"

#include <utility>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Sets `NetworkPredictionOptions` to `value` as a mandatory cloud user policy.
void SetPolicy(policy::PolicyMap* policies, base::Value value) {
  policies->Set(policy::key::kNetworkPredictionOptions,
                policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                policy::POLICY_SOURCE_CLOUD, std::move(value), nullptr);
}

class NetworkPredictionPolicyHandlerTest : public PlatformTest {
 protected:
  // Expects that `CheckPolicySettings()` rejects `policies` with an error on
  // `NetworkPredictionOptions` and that `ApplyPolicySettings()` leaves
  // `prefs::kNetworkPredictionSetting` unset.
  void ExpectPolicyRejected(const policy::PolicyMap& policies) {
    policy::PolicyErrorMap errors;
    EXPECT_FALSE(handler_.CheckPolicySettings(policies, &errors));
    EXPECT_TRUE(errors.HasError(policy::key::kNetworkPredictionOptions));

    PrefValueMap prefs;
    handler_.ApplyPolicySettings(policies, &prefs);
    int pref_value = -1;
    EXPECT_FALSE(
        prefs.GetInteger(prefs::kNetworkPredictionSetting, &pref_value));
  }

  NetworkPredictionPolicyHandler handler_;
};

// Tests that `CheckPolicySettings()` rejects out-of-range integers (including
// `3`, which is `prefetch::NetworkPredictionOptions::kExtended` on desktop and
// Android) instead of clamping them, and that `ApplyPolicySettings()` leaves
// the pref unset.
TEST_F(NetworkPredictionPolicyHandlerTest, RejectsOutOfRangeValues) {
  for (int invalid_value : {-1, 3}) {
    SCOPED_TRACE(invalid_value);
    policy::PolicyMap policies;
    SetPolicy(&policies, base::Value(invalid_value));
    ExpectPolicyRejected(policies);
  }
}

// Tests that `CheckPolicySettings()` rejects non-integer values and that
// `ApplyPolicySettings()` leaves the pref unset.
TEST_F(NetworkPredictionPolicyHandlerTest, RejectsNonIntegerValues) {
  {
    SCOPED_TRACE("string");
    policy::PolicyMap policies;
    SetPolicy(&policies, base::Value("0"));
    ExpectPolicyRejected(policies);
  }
  {
    SCOPED_TRACE("boolean");
    policy::PolicyMap policies;
    SetPolicy(&policies, base::Value(true));
    ExpectPolicyRejected(policies);
  }
}

}  // namespace
