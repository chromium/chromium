// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "features.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::PromoStyleSecondaryActionButtonMatcher;
using chrome_test_util::SettingsMenuPrivacyButton;

// Test Privacy Guide steps.
@interface PrivacyGuideTestCase : ChromeTestCase

@end

@implementation PrivacyGuideTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kPrivacyGuideIos);
  return config;
}

// Test that the Privacy Guide can be dismissed via the 'Cancel' button.
- (void)testDismissPrivacyGuideWithCancelButton {
  [self openPrivacyGuide];

  // Dismiss the Privacy Guide by tapping the 'Cancel' button.
  [[EarlGrey selectElementWithMatcher:PromoStyleSecondaryActionButtonMatcher()]
      performAction:grey_tap()];

  // Verify that the Privacy Guide is dismissed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPrivacyGuideWelcomeViewId)]
      assertWithMatcher:grey_nil()];
}

// Test that the Privacy Guide can be dismissed by swipping down.
- (void)testDismissPrivacyGuideWithSwipeDown {
  [self openPrivacyGuide];

  // Dismiss the Privacy Guide by swipping down.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPrivacyGuideWelcomeViewId)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Verify that the Privacy Guide is dismissed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPrivacyGuideWelcomeViewId)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

- (void)openPrivacyGuide {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ButtonWithAccessibilityLabelId(
                                             IDS_IOS_PRIVACY_GUIDE_TITLE)];
}
@end
