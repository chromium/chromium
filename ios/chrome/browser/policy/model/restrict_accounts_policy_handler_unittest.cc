// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/restrict_accounts_policy_handler.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace policy {

class RestrictAccountsPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    Schema chrome_schema = Schema::Wrap(policy::GetChromeSchemaData());
    handler_list_.AddHandler(
        base::WrapUnique<policy::ConfigurationPolicyHandler>(
            new RestrictAccountsPolicyHandler(chrome_schema)));
  }

 protected:
  // Returns a string of valid patterns.
  std::string ValidPatternsString() {
    return R"(
        [
          "*@example.com",
          "user@managedchrome.com"
        ]
        )";
  }

  // Returns a List of valid patterns.
  base::Value ValidPatterns() {
    base::Value::List value;
    value.Append("*@example.com");
    value.Append("user@managedchrome.com");
    return base::Value(std::move(value));
  }

  // Returns a List of invalid patterns.
  base::Value InvalidPatterns() {
    base::Value::List value;
    value.Append("*@example.com");
    value.Append("invalidPattern\\");
    value.Append("user@managedchrome.com");
    return base::Value(std::move(value));
  }
};

// Tests that the settings are correctly applied when the patterns are all
// valid.
TEST_F(RestrictAccountsPolicyHandlerTest, ApplyPolicySettings) {
  EXPECT_FALSE(store_->GetValue(prefs::kRestrictAccountsToPatterns, nullptr));

  PolicyMap policy;
  policy.Set(key::kRestrictAccountsToPatterns, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, ValidPatterns(), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kRestrictAccountsToPatterns, &pref_value));
  ASSERT_TRUE(pref_value);

  std::optional<base::Value> expected = ValidPatterns();
  EXPECT_EQ(expected, *pref_value);
}

// Tests that the settings are correctly applied when a pattern is invalid.
TEST_F(RestrictAccountsPolicyHandlerTest, ApplyInvalidPolicySettings) {
  EXPECT_FALSE(store_->GetValue(prefs::kRestrictAccountsToPatterns, nullptr));

  PolicyMap policy;
  policy.Set(key::kRestrictAccountsToPatterns, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, InvalidPatterns(),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  EXPECT_TRUE(
      store_->GetValue(prefs::kRestrictAccountsToPatterns, &pref_value));
  ASSERT_TRUE(pref_value);

  // The setting is not filtering the invalid patterns out.
  std::optional<base::Value> expected = InvalidPatterns();
  EXPECT_EQ(expected, *pref_value);
}

// Tests that the pref is not updated when the type of the policy wrong.
TEST_F(RestrictAccountsPolicyHandlerTest, WrongPolicyType) {
  PolicyMap policy;
  // The expected type is a list base::Value, but this policy sets it as an
  // unparsed base::Value. Any type other than list should fail.
  policy.Set(key::kRestrictAccountsToPatterns, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(ValidPatternsString()), nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(store_->GetValue(prefs::kRestrictAccountsToPatterns, nullptr));
}

// Tests that the error handling is correct.
TEST_F(RestrictAccountsPolicyHandlerTest, CheckPolicySettings) {
  Schema chrome_schema = Schema::Wrap(policy::GetChromeSchemaData());
  RestrictAccountsPolicyHandler handler(chrome_schema);
  policy::PolicyErrorMap errors;
  PolicyMap policy;

  // Valid patterns.
  policy.Set(key::kRestrictAccountsToPatterns, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, ValidPatterns(), nullptr);

  EXPECT_TRUE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());

  errors.Clear();

  // Invalid patterns.
  policy.Set(key::kRestrictAccountsToPatterns, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, InvalidPatterns(),
             nullptr);

  EXPECT_TRUE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kRestrictAccountsToPatterns).empty());

  errors.Clear();

  // Empty patterns.
  policy.Set(key::kRestrictAccountsToPatterns, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::JSONReader::Read(""),
             nullptr);

  EXPECT_TRUE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());

  errors.Clear();

  // Non-list patterns.
  policy.Set(key::kRestrictAccountsToPatterns, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(ValidPatternsString()), nullptr);

  EXPECT_FALSE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(key::kRestrictAccountsToPatterns).empty());

  errors.Clear();
}

}  // namespace policy
