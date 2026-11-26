// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

@interface ToolbarTestCase : ChromeTestCase
@end

@implementation ToolbarTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kChromeNextIa);
  if ([self isRunningTest:@selector(testLandscapeBackButtonInRTL)]) {
    // Launch the app in Arabic to enable an RTL layout.
    config.language = "ar";
  }
  return config;
}

// Tests loading a page and checking that the URL is displayed in the location
// bar.
- (void)testLoadPage {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL("/echo");

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::DefocusedLocationView(),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:chrome_test_util::LocationViewContainingText(
                            self.testServer->base_url().GetHost())];
}

// Tests that long-pressing the Tab Grid button displays the context menu with
// expected actions.
- (void)testTabGridButtonLongPress {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is only supported on iPad as the tab "
                            @"grid button is iPad-only under Next IA.");
  }

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL("/echo");

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Long press the tab grid button.
  id<GREYMatcher> tabGridButton =
      grey_allOf(grey_accessibilityID(kToolbarTabGridButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:tabGridButton];
  [[EarlGrey selectElementWithMatcher:tabGridButton]
      performAction:grey_longPress()];

  // Verify that the context menu is displayed with expected actions.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_NEW_TAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_CLOSE_TAB)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the toolbar Back button remains visible and tappable (not obscured
// by the App Bar) in both landscape orientations when using an RTL language.
// Regression test for crbug.com/535516021.
- (void)testLandscapeBackButtonInRTL {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Side App Bar in landscape is only shown on iPhone.");
  }

  GREYAssertTrue([ChromeEarlGrey isRTL],
                 @"App should be running with an RTL layout.");
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  const GURL firstURL = self.testServer->GetURL("/destination.html");
  const GURL secondURL = self.testServer->GetURL("/echo");

  [self setTearDownHandler:^{
    [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                     error:nil];
  }];

  const UIInterfaceOrientation landscapeOrientations[] = {
      UIInterfaceOrientationLandscapeLeft,
      UIInterfaceOrientationLandscapeRight,
  };

  for (UIInterfaceOrientation orientation : landscapeOrientations) {
    [ChromeEarlGrey loadURL:firstURL];
    [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
    [ChromeEarlGrey loadURL:secondURL];
    [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

    GREYAssertTrue([EarlGrey rotateInterfaceToOrientation:orientation
                                                    error:nil],
                   @"Failed to rotate to landscape orientation.");
    [ChromeEarlGreyUI waitForAppToIdle];

    // Verify the App Bar is visible and does not obscure either edge of the
    // toolbar (Tools Menu on the left, Back button on the right in RTL).
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kAppBarTabGridButtonIdentifier),
                                            grey_ancestor(grey_kindOfClassName(
                                                @"AppBarView")),
                                            nil)]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kAppBarNewTabButtonIdentifier),
                                            grey_ancestor(grey_kindOfClassName(
                                                @"AppBarView")),
                                            nil)]
        assertWithMatcher:grey_sufficientlyVisible()];

    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        assertWithMatcher:grey_interactable()];

    id<GREYMatcher> backButton = chrome_test_util::BackButton();
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:backButton];
    [[EarlGrey selectElementWithMatcher:backButton]
        assertWithMatcher:grey_interactable()];
    [[EarlGrey selectElementWithMatcher:backButton] performAction:grey_tap()];
    [ChromeEarlGrey waitForWebStateVisibleURL:firstURL];
  }
}

@end
