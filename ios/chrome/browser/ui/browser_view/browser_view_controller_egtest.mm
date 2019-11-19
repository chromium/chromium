// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#include "base/feature_list.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This test suite only tests javascript in the omnibox. Nothing to do with BVC
// really, the name is a bit misleading.
@interface BrowserViewControllerTestCase : ChromeTestCase
@end

@implementation BrowserViewControllerTestCase

// Tests that evaluating JavaScript in the omnibox (e.g, a bookmarklet) works.
- (void)testJavaScriptInOmnibox {
  // TODO(crbug.com/703855): Keyboard entry inside the omnibox fails only on
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
      [NSString stringWithFormat:@"javascript:location.href='%s'\n",
                                 destinationURL.spec().c_str()];
  [ChromeEarlGreyUI focusOmniboxAndType:script];

  // In the omnibox, the new URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(
                            destinationURL.GetContent())];

  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(destinationURL,
                  chrome_test_util::GetCurrentWebState()->GetVisibleURL(),
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

  // Invoke the file picker and tap on the "Cancel" button to dismiss the file
  // picker.
  [ChromeEarlGrey tapWebStateElementWithID:@"file"];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

#pragma mark - Open URL

// Tests that BVC properly handles open URL. When NTP is visible, the URL
// should be opened in the same tab (not create a new tab).
- (void)testOpenURLFromNTP {
  id<UIApplicationDelegate> appDelegate =
      [[UIApplication sharedApplication] delegate];
  [appDelegate application:[UIApplication sharedApplication]
                   openURL:[NSURL URLWithString:@"https://anything"]
                   options:[NSDictionary dictionary]];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  // TODO(crbug.com/931280): This should be 1, but for the time being will be 2
  // to work around an NTP bug.
  int mainTabCount = 1;
  if ([ChromeEarlGrey isBlockNewTabPagePendingLoadEnabled])
    mainTabCount = 2;
  [ChromeEarlGrey waitForMainTabCount:mainTabCount];
}

// Tests that BVC properly handles open URL. When BVC is showing a non-NTP
// tab, the URL should be opened in a new tab, adding to the tab count.
- (void)testOpenURLFromTab {
  [ChromeEarlGrey loadURL:GURL("https://invalid")];
  id<UIApplicationDelegate> appDelegate =
      [[UIApplication sharedApplication] delegate];
  [appDelegate application:[UIApplication sharedApplication]
                   openURL:[NSURL URLWithString:@"https://anything"]
                   options:[NSDictionary dictionary]];
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
  id<UIApplicationDelegate> appDelegate =
      [[UIApplication sharedApplication] delegate];
  [appDelegate application:[UIApplication sharedApplication]
                   openURL:[NSURL URLWithString:@"https://anything"]
                   options:[NSDictionary dictionary]];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

#pragma mark - WebState visibility

// Tests that WebStates are properly marked as shown or hidden when switching
// tabs.
- (void)testWebStateVisibilityAfterTabSwitch {
  const GURL testURL = web::test::HttpServer::MakeUrl("http://origin");
  const std::string testPageContents("Test Page");

  std::map<GURL, std::string> responses;
  responses[testURL] = testPageContents;
  web::test::SetUpSimpleHttpServer(responses);

  // Load the test page.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:testPageContents];
  web::WebState* firstWebState = chrome_test_util::GetCurrentWebState();

  // And do the same in a second tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:testPageContents];
  web::WebState* secondWebState = chrome_test_util::GetCurrentWebState();

  // Check visibility before and after switching tabs.
  GREYAssert(secondWebState->IsVisible(), @"secondWebState not visible");
  GREYAssert(!firstWebState->IsVisible(),
             @"firstWebState unexpectedly visible");

  [ChromeEarlGrey selectTabAtIndex:0];
  GREYAssert(firstWebState->IsVisible(), @"firstWebState not visible");
  GREYAssert(!secondWebState->IsVisible(),
             @"secondWebState unexpectedly visible");

  [ChromeEarlGrey selectTabAtIndex:1];
  GREYAssert(secondWebState->IsVisible(), @"secondWebState not visible");
  GREYAssert(!firstWebState->IsVisible(),
             @"firstWebState unexpectedly visible");
}

@end
