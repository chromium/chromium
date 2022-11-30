// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

id<GREYMatcher> TitleMatcher() {
  return grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier);
}

id<GREYMatcher> SubtitleMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertSubtitleAccessibilityIdentifier);
}

id<GREYMatcher> PrimaryActionButtonMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertPrimaryActionAccessibilityIdentifier);
}

id<GREYMatcher> SecondaryActionButtonMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertSecondaryActionAccessibilityIdentifier);
}

}  // namespace

// Tests for the default browser fullscreen promo view controller.
@interface SCDefaultBrowserFullscreenPromoTestCase : ShowcaseTestCase
@end

@implementation SCDefaultBrowserFullscreenPromoTestCase

// Tests the correct elements are visible in the fullscreen modal.
- (void)testFullscreenModal {
  showcase_utils::Open(@"DefaultBrowserPromoViewController");
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:SubtitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionButtonMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:SecondaryActionButtonMatcher()]
      assertWithMatcher:grey_interactable()];

  showcase_utils::Close();
}

@end
