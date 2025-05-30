// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "testing/gmock/include/gmock/gmock-matchers.h"

// Tests interactions with Reader Mode on a web page.
@interface ReaderModeTestCase : ChromeTestCase
@end

@implementation ReaderModeTestCase

- (void)setUp {
  [super setUp];
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kEnableReaderMode);

  if ([self isRunningTest:@selector
            (testNotEligibleReaderModePageEnabledInToolsMenu)]) {
    config.features_disabled.push_back(
        kEnableReaderModePageEligibilityForToolsMenu);
  } else {
    config.features_enabled.push_back(
        kEnableReaderModePageEligibilityForToolsMenu);
  }
  return config;
}

#pragma mark - Helpers

// Asserts that the matcher has the expected content in the Tools Menu.
- (void)assertReaderModeInToolsMenuWithMatcher:(id<GREYMatcher>)matcher {
  [ChromeEarlGreyUI openToolsMenu];

  id<GREYMatcher> tableViewMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
  id<GREYMatcher> readerModeButtonMatcher =
      grey_allOf(grey_accessibilityID(kToolsMenuReaderMode),
                 grey_accessibilityTrait(UIAccessibilityTraitButton),
                 grey_sufficientlyVisible(), nil);

  [[[EarlGrey selectElementWithMatcher:readerModeButtonMatcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:tableViewMatcher] assertWithMatcher:matcher];

  [ChromeEarlGreyUI closeToolsMenu];
}

#pragma mark - Tests

// Tests that the user can show / hide Reader Mode from the tools menu
// entrypoint on an eligible web page.
- (void)testToggleReaderModeInToolsMenuForDistillablePage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];

  // Close Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  // The Reader Mode UI is not visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
}

// Test that a page that is not eligible for Reader Mode shows as a disabled
// option in the Tools menu.
- (void)testNotEligibleReaderModePageDisabledInToolsMenu {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  [self assertReaderModeInToolsMenuWithMatcher:
            grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)];
}

// Test that a page that is not eligible for Reader Mode shows as an enabled
// option in the Tools menu when there is no page eligibility criteria.
- (void)testNotEligibleReaderModePageEnabledInToolsMenu {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  [self assertReaderModeInToolsMenuWithMatcher:
            grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))];
}

// Tests that swiping between a Reader Mode web state and a normal web
// state shows the expected view.
- (void)testSideSwipeReaderMode {
  const GURL readerModeURL = self.testServer->GetURL("/article.html");
  [ChromeEarlGrey loadURL:readerModeURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];

  // Open a new Tab with an article to have a tab to switch to.
  [ChromeEarlGreyUI openNewTab];
  const GURL nonReaderModeURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:nonReaderModeURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Side swipe on the toolbar.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"PrimaryToolbarView")]
      performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];

  // Reader Mode view is visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(readerModeURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to Reader Mode url.");

  // Side swipe back to the non-Reader mode page on the toolbar.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"PrimaryToolbarView")]
      performAction:grey_swipeSlowInDirection(kGREYDirectionLeft)];

  // Non-Reader Mode view is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(nonReaderModeURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to non-Reader Mode url.");
}

// Tests that the user can show Reader Mode from the contextual panel entrypoint
// on an eligible web page.
- (void)testToggleReaderModeInContextualPanelEntrypointForDistillablePage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(@"ContextualPanelEntrypointImageViewAXID")];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
}

@end
