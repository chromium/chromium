// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Matcher for the overflow pin action.
id<GREYMatcher> PinOverflowAction() {
  return grey_accessibilityID(kToolsMenuPinTabId);
}

// Matcher for the overflow unpin action.
id<GREYMatcher> UnpinOverflowAction() {
  return grey_accessibilityID(kToolsMenuUnpinTabId);
}

// Matcher for the snackbar displayed after a pin action.
id<GREYMatcher> PinTabSnackbar() {
  NSString* pinTabSnackbarMessage =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_PINNED_TAB);
  return grey_accessibilityLabel(pinTabSnackbarMessage);
}

// Matcher for the snackbar displayed after an unpin action.
id<GREYMatcher> UnpinTabSnackbar() {
  NSString* unpinTabSnackbarMessage =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_UNPINNED_TAB);
  return grey_accessibilityLabel(unpinTabSnackbarMessage);
}

// Matcher for the UNDO action displayed in a snackbar.
id<GREYMatcher> UndoSnackbarAction() {
  NSString* undoMessage = l10n_util::GetNSString(IDS_IOS_SNACKBAR_ACTION_UNDO);
  return grey_accessibilityLabel(undoMessage);
}

}  // namespace

// Tests related to Pinned Tabs feature on the OverflowMenu surface.
@interface PinnedTabsOverflowTestCase : ChromeTestCase
@end

@implementation PinnedTabsOverflowTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testOverflowMenuParamDisabled)]) {
    config.features_enabled.push_back(kEnablePinnedTabs);
    return config;
  }

  config.additional_args.push_back(
      "--enable-features=" + std::string(kEnablePinnedTabs.name) + ":" +
      kEnablePinnedTabsOverflowParam + "/true");
  return config;
}

- (void)setUp {
  [super setUp];

  // Make sure that the pinned tabs feature has been used from the
  // overflow menu in order to avoid triggering IPH.
  [ChromeEarlGrey setUserDefaultObject:@(1) forKey:kPinnedTabsOverflowEntryKey];
}

// Checks that when `kEnablePinnedTabsOverflowParam` is not enabled, the pin and
// unpin action are not added in the overflow menu.
- (void)testOverflowMenuParamDisabled {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  [ChromeEarlGreyUI openToolsMenu];

  // Test the pin action.
  [[EarlGrey selectElementWithMatcher:PinOverflowAction()]
      assertWithMatcher:grey_nil()];

  // Test the unpin action.
  [[EarlGrey selectElementWithMatcher:PinOverflowAction()]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGreyUI closeToolsMenu];
}

// Checks that the Pinned Tabs feature is disabled on iPad.
- (void)testOverflowMenuOniPad {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPhone.");
  }

  [ChromeEarlGreyUI openToolsMenu];

  // Test the pin action.
  [[EarlGrey selectElementWithMatcher:PinOverflowAction()]
      assertWithMatcher:grey_nil()];

  // Test the unpin action.
  [[EarlGrey selectElementWithMatcher:PinOverflowAction()]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGreyUI closeToolsMenu];
}

// Checks that pinning and unpinning a tab from the overflow menu updates the
// pin state of the tab and displays a snackbar.
- (void)testPinAndUnpinTabFromOverflowMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  [ChromeEarlGreyUI openToolsMenu];

  // Test the pin action.
  [ChromeEarlGreyUI tapToolsMenuAction:PinOverflowAction()];
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];

  // Test the unpin action.
  [ChromeEarlGreyUI tapToolsMenuAction:UnpinOverflowAction()];
  [[EarlGrey selectElementWithMatcher:UnpinTabSnackbar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:UnpinTabSnackbar()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];

  // Test the pin action on more time.
  [ChromeEarlGreyUI tapToolsMenuAction:PinOverflowAction()];
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      performAction:grey_tap()];
}

// Checks that pinning a tab from the overflow menu and tapping on the UNDO
// action displayed in the snackbar cancels the pin action.
- (void)testPinTabAndUndoFromOverflowMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  [ChromeEarlGreyUI openToolsMenu];

  // Test the pin action.
  [ChromeEarlGreyUI tapToolsMenuAction:PinOverflowAction()];
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the UNDO action displayed in the snackbar.
  [[EarlGrey selectElementWithMatcher:UndoSnackbarAction()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];

  // The pin action should still be displayed.
  [ChromeEarlGreyUI tapToolsMenuAction:PinOverflowAction()];
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      performAction:grey_tap()];
}

// Checks that unpinning a tab from the overflow menu and tapping on the UNDO
// action displayed in the snackbar cancels the unpin action.
- (void)testUnpinTabAndUndoFromOverflowMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad. The Pinned Tabs feature is only "
                           @"supported on iPhone.");
  }

  [ChromeEarlGreyUI openToolsMenu];

  // Test the pin action.
  [ChromeEarlGreyUI tapToolsMenuAction:PinOverflowAction()];
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:PinTabSnackbar()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];

  // Test the unpin action.
  [ChromeEarlGreyUI tapToolsMenuAction:UnpinOverflowAction()];
  [[EarlGrey selectElementWithMatcher:UnpinTabSnackbar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the UNDO action displayed in the snackbar.
  [[EarlGrey selectElementWithMatcher:UndoSnackbarAction()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];

  // The unpin action should still be displayed.
  [ChromeEarlGreyUI tapToolsMenuAction:UnpinOverflowAction()];
  [[EarlGrey selectElementWithMatcher:UnpinTabSnackbar()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:UnpinTabSnackbar()]
      performAction:grey_tap()];
}

@end
