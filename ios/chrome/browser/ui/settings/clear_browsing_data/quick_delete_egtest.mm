// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
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

// Returns a matcher for the row with the `timeRange` on the popup menu.
- (id<GREYMatcher>)popoverCellMenuItemWithTimeRange:(NSString*)timeRange {
  return grey_allOf(
      grey_not(grey_accessibilityID(kQuickDeletePopUpButtonIdentifier)),
      ContextMenuItemWithAccessibilityLabel(timeRange), nil);
}

// Returns a matcher for the actual button with the `timeRange` inside the time
// range popup row.
- (id<GREYMatcher>)popupCellButtonWithTimeRange:(NSString*)timeRange {
  return grey_allOf(grey_accessibilityID(kQuickDeletePopUpButtonIdentifier),
                    ContextMenuItemWithAccessibilityLabel(timeRange), nil);
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

// Tests the selection time range for the browsing data deletion: the time range
// selection is shown with the pref value and a new selection updates the pref.
// TODO(crbug.com/341306396): Reenable after fix.
- (void)DISALBED_testTimeRangeForDeletionSelection {
  // Set pref to the last hour.
  [ChromeEarlGrey
      setIntegerValue:static_cast<int>(browsing_data::TimePeriod::LAST_HOUR)
          forUserPref:browsing_data::prefs::kDeleteTimePeriod];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the time range row is presented and the correct time range, last
  // hour, is selected.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          [self
              popupCellButtonWithTimeRange:
                  l10n_util::GetNSString(
                      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR)]]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the time range button.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR))]
      performAction:grey_tap()];

  // Tap on the past week option on the popup menu.
  [[EarlGrey
      selectElementWithMatcher:
          [self
              popoverCellMenuItemWithTimeRange:
                  l10n_util::GetNSString(
                      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK)]]
      performAction:grey_tap()];

  // Make sure the menu was dismissed after the tap.
  [[EarlGrey
      selectElementWithMatcher:
          [self
              popoverCellMenuItemWithTimeRange:
                  l10n_util::GetNSString(
                      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK)]]
      assertWithMatcher:grey_notVisible()];

  // Check that the cell has changed to the correct selection, i.e. is showing
  // the last week time range.
  [[EarlGrey
      selectElementWithMatcher:
          [self
              popupCellButtonWithTimeRange:
                  l10n_util::GetNSString(
                      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK)]]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Confirm that the pref was saved with the new value of last week.
  GREYAssertEqual(
      [ChromeEarlGrey userIntegerPref:browsing_data::prefs::kDeleteTimePeriod],
      static_cast<int>(browsing_data::TimePeriod::LAST_WEEK),
      @"Incorrect local pref value.");
}

@end
