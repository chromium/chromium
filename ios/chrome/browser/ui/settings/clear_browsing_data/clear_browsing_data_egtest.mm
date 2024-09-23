// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Identifier used to find the 'Search history' link.
NSString* const kSearchHistory = @"Search history";

// URL of the help center page.
char kMyActivityURL[] = "myactivity.google.com";

// Matcher for an element with or without the
// UIAccessibilityTraitSelected accessibility trait depending on `selected`.
id<GREYMatcher> ElementIsSelected(BOOL selected) {
  return selected
             ? grey_accessibilityTrait(UIAccessibilityTraitSelected)
             : grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected));
}

// Returns a matcher (which always matches) that records the selection
// state of matched element in `selected` parameter.
id<GREYMatcher> RecordElementSelectionState(BOOL& selected) {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    selected = ([view accessibilityTraits] & UIAccessibilityTraitSelected) != 0;
    return YES;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:@"Selected Check"];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> clearBrowsingDataButton() {
  return grey_accessibilityID(kClearBrowsingDataButtonIdentifier);
}

id<GREYMatcher> confirmClearBrowsingDataButton() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_not(grey_accessibilityID(kClearBrowsingDataButtonIdentifier)),
      grey_userInteractionEnabled(), nil);
}

id<GREYMatcher> clearBrowsingHistoryButton() {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearBrowsingHistoryCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> clearCookiesButton() {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearCookiesCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> clearCacheButton() {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearCacheCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> clearSavedPasswordsButton() {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearSavedPasswordsCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> clearAutofillButton() {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearAutofillCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

}  // namespace

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::WindowWithNumber;

@interface ClearBrowsingDataSettingsTestCase : ChromeTestCase
@end

@implementation ClearBrowsingDataSettingsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled.push_back(kIOSQuickDelete);
  return config;
}

- (void)openClearBrowsingDataDialog {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];

  NSString* clearBrowsingDataDialogLabel =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  [ChromeEarlGreyUI tapPrivacyMenuButton:ButtonWithAccessibilityLabel(
                                             clearBrowsingDataDialogLabel)];
}

- (void)openClearBrowsingDataDialogInWindowWithNumber:(int)windowNumber {
  [ChromeEarlGreyUI openSettingsMenuInWindowWithNumber:windowNumber];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];

  NSString* clearBrowsingDataDialogLabel =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  [ChromeEarlGreyUI tapPrivacyMenuButton:ButtonWithAccessibilityLabel(
                                             clearBrowsingDataDialogLabel)];
}

// Test that opening the clear browsing data dialog does not crash.
- (void)testOpenClearBrowsingDataDialogUI {
  [self openClearBrowsingDataDialog];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Verifies that the CBD screen can be swiped down to dismiss.
- (void)testClearBrowsingDataSwipeDown {
  [self openClearBrowsingDataDialog];

  // Check that CBD is presented.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that opening the clear browsing data dialog in two windows does not
// crash.
- (void)testClearBrowsingDataDialogInMultiWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // TODO(crbug.com/40210654).
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_DISABLED(
        @"Earl Grey doesn't work properly with SwiftUI and multiwindow");
  }

  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [self openClearBrowsingDataDialogInWindowWithNumber:0];
  [self openClearBrowsingDataDialogInWindowWithNumber:1];

  // Grab start states.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  BOOL isclearBrowsingHistoryButtonSelected = NO;
  BOOL isClearCookiesButtonSelected = NO;
  BOOL isClearCacheButtonSelected = NO;
  BOOL isClearSavedPasswordsButtonSelected = NO;
  BOOL isClearAutofillButtonSelected = NO;
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isclearBrowsingHistoryButtonSelected)];
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearCookiesButtonSelected)];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearCacheButtonSelected)];
  [[EarlGrey selectElementWithMatcher:clearSavedPasswordsButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearSavedPasswordsButtonSelected)];
  [[EarlGrey selectElementWithMatcher:clearAutofillButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearAutofillButtonSelected)];

  // Verify it matches second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      assertWithMatcher:ElementIsSelected(
                            isclearBrowsingHistoryButtonSelected)];
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      assertWithMatcher:ElementIsSelected(isClearCookiesButtonSelected)];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      assertWithMatcher:ElementIsSelected(isClearCacheButtonSelected)];
  [[EarlGrey selectElementWithMatcher:clearSavedPasswordsButton()]
      assertWithMatcher:ElementIsSelected(isClearSavedPasswordsButtonSelected)];
  [[EarlGrey selectElementWithMatcher:clearAutofillButton()]
      assertWithMatcher:ElementIsSelected(isClearAutofillButtonSelected)];

  // Switch Clear Browsing History Button in window 0 and make sure it is
  // deselected in both.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      assertWithMatcher:ElementIsSelected(
                            !isclearBrowsingHistoryButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      assertWithMatcher:ElementIsSelected(
                            !isclearBrowsingHistoryButtonSelected)];

  // Switch Clear Browsing History Button in window 1 and make sure it is
  // deselected in both.
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      assertWithMatcher:ElementIsSelected(!isClearCookiesButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      assertWithMatcher:ElementIsSelected(!isClearCookiesButtonSelected)];

  // Switch Clear Cache Button in window 0 and make sure it is
  // deselected in both.
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      assertWithMatcher:ElementIsSelected(!isClearCacheButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      assertWithMatcher:ElementIsSelected(!isClearCacheButtonSelected)];

  // Switch Clear Saved Passwords Button in window 1 and make sure it is
  // deselected in both.
  [[EarlGrey selectElementWithMatcher:clearSavedPasswordsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearSavedPasswordsButton()]
      assertWithMatcher:ElementIsSelected(
                            !isClearSavedPasswordsButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:clearSavedPasswordsButton()]
      assertWithMatcher:ElementIsSelected(
                            !isClearSavedPasswordsButtonSelected)];

  // Switch Clear Autofill Button in window 0 and make sure it is
  // deselected in both.
  [[EarlGrey selectElementWithMatcher:clearAutofillButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearAutofillButton()]
      assertWithMatcher:ElementIsSelected(!isClearAutofillButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:clearAutofillButton()]
      assertWithMatcher:ElementIsSelected(!isClearAutofillButtonSelected)];

  // Restore to intial state.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearSavedPasswordsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearAutofillButton()]
      performAction:grey_tap()];

  // Cleanup.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that opening the Clear Browsing interface from the History and tapping
// the "Search history" link opens the help center.
- (void)testTapSearchHistoryFromHistory {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::HistoryDestinationButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          HistoryClearBrowsingDataButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(kSearchHistory),
                                   grey_kindOfClassName(
                                       @"UIAccessibilityLinkSubelement"),
                                   nil)]
      performAction:chrome_test_util::TapAtPointPercentage(0.95, 0.05)];

  // Check that the URL of the help center was opened.
  GREYAssertEqual(std::string(kMyActivityURL),
                  [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the search activity url.");
}

// Clear browsing data.
- (void)openCBDAndClearData {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:chrome_test_util::ButtonWithAccessibilityLabelId(
                               IDS_IOS_CLEAR_BROWSING_DATA_TITLE)];
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:clearBrowsingDataButton()];
  [[EarlGrey selectElementWithMatcher:confirmClearBrowsingDataButton()]
      performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();
}

// Tests that a user in the `ConsentLevel::kSignin` state will remain signed in
// after clearing their browsing history.
- (void)testUserSignedInWhenClearingBrowsingData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [self openCBDAndClearData];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that a supervised user in the `ConsentLevel::kSync` state will remain
// signed-in after clearing their browsing history.
// TODO(crbug.com/40066949): Delete this test after the syncing state is gone.
- (void)testSupervisedUserSyncingWhenClearingBrowsingData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];

  [self openCBDAndClearData];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that a supervised user in the `ConsentLevel::kSignin` state will remain
// signed-in after clearing their browsing history.
- (void)testSupervisedUserSignedInWhenClearingBrowsingData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [self openCBDAndClearData];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that if the time range pref is set to 15 minutes, then the clear
// browsing data button is disabled to make the user chose a different time
// range.
- (void)testDisabledClearButtonWith15MinutesTimeRange {
  // Set pref to the last 15 minutes.
  [ChromeEarlGrey
      setIntegerValue:static_cast<int>(
                          browsing_data::TimePeriod::LAST_15_MINUTES)
          forUserPref:browsing_data::prefs::kDeleteTimePeriod];

  [self openClearBrowsingDataDialog];

  [[EarlGrey selectElementWithMatcher:clearBrowsingDataButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that if the time range pref is not set to 15 minutes, for example last
// hour, then the clear browsing data button is enabled.
- (void)testEnabledClearButtonWithLastHourTimeRange {
  // Set pref to the last hour.
  [ChromeEarlGrey
      setIntegerValue:static_cast<int>(browsing_data::TimePeriod::LAST_HOUR)
          forUserPref:browsing_data::prefs::kDeleteTimePeriod];

  [self openClearBrowsingDataDialog];

  [[EarlGrey selectElementWithMatcher:clearBrowsingDataButton()]
      assertWithMatcher:grey_enabled()];
}

@end
