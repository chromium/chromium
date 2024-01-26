// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/unified_consent/pref_names.h"
#import "features.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"

namespace {

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::PromoStylePrimaryActionButtonMatcher;
using chrome_test_util::PromoStyleSecondaryActionButtonMatcher;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::TableViewSwitchCell;
using chrome_test_util::TurnTableViewSwitchOn;
using unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled;

// Matcher for the navigation bar "Back" button on the Privacy Guide.
id<GREYMatcher> PrivacyGuideBackButton() {
  return grey_allOf(
      testing::NavigationBarBackButton(),
      grey_ancestor(grey_accessibilityID(kPrivacyGuideNavigationBarViewID)),
      nil);
}

// Matcher for the URL usage switch.
id<GREYMatcher> PrivacyGuideURLUsageSwitch(BOOL is_on) {
  return TableViewSwitchCell(kPrivacyGuideURLUsageSwitchID, is_on);
}

}  // namespace

// Test Privacy Guide steps.
@interface PrivacyGuideTestCase : ChromeTestCase

@end

@implementation PrivacyGuideTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kPrivacyGuideIos);
  return config;
}

// Tests that the Privacy Guide can be dismissed via the 'Cancel' button.
- (void)testDismissPrivacyGuideWithCancelButton {
  [self openPrivacyGuide];

  // Dismiss the Privacy Guide by tapping the 'Cancel' button.
  [[EarlGrey selectElementWithMatcher:PromoStyleSecondaryActionButtonMatcher()]
      performAction:grey_tap()];

  // Verify that the Privacy Guide is dismissed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPrivacyGuideWelcomeViewID)]
      assertWithMatcher:grey_nil()];
}

// Tests that the Privacy Guide can be dismissed by swipping down.
- (void)testDismissPrivacyGuideWithSwipeDown {
  [self openPrivacyGuide];

  // Dismiss the Privacy Guide by swipping down.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPrivacyGuideWelcomeViewID)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Verify that the Privacy Guide is dismissed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPrivacyGuideWelcomeViewID)]
      assertWithMatcher:grey_nil()];
}

// Test the e2e navigation of the Privacy Guide.
- (void)testForwardAndBackwardNavigation {
  [self openPrivacyGuide];

  // 1. Test forward navigation.
  // Tap the 'Let's go' button.
  [[EarlGrey selectElementWithMatcher:PromoStylePrimaryActionButtonMatcher()]
      performAction:grey_tap()];

  // Verify that the next step is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacyGuideURLUsageViewID)]
      assertWithMatcher:grey_notNil()];

  // Tap the 'Next' button.
  [[EarlGrey selectElementWithMatcher:PromoStylePrimaryActionButtonMatcher()]
      performAction:grey_tap()];

  // Verify that the next step is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacyGuideHistorySyncViewID)]
      assertWithMatcher:grey_notNil()];

  // Tap the 'Next' button.
  [[EarlGrey selectElementWithMatcher:PromoStylePrimaryActionButtonMatcher()]
      performAction:grey_tap()];

  // Verify that the next step is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacyGuideSafeBrowsingViewID)]
      assertWithMatcher:grey_notNil()];

  // 2. Test backward navigation.
  // Tap the 'Back' button.
  [[EarlGrey selectElementWithMatcher:PrivacyGuideBackButton()]
      performAction:grey_tap()];

  // Verify that the previous step is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacyGuideHistorySyncViewID)]
      assertWithMatcher:grey_notNil()];

  // Tap the 'Back' button.
  [[EarlGrey selectElementWithMatcher:PrivacyGuideBackButton()]
      performAction:grey_tap()];

  // Verify that the previous step is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacyGuideURLUsageViewID)]
      assertWithMatcher:grey_notNil()];

  // Tap the 'Back' button.
  [[EarlGrey selectElementWithMatcher:PrivacyGuideBackButton()]
      performAction:grey_tap()];

  // Verify that the previous step is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPrivacyGuideWelcomeViewID)]
      assertWithMatcher:grey_notNil()];
}

// Test the URL usage switch and pref interaction.
- (void)testURLUsageSwitch {
  // Set URL usage pref to NO.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:kUrlKeyedAnonymizedDataCollectionEnabled];

  // Open the Privacy Guide and go to the URL usage step.
  [self openPrivacyGuide];
  [[EarlGrey selectElementWithMatcher:PromoStylePrimaryActionButtonMatcher()]
      performAction:grey_tap()];

  // 1. Test initialization and switch tapping.
  // Verify that the switch is OFF and turn in ON.
  [[EarlGrey selectElementWithMatcher:PrivacyGuideURLUsageSwitch(NO)]
      performAction:TurnTableViewSwitchOn(YES)];

  // Verify that the pref is set to YES.
  GREYAssertTrue(
      [ChromeEarlGrey userBooleanPref:kUrlKeyedAnonymizedDataCollectionEnabled],
      @"Incorrect URL usage pref value.");

  // 2. Test async pref changes.
  // Set URL usage pref to NO.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:kUrlKeyedAnonymizedDataCollectionEnabled];

  // Verify that the switch is turned OFF.
  [[EarlGrey selectElementWithMatcher:PrivacyGuideURLUsageSwitch(NO)]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Helpers

- (void)openPrivacyGuide {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ButtonWithAccessibilityLabelId(
                                             IDS_IOS_PRIVACY_GUIDE_TITLE)];
}

@end
