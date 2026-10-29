// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_util.h"

#import "base/test/scoped_feature_list.h"
#import "components/contextual_search/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

std::unique_ptr<KeyedService> BuildMockIOSChromeAimEligibilityService(
    ProfileIOS* profile) {
  return MockIOSChromeAimEligibilityService::CreateTestingProfileService(
      profile);
}

}  // namespace

class CobrowseUtilTest : public PlatformTest {
 public:
  CobrowseUtilTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockIOSChromeAimEligibilityService));
    profile_ = std::move(builder).Build();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests IsAimCobrowseWebSelectionSearchEligible when all conditions and flags
// are enabled.
TEST_F(CobrowseUtilTest,
       TestIsAimCobrowseWebSelectionSearchEligibleWhenEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kAimCobrowse, kAssistantContainer, kAimCobrowseWebSelectionSearch}, {});

  EXPECT_TRUE(IsAimCobrowseWebSelectionSearchEligible(profile_.get()));
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(IsAimCobrowseWebSelectionSearchEligible(aim_eligibility_service,
                                                      profile_->GetPrefs()));
}

// Tests IsAimCobrowseWebSelectionSearchEligible when
// kAimCobrowseWebSelectionSearch is disabled.
TEST_F(CobrowseUtilTest,
       TestIsAimCobrowseWebSelectionSearchEligibleWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kAimCobrowse, kAssistantContainer},
                                {kAimCobrowseWebSelectionSearch});

  EXPECT_FALSE(IsAimCobrowseWebSelectionSearchEligible(profile_.get()));
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(IsAimCobrowseWebSelectionSearchEligible(aim_eligibility_service,
                                                       profile_->GetPrefs()));
}

// Tests IsAimCobrowseWebSelectionSearchEligible when kAimCobrowse is disabled.
TEST_F(CobrowseUtilTest,
       TestIsAimCobrowseWebSelectionSearchEligibleWhenCobrowseDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kAimCobrowseWebSelectionSearch},
                                {kAimCobrowse});

  EXPECT_FALSE(IsAimCobrowseWebSelectionSearchEligible(profile_.get()));
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(IsAimCobrowseWebSelectionSearchEligible(aim_eligibility_service,
                                                       profile_->GetPrefs()));
}

// Tests IsAimCobrowseWebSelectionSearchEligible with incognito (off-the-record)
// profile.
TEST_F(CobrowseUtilTest,
       TestIsAimCobrowseWebSelectionSearchEligibleWhenIncognito) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kAimCobrowse, kAssistantContainer, kAimCobrowseWebSelectionSearch}, {});

  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  EXPECT_FALSE(IsAimCobrowseWebSelectionSearchEligible(otr_profile));
}

// Test `IsAimCobrowseEligible` and `IsAimCobrowseWebSelectionSearchEligible`
// when `kSearchContentSharingSettings` policy is disabled.
TEST_F(CobrowseUtilTest,
       TestIsAimCobrowseEligibleWhenSearchContentSharingDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kAimCobrowse, kAssistantContainer, kAimCobrowseWebSelectionSearch}, {});

  profile_->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  EXPECT_FALSE(IsAimCobrowseEligible(profile_.get()));
  EXPECT_FALSE(IsAimCobrowseWebSelectionSearchEligible(profile_.get()));
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(
      IsAimCobrowseEligible(aim_eligibility_service, profile_->GetPrefs()));
  EXPECT_FALSE(IsAimCobrowseWebSelectionSearchEligible(aim_eligibility_service,
                                                       profile_->GetPrefs()));
}

// Tests IsAimCobrowseAddTabEligible is true when kAimCobrowse is enabled with
// default parameters.
TEST_F(CobrowseUtilTest, TestIsAimCobrowseAddTabEligibleByDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kAimCobrowse, kAssistantContainer}, {});

  EXPECT_TRUE(IsAimCobrowseAddTabEligible(profile_.get()));
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(IsAimCobrowseAddTabEligible(aim_eligibility_service,
                                          profile_->GetPrefs()));
}

// Tests IsAimCobrowseAddTabEligible is true when the Add Tab entrypoint
// parameter is explicitly set to "true".
TEST_F(CobrowseUtilTest, TestIsAimCobrowseAddTabEligibleWhenParamEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kAimCobrowse, {{kAimCobrowseAddTabEntrypointParam, "true"}}},
       {kAssistantContainer, {}}},
      {});

  EXPECT_TRUE(IsAimCobrowseAddTabEligible(profile_.get()));
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(IsAimCobrowseAddTabEligible(aim_eligibility_service,
                                          profile_->GetPrefs()));
}

// Tests IsAimCobrowseAddTabEligible is false when the Add Tab entrypoint
// parameter is disabled, while IsAimCobrowseEligible and
// IsAimCobrowseWebSelectionSearchEligible remain true.
TEST_F(CobrowseUtilTest, TestIsAimCobrowseAddTabEligibleWhenParamDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kAimCobrowse, {{kAimCobrowseAddTabEntrypointParam, "false"}}},
       {kAssistantContainer, {}},
       {kAimCobrowseWebSelectionSearch, {}}},
      {});

  EXPECT_FALSE(IsAimCobrowseAddTabEligible(profile_.get()));
  EXPECT_TRUE(IsAimCobrowseEligible(profile_.get()));
  EXPECT_TRUE(IsAimCobrowseWebSelectionSearchEligible(profile_.get()));
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(IsAimCobrowseAddTabEligible(aim_eligibility_service,
                                           profile_->GetPrefs()));
  EXPECT_TRUE(
      IsAimCobrowseEligible(aim_eligibility_service, profile_->GetPrefs()));
  EXPECT_TRUE(IsAimCobrowseWebSelectionSearchEligible(aim_eligibility_service,
                                                      profile_->GetPrefs()));
}

// Tests IsAimCobrowseAddTabEligible is false when kAimCobrowse is disabled.
TEST_F(CobrowseUtilTest, TestIsAimCobrowseAddTabEligibleWhenCobrowseDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kAssistantContainer}, {kAimCobrowse});

  EXPECT_FALSE(IsAimCobrowseAddTabEligible(profile_.get()));
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(IsAimCobrowseAddTabEligible(aim_eligibility_service,
                                           profile_->GetPrefs()));
}
