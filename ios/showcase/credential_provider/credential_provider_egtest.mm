// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

id<GREYMatcher> ConfirmationAlertTitleMatcher() {
  return grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier);
}

id<GREYMatcher> ConfirmationAlertSubtitleMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertSubtitleAccessibilityIdentifier);
}

id<GREYMatcher> ConfirmationAlertPrimaryActionButtonMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertPrimaryActionAccessibilityIdentifier);
}

id<GREYMatcher> ConfirmationAlertMoreInfoButtonMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertMoreInfoAccessibilityIdentifier);
}

id<GREYMatcher> PromoStyleTitleMatcher() {
  return grey_accessibilityID(kPromoStyleTitleAccessibilityIdentifier);
}

id<GREYMatcher> PromoStyleSubtitleMatcher() {
  return grey_accessibilityID(kPromoStyleSubtitleAccessibilityIdentifier);
}

id<GREYMatcher> PromoStylePrimaryActionButtonMatcher() {
  return grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier);
}

id<GREYMatcher> PromoStyleMoreInfoButtonMatcher() {
  return grey_accessibilityID(
      kPromoStyleLearnMoreActionAccessibilityIdentifier);
}

}  // namespace

// Tests for the suggestions view controller.
@interface SCCredentialProviderTestCase : ShowcaseTestCase
@end

@implementation SCCredentialProviderTestCase

// Tests ConsentViewController.
- (void)testConsentScreen {
  showcase_utils::Open(@"ConsentViewController");
  [[EarlGrey selectElementWithMatcher:PromoStyleTitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:PromoStyleSubtitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:PromoStylePrimaryActionButtonMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:PromoStyleMoreInfoButtonMatcher()]
      assertWithMatcher:grey_interactable()];

  showcase_utils::Close();
}

// Tests ConsentViewController.
- (void)testEmptyCredentialsScreen {
  showcase_utils::Open(@"EmptyCredentialsViewController");
  [[EarlGrey selectElementWithMatcher:ConfirmationAlertTitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:ConfirmationAlertSubtitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey
      selectElementWithMatcher:ConfirmationAlertPrimaryActionButtonMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ConfirmationAlertMoreInfoButtonMatcher()]
      assertWithMatcher:grey_nil()];

  showcase_utils::Close();
}

// Tests ConsentViewController.
- (void)testStaleCredentialsScreen {
  showcase_utils::Open(@"StaleCredentialsViewController");
  [[EarlGrey selectElementWithMatcher:ConfirmationAlertTitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:ConfirmationAlertSubtitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey
      selectElementWithMatcher:ConfirmationAlertPrimaryActionButtonMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ConfirmationAlertMoreInfoButtonMatcher()]
      assertWithMatcher:grey_nil()];

  showcase_utils::Close();
}

@end
