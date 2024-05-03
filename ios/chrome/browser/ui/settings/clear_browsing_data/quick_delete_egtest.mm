// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::SettingsMenuPrivacyButton;

}  // namespace

// Tests the Quick Delete UI, the new version of Delete Browsing Data.
@interface QuickDeleteTestCase : ChromeTestCase
@end

@implementation QuickDeleteTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(kIOSQuickDelete);
  return config;
}

// Opens Quick Delete from the Privacy page in Settings.
- (void)openQuickDeleteFromPrivacySettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];

  [ChromeEarlGreyUI
      tapPrivacyMenuButton:ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                               IDS_IOS_CLEAR_BROWSING_DATA_TITLE))];
}

// Opens Quick Delete from the three dot menu.
- (void)openQuickDeleteFromThreeDotMenu {
  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_CLEAR_BROWSING_DATA_TITLE))]
      performAction:grey_tap()];
}

// Returns a matcher for the title of the Quick Delete bottom sheet.
- (id<GREYMatcher>)quickDeleteTitle {
  return grey_allOf(
      grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)),
      nil);
}

// Tests if the Quick Delete UI is shown correctly from Privacy settings.
- (void)testOpenAndDismissQuickDeleteFromPrivacySettings {
  [self openQuickDeleteFromPrivacySettings];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Swipe the bottom sheet down.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Quick Delete has been dismissed.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_nil()];

  // Check that the privacy table is in view.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SETTINGS_PRIVACY_TITLE))]
      assertWithMatcher:grey_notNil()];
}

// Tests if the Quick Delete UI is shown correctly from the three dot menu entry
// point.
- (void)testOpenAndDismissQuickDeleteFromThreeDotMenu {
  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Swipe the bottom sheet down.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Quick Delete has been dismissed.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_nil()];
}

// TODO(crbug.com/335387869): Also test opening Quick Delete from the History
// page, once that path is implemented.

@end
