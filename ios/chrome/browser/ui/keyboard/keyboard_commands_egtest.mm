// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::RecentTabsMenuButton;
using chrome_test_util::SettingsDoneButton;

// Test cases to verify that keyboard commands are and are not registered when
// expected.
@interface KeyboardCommandsTestCase : ChromeTestCase
@end

@implementation KeyboardCommandsTestCase

#pragma mark - Helpers

// Verifies that keyboard commands are registered by the BVC.
- (void)verifyKeyboardCommandsAreRegistered {
  BOOL (^confirmKeyCommands)() = ^BOOL() {
    return [ChromeEarlGrey registeredKeyCommandCount] > 0;
  };

  GREYCondition* keyboardCommands =
      [GREYCondition conditionWithName:@"Keyboard commands registered"
                                 block:confirmKeyCommands];

  BOOL success = [keyboardCommands waitWithTimeout:5];
  if (!success) {
    GREYFail(@"No keyboard commands are registered.");
  }
}

// Verifies that no keyboard commands are registered by the BVC.
- (void)verifyNoKeyboardCommandsAreRegistered {
  BOOL (^confirmNoKeyCommands)() = ^BOOL() {
    return [ChromeEarlGrey registeredKeyCommandCount] == 0;
  };

  GREYCondition* noKeyboardCommands =
      [GREYCondition conditionWithName:@"No keyboard commands registered"
                                 block:confirmNoKeyCommands];

  BOOL success = [noKeyboardCommands waitWithTimeout:5];
  if (!success) {
    GREYFail(@"Some keyboard commands are registered.");
  }
}

// Waits for the bookmark editor to display.
- (void)waitForSingleBookmarkEditorToDisplay {
  BOOL (^confirmBookmarkEditorVisible)() = ^BOOL() {
    NSError* error = nil;
    id<GREYMatcher> singleBookmarkEditor =
        grey_accessibilityLabel(kBookmarkEditViewContainerIdentifier);
    [[EarlGrey selectElementWithMatcher:singleBookmarkEditor]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error != nil;
  };
  GREYCondition* editorDisplayed = [GREYCondition
      conditionWithName:@"Waiting for bookmark editor to display."
                  block:confirmBookmarkEditorVisible];

  BOOL success = [editorDisplayed waitWithTimeout:5];
  GREYAssert(success, @"The bookmark editor was not displayed.");
}

#pragma mark - Tests

// Tests that keyboard commands are registered when the BVC is showing without
// modals currently presented.
- (void)testKeyboardCommandsRegistered {
  [self verifyKeyboardCommandsAreRegistered];
}

// Tests that keyboard commands are not registered when Settings are shown.
- (void)testKeyboardCommandsNotRegistered_SettingsPresented {
  [ChromeEarlGreyUI openSettingsMenu];

  [self verifyNoKeyboardCommandsAreRegistered];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that keyboard commands are not registered when the bookmark UI is
// shown.
- (void)testKeyboardCommandsNotRegistered_AddBookmarkPresented {
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];

  // Load a webpage because the NTP is not always bookmarkable.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];

  // Bookmark page
  if ([ChromeEarlGrey isIPadIdiom]) {
    id<GREYMatcher> bookmarkMatcher =
        chrome_test_util::ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
    [[EarlGrey selectElementWithMatcher:bookmarkMatcher]
        performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openToolsMenu];
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kToolsMenuAddToBookmarks),
                                            grey_sufficientlyVisible(), nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
        onElementWithMatcher:grey_accessibilityID(
                                 kPopupMenuToolsMenuTableViewId)]
        performAction:grey_tap()];
  }

  // Tap on the HUD.
  id<GREYMatcher> edit = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON);
  [[EarlGrey selectElementWithMatcher:edit] performAction:grey_tap()];

  [self waitForSingleBookmarkEditorToDisplay];

  [self verifyNoKeyboardCommandsAreRegistered];

  id<GREYMatcher> cancel = grey_accessibilityID(@"Cancel");
  [[EarlGrey selectElementWithMatcher:cancel] performAction:grey_tap()];
}

// Tests that keyboard commands are not registered when the Bookmarks UI is
// shown.
- (void)testKeyboardCommandsNotRegistered_BookmarksPresented {
  // Open Bookmarks
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [self verifyNoKeyboardCommandsAreRegistered];

  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that keyboard commands are not registered when the Recent Tabs UI is
// shown.
- (void)testKeyboardCommands_RecentTabsPresented {
  // Open Recent Tabs
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:RecentTabsMenuButton()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [self verifyNoKeyboardCommandsAreRegistered];

  // Clean up by dismissing the recent tabs UI before ending the test.
  id<GREYMatcher> exitMatcher =
      grey_accessibilityID(kTableViewNavigationDismissButtonId);
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];
}

// Tests that when the app is opened on a web page and a key is pressed, the
// web view is the first responder.
- (void)testWebViewIsFirstResponderUponKeyPress {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];

  [self verifyKeyboardCommandsAreRegistered];

  [[EarlGrey selectElementWithMatcher:grey_firstResponder()]
      assertWithMatcher:grey_kindOfClassName(@"WKContentView")];
}

@end
