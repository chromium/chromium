// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

id<GREYMatcher> TitleMatcher() {
  return grey_accessibilityID(kPromoStyleTitleAccessibilityIdentifier);
}

id<GREYMatcher> SubtitleMatcher() {
  return grey_accessibilityID(kPromoStyleSubtitleAccessibilityIdentifier);
}

id<GREYMatcher> PrimaryActionButtonMatcher() {
  return grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier);
}

id<GREYMatcher> SecondaryActionButtonMatcher() {
  return grey_accessibilityID(
      kPromoStyleSecondaryActionAccessibilityIdentifier);
}

id<GREYMatcher> TertiaryActionButtonMatcher() {
  return grey_accessibilityID(kPromoStyleTertiaryActionAccessibilityIdentifier);
}

}  // namespace

// Tests for the Content Notification fullscreen promo view controller.
@interface SCContentNotificationFullscreenPromoTestCase : ShowcaseTestCase
@end

@implementation SCContentNotificationFullscreenPromoTestCase

// Tests the correct elements are visible in the fullscreen modal.
- (void)testFullscreenModal {
  showcase_utils::Open(@"SCContentNotificationPromoViewController");
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:SubtitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionButtonMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:SecondaryActionButtonMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:TertiaryActionButtonMatcher()]
      assertWithMatcher:grey_interactable()];

  showcase_utils::Close();
}

@end
