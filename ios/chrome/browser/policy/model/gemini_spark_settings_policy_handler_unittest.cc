// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/gemini_spark_settings_policy_handler.h"

#include <utility>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void SetPolicy(policy::PolicyMap* policies,
               const char* key,
               base::Value value) {
  policies->Set(key, policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                policy::POLICY_SOURCE_CLOUD, std::move(value), nullptr);
}

class GeminiSparkSettingsPolicyHandlerTest : public PlatformTest {
 protected:
  GeminiSparkSettingsPolicyHandler handler_;
};

// Tests that `CheckPolicySettings()` succeeds and `ApplyPolicySettings()`
// leaves `kGeminiSparkSettings` unset when `GeminiSparkSettings` is unset.
TEST_F(GeminiSparkSettingsPolicyHandlerTest, PrefUnsetWhenPolicyUnset) {
  policy::PolicyMap policies;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;

  EXPECT_TRUE(handler_.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.HasError(policy::key::kGeminiSparkSettings));

  handler_.ApplyPolicySettings(policies, &prefs);
  int pref_value = -1;
  EXPECT_FALSE(prefs.GetInteger(prefs::kGeminiSparkSettings, &pref_value));
}

// Tests that `CheckPolicySettings()` succeeds and `ApplyPolicySettings()` maps
// valid `GeminiSparkSettings` values (`0` and `1`) to `kGeminiSparkSettings`.
TEST_F(GeminiSparkSettingsPolicyHandlerTest, AppliesPrefWhenPolicyValueValid) {
  for (int policy_value : {0, 1}) {
    SCOPED_TRACE(policy_value);
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kGeminiSparkSettings,
              base::Value(policy_value));
    policy::PolicyErrorMap errors;
    PrefValueMap prefs;

    ASSERT_TRUE(handler_.CheckPolicySettings(policies, &errors));
    EXPECT_FALSE(errors.HasError(policy::key::kGeminiSparkSettings));

    handler_.ApplyPolicySettings(policies, &prefs);
    int pref_value = -1;
    ASSERT_TRUE(prefs.GetInteger(prefs::kGeminiSparkSettings, &pref_value));
    EXPECT_EQ(pref_value, policy_value);
  }
}

// Tests that `CheckPolicySettings()` rejects out-of-range integer values and
// records an error on `GeminiSparkSettings`.
TEST_F(GeminiSparkSettingsPolicyHandlerTest, RejectsPolicyWhenValueOutOfRange) {
  for (int invalid_value : {-1, 2}) {
    SCOPED_TRACE(invalid_value);
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kGeminiSparkSettings,
              base::Value(invalid_value));
    policy::PolicyErrorMap errors;

    EXPECT_FALSE(handler_.CheckPolicySettings(policies, &errors));
    EXPECT_TRUE(errors.HasError(policy::key::kGeminiSparkSettings));
  }
}

// Tests that `CheckPolicySettings()` rejects non-integer values and records an
// error on `GeminiSparkSettings`.
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       RejectsPolicyWhenValueHasInvalidType) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value("0"));
  policy::PolicyErrorMap errors;

  EXPECT_FALSE(handler_.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(policy::key::kGeminiSparkSettings));
}

// Tests that `CheckPolicySettings()` rejects `GeminiSparkSettings` = 0 when
// `GeminiSettings` is disabled (`1`) and names `GeminiSettings` / `Allowed`.
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       RejectedWhenGeminiSettingsDisabled) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
  SetPolicy(&policies, policy::key::kGeminiSettings, base::Value(1));
  policy::PolicyErrorMap errors;

  EXPECT_FALSE(handler_.CheckPolicySettings(policies, &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kGeminiSparkSettings),
            l10n_util::GetStringFUTF16(IDS_POLICY_DEPENDENCY_ERROR,
                                       u"GeminiSettings", u"Allowed"));
}

// Tests that `CheckPolicySettings()` rejects `GeminiSparkSettings` = 0 when
// `GeminiSettings` is unset and `GenAiDefaultSettings` is disabled (`2`),
// naming `GenAiDefaultSettings` / `Allowed or AllowedWithoutLogging`.
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       RejectedWhenGeminiSettingsUnsetAndGenAiDefaultDisabled) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
  SetPolicy(&policies, policy::key::kGenAiDefaultSettings, base::Value(2));
  policy::PolicyErrorMap errors;

  EXPECT_FALSE(handler_.CheckPolicySettings(policies, &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kGeminiSparkSettings),
            l10n_util::GetStringFUTF16(IDS_POLICY_DEPENDENCY_ERROR,
                                       u"GenAiDefaultSettings",
                                       u"Allowed or AllowedWithoutLogging"));
}

// Tests that `CheckPolicySettings()` allows `GeminiSparkSettings` = 0 when
// `GeminiSettings` is explicitly allowed (`0`), overriding a disabled
// `GenAiDefaultSettings` (`2`).
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       AllowedWhenGeminiSettingsOverridesDisabledGenAiDefault) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
  SetPolicy(&policies, policy::key::kGenAiDefaultSettings, base::Value(2));
  SetPolicy(&policies, policy::key::kGeminiSettings, base::Value(0));
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;

  ASSERT_TRUE(handler_.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.HasError(policy::key::kGeminiSparkSettings));

  handler_.ApplyPolicySettings(policies, &prefs);
  int pref_value = -1;
  ASSERT_TRUE(prefs.GetInteger(prefs::kGeminiSparkSettings, &pref_value));
  EXPECT_EQ(pref_value, 0);
}

// Tests that `CheckPolicySettings()` rejects `GeminiSparkSettings` = 0 when
// `GeminiActOnWebSettings` is disabled (`1`) and names `GeminiActOnWebSettings`
// / `Enabled`.
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       RejectedWhenGeminiActOnWebSettingsDisabled) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
  SetPolicy(&policies, policy::key::kGeminiActOnWebSettings, base::Value(1));
  policy::PolicyErrorMap errors;

  EXPECT_FALSE(handler_.CheckPolicySettings(policies, &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kGeminiSparkSettings),
            l10n_util::GetStringFUTF16(IDS_POLICY_DEPENDENCY_ERROR,
                                       u"GeminiActOnWebSettings", u"Enabled"));
}

// Tests that `CheckPolicySettings()` allows `GeminiSparkSettings` = 0 when
// `GeminiSettings` and `GeminiActOnWebSettings` are explicitly allowed (`0`)
// and `GenAiDefaultSettings` is `0` or `1`.
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       AllowedWhenDependenciesExplicitlyAllowed) {
  for (int gen_ai_default : {0, 1}) {
    SCOPED_TRACE(gen_ai_default);
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
    SetPolicy(&policies, policy::key::kGeminiSettings, base::Value(0));
    SetPolicy(&policies, policy::key::kGeminiActOnWebSettings, base::Value(0));
    SetPolicy(&policies, policy::key::kGenAiDefaultSettings,
              base::Value(gen_ai_default));
    policy::PolicyErrorMap errors;

    EXPECT_TRUE(handler_.CheckPolicySettings(policies, &errors));
    EXPECT_FALSE(errors.HasError(policy::key::kGeminiSparkSettings));
  }
}

// Tests that `CheckPolicySettings()` ignores a wrong-typed dependency policy
// rather than treating it as disabled.
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       AllowedWhenDependencyHasInvalidType) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
  SetPolicy(&policies, policy::key::kGeminiSettings, base::Value("1"));
  policy::PolicyErrorMap errors;

  EXPECT_TRUE(handler_.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.HasError(policy::key::kGeminiSparkSettings));
}

// Tests that `CheckPolicySettings()` returns `false` without crashing when
// `errors` is null and a prerequisite dependency rejects `GeminiSparkSettings`
// = 0.
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       RejectsWithoutCrashingWhenErrorMapIsNull) {
  {
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
    SetPolicy(&policies, policy::key::kGeminiSettings, base::Value(1));
    EXPECT_FALSE(handler_.CheckPolicySettings(policies, /*errors=*/nullptr));
  }
  {
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
    SetPolicy(&policies, policy::key::kGenAiDefaultSettings, base::Value(2));
    EXPECT_FALSE(handler_.CheckPolicySettings(policies, /*errors=*/nullptr));
  }
  {
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(0));
    SetPolicy(&policies, policy::key::kGeminiActOnWebSettings, base::Value(1));
    EXPECT_FALSE(handler_.CheckPolicySettings(policies, /*errors=*/nullptr));
  }
}

// Tests that `CheckPolicySettings()` allows `GeminiSparkSettings` = 1 even when
// `GeminiSettings` and `GeminiActOnWebSettings` are disabled.
TEST_F(GeminiSparkSettingsPolicyHandlerTest,
       AllowsDisabledValueWhenDependenciesDisabled) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kGeminiSparkSettings, base::Value(1));
  SetPolicy(&policies, policy::key::kGeminiSettings, base::Value(1));
  SetPolicy(&policies, policy::key::kGeminiActOnWebSettings, base::Value(1));
  policy::PolicyErrorMap errors;

  EXPECT_TRUE(handler_.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.HasError(policy::key::kGeminiSparkSettings));
}

}  // namespace
