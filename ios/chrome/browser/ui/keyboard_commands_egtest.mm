// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/test/scoped_feature_list.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"

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
  BOOL (^block)
  () = ^BOOL {
    return chrome_test_util::GetRegisteredKeyCommandsCount() > 0;
  };

  GREYCondition* keyboardCommands =
      [GREYCondition conditionWithName:@"Keyboard commands registered"
                                 block:block];

  BOOL success = [keyboardCommands waitWithTimeout:5];
  if (!success) {
    GREYFail(@"No keyboard commands are registered.");
  }
}

// Verifies that no keyboard commands are registered by the BVC.
- (void)verifyNoKeyboardCommandsAreRegistered {
  BOOL (^block)
  () = ^BOOL {
    return chrome_test_util::GetRegisteredKeyCommandsCount() == 0;
  };
  GREYCondition* noKeyboardCommands =
      [GREYCondition conditionWithName:@"No keyboard commands registered"
                                 block:block];

  BOOL success = [noKeyboardCommands waitWithTimeout:5];
  if (!success) {
    GREYFail(@"Some keyboard commands are registered.");
  }
}

// Waits for the bookmark editor to display.
- (void)waitForSingleBookmarkEditorToDisplay {
  BOOL (^block)
  () = ^BOOL {
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
                  block:block];

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
  BOOL success = chrome_test_util::ClearBookmarks();
  GREYAssert(success, @"Not all bookmarks were removed.");

  // Load a webpage because the NTP is not always bookmarkable.
  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:URL];

  // Bookmark page
  if (IsIPadIdiom()) {
    id<GREYMatcher> bookmarkMatcher =
        chrome_test_util::ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
    [[EarlGrey selectElementWithMatcher:bookmarkMatcher]
        performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openToolsMenu];
    if (IsUIRefreshPhase1Enabled()) {
      [[[EarlGrey
          selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                  kToolsMenuAddToBookmarks),
                                              grey_sufficientlyVisible(), nil)]
             usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
          onElementWithMatcher:grey_accessibilityID(
                                   kPopupMenuToolsMenuTableViewId)]
          performAction:grey_tap()];

    } else {
      [[EarlGrey
          selectElementWithMatcher:grey_accessibilityLabel(@"Add Bookmark")]
          performAction:grey_tap()];
    }
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

  // Clean up by dismissing the recent tabs UI before ending the test. The
  // a11y ID for the dismiss button depends on the UIRefresh experiment.
  id<GREYMatcher> exitMatcher = nil;
  if (IsUIRefreshPhase1Enabled()) {
    exitMatcher = grey_accessibilityID(kTableViewNavigationDismissButtonId);
  } else {
    exitMatcher = grey_accessibilityID(@"Exit");
  }
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];
}

// Tests that when the app is opened on a web page and a key is pressed, the
// web view is the first responder.
- (void)testWebViewIsFirstResponderUponKeyPress {
  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:URL];

  [self verifyKeyboardCommandsAreRegistered];

  UIResponder* firstResponder = GetFirstResponder();
  GREYAssert(
      [firstResponder isKindOfClass:NSClassFromString(@"WKContentView")],
      @"Expected first responder to be a WKContentView. Instead, is a %@",
      NSStringFromClass([firstResponder class]));
}

@end
