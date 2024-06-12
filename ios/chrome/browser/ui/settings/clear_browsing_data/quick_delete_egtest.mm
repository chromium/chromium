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
using chrome_test_util::ContainsPartialText;
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
- (id<GREYMatcher>)popupCellMenuItemWithTimeRange:(NSString*)timeRange {
  return grey_allOf(
      grey_not(grey_accessibilityID(kQuickDeletePopUpButtonIdentifier)),
      ContextMenuItemWithAccessibilityLabel(timeRange), nil);
}

// Returns a matcher for the actual button with the `timeRange` inside the time
// range popup row.
- (id<GREYMatcher>)popupCellWithTimeRange:(NSString*)timeRange {
  return grey_allOf(grey_accessibilityID(kQuickDeletePopUpButtonIdentifier),
                    grey_text(timeRange), nil);
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
- (void)testTimeRangeForDeletionSelection {
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
              popupCellWithTimeRange:
                  l10n_util::GetNSString(
                      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR)]]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the time range button.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE))]
      performAction:grey_tap()];

  // Tap on the past week option on the popup menu.
  [[EarlGrey
      selectElementWithMatcher:
          [self
              popupCellMenuItemWithTimeRange:
                  l10n_util::GetNSString(
                      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK)]]
      performAction:grey_tap()];

  // Make sure the menu was dismissed after the tap.
  [[EarlGrey
      selectElementWithMatcher:
          [self
              popupCellMenuItemWithTimeRange:
                  l10n_util::GetNSString(
                      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK)]]
      assertWithMatcher:grey_notVisible()];

  // Check that the cell has changed to the correct selection, i.e. is showing
  // the last week time range.
  [[EarlGrey
      selectElementWithMatcher:
          [self
              popupCellWithTimeRange:
                  l10n_util::GetNSString(
                      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK)]]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Confirm that the pref was saved with the new value of last week.
  GREYAssertEqual(
      [ChromeEarlGrey userIntegerPref:browsing_data::prefs::kDeleteTimePeriod],
      static_cast<int>(browsing_data::TimePeriod::LAST_WEEK),
      @"Incorrect local pref value.");
}

// Tests that the number of browsing history items is shown on the browsing data
// row when browsing history is selected as a data type to be deleted.
- (void)testBrowsingHistoryForDeletion {
  // Set pref to select deletion of browsing history.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the browsing history substring are
  // presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // TODO(crbug.com/341097601): Use the actual number of sites that could be
  // deleted for the selected time frame.
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES, 2))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the number of browsing history items is not shown on the browsing
// data row when browsing history is not selected as a data type to be deleted.
- (void)testKeepBrowsingHistory {
  // Set pref to keep browsing history.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteBrowsingHistory];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is present but that the browsing history
  // substring is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // TODO(crbug.com/341097601): Use the actual number of sites that could be
  // deleted for the selected time frame.
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES, 2))]
      assertWithMatcher:grey_nil()];
}

// Tests that cookies are shown as a possible type to be deleted on the browsing
// data row when cookies are selected as a data type for deletion.
- (void)testCookiesForDeletion {
  // Set pref to select deletion of cookies.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteCookies];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the cookie substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetNSString(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITE_DATA))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that cookies are not shown as a possible type to be deleted on the
// browsing data row when cookies are not selected as a data type for deletion.
- (void)testKeepCookies {
  // Set pref to keep cookies.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCookies];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is present but that the cookie substring
  // is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetNSString(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITE_DATA))]
      assertWithMatcher:grey_nil()];
}

// Tests that cache is shown as a possible type to be deleted on the browsing
// data row when cache is selected as a data type for deletion.
- (void)testCacheForDeletion {
  // Set pref to select deletion of cache.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteCache];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the cached substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetNSString(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_CACHED_FILES))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that cache is not shown as a possible type to be deleted on the
// browsing data row when cache is not selected as a data type for deletion.
- (void)testKeepCache {
  // Set pref to keep cache.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteCache];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is presented but that the cached substring
  // is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetNSString(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_CACHED_FILES))]
      assertWithMatcher:grey_nil()];
}

// Tests that the number of passwords is shown on the browsing data row if
// passwords is selected as a data type to be deleted.
- (void)testPasswordsForDeletion {
  // Set pref to select deletion of passwords.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeletePasswords];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the passwords substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // TODO(crbug.com/341097601): Use the actual number of passwords that could
  // be deleted for the selected time frame.
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the number of passwords is not shown on the browsing data row if
// passwords is not selected as a data type to be deleted.
- (void)testKeepPasswords {
  // Set pref to keep passwords.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeletePasswords];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is present but that the passwords
  // substring is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // TODO(crbug.com/341097601): Use the actual number of passwords that could
  // be deleted for the selected time frame.
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS, 1))]
      assertWithMatcher:grey_nil()];
}

// Tests that the number of form data items is shown on the browsing data row if
// form data is selected as a data type to be deleted.
- (void)testFormDataForDeletion {
  // Set pref to select deletion of form data.
  [ChromeEarlGrey setBoolValue:true
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row and the form data substring are presented.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // TODO(crbug.com/341097601): Use the actual number of form data that could
  // be deleted for the selected time frame.
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_AUTOFILL_DATA, 5))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the number of form data items is not shown on the browsing data
// row if form data is not selected as a data type to be deleted.
- (void)testKeepFormData {
  // Set pref to keep form data.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  [self openQuickDeleteFromThreeDotMenu];

  // Check that Quick Delete is presented.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Check that the browsing data row is presented but that the form data
  // substring is not.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_DELETE_BROWSING_DATA_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // TODO(crbug.com/341097601): Use the actual number of form data that could
  // be deleted for the selected time frame.
  [[EarlGrey selectElementWithMatcher:
                 ContainsPartialText(l10n_util::GetPluralNSStringF(
                     IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_AUTOFILL_DATA, 5))]
      assertWithMatcher:grey_nil()];
}

@end
