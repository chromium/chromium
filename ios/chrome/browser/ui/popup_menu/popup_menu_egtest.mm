// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPDFURL[] = "http://ios/testing/data/http_server_files/testpage.pdf";
}  // namespace

// Tests for the popup menus.
@interface PopupMenuTestCase : ChromeTestCase
@end

@implementation PopupMenuTestCase

// Rotate the device back to portrait if needed, since some tests attempt to run
// in landscape.
- (void)tearDown {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
#if defined(CHROME_EARL_GREY_1)
                           errorOrNil:nil];
#elif defined(CHROME_EARL_GREY_2)
                                error:nil];
#endif
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
  [[EarlGrey selectElementWithMatcher:grey_text(entry0)]
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
#if defined(CHROME_EARL_GREY_1)
                           errorOrNil:nil];
#elif defined(CHROME_EARL_GREY_2)
                                error:nil];
#endif

  // Expect that the tools menu has disappeared.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_nil()];
}

// Tests that the menu is closed when tapping the close button or the scrim.
- (void)testOpenAndCloseToolsMenu {
  [ChromeEarlGreyUI openToolsMenu];

  // A scrim covers the whole window and tapping on this scrim dismisses the
  // tools menu.  The "Tools Menu" button happens to be outside of the bounds of
  // the menu and is a convenient place to tap to activate the scrim.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notVisible()];
}

// Navigates to a pdf page and verifies that the "Find in Page..." tool
// is not enabled
- (void)testNoSearchForPDF {
  web::test::SetUpFileBasedHttpServer();
  const GURL URL = web::test::HttpServer::MakeUrl(kPDFURL);

  // Navigate to a mock pdf and verify that the find button is disabled.
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuFindInPageId)]
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

@end
