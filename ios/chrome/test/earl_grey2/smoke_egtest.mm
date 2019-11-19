// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <TestLib/EarlGreyImpl/EarlGrey.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test case to verify that EarlGrey tests can be launched and perform basic
// UI interactions.
@interface SmokeTestCase : ChromeTestCase
@end

@implementation SmokeTestCase

// Tests that a tab can be opened.
- (void)testOpenTab {
  // Open tools menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  // Open new tab.
  // TODO(crbug.com/917114): Calling the string directly is temporary while we
  // roll out a solution to access constants across the code base for EG2.
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(@"kToolsMenuNewTabId");
  [[EarlGrey selectElementWithMatcher:newTabButtonMatcher]
      performAction:grey_tap()];

  // Wait until tab opened and test if there're 2 tabs in total.
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that helpers from chrome_matchers.h are available for use in tests.
- (void)testTapToolsMenu {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  // Tap a second time to close the menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
}

// Tests that helpers from chrome_actions.h are available for use in tests.
- (void)testToggleSettingsSwitch {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuPasswordsButton()]
      performAction:grey_tap()];

  // Toggle the passwords switch off and on.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"savePasswordsItem_switch")]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"savePasswordsItem_switch")]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Close the settings menu.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that helpers from chrome_earl_grey.h are available for use in tests.
- (void)testClearBrowsingHistory {
  [ChromeEarlGrey clearBrowsingHistory];
}

// Tests that string resources are loaded into the ResourceBundle and available
// for use in tests.
- (void)testAppResourcesArePresent {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  NSString* settingsLabel = l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(settingsLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap a second time to close the menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
}

// Tests that helpers in chrome_earl_grey_ui.h are available for use in tests.
- (void)testReload {
  [ChromeEarlGreyUI reload];
}

// Tests navigation-related converted helpers in chrome_earl_grey.h.
- (void)testURLNavigation {
  [ChromeEarlGrey loadURL:GURL("chrome://terms")];
  [ChromeEarlGrey reload];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey goForward];
}

// Tests tab open/close-related converted helpers in chrome_earl_grey.h.
- (void)testTabOpeningAndClosing {
  [ChromeEarlGrey closeAllTabsInCurrentMode];
  [ChromeEarlGrey closeAllIncognitoTabs];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  [ChromeEarlGrey closeAllTabsInCurrentMode];
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey waitForMainTabCount:0];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  [ChromeEarlGrey openNewTab];
}

// Tests bookmark converted helpers in chrome_earl_grey.h.
- (void)testBookmarkHelpers {
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
}

// Tests helpers involving fake sync servers and autofill profiles in
// chrome_earl_grey.h
- (void)testAutofillProfileSyncToFakeServer {
  std::string fakeGUID = "b67e5ca1e09345d0aecfc2155c1f6b11";
  std::string profileName = "testAutofillProfileSyncToFakeServer";

  [ChromeEarlGrey clearAutofillProfileWithGUID:fakeGUID];
  GREYAssertTrue(![ChromeEarlGrey isAutofillProfilePresentWithGUID:fakeGUID
                                               autofillProfileName:profileName],
                 @"Autofill profile should not be present.");
  [ChromeEarlGrey injectAutofillProfileOnFakeSyncServerWithGUID:fakeGUID
                                            autofillProfileName:profileName];
}

// Tests waitForSufficientlyVisibleElementWithMatcher in chrome_earl_grey.h
- (void)testWaitForSufficientlyVisibleElementWithMatcher {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

// Tests sync server converted helpers in chrome_earl_grey.h.
- (void)testSyncServerHelpers {
  [ChromeEarlGrey startSync];
  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:10.0];
  [ChromeEarlGrey clearSyncServerData];
}

// Tests executeJavaScript:error: in chrome_earl_grey.h
- (void)testExecuteJavaScript {
  id actualResult = [ChromeEarlGrey executeJavaScript:@"0"];
  GREYAssertEqualObjects(@0, actualResult,
                         @"Actual JavaScript execution result: %@",
                         actualResult);
}

// Tests typed URL converted helpers in chrome_earl_grey.h.
- (void)testTypedURLHelpers {
  const GURL mockURL("http://not-a-real-site.test/");

  [ChromeEarlGrey addHistoryServiceTypedURL:mockURL];
  [ChromeEarlGrey deleteHistoryServiceTypedURL:mockURL];
}

// Tests accessibility util converted helper in chrome_earl_grey.h.
- (void)testAccessibilityUtil {
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests enabling/disabling features through [AppLaunchManager
// ensureAppLaunchedWithFeaturesEnabled]
- (void)testAppLaunchManagerLaunchWithFeatures {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:
          {kNewOmniboxPopupLayout, web::features::kSlimNavigationManager}
                                  disabled:{}
                              forceRestart:NO];

  GREYAssertTrue([ChromeEarlGrey isNewOmniboxPopupLayoutEnabled],
                 @"NewOmniboxPopupLayout should be enabled");
  GREYAssertTrue([ChromeEarlGrey isSlimNavigationManagerEnabled],
                 @"SlimNavigationManager should be enabled");

  GREYAssertEqual([ChromeEarlGrey mainTabCount], 1U,
                  @"Exactly one new tab should be opened.");
}

// Tests isCompactWidth method in chrome_earl_grey.h.
- (void)testisCompactWidth {
  BOOL expectedIsCompactWidth =
      [[[[GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication] keyWindow]
          traitCollection] horizontalSizeClass] ==
      UIUserInterfaceSizeClassCompact;
  GREYAssertTrue([ChromeEarlGrey isCompactWidth] == expectedIsCompactWidth,
                 @"isCompactWidth should return %@",
                 expectedIsCompactWidth ? @"YES" : @"NO");
}

@end
