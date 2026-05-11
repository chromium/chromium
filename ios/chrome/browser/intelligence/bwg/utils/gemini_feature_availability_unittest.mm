// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"

#import "base/test/scoped_feature_list.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace gemini {

class GeminiFeatureAvailabilityTest : public PlatformTest {
 protected:
  GeminiFeatureAvailabilityTest() {}
  ~GeminiFeatureAvailabilityTest() override {}

  web::WebTaskEnvironment task_environment_;

  // Helper to create AccountInfo with specific capability.
  AccountInfo CreateAccountInfoWithCapability(bool can_use_model_execution) {
    AccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
    account_info.email = "test@example.com";
    account_info.gaia = GaiaId("test_gaia_id");

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(can_use_model_execution);
    return account_info;
  }

  // Helper to create a TestProfileIOS with a signed-in account and specific
  // capability.
  std::unique_ptr<TestProfileIOS> CreateProfileWithAccount(
      bool can_use_model_execution) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile.get());

    AccountInfo account_info =
        signin::MakeAccountAvailable(identity_manager, "test@example.com");
    signin::SetPrimaryAccount(identity_manager, "test@example.com",
                              signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(can_use_model_execution);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    return profile;
  }
};

#pragma mark - Image Remix

// Tests that Feature::kImageRemix is unavailable when its feature flag is
// disabled.
TEST_F(GeminiFeatureAvailabilityTest, ImageRemixDisabledByFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kPageActionMenu}, {kGeminiImageRemixTool});

  AccountInfo account_info;
  EXPECT_FALSE(IsFeatureAvailable(Feature::kImageRemix, account_info));
}

// Tests that Feature::kImageRemix is available when its feature flag is enabled
// and updated eligibility is disabled.
TEST_F(GeminiFeatureAvailabilityTest, ImageRemixEnabledByFlagOldEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiImageRemixTool, kPageActionMenu},
                                {kGeminiUpdatedEligibility});

  AccountInfo account_info;
  EXPECT_TRUE(IsFeatureAvailable(Feature::kImageRemix, account_info));
}

// Tests that Feature::kImageRemix is unavailable when updated eligibility is
// enabled but the account info is empty.
TEST_F(GeminiFeatureAvailabilityTest, ImageRemixEmptyAccountNewEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kGeminiImageRemixTool, kPageActionMenu, kGeminiUpdatedEligibility}, {});

  AccountInfo account_info;  // Empty account
  EXPECT_FALSE(IsFeatureAvailable(Feature::kImageRemix, account_info));
}

// Tests that Feature::kImageRemix is unavailable when updated eligibility is
// enabled and the account lacks the required capability.
TEST_F(GeminiFeatureAvailabilityTest, ImageRemixNoCapabilityNewEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kGeminiImageRemixTool, kPageActionMenu, kGeminiUpdatedEligibility}, {});

  AccountInfo account_info = CreateAccountInfoWithCapability(false);
  EXPECT_FALSE(IsFeatureAvailable(Feature::kImageRemix, account_info));
}

// Tests that Feature::kImageRemix is available when updated eligibility is
// enabled and the account has the required capability.
TEST_F(GeminiFeatureAvailabilityTest, ImageRemixHasCapabilityNewEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kGeminiImageRemixTool, kPageActionMenu, kGeminiUpdatedEligibility}, {});

  AccountInfo account_info = CreateAccountInfoWithCapability(true);
  EXPECT_TRUE(IsFeatureAvailable(Feature::kImageRemix, account_info));
}

#pragma mark - IdentityManager Overload

TEST_F(GeminiFeatureAvailabilityTest, IdentityManagerNil) {
  EXPECT_FALSE(IsFeatureAvailable(
      Feature::kImageRemix, static_cast<signin::IdentityManager*>(nullptr)));
}

TEST_F(GeminiFeatureAvailabilityTest, IdentityManagerAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kGeminiImageRemixTool, kPageActionMenu, kGeminiUpdatedEligibility}, {});

  std::unique_ptr<TestProfileIOS> profile = CreateProfileWithAccount(true);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile.get());

  EXPECT_TRUE(IsFeatureAvailable(Feature::kImageRemix, identity_manager));
}

TEST_F(GeminiFeatureAvailabilityTest, IdentityManagerUnavailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kGeminiImageRemixTool, kPageActionMenu, kGeminiUpdatedEligibility}, {});

  std::unique_ptr<TestProfileIOS> profile = CreateProfileWithAccount(false);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile.get());

  EXPECT_FALSE(IsFeatureAvailable(Feature::kImageRemix, identity_manager));
}

#pragma mark - ProfileIOS Overload

TEST_F(GeminiFeatureAvailabilityTest, ProfileNil) {
  EXPECT_FALSE(IsFeatureAvailable(Feature::kImageRemix,
                                  static_cast<ProfileIOS*>(nullptr)));
}

TEST_F(GeminiFeatureAvailabilityTest, ProfileAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kGeminiImageRemixTool, kPageActionMenu, kGeminiUpdatedEligibility}, {});

  std::unique_ptr<TestProfileIOS> profile = CreateProfileWithAccount(true);

  EXPECT_TRUE(IsFeatureAvailable(Feature::kImageRemix, profile.get()));
}

TEST_F(GeminiFeatureAvailabilityTest, ProfileUnavailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kGeminiImageRemixTool, kPageActionMenu, kGeminiUpdatedEligibility}, {});

  std::unique_ptr<TestProfileIOS> profile = CreateProfileWithAccount(false);

  EXPECT_FALSE(IsFeatureAvailable(Feature::kImageRemix, profile.get()));
}

#pragma mark - Capabilities

// Tests HasGeminiInChromeCapability with an empty account.
TEST_F(GeminiFeatureAvailabilityTest,
       HasGeminiInChromeCapability_EmptyAccount) {
  AccountInfo empty_account;
  EXPECT_FALSE(HasGeminiInChromeCapability(empty_account));
}

// Tests HasGeminiInChromeCapability when updated eligibility is disabled.
TEST_F(GeminiFeatureAvailabilityTest,
       HasGeminiInChromeCapability_LegacyEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kPageActionMenu}, {kGeminiUpdatedEligibility});

  AccountInfo account = CreateAccountInfoWithCapability(true);
  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_can_use_gemini_in_chrome(false);
  EXPECT_TRUE(HasGeminiInChromeCapability(account));
}

// Tests HasGeminiInChromeCapability when updated eligibility is enabled.
TEST_F(GeminiFeatureAvailabilityTest,
       HasGeminiInChromeCapability_UpdatedEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kPageActionMenu, kGeminiUpdatedEligibility},
                                {});

  AccountInfo account = CreateAccountInfoWithCapability(false);
  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_can_use_gemini_in_chrome(true);
  EXPECT_TRUE(HasGeminiInChromeCapability(account));

  mutator.set_can_use_gemini_in_chrome(false);
  EXPECT_FALSE(HasGeminiInChromeCapability(account));
}

// Tests HasModelExecutionCapability with an empty account.
TEST_F(GeminiFeatureAvailabilityTest,
       HasModelExecutionCapability_EmptyAccount) {
  AccountInfo empty_account;
  EXPECT_FALSE(HasModelExecutionCapability(empty_account));
}

// Tests HasModelExecutionCapability when standard capability is possessed.
TEST_F(GeminiFeatureAvailabilityTest, HasModelExecutionCapability_Enabled) {
  AccountInfo account = CreateAccountInfoWithCapability(true);
  EXPECT_TRUE(HasModelExecutionCapability(account));
}

// Tests HasModelExecutionCapability when standard capability is not possessed.
TEST_F(GeminiFeatureAvailabilityTest, HasModelExecutionCapability_Disabled) {
  AccountInfo account = CreateAccountInfoWithCapability(false);
  EXPECT_FALSE(HasModelExecutionCapability(account));
}

}  // namespace gemini
