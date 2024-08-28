// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <map>
#import <tuple>

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util.h"

// This test suite only tests javascript in the omnibox. Nothing to do with BVC
// really, the name is a bit misleading.
@interface BrowserViewControllerTestCase : WebHttpServerChromeTestCase
@end

@implementation BrowserViewControllerTestCase

// Tests that the NTP is interactable even when multiple NTP are opened during
// the animation of the first NTP opening. See crbug.com/1032544.
- (void)testPageInteractable {
  // Ensures that the first favicon in Most Visited row is the test URL.
  [ChromeEarlGrey clearBrowsingHistory];
  std::map<GURL, std::string> responses;
  const GURL firstURL = web::test::HttpServer::MakeUrl("http://first");
  responses[firstURL] = "First window";
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:firstURL];

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    [ChromeEarlGrey openNewTab];

    // Wait for 0.05s before opening the new one.
    GREYCondition* myCondition = [GREYCondition conditionWithName:@"Wait block"
                                                            block:^BOOL {
                                                              return NO;
                                                            }];
    std::ignore = [myCondition waitWithTimeout:0.05];

    [ChromeEarlGrey openNewTab];
  }  // End of the sync disabler scope.

  [ChromeEarlGrey waitForMainTabCount:3];

  NSInteger faviconIndex = 0;
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID([NSString
              stringWithFormat:
                  @"%@%li",
                  kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                  faviconIndex])] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:responses[firstURL]];

  [ChromeEarlGrey selectTabAtIndex:1];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID([NSString
              stringWithFormat:
                  @"%@%li",
                  kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                  faviconIndex])] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:responses[firstURL]];
}

// Tests that evaluating JavaScript in the omnibox (e.g, a bookmarklet) works.
// TODO(crbug.com/362621166): Test is flaky.
- (void)DIABLED_testJavaScriptInOmnibox {
  // TODO(crbug.com/40511873): Keyboard entry inside the omnibox fails only on
  // iPad running iOS 10.
  if ([ChromeEarlGrey isIPadIdiom])
    return;

  // Preps the http server with two URLs serving content.
  std::map<GURL, std::string> responses;
  const GURL startURL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  responses[startURL] = "Start";
  responses[destinationURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  // Just load the first URL.
  [ChromeEarlGrey loadURL:startURL];

  // Waits for the page to load and check it is the expected content.
  [ChromeEarlGrey waitForWebStateContainingText:responses[startURL]];

  // In the omnibox, the URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(startURL.GetContent())];

  // Types some javascript in the omnibox to trigger a navigation.
  NSString* script =
      [NSString stringWithFormat:@"javascript:location.href='%s'",
                                 destinationURL.spec().c_str()];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:script];

  // The omnibox popup may update multiple times.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // In the omnibox, the new URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(
                            destinationURL.GetContent())];

  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(destinationURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to the destination url.");

  // Verifies that the destination page is shown.
  [ChromeEarlGrey waitForWebStateContainingText:responses[destinationURL]];
}

// Tests the fix for the regression reported in https://crbug.com/801165.  The
// bug was triggered by opening an HTML file picker and then dismissing it.
- (void)testFixForCrbug801165 {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no action sheet on tablet)");
  }

  std::map<GURL, std::string> responses;
  const GURL testURL = web::test::HttpServer::MakeUrl("http://origin");
  responses[testURL] = "File Picker Test <input id=\"file\" type=\"file\">";
  web::test::SetUpSimpleHttpServer(responses);

  // Load the test page.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"File Picker Test"];

  // Invoke the file picker.
  [ChromeEarlGrey tapWebStateElementWithID:@"file"];

  // Tap on the toolbar to dismiss the file picker on iOS14.  In iOS14 a
  // UIDropShadowView covers the entire app, so tapping anywhere should
  // dismiss the file picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PrimaryToolbar()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];
}

#pragma mark - Open URL

// Tests that BVC properly handles open URL. When NTP is visible, the URL
// should be opened in the same tab (not create a new tab).
- (void)testOpenURLFromNTP {
  [ChromeEarlGrey sceneOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that BVC properly handles open URL. When BVC is showing a non-NTP
// tab, the URL should be opened in a new tab, adding to the tab count.
- (void)testOpenURLFromTab {
  [ChromeEarlGrey loadURL:GURL("https://invalid")];
  [ChromeEarlGrey sceneOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that BVC properly handles open URL. When tab switcher is showing,
// the URL should be opened in a new tab, and BVC should be shown.
- (void)testOpenURLFromTabSwitcher {
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForMainTabCount:0];
  [ChromeEarlGrey sceneOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that the Search Widget URL loads the NTP with the Omnibox focused.
- (void)testOpenSearchWidget {
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/search")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

#pragma mark - Multiwindow

// TODO(crbug.com/40240588): Re-enable this test.
- (void)DISABLED_testMultiWindowURLLoading {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // Preps the http server with two URLs serving content.
  std::map<GURL, std::string> responses;
  const GURL firstURL = web::test::HttpServer::MakeUrl("http://first");
  const GURL secondURL = web::test::HttpServer::MakeUrl("http://second");
  responses[firstURL] = "First window";
  responses[secondURL] = "Second window";
  web::test::SetUpSimpleHttpServer(responses);

  // Loads url in first window.
  [ChromeEarlGrey loadURL:firstURL inWindowWithNumber:0];

  // Opens second window and loads url.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey loadURL:secondURL inWindowWithNumber:1];

  // Checks loads worked.
  [ChromeEarlGrey waitForWebStateContainingText:responses[firstURL]
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:responses[secondURL]
                             inWindowWithNumber:1];

  // Closes first window and renumbers second window as first
  [ChromeEarlGrey closeWindowWithNumber:0];
  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [ChromeEarlGrey changeWindowWithNumber:1 toNewNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:responses[secondURL]
                             inWindowWithNumber:0];

  // Opens a 'new' second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Loads urls in both windows, and verifies.
  [ChromeEarlGrey loadURL:firstURL inWindowWithNumber:0];
  [ChromeEarlGrey loadURL:secondURL inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:responses[firstURL]
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:responses[secondURL]
                             inWindowWithNumber:1];
}

@end
