// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#include "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
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

// Identifier used to find the 'Learn more' link.
NSString* const kLearnMoreIdentifier = @"Learn more";

// URL of the help center page.
char kHelpCenterURL[] = "support.google.com";

// Matcher for the history button in the tools menu.
id<GREYMatcher> HistoryButton() {
  return grey_accessibilityID(kToolsMenuHistoryId);
}

}  // namespace

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;

@interface ClearBrowsingDataSettingsTestCase : ChromeTestCase
@end

@implementation ClearBrowsingDataSettingsTestCase

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

// Tests that tapping the "Learn more" link opens the help center.
- (void)testTapLearnMore {
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS 12.");
  }

  [self openClearBrowsingDataDialog];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              kLearnMoreIdentifier),
                                          grey_kindOfClassName(
                                              @"UIAccessibilityLinkSubelement"),
                                          nil)]
      performAction:chrome_test_util::TapAtPointPercentage(0.95, 0.05)];

  // Check that the URL of the help center was opened.
  GREYAssertEqual(kHelpCenterURL, [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the help center url.");
}

// Tests that opening the Clear Browsing interface from the History and tapping
// the "Learn more" link opens the help center.
- (void)testTapLearnMoreFromHistory {
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS 12.");
  }

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:HistoryButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          HistoryClearBrowsingDataButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              kLearnMoreIdentifier),
                                          grey_kindOfClassName(
                                              @"UIAccessibilityLinkSubelement"),
                                          nil)]
      performAction:chrome_test_util::TapAtPointPercentage(0.95, 0.05)];

  // Check that the URL of the help center was opened.
  GREYAssertEqual(kHelpCenterURL, [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the help center url.");
}

@end
