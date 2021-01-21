// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::TabGridOtherDevicesPanelButton;

namespace {
char kURL1[] = "http://firstURL";
char kURL2[] = "http://secondURL";
char kURL3[] = "http://thirdURL";
char kTitle1[] = "Page 1";
char kTitle2[] = "Page 2";
char kResponse1[] = "Test Page 1 content";
char kResponse2[] = "Test Page 2 content";
char kResponse3[] = "Test Page 3 content";

// Matcher for the 'Close All' confirmation button.
id<GREYMatcher> CloseAllTabsConfirmationWithNumberOfTabs(
    NSInteger numberOfTabs) {
  NSString* closeTabs =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION, numberOfTabs));
  return grey_allOf(grey_accessibilityLabel(closeTabs),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}
}  // namespace

@interface TabGridTestCase : WebHttpServerChromeTestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
}
@end

@implementation TabGridTestCase

- (void)setUp {
  [super setUp];

  _URL1 = web::test::HttpServer::MakeUrl(kURL1);
  _URL2 = web::test::HttpServer::MakeUrl(kURL2);
  _URL3 = web::test::HttpServer::MakeUrl(kURL3);

  std::map<GURL, std::string> responses;
  const char kPageFormat[] = "<head><title>%s</title></head><body>%s</body>";
  responses[_URL1] = base::StringPrintf(kPageFormat, kTitle1, kResponse1);
  responses[_URL2] = base::StringPrintf(kPageFormat, kTitle2, kResponse2);
  // Page 3 does not have <title> tag, so URL will be its title.
  responses[_URL3] = kResponse3;
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

// Tests the Copy Link action on a recent tab's context menu.
- (void)testRecentTabsContextMenuCopyLink {
  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressRecentTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

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
  [self longPressRecentTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

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

  [ChromeEarlGrey waitForForegroundWindowCount:1];

  [self longPressRecentTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  // Select "Open in New Window" and confirm that new tab is opened with
  // selected URL in the new window.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OpenLinkInNewWindowButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          _URL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey closeAllExtraWindows];
}

// Tests the Share action on a recent tab's context menu.
- (void)testRecentTabsContextMenuShare {
  if (![ChromeEarlGrey isNativeContextMenusEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Test disabled when Native Context Menus feature flag is off.");
  }

  [self prepareRecentTabWithURL:_URL1 response:kResponse1];
  [self longPressRecentTabWithTitle:[NSString stringWithUTF8String:kTitle1]];

  [ChromeEarlGrey
      verifyShareActionWithPageTitle:[NSString stringWithUTF8String:kTitle1]];
}

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

- (void)longPressRecentTabWithTitle:(NSString*)title {
  // The test page may be there multiple times.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(title),
                                          grey_sufficientlyVisible(), nil)]
      atIndex:0] performAction:grey_longPress()];
}

@end
