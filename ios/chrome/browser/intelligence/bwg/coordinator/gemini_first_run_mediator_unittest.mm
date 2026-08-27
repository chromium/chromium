// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

std::unique_ptr<KeyedService> CreateTestTracker(ProfileIOS* context) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}

}  // namespace

class GeminiFirstRunMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindOnce(&CreateTestTracker));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    PrefService* prefs = profile_->GetPrefs();
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile_.get());

    mediator_ = [[GeminiFirstRunMediator alloc]
          initWithPrefService:prefs
                 webStateList:browser_->GetWebStateList()
           baseViewController:nil
                geminiService:GeminiServiceFactory::GetForProfile(
                                  profile_.get())
        authenticationService:AuthenticationServiceFactory::GetForProfile(
                                  profile_.get())
              identityManager:IdentityManagerFactory::GetForProfile(
                                  profile_.get())
                      tracker:tracker
                   entryPoint:gemini::EntryPoint::Promo
            completionHandler:nil];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  GeminiFirstRunMediator* mediator_ = nil;
};

// Tests that stepsForFirstRunType returns promo and consent steps for NewUser
// by default.
TEST_F(GeminiFirstRunMediatorTest, StepsForFirstRunType_DefaultNewUser) {
  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kNewUser],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kPromo,
                                   GeminiFirstRunStepIdentifier::kConsent));
}

// Tests that stepsForFirstRunType returns only the consent step for NewUser
// when promo impressions are exhausted.
TEST_F(GeminiFirstRunMediatorTest,
       StepsForFirstRunType_DefaultNewUser_PromoImpressionsExhausted) {
  profile_->GetPrefs()->SetInteger(prefs::kIOSBWGPromoImpressionCount, 3);

  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kNewUser],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kConsent));
}

// Tests that stepsForFirstRunType returns only the consent step for Live.
TEST_F(GeminiFirstRunMediatorTest, StepsForFirstRunType_Live) {
  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kLive],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kConsent));
}

// Tests that stepsForFirstRunType returns only the VisualRich step when the
// Visual Rich experiment is enabled.
TEST_F(GeminiFirstRunMediatorTest, StepsForFirstRunType_VisualRich) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kGeminiFRERefactor, {}},
       {kGeminiFREExperiment,
        {{kGeminiFREExperimentParam, kGeminiFREExperimentParamVisualRich}}}},
      {});

  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kNewUser],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kVisualRich));
}

// Tests that stepsForFirstRunType returns only the Lightweight step when the
// Lightweight experiment is enabled.
TEST_F(GeminiFirstRunMediatorTest, StepsForFirstRunType_Lightweight) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kGeminiFRERefactor, {}},
       {kGeminiFREExperiment,
        {{kGeminiFREExperimentParam,
          kGeminiFREExperimentParamLightweightConvenience}}}},
      {});

  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kNewUser],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kLightweight));
}

// Tests that shouldShowBrandingHeader returns true for NewUser by default.
TEST_F(GeminiFirstRunMediatorTest, ShouldShowBrandingHeader_DefaultNewUser) {
  EXPECT_TRUE([mediator_
      shouldShowBrandingHeaderForFirstRunType:GeminiFirstRunType::kNewUser]);
}

// Tests that shouldShowBrandingHeader returns false for Live.
TEST_F(GeminiFirstRunMediatorTest, ShouldShowBrandingHeader_Live) {
  EXPECT_FALSE([mediator_
      shouldShowBrandingHeaderForFirstRunType:GeminiFirstRunType::kLive]);
}

// Tests that shouldShowBrandingHeader returns false for Visual Rich.
TEST_F(GeminiFirstRunMediatorTest, ShouldShowBrandingHeader_VisualRich) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kGeminiFRERefactor, {}},
       {kGeminiFREExperiment,
        {{kGeminiFREExperimentParam, kGeminiFREExperimentParamVisualRich}}}},
      {});

  EXPECT_FALSE([mediator_
      shouldShowBrandingHeaderForFirstRunType:GeminiFirstRunType::kNewUser]);
}

// Tests that shouldShowBrandingHeader returns true for Lightweight.
TEST_F(GeminiFirstRunMediatorTest, ShouldShowBrandingHeader_Lightweight) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kGeminiFRERefactor, {}},
       {kGeminiFREExperiment,
        {{kGeminiFREExperimentParam,
          kGeminiFREExperimentParamLightweightConvenience}}}},
      {});

  EXPECT_TRUE([mediator_
      shouldShowBrandingHeaderForFirstRunType:GeminiFirstRunType::kNewUser]);
}

// Tests that the mediator returns the appropriate lightweight promo title
// for each variant.
TEST_F(GeminiFirstRunMediatorTest, LightweightPromoTitle_Variants) {
  const struct TestCase {
    std::string experiment_param;
    int expected_title_id;
  } kTestCases[] = {
      {kGeminiFREExperimentParamLightweightConvenience,
       IDS_IOS_BWG_LIGHTWEIGHT_PROMO_CONVENIENCE_TITLE},
      {kGeminiFREExperimentParamLightweightPageSharing,
       IDS_IOS_BWG_LIGHTWEIGHT_PROMO_PAGE_SHARING_TITLE},
      {kGeminiFREExperimentParamLightweightDiverse,
       IDS_IOS_BWG_LIGHTWEIGHT_PROMO_DIVERSE_TITLE},
  };

  for (const auto& test_case : kTestCases) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kGeminiFRERefactor, {}},
         {kGeminiFREExperiment,
          {{kGeminiFREExperimentParam, test_case.experiment_param}}}},
        {});
    EXPECT_NSEQ([mediator_ lightweightPromoTitle],
                l10n_util::GetNSString(test_case.expected_title_id));
  }
}
