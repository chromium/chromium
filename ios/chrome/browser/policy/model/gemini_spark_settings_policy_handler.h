// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_GEMINI_SPARK_SETTINGS_POLICY_HANDLER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_GEMINI_SPARK_SETTINGS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
class PolicyErrorMap;
class PolicyMap;
}  // namespace policy

// Policy handler for the `GeminiSparkSettings` enterprise policy on iOS.
// Validates that the value is in `[0, 1]` and rejects `0` (Enabled) when
// `GeminiSettings = 1`, `GenAiDefaultSettings = 2` (if `GeminiSettings` is
// unset), or `GeminiActOnWebSettings = 1`.
//
// Because handlers run per policy level, consumers must still check
// prerequisite policies at runtime rather than relying on this pref alone.
class GeminiSparkSettingsPolicyHandler : public policy::IntRangePolicyHandler {
 public:
  GeminiSparkSettingsPolicyHandler();
  GeminiSparkSettingsPolicyHandler(const GeminiSparkSettingsPolicyHandler&) =
      delete;
  GeminiSparkSettingsPolicyHandler& operator=(
      const GeminiSparkSettingsPolicyHandler&) = delete;
  ~GeminiSparkSettingsPolicyHandler() override;

  // policy::IntRangePolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_GEMINI_SPARK_SETTINGS_POLICY_HANDLER_H_
