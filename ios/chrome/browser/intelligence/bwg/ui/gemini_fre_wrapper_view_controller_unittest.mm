// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_fre_wrapper_view_controller.h"

#import "base/run_loop.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_promo_view_controller.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

// Test fixture for GeminiFREWrapperViewController.
class GeminiFREWrapperViewControllerTest : public PlatformTest {
 public:
  GeminiFREWrapperViewController* CreateController(
      bool with_promo,
      bool is_account_managed,
      bool use_strict_consent = false) {
    GeminiFREWrapperViewController* view_controller =
        [[GeminiFREWrapperViewController alloc]
                    initWithPromo:with_promo
                 isAccountManaged:is_account_managed
            useStrictLegalConsent:use_strict_consent
                          FREType:GeminiFREType::kNewUser
                          country:@"us"];
    mock_mutator_ =
        [OCMockObject mockForProtocol:@protocol(GeminiConsentMutator)];
    [[[mock_mutator_ stub] andReturnValue:@NO] shouldShowImageRemixRow];
    view_controller.mutator = mock_mutator_;
    // Force view initialisation since this view controller is never added into
    // the hierarchy in this unit test.
    [view_controller view];
    promo_view_controller_ = GetPromoViewController(view_controller);
    consent_view_controller_ = GetConsentViewController(view_controller);
    return view_controller;
  }

  GeminiPromoViewController* GetPromoViewController(
      GeminiFREWrapperViewController* view_controller) {
    for (UIViewController* child in view_controller.childViewControllers) {
      if ([child isKindOfClass:[GeminiPromoViewController class]]) {
        return static_cast<GeminiPromoViewController*>(child);
      }
    }
    return nil;
  }

  GeminiConsentViewController* GetConsentViewController(
      GeminiFREWrapperViewController* view_controller) {
    for (UIViewController* child in view_controller.childViewControllers) {
      if ([child isKindOfClass:[GeminiConsentViewController class]]) {
        return static_cast<GeminiConsentViewController*>(child);
      }
    }
    return nil;
  }

  void PrimaryAction(GeminiFREWrapperViewController* view_controller) {
    id<ButtonStackActionDelegate> actionDelegate =
        (id<ButtonStackActionDelegate>)view_controller;
    [actionDelegate didTapPrimaryActionButton];
  }

  void SecondaryAction(GeminiFREWrapperViewController* view_controller) {
    id<ButtonStackActionDelegate> actionDelegate =
        (id<ButtonStackActionDelegate>)view_controller;
    [actionDelegate didTapSecondaryActionButton];
  }

  void StubMutatorActions() {
    [[mock_mutator_ stub] didConsentGemini];
    [[mock_mutator_ stub] didRefuseGeminiConsent];
    [[mock_mutator_ stub] didCloseGeminiPromo];
  }

 protected:
  id mock_mutator_;
  GeminiPromoViewController* promo_view_controller_ = nil;
  GeminiConsentViewController* consent_view_controller_ = nil;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  void SetUp() override {
    PlatformTest::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
};

// Tests first run for Gemini promo being shown.
TEST_F(GeminiFREWrapperViewControllerTest, FirstRunGeminiPromoShown) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(true, true);
  EXPECT_NE(nil, view_controller);
  EXPECT_NE(nil, promo_view_controller_);
  EXPECT_NE(nil, consent_view_controller_);

  // Promo view shouldn't be hidden. Consent view should be hidden.
  EXPECT_FALSE(promo_view_controller_.view.accessibilityElementsHidden);
  EXPECT_TRUE(consent_view_controller_.view.accessibilityElementsHidden);
}

// Tests nonconsent flow after the (First Run Experience) FRE Gemini promo.
TEST_F(GeminiFREWrapperViewControllerTest, PostFRENonConsentFlow) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(false, true);
  EXPECT_NE(nil, view_controller);
  EXPECT_NE(nil, consent_view_controller_);
  EXPECT_EQ(nil, promo_view_controller_);

  // Promo view should already be hidden. Consent view should be shown.
  EXPECT_FALSE(consent_view_controller_.view.accessibilityElementsHidden);
}

// Tests the flow for continuing after the promo and a user accepting consent.
TEST_F(GeminiFREWrapperViewControllerTest, FullAcceptFlow) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(true, false);
  EXPECT_NE(nil, promo_view_controller_);
  EXPECT_NE(nil, consent_view_controller_);

  // Promo view controller should be shown and consent view controller hidden.
  EXPECT_FALSE(promo_view_controller_.view.accessibilityElementsHidden);
  EXPECT_TRUE(consent_view_controller_.view.accessibilityElementsHidden);

  PrimaryAction(view_controller);

  // Promo view controller should be hidden. Consent view controller should
  // show.
  EXPECT_TRUE(promo_view_controller_.view.accessibilityElementsHidden);
  EXPECT_FALSE(consent_view_controller_.view.accessibilityElementsHidden);
}

// Tests that tapping the primary button records metrics for the consent.
TEST_F(GeminiFREWrapperViewControllerTest, ConsentPrimaryButtonRecordsMetrics) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(false, false);

  StubMutatorActions();
  PrimaryAction(view_controller);
  histogram_tester_->ExpectUniqueSample(
      kConsentActionHistogram, static_cast<int>(IOSGeminiFREAction::kAccept),
      1);
}

// Tests that tapping the primary button calls the right mutator function while
//  on the consent.
TEST_F(GeminiFREWrapperViewControllerTest, ConsentPrimaryButtonCallsMutator) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(false, false);

  OCMExpect([mock_mutator_ didConsentGemini]);
  PrimaryAction(view_controller);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that tapping the secondary button records metrics for the consent.
TEST_F(GeminiFREWrapperViewControllerTest,
       ConsentSecondaryButtonRecordsMetrics) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(false, false);

  StubMutatorActions();
  SecondaryAction(view_controller);
  histogram_tester_->ExpectUniqueSample(
      kConsentActionHistogram, static_cast<int>(IOSGeminiFREAction::kDismiss),
      1);
}

// Tests that tapping the secondary button calls the right mutator function
// while on the consent.
TEST_F(GeminiFREWrapperViewControllerTest, ConsentSecondaryButtonCallsMutator) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(false, false);

  OCMExpect([mock_mutator_ didRefuseGeminiConsent]);
  SecondaryAction(view_controller);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that tapping the primary button records metrics for the promo.
TEST_F(GeminiFREWrapperViewControllerTest, PromoPrimaryButtonRecordsMetrics) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(true, false);

  StubMutatorActions();
  PrimaryAction(view_controller);
  histogram_tester_->ExpectUniqueSample(
      kPromoActionHistogram, static_cast<int>(IOSGeminiFREAction::kAccept), 1);
}

// Tests that tapping the secondary button records metrics for the promo.
TEST_F(GeminiFREWrapperViewControllerTest, PromoSecondaryButtonRecordsMetrics) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(true, false);

  StubMutatorActions();
  SecondaryAction(view_controller);
  histogram_tester_->ExpectUniqueSample(
      kPromoActionHistogram, static_cast<int>(IOSGeminiFREAction::kDismiss), 1);
}

// Tests that tapping the secondary button calls the right mutator function
// while on the promo.
TEST_F(GeminiFREWrapperViewControllerTest, PromoSecondaryButtonCallsMutator) {
  GeminiFREWrapperViewController* view_controller =
      CreateController(true, false);

  OCMExpect([mock_mutator_ didCloseGeminiPromo]);
  SecondaryAction(view_controller);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}
