// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_UI_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_UI_H_

#import <Foundation/Foundation.h>

#import <string>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

@protocol GREYMatcher;

// Public macro to invoke helper methods in test methods (Test Process). Usage
// example:
//
// @interface PageLoadTestCase : XCTestCase
// @end
// @implementation PageLoadTestCase
// - (void)testPageload {
//   [ChromeEarlGreyUI loadURL:GURL("https://chromium.org")];
// }
//
// In this example ChromeEarlGreyUIImpl must implement -loadURL:.
//
#define ChromeEarlGreyUI \
  [ChromeEarlGreyUIImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Test methods that perform actions on Chrome. These methods only affect Chrome
// using the UI with Earl Grey. Used for logging the failure. Compiled in Test
// Process for EG2 and EG1. Can be extended with category methods to provide
// additional test helpers. Category method names must be unique.
@interface ChromeEarlGreyUIImpl : BaseEGTestHelperImpl

// Makes the toolbar visible by swiping downward, if necessary. Then taps on
// the Tools menu button. At least one tab needs to be open and visible when
// calling this method.
- (void)openToolsMenu;

// Closes the tools menu by tapping on the Tools menu button, or tapping the
// background scrim, depending on the current version of the tools menu.
- (void)closeToolsMenu;

// Makes the toolbar visible by swiping downward, if necessary. Then taps on
// the Tools menu button. At least one tab needs to be open and visible when
// calling this method.
// Sets and Leaves the root matcher to the given window with `windowNumber`.
- (void)openToolsMenuInWindowWithNumber:(int)windowNumber;

// Opens the settings menu by opening the tools menu, and then tapping the
// Settings button. There will be a GREYAssert if the tools menu is open when
// calling this method.
- (void)openSettingsMenu;

// Opens the settings menu by opening the tools menu, and then tapping the
// Settings button. There will be a GREYAssert if the tools menu is open when
// calling this method.
// Sets and Leaves the root matcher to the given window with `windowNumber`.
- (void)openSettingsMenuInWindowWithNumber:(int)windowNumber;

// Makes the toolbar visible by swiping downward, if necessary. Then long-
// presses on the New Tab menu button. At least one tab needs to be open and
// visible when calling this method.
- (void)openNewTabMenu;

// Scrolls to find the button in the Tools menu with the corresponding
// `buttonMatcher`, and then taps it. If `buttonMatcher` is not found, or
// the Tools menu is not open when this is called there will be a GREYAssert.
- (void)tapToolsMenuButton:(id<GREYMatcher>)buttonMatcher;

// Scrolls to find the action in the Tools menu with the corresponding
// `buttonMatcher`, and then taps it. If `buttonMatcher` is not found, or
// the Tools menu is not open when this is called there will be a GREYAssert.
- (void)tapToolsMenuAction:(id<GREYMatcher>)buttonMatcher;

// Scrolls to find the button in the Settings menu with the corresponding
// `buttonMatcher`, and then taps it. If `buttonMatcher` is not found, or
// the Settings menu is not open when this is called there will be a GREYAssert.
- (void)tapSettingsMenuButton:(id<GREYMatcher>)buttonMatcher;

// Scrolls to find the button in the Privacy menu with the corresponding
// `buttonMatcher`, and then taps it. If `buttonMatcher` is not found, or
// the Privacy menu is not open when this is called there will be a GREYAssert.
- (void)tapPrivacyMenuButton:(id<GREYMatcher>)buttonMatcher;

// Scrolls to find the button in the Privacy Safe Browsing menu with the
// corresponding `buttonMatcher`, and then taps it. If `buttonMatcher` is not
// found, or the Privacy Safe Browsing menu is not open when this is called
// there will be a GREYAssert.
- (void)tapPrivacySafeBrowsingMenuButton:(id<GREYMatcher>)buttonMatcher;

// Scrolls to find the button in the Price Notifications menu with the
// corresponding `buttonMatcher`, and then taps it. If `buttonMatcher` is not
// found, or the Price Notifications menu is not open when this is called
// there will be a GREYAssert.
- (void)tapPriceNotificationsMenuButton:(id<GREYMatcher>)buttonMatcher;

// Scrolls to find the button in the Tracking Price menu with the
// corresponding `buttonMatcher`, and then taps it. If `buttonMatcher` is not
// found, or the Tracking Price menu is not open when this is called
// there will be a GREYAssert.
- (void)tapTrackingPriceMenuButton:(id<GREYMatcher>)buttonMatcher;

// Scrolls to find the button in the Clear Browsing Data menu with the
// corresponding `buttonMatcher`, and then taps it. If `buttonMatcher` is
// not found, or the Clear Browsing Data menu is not open when this is called
// there will be a GREYAssert.
- (void)tapClearBrowsingDataMenuButton:(id<GREYMatcher>)buttonMatcher;

// Scrolls to find the button in the accounts menu with the corresponding
// `buttonMatcher`, and then taps it. If `buttonMatcher` is not found, or the
// accounts menu is not open when this is called there will be a GREYAssert.
- (void)tapAccountsMenuButton:(id<GREYMatcher>)buttonMatcher;

// Focuses the omnibox by tapping and types `text` into it. The '\n' symbol can
// be passed in order to commit the string.
// If `text` is empty or nil, the omnibox is just focused.
//
// Note: This approach differs from text replacement by simulating the user's
// keystrokes in the omnibox, rather than programmatically modifying its
// content.
- (void)focusOmniboxAndType:(NSString*)text;

// Focuses the omnibox by tapping and replaces its content with `text`.
// The '\n' symbol can be passed in order to commit the string.
// If `text` is empty or nil, the omnibox is just focused.
- (void)focusOmniboxAndReplaceText:(NSString*)text;

// Focuses the omnibox by tapping it.
- (void)focusOmnibox;

// Opens a new tab via the tools menu.
- (void)openNewTab;

// Opens a new incognito tab via the tools menu.
- (void)openNewIncognitoTab;

// Opens the tab grid.
- (void)openTabGrid;

// Opens and clear browsing data from history.
- (void)openAndClearBrowsingDataFromHistory;

// Asserts that history is empty.
- (void)assertHistoryHasNoEntries;

// Reloads the page via the reload button, and does not wait for the page to
// finish loading.
- (void)reload;

// Opens the share menu via the share button.
// This method requires that there is at least one tab open.
- (void)openShareMenu;

// Waits for toolbar to become visible if `isVisible` is YES, otherwise waits
// for it to disappear. If the condition is not met within a timeout, a
// GREYAssert is induced.
- (void)waitForToolbarVisible:(BOOL)isVisible;

// Waits for the app to idle.
- (void)waitForAppToIdle;

// Opens pageInfo via the tools menu.
- (void)openPageInfo;

// Tries to dismiss any presented native context menu.
// Returns `YES` if a context menu was dismissed, otherwise returns `NO`.
- (BOOL)dismissContextMenuIfPresent;

// Cleans up the view hierarchy after showing the system alert on certain OS
// versions.
- (void)cleanupAfterShowingAlert;

// Type `text` in Omnibox and optionally press Enter if `shouldPressEnter` is
// YES.
- (void)typeTextInOmnibox:(std::string const&)text
            andPressEnter:(BOOL)shouldPressEnter;

// Dismisses the window of the popover by tapping on the original point.
// `matcher` can be any view in the popover. Throws if the window is not
// dismissable by tapping.
- (void)dismissByTappingOnTheWindowOfPopover:(id<GREYMatcher>)matcher;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_UI_H_
