// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/browser_signin_policy_handler.h"

#import <Foundation/Foundation.h>

#import "base/command_line.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/policy/core/browser/policy_error_map.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/schema.h"
#import "components/policy/policy_constants.h"
#import "components/prefs/pref_value_map.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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

class BrowserSigninPolicyHandlerTest : public PlatformTest {
 protected:
  BrowserSigninPolicyHandlerTest() {
    // Make sure there is no pre-existing policy present.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
  }

  ~BrowserSigninPolicyHandlerTest() override {
    // Cleanup any policies left from the test.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

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
    int expected_pref_value;
  };

  const TestCase test_cases[] = {
      {BrowserSigninMode::kDisabled,
       static_cast<int>(BrowserSigninMode::kDisabled)},
      {BrowserSigninMode::kEnabled,
       static_cast<int>(BrowserSigninMode::kEnabled)},
      {BrowserSigninMode::kForced,
       static_cast<int>(BrowserSigninMode::kForced)},
  };

  const auto schema = Schema::Parse(kTestSchema);
  ASSERT_TRUE(schema.has_value());

  BrowserSigninPolicyHandler handler(*schema);

  for (const TestCase& test_case : test_cases) {
    PolicyMap policies;

    PolicyMap::Entry entry;
    entry.set_value(base::Value(static_cast<int>(test_case.mode)));
    policies.Set(key::kBrowserSignin, std::move(entry));

    int value = -1;
    PrefValueMap prefs;
    handler.ApplyPolicySettings(policies, &prefs);
    EXPECT_TRUE(prefs.GetInteger(prefs::kBrowserSigninPolicy, &value));
    EXPECT_EQ(test_case.expected_pref_value, value)
        << "For test case: mode = "
        << BrowserSigninModeToString(test_case.mode);
  }
}

// Check that calling `ApplyPolicySettings` does not set the
// preference when policies does not overrides "BrowserSignin".
TEST_F(BrowserSigninPolicyHandlerTest, ApplyPolicySettings_NoOverride) {
  const auto schema = Schema::Parse(kTestSchema);
  ASSERT_TRUE(schema.has_value());

  BrowserSigninPolicyHandler handler(*schema);

  bool value = false;
  PolicyMap policies;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policies, &prefs);
  EXPECT_FALSE(prefs.GetBoolean(prefs::kSigninAllowed, &value));
}

// Check that `CheckPolicySettings` does not report an error if
// policies overrides "BrowserSignin" to support values.
TEST_F(BrowserSigninPolicyHandlerTest, CheckPolicySettings) {
  const auto schema = Schema::Parse(kTestSchema);
  ASSERT_TRUE(schema.has_value());

  BrowserSigninPolicyHandler handler(*schema);

  const BrowserSigninMode supported_modes[] = {
      BrowserSigninMode::kDisabled,
      BrowserSigninMode::kEnabled,
      BrowserSigninMode::kForced,
  };

  for (BrowserSigninMode mode : supported_modes) {
    PolicyMap policies;

    PolicyMap::Entry entry;
    entry.scope = POLICY_SCOPE_MACHINE;
    entry.set_value(base::Value(static_cast<int>(mode)));
    policies.Set(key::kBrowserSignin, std::move(entry));

    PolicyErrorMap errors;
    ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
    EXPECT_FALSE(errors.HasError(key::kBrowserSignin))
        << "For mode: " << BrowserSigninModeToString(mode);
  }
}

TEST_F(BrowserSigninPolicyHandlerTest, CheckPolicySettings_UserScopeWarning) {
  const auto schema = Schema::Parse(kTestSchema);
  ASSERT_TRUE(schema.has_value());

  BrowserSigninPolicyHandler handler(*schema);
  PolicyMap policies;

  PolicyMap::Entry entry;
  entry.scope = POLICY_SCOPE_USER;
  entry.set_value(base::Value(static_cast<int>(BrowserSigninMode::kDisabled)));
  policies.Set(key::kBrowserSignin, std::move(entry));

  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kBrowserSignin));
  EXPECT_FALSE(errors.HasFatalError(key::kBrowserSignin));
}

TEST_F(BrowserSigninPolicyHandlerTest,
       ApplyPolicySettings_LogsUserScopeMetric) {
  const auto schema = Schema::Parse(kTestSchema);
  ASSERT_TRUE(schema.has_value());

  BrowserSigninPolicyHandler handler(*schema);
  base::HistogramTester histogram_tester;

  PolicyMap policies;
  PolicyMap::Entry entry;
  entry.scope = POLICY_SCOPE_USER;
  entry.set_value(base::Value(static_cast<int>(BrowserSigninMode::kDisabled)));
  policies.Set(key::kBrowserSignin, std::move(entry));

  PrefValueMap prefs;
  handler.ApplyPolicySettings(policies, &prefs);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserSignin.UserScope.PolicyValue",
      BrowserSigninMode::kDisabled, 1);
}

TEST_F(BrowserSigninPolicyHandlerTest,
       ApplyPolicySettings_RepeatedApplyDoesNotOvercountMetric) {
  const auto schema = Schema::Parse(kTestSchema);
  ASSERT_TRUE(schema.has_value());

  BrowserSigninPolicyHandler handler(*schema);
  base::HistogramTester histogram_tester;

  PolicyMap policies;
  PolicyMap::Entry entry;
  entry.scope = POLICY_SCOPE_USER;
  entry.set_value(base::Value(static_cast<int>(BrowserSigninMode::kDisabled)));
  policies.Set(key::kBrowserSignin, std::move(entry));

  PrefValueMap prefs;
  handler.ApplyPolicySettings(policies, &prefs);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.BrowserSignin.UserScope.PolicyValue",
      BrowserSigninMode::kDisabled, 1);

  // Periodic policy refresh with same value should not record again.
  handler.ApplyPolicySettings(policies, &prefs);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue", 1);

  // Changing the policy value should record the new value.
  PolicyMap::Entry new_entry;
  new_entry.scope = POLICY_SCOPE_USER;
  new_entry.set_value(
      base::Value(static_cast<int>(BrowserSigninMode::kEnabled)));
  policies.Set(key::kBrowserSignin, std::move(new_entry));

  handler.ApplyPolicySettings(policies, &prefs);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue", 2);
  histogram_tester.ExpectBucketCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue",
      BrowserSigninMode::kEnabled, 1);

  // Refreshing again with kEnabled should not record again.
  handler.ApplyPolicySettings(policies, &prefs);
  histogram_tester.ExpectTotalCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue", 2);
}

}  // namespace
}  // namespace policy
