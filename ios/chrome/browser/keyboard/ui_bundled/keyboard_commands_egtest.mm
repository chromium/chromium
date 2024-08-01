// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::RecentTabsDestinationButton;
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

// Tests that keyboard commands are registered when the BVC is showing in
// MultiWindow mode.
- (void)testKeyboardCommandsRegistered_MultiWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

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
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];

  // Load a webpage because the NTP is not always bookmarkable.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];

  // Bookmark the page.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"d"
                                          flags:UIKeyModifierCommand];
  // Edit the bookmark.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"d"
                                          flags:UIKeyModifierCommand];

  [self waitForSingleBookmarkEditorToDisplay];

  [self verifyNoKeyboardCommandsAreRegistered];

  id<GREYMatcher> cancel = grey_accessibilityID(@"Cancel");
  [[EarlGrey selectElementWithMatcher:cancel] performAction:grey_tap()];

  [self verifyKeyboardCommandsAreRegistered];
}

// Tests that keyboard commands are not registered when the Bookmarks UI is
// shown.
- (void)testKeyboardCommandsNotRegistered_BookmarksPresented {
  // Open Bookmarks
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

  [self verifyNoKeyboardCommandsAreRegistered];

  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that keyboard commands are not registered when the Recent Tabs UI is
// shown.
- (void)testKeyboardCommands_RecentTabsPresented {
  // Open Recent Tabs
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:RecentTabsDestinationButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

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
