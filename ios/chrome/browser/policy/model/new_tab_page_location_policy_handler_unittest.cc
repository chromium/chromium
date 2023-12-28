// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/new_tab_page_location_policy_handler.h"

#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace policy {

class NewTabPageLocationPolicyHandlerTest : public PlatformTest {};

// Tests that calling `ApplyPolicySettings` set the preference to the correct
// value when the policy overrides "NewTabPageLocation".
TEST_F(NewTabPageLocationPolicyHandlerTest, ApplyPolicySettings) {
  std::string value = "https://store.google.com";

  PolicyMap::Entry entry;
  entry.set_value(base::Value(value));

  PolicyMap policies;
  policies.Set(key::kNewTabPageLocation, std::move(entry));

  PrefValueMap prefs;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  handler.ApplyPolicySettings(policies, &prefs);

  EXPECT_TRUE(prefs.GetString(prefs::kNewTabPageLocationOverride, &value));
}

// Tests that calling `ApplyPolicySettings` set the preference to the correct
// value when the policy overrides "NewTabPageLocation" even if the URL is
// missing the scheme.
TEST_F(NewTabPageLocationPolicyHandlerTest, ApplyPolicySettings_NoScheme) {
  std::string value = "m.google.com";

  PolicyMap::Entry entry;
  entry.set_value(base::Value(value));

  PolicyMap policies;
  policies.Set(key::kNewTabPageLocation, std::move(entry));

  PrefValueMap prefs;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  handler.ApplyPolicySettings(policies, &prefs);

  std::string new_value = "https://m.google.com";
  EXPECT_TRUE(prefs.GetString(prefs::kNewTabPageLocationOverride, &new_value));
}

// Tests that calling `ApplyPolicySettings` does not set the preference when the
// policy does not override "NewTabPageLocation".
TEST_F(NewTabPageLocationPolicyHandlerTest, ApplyPolicySettings_NoOverride) {
  bool value = true;
  PolicyMap policies;
  PrefValueMap prefs;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  handler.ApplyPolicySettings(policies, &prefs);
  EXPECT_FALSE(prefs.GetBoolean(prefs::kNewTabPageLocationOverride, &value));
}

// Tests that `CheckPolicySettings` does not report an error if the policy
// overrides "NewTabPageLocation" with a valid value.
TEST_F(NewTabPageLocationPolicyHandlerTest, CheckPolicySettings) {
  std::string value = "https://store.google.com";

  PolicyMap::Entry entry;
  entry.set_value(base::Value(value));

  PolicyMap policies;
  policies.Set(key::kNewTabPageLocation, std::move(entry));

  PolicyErrorMap errors;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.HasError(key::kNewTabPageLocation));
}

// Tests that `CheckPolicySettings` does not report an error if the policy
// overrides "NewTabPageLocation" with a valid value even though it is missing
// the scheme.
TEST_F(NewTabPageLocationPolicyHandlerTest, CheckPolicySettings_NoSchemeURL) {
  std::string value = "wayfair.com";

  PolicyMap::Entry entry;
  entry.set_value(base::Value(value));

  PolicyMap policies;
  policies.Set(key::kNewTabPageLocation, std::move(entry));

  PolicyErrorMap errors;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  ASSERT_TRUE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_FALSE(errors.HasError(key::kNewTabPageLocation));
}

// Tests that `CheckPolicySettings` report an error if the policy overrides
// "NewTabPageLocation" with an empty value.
TEST_F(NewTabPageLocationPolicyHandlerTest, CheckPolicySettings_EmptyValue) {
  std::string value = "";

  PolicyMap::Entry entry;
  entry.set_value(base::Value(value));

  PolicyMap policies;
  policies.Set(key::kNewTabPageLocation, std::move(entry));

  PolicyErrorMap errors;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kNewTabPageLocation));
}

// Tests that `CheckPolicySettings` does report an error if the policy can't
// override "NewTabPageLocation" because it is not a valid type.
TEST_F(NewTabPageLocationPolicyHandlerTest, CheckPolicySettings_InvalidType) {
  bool value = true;

  PolicyMap::Entry entry;
  entry.set_value(base::Value(value));

  PolicyMap policies;
  policies.Set(key::kNewTabPageLocation, std::move(entry));

  PolicyErrorMap errors;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
}

}  // namespace policy
