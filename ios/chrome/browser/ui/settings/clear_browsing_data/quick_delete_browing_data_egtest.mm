// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "components/browsing_data/core/pref_names.h"
#import "components/sync/base/command_line_switches.h"
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

}  // namespace

// Tests the Quick Delete Browsing Data page.
@interface QuickDeleteBrowsingDataTestCase : ChromeTestCase
@end

@implementation QuickDeleteBrowsingDataTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey resetBrowsingDataPrefs];
}

- (void)tearDown {
  [ChromeEarlGrey resetBrowsingDataPrefs];
  // Dismiss the quick delete bottom sheet to avoid flakiness due to UI
  // persisting accross tests.
  [self dismissQuickDelete];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(kIOSQuickDelete);
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);
  return config;
}

// Returns a matcher for the title of the Quick Delete bottom sheet.
- (id<GREYMatcher>)quickDeleteTitle {
  return grey_allOf(
      grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)),
      nil);
}

// Returns a matcher for the Quick Delete Browsing Data button on the main page.
- (id<GREYMatcher>)quickDeleteBrowsingDataButton {
  return grey_accessibilityID(kQuickDeleteBrowsingDataButtonIdentifier);
}

// Returns a matcher for the title of the Quick Delete Browsing Data page.
- (id<GREYMatcher>)quickDeleteBrowsingDataPageTitle {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
      IDS_IOS_DELETE_BROWSING_DATA_TITLE);
}

// Returns a matcher for the confirm button on the navigation bar.
- (id<GREYMatcher>)navigationBarConfirmButton {
  return grey_accessibilityID(kQuickDeleteBrowsingDataConfirmButtonIdentifier);
}

// Returns a matcher for the autofill cell.
- (id<GREYMatcher>)autofillCell {
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataAutofillIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Returns matcher for an element with or without the
// UIAccessibilityTraitSelected accessibility trait depending on `selected`.
- (id<GREYMatcher>)elementIsSelected:(BOOL)selected {
  return selected
             ? grey_accessibilityTrait(UIAccessibilityTraitSelected)
             : grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected));
}

// Opens Quick Delete browsing data page.
- (void)openQuickDeleteBrowsingDataPage {
  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_CLEAR_BROWSING_DATA_TITLE))]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:[self quickDeleteBrowsingDataButton]]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      [self quickDeleteBrowsingDataPageTitle]];
}

// Dismisses Quick Delete bottom sheet.
- (void)dismissQuickDelete {
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

// Tests the cancel button dismisses the browsing data page.
- (void)testPageNavigationCancelButton {
  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Tap cancel button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Ensure the browsing data page is closed while quick delete bottom sheet is
  // still open.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteBrowsingDataPageTitle]]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];
}

// Tests the confirm button dismisses the browsing data page.
- (void)testPageNavigationConfirmButton {
  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Tap confirm button.
  [[EarlGrey selectElementWithMatcher:[self navigationBarConfirmButton]]
      performAction:grey_tap()];

  // Ensure the page is closed while quick delete bottom sheet is still open.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteBrowsingDataPageTitle]]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];
}

// Tests the cancel button does not save changes to prefs.
- (void)testCancelButtonDoesNotUpdatePrefs {
  // Set autofill pref to false.
  [ChromeEarlGrey setBoolValue:(BOOL)NO
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Assert autofill row is not selected.
  [[EarlGrey selectElementWithMatcher:[self autofillCell]]
      assertWithMatcher:[self elementIsSelected:NO]];

  // Tap on the autofill cell to toggle the selection.
  [[EarlGrey selectElementWithMatcher:[self autofillCell]]
      performAction:grey_tap()];

  // Assert autofill row is selected.
  [[EarlGrey selectElementWithMatcher:[self autofillCell]]
      assertWithMatcher:[self elementIsSelected:YES]];

  // Tap cancel button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Ensure the browsing data page is closed while quick delete bottom sheet is
  // still open.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteBrowsingDataPageTitle]]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Assert that the pref remains false on cancel.
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeleteFormData],
      NO, @"Pref changed on cancel.");
}

// Tests the confirm button should save changes to prefs.
- (void)testConfirmButtonShouldUpdatePrefs {
  // Set autofill pref to false.
  [ChromeEarlGrey setBoolValue:(BOOL)NO
                   forUserPref:browsing_data::prefs::kDeleteFormData];

  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Assert autofill row is not selected.
  [[EarlGrey selectElementWithMatcher:[self autofillCell]]
      assertWithMatcher:[self elementIsSelected:NO]];

  // Tap on the autofill cell to toggle the selection.
  [[EarlGrey selectElementWithMatcher:[self autofillCell]]
      performAction:grey_tap()];

  // Assert autofill row is selected.
  [[EarlGrey selectElementWithMatcher:[self autofillCell]]
      assertWithMatcher:[self elementIsSelected:YES]];

  // Tap confirm button.
  [[EarlGrey selectElementWithMatcher:[self navigationBarConfirmButton]]
      performAction:grey_tap()];

  // Ensure the browsing data page is closed while quick delete bottom sheet is
  // still open.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteBrowsingDataPageTitle]]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // Assert that the pref was updated to true on confirm.
  GREYAssertEqual(
      [ChromeEarlGrey userBooleanPref:browsing_data::prefs::kDeleteFormData],
      YES, @"Failed to save pref change on confirm.");
}

@end
