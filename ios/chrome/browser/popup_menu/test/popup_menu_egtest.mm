// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
const char kPDFPath[] = "/testpage.pdf";

// Handles responses for the test server.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  if (request.relative_url == "/page1") {
    response->set_content("page1");
  } else if (request.relative_url == "/page2") {
    response->set_content("page2");
  } else if (request.relative_url == "/page3") {
    response->set_content("page3");
  } else if (request.relative_url == "/page4") {
    response->set_content("page4");
  } else {
    response->set_code(net::HTTP_NOT_FOUND);
  }
  return response;
}
}  // namespace

// Tests for the popup menus.
@interface PopupMenuTestCase : ChromeTestCase
@end

@implementation PopupMenuTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(),
                 @"EmbeddedTestServer failed to start.");
}

// Rotate the device back to portrait if needed, since some tests attempt to run
// in landscape.
- (void)tearDownHelper {
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                   error:nil];
  [super tearDownHelper];
}

#pragma mark - Private

// Returns the launch configuration with the History IPH feature enabled.
- (AppLaunchConfiguration)appConfigurationForHistoryIPH {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.iph_feature_enabled = "IPH_iOSHistoryOnOverflowMenuFeature";
  config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
  config.additional_args.push_back("SyncedAndFirstDevice");
  return config;
}

// Prepares the app by launching it and loading a mock web page, then returns
// the relaunch configuration with the History IPH enabled.
- (AppLaunchConfiguration)prepareAppForHistoryIPH {
  // Launch the app and navigate to a web page to set up the session state.
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  // Prepare relaunch configuration, preserving the session state.
  AppLaunchConfiguration config = [self appConfigurationForHistoryIPH];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

#pragma mark - TabHistory

// Test that the tab history back and forward menus contain the expected entries
// for a series of navigations, and that tapping entries performs the
// appropriate navigation.
- (void)testTabHistoryMenu {
  const GURL URL1 = self.testServer->GetURL("/page1");
  const GURL URL2 = self.testServer->GetURL("/page2");
  const GURL URL3 = self.testServer->GetURL("/page3");
  const GURL URL4 = self.testServer->GetURL("/page4");
  NSString* entry0 = @"New Tab";
  NSString* entry1 = [ChromeEarlGrey displayTitleForURL:URL1];
  NSString* entry2 = [ChromeEarlGrey displayTitleForURL:URL2];
  NSString* entry3 = [ChromeEarlGrey displayTitleForURL:URL3];
  NSString* entry4 = [ChromeEarlGrey displayTitleForURL:URL4];

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
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ContextMenuItemWithAccessibilityLabel(entry0),
              grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     entry1)] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     entry2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     entry3)] assertWithMatcher:grey_notNil()];

  // Tap entry to go back 3 pages, and verify that entry 1 is loaded.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     entry1)] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateVisibleURL:URL1];

  // Long press forward button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      performAction:grey_longPress()];

  // Check that entries 2, 3, and 4 are in the forward tab history menu.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     entry2)] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     entry3)] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     entry4)] assertWithMatcher:grey_notNil()];
  // Tap entry to go forward 2 pages, and verify that entry 3 is loaded.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     entry3)] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateVisibleURL:URL3];
}

#pragma mark - Tools Menu

// Tests that the menu is opened and closed correctly, whatever the current
// device type is.
- (void)testOpenAndCloseToolsMenu {
  // TODO(crbug.com/40817696): This test only fails on ipad bots with
  // multitasking enabled (e.g. compact width).
  if ([ChromeEarlGrey isIPadIdiom] && [ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad multitasking.");
  }
  [ChromeEarlGreyUI openToolsMenu];

  // If using the new overflow menu, swipe up to expand the menu to the full
  // height to make sure that `closeToolsMenu` still closes it.
  if ([ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  }

  [ChromeEarlGreyUI closeToolsMenu];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testNewWindowFromToolsMenu {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

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
  const GURL URL = self.testServer->GetURL(kPDFPath);

  // Navigate to a mock pdf and verify that the find button is disabled.
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> tableViewMatcher =
      grey_accessibilityID(kPopupMenuToolsMenuActionListId);
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
  // The IPH appears immediately on startup, so don't open a new tab when the
  // app starts up.
  [[self class] testForStartup];

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    AppLaunchConfiguration config = [self prepareAppForHistoryIPH];
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:config];

    // The app relaunch may take a while, therefore the timeout is extended to
    // 15 seconds.
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
  // The IPH appears immediately on startup, so don't open a new tab when the
  // app starts up.
  [[self class] testForStartup];

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    AppLaunchConfiguration config = [self prepareAppForHistoryIPH];
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:config];

    // The app relaunch may take a while, therefore the timeout is extended to
    // 15 seconds.
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
  // Enable the IPH flag to ensure the IPH triggers
  AppLaunchConfiguration config = [self appConfigurationForHistoryIPH];

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
