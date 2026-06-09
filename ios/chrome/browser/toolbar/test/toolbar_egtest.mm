// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
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
  config.features_enabled.push_back(kComposeboxIpad);
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

@end
