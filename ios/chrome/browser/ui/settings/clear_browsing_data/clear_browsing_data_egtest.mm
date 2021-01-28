// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Matcher for an element with or without the
// UIAccessibilityTraitSelected accessibility trait depending on |selected|.
id<GREYMatcher> ElementIsSelected(BOOL selected) {
  return selected
             ? grey_accessibilityTrait(UIAccessibilityTraitSelected)
             : grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected));
}

// Returns a matcher (which always matches) that records the selection
// state of matched element in |selected| parameter.
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

}  // namespace

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ClearAutofillButton;
using chrome_test_util::ClearBrowsingHistoryButton;
using chrome_test_util::ClearCookiesButton;
using chrome_test_util::ClearCacheButton;
using chrome_test_util::ClearSavedPasswordsButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::WindowWithNumber;

@interface ClearBrowsingDataSettingsTestCase : ChromeTestCase
@end

@implementation ClearBrowsingDataSettingsTestCase

- (void)tearDown {
  // No-op if only one window presents.
  [EarlGrey setRootMatcherForSubsequentInteractions:nil];
  [ChromeEarlGrey closeAllExtraWindows];
  [super tearDown];
}

- (void)openClearBrowsingDataDialog {
  [ChromeEarlGreyUI openSettingsMenu];
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
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
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

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [self openClearBrowsingDataDialog];

  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [self openClearBrowsingDataDialog];

  // Grab start states.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  BOOL isClearBrowsingHistoryButtonSelected = NO;
  BOOL isClearCookiesButtonSelected = NO;
  BOOL isClearCacheButtonSelected = NO;
  BOOL isClearSavedPasswordsButtonSelected = NO;
  BOOL isClearAutofillButtonSelected = NO;
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearBrowsingHistoryButtonSelected)];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearCookiesButtonSelected)];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearCacheButtonSelected)];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearSavedPasswordsButtonSelected)];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      assertWithMatcher:RecordElementSelectionState(
                            isClearAutofillButtonSelected)];

  // Verify it matches second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:ElementIsSelected(
                            isClearBrowsingHistoryButtonSelected)];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      assertWithMatcher:ElementIsSelected(isClearCookiesButtonSelected)];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      assertWithMatcher:ElementIsSelected(isClearCacheButtonSelected)];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      assertWithMatcher:ElementIsSelected(isClearSavedPasswordsButtonSelected)];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      assertWithMatcher:ElementIsSelected(isClearAutofillButtonSelected)];

  // Switch Clear Browsing History Button in window 0 and make sure it is
  // deselected in both.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:ElementIsSelected(
                            !isClearBrowsingHistoryButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      assertWithMatcher:ElementIsSelected(
                            !isClearBrowsingHistoryButtonSelected)];

  // Switch Clear Browsing History Button in window 1 and make sure it is
  // deselected in both.
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      assertWithMatcher:ElementIsSelected(!isClearCookiesButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      assertWithMatcher:ElementIsSelected(!isClearCookiesButtonSelected)];

  // Switch Clear Cache Button in window 0 and make sure it is
  // deselected in both.
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      assertWithMatcher:ElementIsSelected(!isClearCacheButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      assertWithMatcher:ElementIsSelected(!isClearCacheButtonSelected)];

  // Switch Clear Saved Passwords Button in window 1 and make sure it is
  // deselected in both.
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      assertWithMatcher:ElementIsSelected(
                            !isClearSavedPasswordsButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      assertWithMatcher:ElementIsSelected(
                            !isClearSavedPasswordsButtonSelected)];

  // Switch Clear Autofill Button in window 0 and make sure it is
  // deselected in both.
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      assertWithMatcher:ElementIsSelected(!isClearAutofillButtonSelected)];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      assertWithMatcher:ElementIsSelected(!isClearAutofillButtonSelected)];

  // Restore to intial state.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearAutofillButton()]
      performAction:grey_tap()];

  // Cleanup.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
