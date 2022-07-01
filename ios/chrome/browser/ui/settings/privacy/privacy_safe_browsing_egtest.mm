// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using l10n_util::GetNSString;

// Integration tests using the Privacy Safe Browsing settings screen.
@interface PrivacySafeBrowsingTestCase : ChromeTestCase
@end

@implementation PrivacySafeBrowsingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(safe_browsing::kEnhancedProtection);
  return config;
}

- (void)testOpenPrivacySafeBrowsingSettings {
  [self openPrivacySafeBrowsingSettings];
}

// TODO(crbug.com/1333625): Enable once activation point is fixed.
- (void)DISABLED_testEachSafeBrowsingOption {
  [self openPrivacySafeBrowsingSettings];

  // Presses each of the Safe Browsing options.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kSettingsSafeBrowsingEnhancedProtectionCellId)]
      performAction:grey_tap()];
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                 @"Failed to toggle-on Enhanced Safe Browsing");

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kSettingsSafeBrowsingStandardProtectionCellId)]
      performAction:grey_tap()];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                  @"Failed to toggle-off Enhanced Safe Browsing");
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to toggle-on Standard Safe Browsing");

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsSafeBrowsingNoProtectionCellId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(GetNSString(IDS_CANCEL))]
      performAction:grey_tap()];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                  @"Failed to keep Enhanced Safe Browsing off");
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to keep Standard Safe Browsing on");

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsSafeBrowsingNoProtectionCellId)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_buttonTitle(GetNSString(
              IDS_IOS_SAFE_BROWSING_NO_PROTECTION_CONFIRMATION_DIALOG_CONFIRM))]
      performAction:grey_tap()];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                  @"Failed to toggle-off Standard Safe Browsing");
}

- (void)testPrivacySafeBrowsingDoneButton {
  [self openPrivacySafeBrowsingSettings];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

- (void)testPrivacySafeBrowsingSwipeDown {
  [self openPrivacySafeBrowsingSettings];

  // Check that ESB is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacySafeBrowsingTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacySafeBrowsingTableViewId)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacySafeBrowsingTableViewId)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

- (void)openPrivacySafeBrowsingSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:ButtonWithAccessibilityLabelId(
                               IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE)];
}

@end
