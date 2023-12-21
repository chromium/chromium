// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using TestProvider = extensions::TestManagementPolicyProvider;
using extensions::Extension;

class ManagementPolicyTest : public testing::Test {
 public:
  void SetUp() override {
    allow_all_.SetProhibitedActions(TestProvider::ALLOW_ALL);
    no_modify_status_.SetProhibitedActions(
        TestProvider::PROHIBIT_MODIFY_STATUS);
    no_load_.SetProhibitedActions(TestProvider::PROHIBIT_LOAD);
    must_remain_enabled_.SetProhibitedActions(
        TestProvider::MUST_REMAIN_ENABLED);
    must_remain_disabled_.SetProhibitedActions(
        TestProvider::MUST_REMAIN_DISABLED);
    must_remain_disabled_.SetDisableReason(
        extensions::disable_reason::DISABLE_SIDELOAD_WIPEOUT);
    must_remain_installed_.SetProhibitedActions(
        TestProvider::MUST_REMAIN_INSTALLED);
    restrict_all_.SetProhibitedActions(TestProvider::PROHIBIT_MODIFY_STATUS |
                                       TestProvider::PROHIBIT_LOAD |
                                       TestProvider::MUST_REMAIN_ENABLED);
  }

 protected:
  extensions::ManagementPolicy policy_;

  TestProvider allow_all_;
  TestProvider no_modify_status_;
  TestProvider no_load_;
  TestProvider must_remain_enabled_;
  TestProvider must_remain_disabled_;
  TestProvider must_remain_installed_;
  TestProvider restrict_all_;
};

TEST_F(ManagementPolicyTest, RegisterAndUnregister) {
  EXPECT_EQ(0, policy_.GetNumProviders());
  policy_.RegisterProvider(&allow_all_);
  EXPECT_EQ(1, policy_.GetNumProviders());
  policy_.RegisterProvider(&allow_all_);
  EXPECT_EQ(1, policy_.GetNumProviders());

  policy_.RegisterProvider(&no_modify_status_);
  EXPECT_EQ(2, policy_.GetNumProviders());
  policy_.UnregisterProvider(&allow_all_);
  EXPECT_EQ(1, policy_.GetNumProviders());
  policy_.UnregisterProvider(&allow_all_);
  EXPECT_EQ(1, policy_.GetNumProviders());
  policy_.UnregisterProvider(&no_modify_status_);
  EXPECT_EQ(0, policy_.GetNumProviders());

  policy_.RegisterProvider(&allow_all_);
  policy_.RegisterProvider(&no_modify_status_);
  EXPECT_EQ(2, policy_.GetNumProviders());
  policy_.UnregisterAllProviders();
  EXPECT_EQ(0, policy_.GetNumProviders());
}

TEST_F(ManagementPolicyTest, UserMayLoad) {
  // No providers registered.
  std::u16string error;
  // The extension and location are irrelevant to the
  // TestManagementPolicyProviders.
  EXPECT_TRUE(policy_.UserMayLoad(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // One provider, no relevant restriction.
  policy_.RegisterProvider(&no_modify_status_);
  EXPECT_TRUE(policy_.UserMayLoad(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Two providers, no relevant restrictions.
  policy_.RegisterProvider(&must_remain_enabled_);
  EXPECT_TRUE(policy_.UserMayLoad(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Three providers, one with a relevant restriction.
  policy_.RegisterProvider(&no_load_);
  EXPECT_FALSE(policy_.UserMayLoad(nullptr, &error));
  EXPECT_FALSE(error.empty());

  // Remove the restriction.
  policy_.UnregisterProvider(&no_load_);
  error.clear();
  EXPECT_TRUE(policy_.UserMayLoad(nullptr, &error));
  EXPECT_TRUE(error.empty());
}
TEST_F(ManagementPolicyTest, UserMayModifySettings) {
  // No providers registered.
  std::u16string error;
  EXPECT_TRUE(policy_.UserMayModifySettings(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // One provider, no relevant restriction.
  policy_.RegisterProvider(&allow_all_);
  EXPECT_TRUE(policy_.UserMayModifySettings(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Two providers, no relevant restrictions.
  policy_.RegisterProvider(&no_load_);
  EXPECT_TRUE(policy_.UserMayModifySettings(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Three providers, one with a relevant restriction.
  policy_.RegisterProvider(&no_modify_status_);
  EXPECT_FALSE(policy_.UserMayModifySettings(nullptr, &error));
  EXPECT_FALSE(error.empty());

  // Remove the restriction.
  policy_.UnregisterProvider(&no_modify_status_);
  error.clear();
  EXPECT_TRUE(policy_.UserMayModifySettings(nullptr, &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(ManagementPolicyTest, MustRemainEnabled) {
  // No providers registered.
  std::u16string error;
  EXPECT_FALSE(policy_.MustRemainEnabled(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // One provider, no relevant restriction.
  policy_.RegisterProvider(&allow_all_);
  EXPECT_FALSE(policy_.MustRemainEnabled(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Two providers, no relevant restrictions.
  policy_.RegisterProvider(&no_modify_status_);
  EXPECT_FALSE(policy_.MustRemainEnabled(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Three providers, one with a relevant restriction.
  policy_.RegisterProvider(&must_remain_enabled_);
  EXPECT_TRUE(policy_.MustRemainEnabled(nullptr, &error));
  EXPECT_FALSE(error.empty());

  // Remove the restriction.
  policy_.UnregisterProvider(&must_remain_enabled_);
  error.clear();
  EXPECT_FALSE(policy_.MustRemainEnabled(nullptr, &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(ManagementPolicyTest, MustRemainDisabled) {
  // No providers registered.
  std::u16string error;
  EXPECT_FALSE(policy_.MustRemainDisabled(nullptr, nullptr, &error));
  EXPECT_TRUE(error.empty());

  // One provider, no relevant restriction.
  policy_.RegisterProvider(&allow_all_);
  EXPECT_FALSE(policy_.MustRemainDisabled(nullptr, nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Two providers, no relevant restrictions.
  policy_.RegisterProvider(&no_modify_status_);
  EXPECT_FALSE(policy_.MustRemainDisabled(nullptr, nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Three providers, one with a relevant restriction.
  extensions::disable_reason::DisableReason reason =
      extensions::disable_reason::DISABLE_NONE;
  policy_.RegisterProvider(&must_remain_disabled_);
  EXPECT_TRUE(policy_.MustRemainDisabled(nullptr, &reason, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(extensions::disable_reason::DISABLE_SIDELOAD_WIPEOUT, reason);

  // Remove the restriction.
  policy_.UnregisterProvider(&must_remain_disabled_);
  error.clear();
  EXPECT_FALSE(policy_.MustRemainDisabled(nullptr, nullptr, &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(ManagementPolicyTest, MustRemainInstalled) {
  // No providers registered.
  std::u16string error;
  EXPECT_FALSE(policy_.MustRemainInstalled(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // One provider, no relevant restriction.
  policy_.RegisterProvider(&allow_all_);
  EXPECT_FALSE(policy_.MustRemainInstalled(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Two providers, no relevant restrictions.
  policy_.RegisterProvider(&no_modify_status_);
  EXPECT_FALSE(policy_.MustRemainInstalled(nullptr, &error));
  EXPECT_TRUE(error.empty());

  // Three providers, one with a relevant restriction.
  policy_.RegisterProvider(&must_remain_installed_);
  EXPECT_TRUE(policy_.MustRemainInstalled(nullptr, &error));
  EXPECT_FALSE(error.empty());

  // Remove the restriction.
  policy_.UnregisterProvider(&must_remain_installed_);
  error.clear();
  EXPECT_FALSE(policy_.MustRemainInstalled(nullptr, &error));
  EXPECT_TRUE(error.empty());
}

// Tests error handling in the ManagementPolicy.
TEST_F(ManagementPolicyTest, ErrorHandling) {
  // The error parameter should be unchanged if no restriction was found.
  std::string original_error = "Ceci est en effet une erreur.";
  std::u16string original_error16 = base::UTF8ToUTF16(original_error);
  std::u16string error = original_error16;
  EXPECT_TRUE(policy_.UserMayLoad(nullptr, &error));
  EXPECT_EQ(original_error, base::UTF16ToUTF8(error));
  EXPECT_TRUE(policy_.UserMayModifySettings(nullptr, &error));
  EXPECT_EQ(original_error, base::UTF16ToUTF8(error));
  EXPECT_FALSE(policy_.MustRemainEnabled(nullptr, &error));
  EXPECT_EQ(original_error, base::UTF16ToUTF8(error));

  // Ensure no crashes if no error message was requested.
  EXPECT_TRUE(policy_.UserMayLoad(nullptr, nullptr));
  EXPECT_TRUE(policy_.UserMayModifySettings(nullptr, nullptr));
  EXPECT_FALSE(policy_.MustRemainEnabled(nullptr, nullptr));
  policy_.RegisterProvider(&restrict_all_);
  EXPECT_FALSE(policy_.UserMayLoad(nullptr, nullptr));
  EXPECT_FALSE(policy_.UserMayModifySettings(nullptr, nullptr));
  EXPECT_TRUE(policy_.MustRemainEnabled(nullptr, nullptr));

  // Make sure returned error is correct.
  error = original_error16;
  EXPECT_FALSE(policy_.UserMayLoad(nullptr, &error));
  EXPECT_EQ(base::UTF8ToUTF16(TestProvider::expected_error()), error);
  error = original_error16;
  EXPECT_FALSE(policy_.UserMayModifySettings(nullptr, &error));
  EXPECT_EQ(base::UTF8ToUTF16(TestProvider::expected_error()), error);
  error = original_error16;
  EXPECT_TRUE(policy_.MustRemainEnabled(nullptr, &error));
  EXPECT_EQ(base::UTF8ToUTF16(TestProvider::expected_error()), error);
}
