// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/new_tab_page_location_policy_handler.h"

#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "ios/chrome/browser/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace policy {

class NewTabPageLocationPolicyHandlerTest : public PlatformTest {};

// Checks that calling `ApplyPolicySettings` set the preference to the correct
// value when policies overrides "NewTabPageLocation".
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

// Checks that calling `ApplyPolicySettings` does not set the preference when
// policies does not overrides "NewTabPageLocation".
TEST_F(NewTabPageLocationPolicyHandlerTest, ApplyPolicySettings_NoOverride) {
  bool value = true;
  PolicyMap policies;
  PrefValueMap prefs;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  handler.ApplyPolicySettings(policies, &prefs);
  EXPECT_FALSE(prefs.GetBoolean(prefs::kNewTabPageLocationOverride, &value));
}

// Check that `CheckPolicySettings` does not report an error if the policy
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

// Checks that `CheckPolicySettings` reports an error if the policy can't
// overrides "NewTabPageLocation" because it is not a valid URL string.
TEST_F(NewTabPageLocationPolicyHandlerTest,
       CheckPolicySettings_InvalidURLFormat) {
  std::string value = "blabla";

  PolicyMap::Entry entry;
  entry.set_value(base::Value(value));

  PolicyMap policies;
  policies.Set(key::kNewTabPageLocation, std::move(entry));

  PolicyErrorMap errors;
  NewTabPageLocationPolicyHandler handler = NewTabPageLocationPolicyHandler();
  ASSERT_FALSE(handler.CheckPolicySettings(policies, &errors));
  EXPECT_TRUE(errors.HasError(key::kNewTabPageLocation));
}

// Checks that `CheckPolicySettings` does report an error if the policy can't
// overrides "NewTabPageLocation" because it is not a valid type.
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
