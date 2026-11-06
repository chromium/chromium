// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/gemini_spark_settings_policy_handler.h"

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

// Policy integer values from `GeminiSparkSettings.yaml`, `GeminiSettings.yaml`,
// `GenAiDefaultSettings.yaml`, and `GeminiActOnWebSettings.yaml`.
constexpr int kGeminiSparkSettingsEnabled = 0;
constexpr int kGeminiSparkSettingsDisabled = 1;
constexpr int kGeminiSettingsDisabled = 1;
constexpr int kGenAiDefaultSettingsDisabled = 2;
constexpr int kGeminiActOnWebSettingsDisabled = 1;

bool IsIntegerPolicySetTo(const policy::PolicyMap& policies,
                          const char* name,
                          int value) {
  const base::Value* policy_value =
      policies.GetValue(name, base::Value::Type::INTEGER);
  return policy_value && policy_value->GetInt() == value;
}

}  // namespace

GeminiSparkSettingsPolicyHandler::GeminiSparkSettingsPolicyHandler()
    : policy::IntRangePolicyHandler(policy::key::kGeminiSparkSettings,
                                    prefs::kGeminiSparkSettings,
                                    /*min=*/kGeminiSparkSettingsEnabled,
                                    /*max=*/kGeminiSparkSettingsDisabled,
                                    /*clamp=*/false) {}

GeminiSparkSettingsPolicyHandler::~GeminiSparkSettingsPolicyHandler() = default;

bool GeminiSparkSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!policy::IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  if (!IsIntegerPolicySetTo(policies, policy_name(),
                            kGeminiSparkSettingsEnabled)) {
    return true;
  }

  // `GenAiDefaultSettings` only applies as a fallback when `GeminiSettings` is
  // unset.
  const char* gemini_blocking_policy = nullptr;
  const char* gemini_expected_value = nullptr;
  if (IsIntegerPolicySetTo(policies, policy::key::kGeminiSettings,
                           kGeminiSettingsDisabled)) {
    gemini_blocking_policy = policy::key::kGeminiSettings;
    gemini_expected_value = "Allowed";
  } else if (!policies.IsPolicySet(policy::key::kGeminiSettings) &&
             IsIntegerPolicySetTo(policies, policy::key::kGenAiDefaultSettings,
                                  kGenAiDefaultSettingsDisabled)) {
    gemini_blocking_policy = policy::key::kGenAiDefaultSettings;
    gemini_expected_value = "Allowed or AllowedWithoutLogging";
  }
  if (gemini_blocking_policy) {
    if (errors) {
      errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR,
                       gemini_blocking_policy, gemini_expected_value);
    }
    return false;
  }

  if (IsIntegerPolicySetTo(policies, policy::key::kGeminiActOnWebSettings,
                           kGeminiActOnWebSettingsDisabled)) {
    if (errors) {
      errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR,
                       policy::key::kGeminiActOnWebSettings, "Enabled");
    }
    return false;
  }

  return true;
}
