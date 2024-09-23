// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util.h"

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

// Tests that the menu is opened and closed correctly, whatever the current
// device type is.
- (void)testOpenAndCloseToolsMenu {
  // TODO(crbug.com/40817696): This test only fails on ipad bots with
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
  // Disabled for iOS 16.1.1+ since Native Find in Page supports PDFs.
  if (base::ios::IsRunningOnOrLater(16, 1, 1)) {
    return;
  }
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

// Tests that both the 2 steps of the history on overflow menu IPH is displayed,
// when the user opens the menu while the 1st step is displayed.
- (void)testOverflowMenuIPHForHistoryShow2StepsWhenUserOpensMenu {
  if (![ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"The overflow menu IPH only exists when the overflow menu is enabled.")
  }

  // Enable the IPH flag for this test
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.iph_feature_enabled = "IPH_iOSHistoryOnOverflowMenuFeature";
  config.additional_args.push_back("--enable-features=IPHForSafariSwitcher");
  // Force the conditions that allow the iph to show.
  config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
  config.additional_args.push_back("SyncedAndFirstDevice");

  // The IPH appears immediately on startup, so don't open a new tab when the
  // app starts up.
  [[self class] testForStartup];

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:config];

    // The app relaunch (to enable a feature flag) may take a while, therefore
    // the timeout is extended to 15 seconds.
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                                @"BubbleViewLabelIdentifier")
                                    timeout:base::Seconds(15)];

    // Open the tools menu and verify the second tooltip is visible.
    [ChromeEarlGreyUI openToolsMenu];
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        grey_accessibilityID(@"BubbleViewLabelIdentifier")];
  }  // End of the sync disabler scope.
}

// Tests that both the 2 steps of the history on overflow menu IPH is displayed,
// when the user lets the first step times out.
- (void)testOverflowMenuIPHForHistoryShow2StepsWhen1stStepTimeout {
  if (![ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"The overflow menu IPH only exists when the overflow menu is enabled.")
  }

  // Enable the IPH flag for this test
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.iph_feature_enabled = "IPH_iOSHistoryOnOverflowMenuFeature";
  config.additional_args.push_back("--enable-features=IPHForSafariSwitcher");
  // Force the conditions that allow the iph to show.
  config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
  config.additional_args.push_back("SyncedAndFirstDevice");

  // The IPH appears immediately on startup, so don't open a new tab when the
  // app starts up.
  [[self class] testForStartup];

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:config];

    // The app relaunch (to enable a feature flag) may take a while, therefore
    // the timeout is extended to 15 seconds.
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                                @"BubbleViewLabelIdentifier")
                                    timeout:base::Seconds(15)];

    // Wait for the first IPH to time out.
    const int bufferForTimeout = 5;
    [ChromeEarlGrey
        waitForUIElementToDisappearWithMatcher:grey_accessibilityID(
                                                   @"BubbleViewLabelIdentifier")
                                       timeout:
                                           base::Seconds(
                                               (int)kBubbleVisibilityDuration +
                                               bufferForTimeout)];

    // Open the tools menu and verify the second tooltip is visible.
    [ChromeEarlGreyUI openToolsMenu];
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        grey_accessibilityID(@"BubbleViewLabelIdentifier")];
  }  // End of the sync disabler scope.
}

// Tests that the 2nd step of history on overflow menu IPH is not displayed, if
// the 1st step IPH is dismissed by the user by tapping outside.
- (void)testOverflowMenuIPHForHistoryNotShow2ndStep {
  if (![ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"The overflow menu IPH only exists when the overflow menu is enabled.")
  }

  // Enable the IPH flag to ensure the IPH triggers
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.iph_feature_enabled = "IPH_iOSHistoryOnOverflowMenuFeature";
  config.additional_args.push_back("--enable-features=IPHForSafariSwitcher");
  // Force the conditions that allow the iph to show.
  config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
  config.additional_args.push_back("SyncedAndFirstDevice");

  // The IPH appears immediately on startup, so don't open a new tab when the
  // app starts up.
  [[self class] testForStartup];

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:config];

    // The app relaunch (to enable a feature flag) may take a while, therefore
    // the timeout is extended to 15 seconds.
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                                @"BubbleViewLabelIdentifier")
                                    timeout:base::Seconds(15)];
    // Dismiss the IPH by tapping outside.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            @"BubbleViewLabelIdentifier")]
        performAction:grey_tapAtPoint(CGPointMake(0, 0))];

    // Open the tools menu and verify the 2nd step is not shown.
    [ChromeEarlGreyUI openToolsMenu];
    GREYAssert(![ChromeEarlGrey
                   testUIElementAppearanceWithMatcher:
                       grey_accessibilityID(@"BubbleViewLabelIdentifier")],
               @"The 2nd step of the IPH is displayed");
  }  // End of the sync disabler scope.
}

@end
