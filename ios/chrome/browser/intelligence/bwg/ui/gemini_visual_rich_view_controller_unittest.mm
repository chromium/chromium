// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_visual_rich_view_controller.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

// Test fixture for GeminiVisualRichViewController.
class GeminiVisualRichViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mock_mutator_ = OCMProtocolMock(@protocol(GeminiFirstRunMutator));
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    view_controller_ = nil;
    mock_mutator_ = nil;
    PlatformTest::TearDown();
  }

  GeminiVisualRichViewController* CreateViewController(
      BOOL is_account_managed = NO,
      BOOL use_strict_consent = NO,
      NSString* country = @"us") {
    GeminiConsentConfiguration* config = [GeminiConsentConfiguration
        configurationForManaged:is_account_managed
                         strict:use_strict_consent
                           type:GeminiFirstRunType::kNewUser
                        country:country];
    GeminiVisualRichViewController* controller =
        [[GeminiVisualRichViewController alloc] initWithConfiguration:config];
    controller.mutator = mock_mutator_;
    // Trigger view loading and layout.
    [controller view];
    [controller viewWillLayoutSubviews];
    return controller;
  }

  web::WebTaskEnvironment task_environment_;
  GeminiVisualRichViewController* view_controller_;
  id mock_mutator_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests initialization with a non-managed account.
TEST_F(GeminiVisualRichViewControllerTest, TestInitialization) {
  view_controller_ = CreateViewController(NO);

  EXPECT_NE(nil, view_controller_);
  EXPECT_TRUE(view_controller_.view);
  EXPECT_EQ(view_controller_.stepIdentifier,
            GeminiFirstRunStepIdentifier::kVisualRich);
  EXPECT_TRUE([view_controller_ shouldUseFullscreenPresentation]);
  EXPECT_GT([view_controller_ contentHeight], 0.0);
}

// Tests initialization with a managed account.
TEST_F(GeminiVisualRichViewControllerTest, InitializationWithManagedAccount) {
  view_controller_ = CreateViewController(YES);

  EXPECT_NE(nil, view_controller_);
  EXPECT_TRUE(view_controller_.view);
  EXPECT_EQ(view_controller_.stepIdentifier,
            GeminiFirstRunStepIdentifier::kVisualRich);
  EXPECT_TRUE([view_controller_ shouldUseFullscreenPresentation]);
  EXPECT_GT([view_controller_ contentHeight], 0.0);
}

// Tests button configuration with standard (non-strict) consent.
TEST_F(GeminiVisualRichViewControllerTest, ButtonStackConfigurationDefault) {
  view_controller_ = CreateViewController(NO, NO);

  ButtonStackConfiguration* button_config =
      [view_controller_ buttonStackConfiguration];
  EXPECT_NSEQ(button_config.primaryActionString,
              l10n_util::GetNSString(IDS_IOS_BWG_VISUAL_RICH_PRIMARY_BUTTON));
  EXPECT_NSEQ(button_config.secondaryActionString,
              l10n_util::GetNSString(IDS_CANCEL));
}

// Tests button configuration with strict consent.
TEST_F(GeminiVisualRichViewControllerTest, ButtonStackConfigurationStrict) {
  view_controller_ = CreateViewController(NO, YES);

  ButtonStackConfiguration* button_config =
      [view_controller_ buttonStackConfiguration];
  EXPECT_NSEQ(button_config.primaryActionString,
              l10n_util::GetNSString(IDS_IOS_BWG_VISUAL_RICH_PRIMARY_BUTTON));
  EXPECT_NSEQ(button_config.secondaryActionString,
              l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON));
}

// Tests that tapping the primary button records metrics and calls mutator.
TEST_F(GeminiVisualRichViewControllerTest,
       PrimaryButtonRecordsMetricsAndCallsMutator) {
  view_controller_ = CreateViewController();

  OCMExpect([mock_mutator_ didConsentGemini]);
  [view_controller_ didTapPrimaryButton];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);

  histogram_tester_->ExpectUniqueSample(
      kFirstRunConsentActionHistogram,
      static_cast<int>(IOSGeminiFirstRunAction::kAccept), 1);
}

// Tests that tapping the secondary button records metrics and calls mutator.
TEST_F(GeminiVisualRichViewControllerTest,
       SecondaryButtonRecordsMetricsAndCallsMutator) {
  view_controller_ = CreateViewController();

  OCMExpect([mock_mutator_ didRefuseGeminiConsent]);
  [view_controller_ didTapSecondaryButton];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);

  histogram_tester_->ExpectUniqueSample(
      kFirstRunConsentActionHistogram,
      static_cast<int>(IOSGeminiFirstRunAction::kDismiss), 1);
}
