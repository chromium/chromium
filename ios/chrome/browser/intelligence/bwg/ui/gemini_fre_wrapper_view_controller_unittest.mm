// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_fre_wrapper_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller_delegate.h"
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
  BWGFREWrapperViewController* CreateController(bool with_promo,
                                                bool is_account_managed) {
    BWGFREWrapperViewController* view_controller =
        [[BWGFREWrapperViewController alloc] initWithPromo:with_promo
                                          isAccountManaged:with_promo];
    mock_mutator_ = [OCMockObject mockForProtocol:@protocol(BWGConsentMutator)];
    view_controller.mutator = mock_mutator_;
    // Force view initialisation since this view controller is never added into
    // the hierarchy in this unit test.
    [view_controller view];
    promo_view_controller_ = GetPromoViewController(view_controller);
    consent_view_controller_ = GetConsentViewController(view_controller);
    return view_controller;
  }

  BWGPromoViewController* GetPromoViewController(
      BWGFREWrapperViewController* view_controller) {
    for (UIViewController* child in view_controller.childViewControllers) {
      if ([child isKindOfClass:[BWGPromoViewController class]]) {
        return static_cast<BWGPromoViewController*>(child);
      }
    }
    return nil;
  }

  BWGConsentViewController* GetConsentViewController(
      BWGFREWrapperViewController* view_controller) {
    for (UIViewController* child in view_controller.childViewControllers) {
      if ([child isKindOfClass:[BWGConsentViewController class]]) {
        return static_cast<BWGConsentViewController*>(child);
      }
    }
    return nil;
  }

 protected:
  id mock_mutator_;
  BWGPromoViewController* promo_view_controller_ = nil;
  BWGConsentViewController* consent_view_controller_ = nil;
};

// Tests first run for Gemini promo being shown.
TEST_F(GeminiFREWrapperViewControllerTest, FirstRunGeminiPromoShown) {
  BWGFREWrapperViewController* view_controller = CreateController(true, true);
  EXPECT_NE(nil, view_controller);
  EXPECT_NE(nil, promo_view_controller_);
  EXPECT_NE(nil, consent_view_controller_);

  // Promo view shouldn't be hidden. Consent view should be hidden.
  EXPECT_FALSE(promo_view_controller_.view.accessibilityElementsHidden);
  EXPECT_TRUE(consent_view_controller_.view.accessibilityElementsHidden);
}

// Tests nonconsent flow after the (First Run Experience) FRE Gemini promo.
TEST_F(GeminiFREWrapperViewControllerTest, PostFRENonConsentFlow) {
  BWGFREWrapperViewController* view_controller = CreateController(false, true);
  EXPECT_NE(nil, view_controller);
  EXPECT_NE(nil, consent_view_controller_);
  EXPECT_EQ(nil, promo_view_controller_);

  // Promo view should already be hidden. Consent view should be shown.
  EXPECT_FALSE(consent_view_controller_.view.accessibilityElementsHidden);
}

// Tests the flow for continuing after the promo and a user accepting consent.
TEST_F(GeminiFREWrapperViewControllerTest, FullAcceptFlow) {
  BWGFREWrapperViewController* view_controller = CreateController(true, false);
  EXPECT_NE(nil, promo_view_controller_);
  EXPECT_NE(nil, consent_view_controller_);

  // Promo view controller should be shown. Consent view controller should be
  // hidden.
  EXPECT_FALSE(promo_view_controller_.view.accessibilityElementsHidden);
  EXPECT_TRUE(consent_view_controller_.view.accessibilityElementsHidden);

  id<BWGPromoViewControllerDelegate> delegate =
      (id<BWGPromoViewControllerDelegate>)view_controller;

  [delegate didAcceptPromo];

  // Promo view controller should be hidden. Consent view controller should
  // show.
  EXPECT_TRUE(promo_view_controller_.view.accessibilityElementsHidden);
  EXPECT_FALSE(consent_view_controller_.view.accessibilityElementsHidden);
}
