// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/first_run/coordinator/gemini_first_run_mediator.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/first_run/coordinator/gemini_first_run_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/first_run/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
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
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindOnce(&CreateTestTracker));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        GeminiServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeGeminiService>();
            }));

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_delegate_ = OCMProtocolMock(@protocol(GeminiFirstRunMediatorDelegate));
    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));

    mediator_ = CreateMediator(gemini::EntryPoint::Promo, ^(BOOL success) {
      completion_called_ = YES;
      completion_success_ = success;
    });
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    mock_delegate_ = nil;
    mock_scene_handler_ = nil;
    browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  GeminiFirstRunMediator* CreateMediator(gemini::EntryPoint entry_point,
                                         void (^completion)(BOOL success)) {
    PrefService* prefs = profile_->GetPrefs();
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile_.get());

    GeminiFirstRunMediator* mediator = [[GeminiFirstRunMediator alloc]
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
                   entryPoint:entry_point
            completionHandler:completion];
    mediator.delegate = mock_delegate_;
    mediator.sceneHandler = mock_scene_handler_;
    return mediator;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  GeminiFirstRunMediator* mediator_ = nil;
  id mock_delegate_ = nil;
  id mock_scene_handler_ = nil;
  BOOL completion_called_ = NO;
  BOOL completion_success_ = NO;
};

// Tests steps and branding header for NewUser by default.
TEST_F(GeminiFirstRunMediatorTest, FirstRunConfiguration_DefaultNewUser) {
  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kNewUser],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kPromo,
                                   GeminiFirstRunStepIdentifier::kConsent));
  EXPECT_TRUE([mediator_
      shouldShowBrandingHeaderForFirstRunType:GeminiFirstRunType::kNewUser]);
}

// Tests that stepsForFirstRunType returns only the consent step for NewUser
// when promo impressions are exhausted.
TEST_F(GeminiFirstRunMediatorTest,
       StepsForFirstRunType_DefaultNewUser_PromoImpressionsExhausted) {
  profile_->GetPrefs()->SetInteger(prefs::kIOSBWGPromoImpressionCount, 3);

  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kNewUser],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kConsent));
}

// Tests steps and branding header for Live.
TEST_F(GeminiFirstRunMediatorTest, FirstRunConfiguration_Live) {
  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kLive],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kConsent));
  EXPECT_FALSE([mediator_
      shouldShowBrandingHeaderForFirstRunType:GeminiFirstRunType::kLive]);
}

// Tests steps and branding header when Visual Rich experiment is enabled.
TEST_F(GeminiFirstRunMediatorTest, FirstRunConfiguration_VisualRich) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kGeminiFRERefactor, {}},
       {kGeminiFREExperiment,
        {{kGeminiFREExperimentParam, kGeminiFREExperimentParamVisualRich}}}},
      {});

  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kNewUser],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kVisualRich));
  EXPECT_FALSE([mediator_
      shouldShowBrandingHeaderForFirstRunType:GeminiFirstRunType::kNewUser]);
}

// Tests steps and branding header when Lightweight experiment is enabled.
TEST_F(GeminiFirstRunMediatorTest, FirstRunConfiguration_Lightweight) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kGeminiFRERefactor, {}},
       {kGeminiFREExperiment,
        {{kGeminiFREExperimentParam,
          kGeminiFREExperimentParamLightweightConvenience}}}},
      {});

  EXPECT_THAT([mediator_ stepsForFirstRunType:GeminiFirstRunType::kNewUser],
              testing::ElementsAre(GeminiFirstRunStepIdentifier::kLightweight));
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

// Tests that consenting to Live Gemini updates both the Live consent pref and
// the Chrome-level Live microphone setting, notifies the delegate, and calls
// the completion block with success.
TEST_F(GeminiFirstRunMediatorTest, TestDidConsentToLiveGemini) {
  PrefService* prefs = profile_->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kIOSGeminiLiveConsent));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kIOSGeminiLiveMicrophoneSetting));

  OCMExpect([mock_delegate_
      dismissGeminiConsentUIWithCompletion:[OCMArg invokeBlock]]);

  [mediator_ didConsentToLiveGemini];

  EXPECT_TRUE(prefs->GetBoolean(prefs::kIOSGeminiLiveConsent));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kIOSGeminiLiveMicrophoneSetting));
  EXPECT_TRUE(completion_called_);
  EXPECT_TRUE(completion_success_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that shouldShowAIHubIPH returns NO when ChromeNextIa is enabled.
TEST_F(GeminiFirstRunMediatorTest, TestShouldShowAIHubIPH_ChromeNextIaEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kChromeNextIa);
  EXPECT_FALSE([mediator_ shouldShowAIHubIPH]);
}

// Tests didConsentGemini sets consent pref, notifies tracker when
// kGeminiNavigationPromo is enabled, and dismisses UI with completion(YES).
TEST_F(GeminiFirstRunMediatorTest, TestDidConsentGemini) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {feature_engagement::kIPHiOSGeminiFullscreenPromoFeature,
       kGeminiNavigationPromo, kPageActionMenu},
      {});

  auto* tracker = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
  EXPECT_CALL(*tracker,
              NotifyEvent(feature_engagement::events::kIOSGeminiConsentGiven));

  OCMExpect([mock_delegate_
      dismissGeminiConsentUIWithCompletion:[OCMArg invokeBlock]]);

  [mediator_ didConsentGemini];

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kIOSBwgConsent));
  EXPECT_TRUE(completion_called_);
  EXPECT_TRUE(completion_success_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests didRefuseGeminiConsent dismisses the flow and calls completion with NO.
TEST_F(GeminiFirstRunMediatorTest, TestDidRefuseGeminiConsent) {
  OCMExpect([mock_delegate_ dismissGeminiFlow]);
  [mediator_ didRefuseGeminiConsent];
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kIOSBwgConsent));
  EXPECT_TRUE(completion_called_);
  EXPECT_FALSE(completion_success_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests didCloseGeminiPromo dismisses the flow and calls completion with NO.
TEST_F(GeminiFirstRunMediatorTest, TestDidCloseGeminiPromo) {
  OCMExpect([mock_delegate_ dismissGeminiFlow]);
  [mediator_ didCloseGeminiPromo];
  EXPECT_TRUE(completion_called_);
  EXPECT_FALSE(completion_success_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests didRefuseLiveOnboarding dismisses consent UI and calls completion with
// NO.
TEST_F(GeminiFirstRunMediatorTest, TestDidRefuseLiveOnboarding) {
  OCMExpect([mock_delegate_
      dismissGeminiConsentUIWithCompletion:[OCMArg invokeBlock]]);
  [mediator_ didRefuseLiveOnboarding];
  EXPECT_TRUE(completion_called_);
  EXPECT_FALSE(completion_success_);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests didTapConsentLinkWithAction: dismisses the flow and opens a URL for
// a valid action, and does nothing for an unknown action.
TEST_F(GeminiFirstRunMediatorTest, TestDidTapConsentLinkWithAction) {
  OCMExpect([mock_delegate_ dismissGeminiFlow]);
  OCMExpect([mock_scene_handler_ openURLInNewTab:[OCMArg isNotNil]]);
  [mediator_ didTapConsentLinkWithAction:kGeminiFirstFootnoteLinkAction];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);

  // Verify unknown action does not dismiss the flow or open a new tab.
  OCMReject([mock_delegate_ dismissGeminiFlow]);
  OCMReject([mock_scene_handler_ openURLInNewTab:[OCMArg any]]);
  [mediator_ didTapConsentLinkWithAction:@"unknownAction"];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
}
