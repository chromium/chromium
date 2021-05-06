// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/browser_signin_policy_handler.h"

#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "ios/chrome/browser/policy/policy_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace policy {
namespace {

// Schema used by the tests.
const char kTestSchema[] = R"(
    {
      "type": "object",
      "properties": {
        "BrowserSignin": {
          "type": "integer"
        }
      }
    })";

using BrowserSigninPolicyHandlerTest = PlatformTest;

const char* BrowserSigninModeToString(BrowserSigninMode mode) {
  switch (mode) {
    case BrowserSigninMode::kDisabled:
      return "Disabled";

    case BrowserSigninMode::kEnabled:
      return "Enabled";

    case BrowserSigninMode::kForced:
      return "Forced";
  }
}

// Check that calling `ApplyPolicySettings` set the preference
// to the correct value when policies overrides "BrowserSignin".
TEST_F(BrowserSigninPolicyHandlerTest, ApplyPolicySettings) {
  struct TestCase {
    BrowserSigninMode mode;
    bool expected_pref_value;
  };

  const TestCase test_cases[] = {
      {BrowserSigninMode::kDisabled, false},
      {BrowserSigninMode::kEnabled, true},
      {BrowserSigninMode::kForced, true},
  };

  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_EQ(error, "");

  BrowserSigninPolicyHandler handler(schema);

  for (const TestCase& test_case : test_cases) {
    PolicyMap policies;

    PolicyMap::Entry entry;
    entry.set_value(base::Value(static_cast<int>(test_case.mode)));
    policies.Set("BrowserSignin", std::move(entry));

    bool value = false;
    PrefValueMap prefs;
    handler.ApplyPolicySettings(policies, &prefs);
    EXPECT_TRUE(prefs.GetBoolean(prefs::kSigninAllowed, &value));
    EXPECT_EQ(test_case.expected_pref_value, value)
        << "For test case: mode = "
        << BrowserSigninModeToString(test_case.mode);
  }
}

// Check that calling `ApplyPolicySettings` does not set the
// preference when policies does not overrides "BrowserSignin".
TEST_F(BrowserSigninPolicyHandlerTest, ApplyPolicySettings_NoOverride) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_EQ(error, "");

  BrowserSigninPolicyHandler handler(schema);

  bool value = false;
  PolicyMap policies;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policies, &prefs);
  EXPECT_FALSE(prefs.GetBoolean(prefs::kSigninAllowed, &value));
}

// Check that `CheckPolicySettings` does not report an error if
// policies overrides "BrowserSignin" to support values.
TEST_F(BrowserSigninPolicyHandlerTest, CheckPolicySettings) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_EQ(error, "");

  BrowserSigninPolicyHandler handler(schema);

  const BrowserSigninMode supported_modes[] = {
      BrowserSigninMode::kDisabled,
      BrowserSigninMode::kEnabled,
  };

  for (BrowserSigninMode mode : supported_modes) {
    PolicyMap policies;

    PolicyMap::Entry entry;
    entry.set_value(base::Value(static_cast<int>(mode)));
    policies.Set("BrowserSignin", std::move(entry));

    PolicyErrorMap errors;
    ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
    EXPECT_FALSE(errors.HasError("BrowserSignin"))
        << "For mode: " << BrowserSigninModeToString(mode);
  }
}

// Check that `CheckPolicySettings` reports an error if policies
// overrides "BrowserSignin" to `BrowserSigninMode::kForced`.
TEST_F(BrowserSigninPolicyHandlerTest, CheckPolicySettings_Forced) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_EQ(error, "");

  BrowserSigninPolicyHandler handler(schema);

  PolicyMap policies;

  PolicyMap::Entry entry;
  entry.set_value(base::Value(static_cast<int>(BrowserSigninMode::kForced)));
  policies.Set("BrowserSignin", std::move(entry));

  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError("BrowserSignin"));
}

}  // namespace
}  // namespace policy
