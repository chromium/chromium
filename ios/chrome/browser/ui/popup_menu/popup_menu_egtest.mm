// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/feature_engagement/feature_engagement_app_interface.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPDFURL[] = "http://ios/testing/data/http_server_files/testpage.pdf";
}  // namespace

// Tests for the popup menus.
@interface PopupMenuTestCase : WebHttpServerChromeTestCase
@end

@implementation PopupMenuTestCase

// Rotate the device back to portrait if needed, since some tests attempt to run
// in landscape.
- (void)tearDown {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [super tearDown];
}

#pragma mark - TabHistory

// Test that the tab history back and forward menus contain the expected entries
// for a series of navigations, and that tapping entries performs the
// appropriate navigation.
- (void)testTabHistoryMenu {
  const GURL URL1 = web::test::HttpServer::MakeUrl("http://page1");
  const GURL URL2 = web::test::HttpServer::MakeUrl("http://page2");
  const GURL URL3 = web::test::HttpServer::MakeUrl("http://page3");
  const GURL URL4 = web::test::HttpServer::MakeUrl("http://page4");
  NSString* entry0 = @"New Tab";
  NSString* entry1 = [ChromeEarlGrey displayTitleForURL:URL1];
  NSString* entry2 = [ChromeEarlGrey displayTitleForURL:URL2];
  NSString* entry3 = [ChromeEarlGrey displayTitleForURL:URL3];
  NSString* entry4 = [ChromeEarlGrey displayTitleForURL:URL4];

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  responses[URL1] = "page1";
  responses[URL2] = "page2";
  responses[URL3] = "page3";
  responses[URL4] = "page4";
  web::test::SetUpSimpleHttpServer(responses);

  // Load 4 pages.
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey loadURL:URL3];
  [ChromeEarlGrey loadURL:URL4];

  // Long press on back button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_longPress()];

  // Check that the first four entries are shown the back tab history menu.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(entry0),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_text(entry1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_text(entry2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_text(entry3)]
      assertWithMatcher:grey_notNil()];

  // Tap entry to go back 3 pages, and verify that entry 1 is loaded.
  [[EarlGrey selectElementWithMatcher:grey_text(entry1)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(URL1.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Long press forward button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      performAction:grey_longPress()];

  // Check that entries 2, 3, and 4 are in the forward tab history menu.
  [[EarlGrey selectElementWithMatcher:grey_text(entry2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_text(entry3)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_text(entry4)]
      assertWithMatcher:grey_notNil()];
  // Tap entry to go forward 2 pages, and verify that entry 3 is loaded.
  [[EarlGrey selectElementWithMatcher:grey_text(entry3)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(URL3.GetContent())]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Tools Menu

// Tests that rotating the device will automatically dismiss the tools menu.
- (void)testDismissToolsMenuOnDeviceRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  [ChromeEarlGreyUI openToolsMenu];

  // Expect that the tools menu has appeared.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];

  // Expect that the tools menu has disappeared.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_nil()];
}

// Tests that the menu is opened and closed correctly, whatever the current
// device type is.
- (void)testOpenAndCloseToolsMenu {
  // TODO(crbug.com/1289776): This test only fails on ipad bots with
  // multitasking enabled (e.g. compact width).
  if ([ChromeEarlGrey isNewOverflowMenuEnabled] &&
      [ChromeEarlGrey isIPadIdiom] && [ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad multitasking.");
  }
  [ChromeEarlGreyUI openToolsMenu];

  // If using the new overflow menu, swipe up to expand the menu to the full
  // height to make sure that `closeToolsMenu` still closes it.
  if ([ChromeEarlGrey isNewOverflowMenuEnabled] &&
      [ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  }

  [ChromeEarlGreyUI closeToolsMenu];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testNewWindowFromToolsMenu {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::OpenNewWindowMenuButton()];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];

  // Verify the second window.
  [ChromeEarlGrey waitForForegroundWindowCount:2];
}

// Navigates to a pdf page and verifies that the "Find in Page..." tool
// is not enabled
- (void)testNoSearchForPDF {
  const GURL URL = web::test::HttpServer::MakeUrl(kPDFURL);

  // Navigate to a mock pdf and verify that the find button is disabled.
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> tableViewMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuFindInPageId),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:tableViewMatcher]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

// Open tools menu and verify elements are accessible.
- (void)testAccessibilityOnToolsMenu {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  // Close Tools menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
}

// Tests that the overflow menu IPH shows up when triggered.
- (void)testOverflowMenuIPH {
  if (![ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"The overflow menu IPH only exists when the overflow menu is enabled.")
  }
  GREYAssert([FeatureEngagementAppInterface enableOverflowMenuTipTriggering],
             @"Feature Engagement tracker did not load");

  // Open and close tools menu twice with no action to trigger tooltip.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI closeToolsMenu];

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI closeToolsMenu];

  // Background and foreground the app, which should show tooltip.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(@"BubbleViewLabelIdentifier")];

  // Open the tools menu and verify the second tooltip is visible.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(@"BubbleViewLabelIdentifier")];
}

@end
