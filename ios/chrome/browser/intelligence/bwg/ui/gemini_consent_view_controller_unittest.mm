// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

// Test fixture for GeminiConsentViewController.
class GeminiConsentViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mock_mutator_ = OCMProtocolMock(@protocol(GeminiConsentMutator));
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    mock_mutator_ = nil;
    PlatformTest::TearDown();
  }

  GeminiConsentViewController* CreateViewController(
      BOOL is_account_managed,
      NSString* country = nil,
      BOOL use_strict_consent = NO) {
    GeminiConsentConfiguration* config = [GeminiConsentConfiguration
        configurationForManaged:is_account_managed
                         strict:use_strict_consent
                           type:GeminiFirstRunType::kNewUser
                        country:country];
    GeminiConsentViewController* controller =
        [[GeminiConsentViewController alloc] initWithConfiguration:config];
    controller.mutator = mock_mutator_;
    // Force view initialization since this view controller is never added into
    // the hierarchy in this unit test.
    [controller view];
    [controller viewWillLayoutSubviews];
    return controller;
  }

  id mock_mutator_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests initialization with a managed account.
TEST_F(GeminiConsentViewControllerTest, InitializationWithManagedAccount) {
  GeminiConsentViewController* view_controller =
      CreateViewController(YES, @"us");

  EXPECT_NE(nil, view_controller);
  EXPECT_TRUE(view_controller.view);
  EXPECT_TRUE(view_controller.navigationItem.hidesBackButton);
}

// Tests initialization with a non-managed account.
TEST_F(GeminiConsentViewControllerTest, InitializationWithNonManagedAccount) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"us");

  EXPECT_NE(nil, view_controller);
  EXPECT_TRUE(view_controller.view);
  EXPECT_TRUE(view_controller.navigationItem.hidesBackButton);
}

// Tests that tapping the primary button records metrics.
TEST_F(GeminiConsentViewControllerTest, PrimaryButtonRecordsMetrics) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"us");
  [view_controller didTapPrimaryButton];
  histogram_tester_->ExpectUniqueSample(
      kFirstRunConsentActionHistogram,
      static_cast<int>(IOSGeminiFirstRunAction::kAccept), 1);
}

// Tests that tapping the primary button calls the right mutator function.
TEST_F(GeminiConsentViewControllerTest, PrimaryButtonCallsMutator) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"us");
  OCMExpect([mock_mutator_ didConsentGemini]);
  [view_controller didTapPrimaryButton];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that tapping the secondary button records metrics.
TEST_F(GeminiConsentViewControllerTest, SecondaryButtonRecordsMetrics) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"us");
  [view_controller didTapSecondaryButton];
  histogram_tester_->ExpectUniqueSample(
      kFirstRunConsentActionHistogram,
      static_cast<int>(IOSGeminiFirstRunAction::kDismiss), 1);
}

// Tests that tapping the secondary button calls the right mutator function.
TEST_F(GeminiConsentViewControllerTest, SecondaryButtonCallsMutator) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"us");
  OCMExpect([mock_mutator_ didRefuseGeminiConsent]);
  [view_controller didTapSecondaryButton];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that buttonStackConfiguration returns the appropriate primary button
// string for standard and strict consent.
TEST_F(GeminiConsentViewControllerTest, ButtonStackConfigurationStrings) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGeminiUpdatedConsent);

  GeminiConsentViewController* normal_controller = CreateViewController(
      /*is_account_managed=*/NO, @"us", /*use_strict_consent=*/NO);
  ButtonStackConfiguration* normal_config =
      [normal_controller buttonStackConfiguration];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_PRIMARY_BUTTON),
              normal_config.primaryActionString);

  GeminiConsentViewController* u18_controller = CreateViewController(
      /*is_account_managed=*/NO, @"us", /*use_strict_consent=*/YES);
  ButtonStackConfiguration* u18_config =
      [u18_controller buttonStackConfiguration];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_PRIMARY_BUTTON_STRICT),
      u18_config.primaryActionString);
}
