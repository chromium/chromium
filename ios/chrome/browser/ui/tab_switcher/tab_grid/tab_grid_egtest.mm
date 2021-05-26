// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::TabGridOtherDevicesPanelButton;
using chrome_test_util::LongPressCellAndDragToEdge;
using chrome_test_util::LongPressCellAndDragToOffsetOf;
using chrome_test_util::TapAtOffsetOf;
using chrome_test_util::WindowWithNumber;
using chrome_test_util::AddToBookmarksButton;
using chrome_test_util::AddToReadingListButton;
using chrome_test_util::CloseTabMenuButton;

namespace {
char kURL1[] = "http://firstURL";
char kURL2[] = "http://secondURL";
char kURL3[] = "http://thirdURL";
char kURL4[] = "http://fourthURL";
char kTitle1[] = "Page 1";
char kTitle2[] = "Page 2";
char kTitle4[] = "Page 4";
char kResponse1[] = "Test Page 1 content";
char kResponse2[] = "Test Page 2 content";
char kResponse3[] = "Test Page 3 content";
char kResponse4[] = "Test Page 4 content";

const CFTimeInterval kSnackbarAppearanceTimeout = 5;
const CFTimeInterval kSnackbarDisappearanceTimeout = 11;

// Matcher for the 'Close All' confirmation button.
id<GREYMatcher> CloseAllTabsConfirmationWithNumberOfTabs(
    NSInteger numberOfTabs) {
  NSString* closeTabs =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION, numberOfTabs));
  return grey_allOf(grey_accessibilityLabel(closeTabs),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> TabWithTitle(NSString* title) {
  return grey_allOf(grey_accessibilityLabel(title), grey_sufficientlyVisible(),
                    nil);
}

// Identifer for cell at given |index| in the tab grid.
NSString* IdentifierForCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}

}  // namespace

@interface TabGridTestCase : WebHttpServerChromeTestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
  GURL _URL4;
}
@end

@implementation TabGridTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  // Features are enabled or disabled based on the name of the test that is
  // running. This is done because it is inefficient to use
  // ensureAppLaunchedWithConfiguration for each test.
  if ([self isRunningTest:@selector(testTabGridItemContextMenuShare)] ||
      [self isRunningTest:@selector
            (testTabGridItemContextMenuAddToReadingList)] ||
      [self isRunningTest:@selector(testTabGridItemContextCloseTab)] ||
      [self
          isRunningTest:@selector(testTabGridItemContextMenuAddToBookmarks)]) {
    config.features_enabled.push_back(kTabGridContextMenu);
  }

  config.features_disabled.push_back(kStartSurface);

  return config;
}

- (void)setUp {
  [super setUp];

  _URL1 = web::test::HttpServer::MakeUrl(kURL1);
  _URL2 = web::test::HttpServer::MakeUrl(kURL2);
  _URL3 = web::test::HttpServer::MakeUrl(kURL3);
  _URL4 = web::test::HttpServer::MakeUrl(kURL4);

  std::map<GURL, std::string> responses;
  const char kPageFormat[] = "<head><title>%s</title></head><body>%s</body>";
  responses[_URL1] = base::StringPrintf(kPageFormat, kTitle1, kResponse1);
  responses[_URL2] = base::StringPrintf(kPageFormat, kTitle2, kResponse2);
  // Page 3 does not have <title> tag, so URL will be its title.
  responses[_URL3] = kResponse3;
  responses[_URL4] = base::StringPrintf(kPageFormat, kTitle4, kResponse4);
  web::test::SetUpSimpleHttpServer(responses);
}

// Tests entering and leaving the tab grid.
- (void)testEnteringAndLeavingTabGrid {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests that tapping on the first cell shows that tab.
- (void)testTappingOnFirstCell {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that closing the cell shows no tabs, and displays the empty state.
- (void)testClosingFirstCell {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping Close All shows no tabs, shows Undo button, and displays
// the empty state. Then tests tapping Undo shows Close All button again.
- (void)testCloseAllAndUndoCloseAll {
  if ([ChromeEarlGrey isCloseAllTabsConfirmationEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Close All Tabs Confirmation feature flag is on.");
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Undo button is no longer available after tapping Close All,
// then creating a new tab, then coming back to the tab grid.
- (void)testUndoCloseAllNotAvailableAfterNewTabCreation {
  if ([ChromeEarlGrey isCloseAllTabsConfirmationEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Close All Tabs Confirmation feature flag is on.");
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Create a new tab then come back to tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  // Undo is no longer available.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridUndoCloseAllButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that Clear Browsing Data can be successfully done from tab grid.
- (void)DISABLED_testClearBrowsingData {
  // Load history
  [self loadTestURLs];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  // Switch over to Recent Tabs.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];

  // Tap on "Show History"
  // Undo is available after close all action.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridSelectShowHistoryCell()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI openAndClearBrowsingDataFromHistory];
  [ChromeEarlGreyUI assertHistoryHasNoEntries];
}

#pragma mark - Recent Tabs Context Menu

// Tests the Copy Link action on a recent tab's context menu.
- (void)testRecentTabsContextMenuCopyLink {
  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString stringWithUTF8String:_URL1.spec()
                                                                      .c_str()]
                      useNewString:YES];
}

// Tests the Open in New Tab action on a recent tab's context menu.
- (void)testRecentTabsContextMenuOpenInNewTab {
  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey verifyOpenInNewTabActionWithURL:_URL1.GetContent()];

  // Verify that the Tab Grid is closed.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the Open in New Window action on a recent tab's context menu.
- (void)testRecentTabsContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:kResponse1];
}

// Tests the Share action on a recent tab's context menu.
- (void)testRecentTabsContextMenuShare {
  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey
      verifyShareActionWithURL:_URL1
                     pageTitle:[NSString stringWithUTF8String:kTitle1]];
}

#pragma mark - Tab Grid Item Context Menu

// Tests the Share action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuShare {
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_SKIPPED(
        @"Tab Grid context menu only supported on iOS 13 and later.");
  }

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey
      verifyShareActionWithURL:_URL1
                     pageTitle:[NSString stringWithUTF8String:kTitle1]];
}

// Tests the Add to Reading list action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuAddToReadingList {
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_SKIPPED(
        @"Tab Grid context menu only supported on iOS 13 and later.");
  }

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [self waitForSnackBarMessage:IDS_IOS_READING_LIST_SNACKBAR_MESSAGE
      triggeredByTappingItemWithMatcher:AddToReadingListButton()];
}

// Tests the Add to Bookmarks action on a tab grid item's context menu.
- (void)testTabGridItemContextMenuAddToBookmarks {
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_SKIPPED(
        @"Tab Grid context menu only supported on iOS 13 and later.");
  }

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [self waitForSnackBarMessage:IDS_IOS_BOOKMARK_PAGE_SAVED
      triggeredByTappingItemWithMatcher:AddToBookmarksButton()];
}

// Tests the Share action on a tab grid item's context menu.
- (void)testTabGridItemContextCloseTab {
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_SKIPPED(
        @"Tab Grid context menu only supported on iOS 13 and later.");
  }

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [self longPressTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  // Close Tab.
  [[EarlGrey selectElementWithMatcher:CloseTabMenuButton()]
      performAction:grey_tap()];

  // Make sure that the tab is no longer present.
  [[EarlGrey selectElementWithMatcher:TabWithTitle([NSString
                                          stringWithUTF8String:kTitle1])]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridRegularTabsEmptyStateView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark -

// Tests that tapping on "Close All" shows a confirmation dialog.
// It also tests that tapping on "Close x Tab(s)" on the confirmation dialog
// displays an empty grid and tapping on "Cancel" doesn't modify the grid.
- (void)testCloseAllTabsConfirmation {
  if (![ChromeEarlGrey isCloseAllTabsConfirmationEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Close All Tabs Confirmation feature flag is off.");
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  // Taps on "Close All" and Confirm.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CloseAllTabsConfirmationWithNumberOfTabs(
                                          1)] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_nil()];

  // Checks that "Close All" is grayed out.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Creates a new tab then come back to tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  // Taps on "Close All" and Cancel.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
      performAction:grey_tap()];
  if (IsIPadIdiom()) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::TabGridCloseAllButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Drag and drop in Multiwindow

// Tests that dragging a tab grid item to the edge opens a new window and that
// the tab is properly transferred, incuding navigation stack.
- (void)testDragAndDropAtEdgeToCreateNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

// TODO(crbug.com/1184267): Test is flaky on iPad devices.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is flaky on iPad devices.");
  }
#endif

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  GREYAssert(LongPressCellAndDragToEdge(IdentifierForCellAtIndex(0),
                                        kGREYContentEdgeRight, 0),
             @"Failed to DND cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Assert two windows and the expected tabs in each.
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  // Navigate back on second window to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests that dragging a tab grid incognito item to the edge opens a new window
// and that the tab is properly transferred, incuding navigation stack.
// TODO(crbug.com/1176180): re-enable this test when it is fixed.
- (void)DISABLED_testIncognitoDragAndDropAtEdgeToCreateNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForIncognitoTabCount:2 inWindowWithNumber:0];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  GREYAssert(LongPressCellAndDragToEdge(IdentifierForCellAtIndex(0),
                                        kGREYContentEdgeRight, 0),
             @"Failed to DND cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Assert two windows and the expected tabs in each.
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  // Navigate back on second window to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid item between windows.
- (void)testDragAndDropBetweenWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

// TODO(crbug.com/1184267): Test is flaky on iPad devices.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is flaky on iPad devices.");
  }
#endif

  // Setup first window with tabs 1 and 2.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];

  // Open second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Setup second window with tabs 3 and 4.
  [ChromeEarlGrey loadURL:_URL3 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [ChromeEarlGrey openNewTabInWindowWithNumber:1];
  [ChromeEarlGrey loadURL:_URL4 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse4
                             inWindowWithNumber:1];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:1];

  // Open tab grid in both window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of left window to left edge of first tab in second window.
  // Note: move to left half of the destination tile, to avoid unwanted
  // scrolling that would happen closer to the left edge.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0,
                                            IdentifierForCellAtIndex(0), 1,
                                            CGVectorMake(0.4, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:3 inWindowWithNumber:1];

  // Move third cell of second window as second cell in first window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(2), 1,
                                            IdentifierForCellAtIndex(0), 0,
                                            CGVectorMake(1.0, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:1];

  // Check content and order of tabs.
  [self fromGridCheckTabAtIndex:0 inWindowNumber:0 containsText:kResponse2];
  [self fromGridCheckTabAtIndex:1 inWindowNumber:0 containsText:kResponse4];
  [self fromGridCheckTabAtIndex:0 inWindowNumber:1 containsText:kResponse1];
  [self fromGridCheckTabAtIndex:1 inWindowNumber:1 containsText:kResponse3];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid item between windows.
// TODO(crbug.com/1176669): re-enable this test when it is fixed.
- (void)DISABLED_testDragAndDropIncognitoBetweenWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  // Setup first window with one incognito tab.
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];

  // Open second window with main ntp.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Open tab grid in both window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // Try DnDing first incognito tab of left window to main tab panel on right
  // window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0,
                                            IdentifierForCellAtIndex(0), 1,
                                            CGVectorMake(1.0, 0.5)),
             @"Failed to DND cell on cell");

  GREYWaitForAppToIdle(@"App failed to idle");

  // It should fail and both windows should still have only one tab.
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Move second window to incognito tab panel.
  // Note: until reported bug is fixed in EarlGrey, grey_tap() doesn't always
  // work in second window, because it fails the visibility check.
  GREYAssert(TapAtOffsetOf(kTabGridIncognitoTabsPageButtonIdentifier, 1,
                           CGVectorMake(0.5, 0.5)),
             @"Failed to tap incognito panel button");

  // Try again to move tabs.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0, nil,
                                            1, CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Check that it worked and there are 2 incgnito tabs in second window.
  [ChromeEarlGrey waitForIncognitoTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:1];

  // Cleanup.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid item as URL between windows.
- (void)testDragAndDropURLBetweenWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  // Setup first window with tabs 1 and 2.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];

  // Open second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Setup second window with tab 3.
  [ChromeEarlGrey loadURL:_URL3 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Open tab grid in first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of left window to second window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0, nil,
                                            1, CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Tabs should not have changed.
  [ChromeEarlGrey waitForMainTabCount:2 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Second window should show URL1
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:1];

  // Navigate back to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid incognito item as URL to a main windows.
- (void)testDragAndDropIncognitoURLInMainWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

  // Setup first window with one incognito tab 1.
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];

  // Open second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Setup second window with tab 3.
  [ChromeEarlGrey loadURL:_URL3 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Open incognito tab grid in first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of left window to second window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 0, nil,
                                            1, CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Tabs should not have changed.
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Second window should show URL1
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:1];

  // Navigate back to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests dragging tab grid main item as URL to an incognito windows.
- (void)testDragAndDropMainURLInIncognitoWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");

// TODO(crbug.com/1184267): Test is flaky on iPad devices.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is flaky on iPad devices.");
  }
#endif
  
  // Setup first window with one incognito tab 1.
  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];

  // Open second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Setup second window with tab 3.
  [ChromeEarlGrey loadURL:_URL3 inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:1];

  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // Open incognito tab grid in first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  // DnD first tab of second window to first window.
  GREYAssert(LongPressCellAndDragToOffsetOf(IdentifierForCellAtIndex(0), 1, nil,
                                            0, CGVectorMake(0.5, 0.5)),
             @"Failed to DND cell on window");

  GREYWaitForAppToIdle(@"App failed to idle");

  // Tabs should not have changed.
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:0];
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];

  // First window should show URL3
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3
                             inWindowWithNumber:0];

  // Navigate back to check the navigation stack is intact.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                             inWindowWithNumber:0];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Helper Methods

- (void)loadTestURLs {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];
}

// Loads a URL in a new tab and deletes it to populate Recent Tabs. Then,
// navigates to the Recent tabs via tab grid.
- (void)prepareRecentTabWithURL:(const GURL&)URL
                       response:(const char*)response {
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:response];

  // Close the tab, making it appear in Recent Tabs.
  [ChromeEarlGrey closeCurrentTab];

  // Switch over to Recent Tabs.
  [[EarlGrey selectElementWithMatcher:TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
}

// Long press on the recent tab entry or the tab item in the tab grid with
// |title|.
- (void)longPressTabWithTitle:(NSString*)title {
  // The test page may be there multiple times.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(title),
                                          grey_sufficientlyVisible(), nil)]
      atIndex:0] performAction:grey_longPress()];
}

// Checks if the content of the given tab in the given window matches given
// text. This method exits the tab grid and re-enters it afterward.
- (void)fromGridCheckTabAtIndex:(int)tabIndex
                 inWindowNumber:(int)windowNumber
                   containsText:(const char*)text {
  [EarlGrey
      setRootMatcherForSubsequentInteractions:WindowWithNumber(windowNumber)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(
                                          tabIndex)] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:text
                             inWindowWithNumber:windowNumber];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
}

- (void)waitForSnackBarMessage:(int)messageIdentifier
    triggeredByTappingItemWithMatcher:(id<GREYMatcher>)matcher {
  NSString* snackBarLabel = l10n_util::GetNSStringWithFixup(messageIdentifier);
  // Start custom monitor, because there's a chance the snackbar is
  // already gone by the time we wait for it (and it was like that sometimes).
  [ChromeEarlGrey watchForButtonsWithLabels:@[ snackBarLabel ]
                                    timeout:kSnackbarAppearanceTimeout];

  // Add the page to the reading list.
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbar_matcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(messageIdentifier);
  ConditionBlock wait_for_appearance = ^{
    return [ChromeEarlGrey watcherDetectedButtonWithLabel:snackBarLabel];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarAppearanceTimeout, wait_for_appearance),
             @"Snackbar did not appear.");

  // Wait for the snackbar to disappear.
  ConditionBlock wait_for_disappearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarDisappearanceTimeout, wait_for_disappearance),
             @"Snackbar did not disappear.");
}

@end
