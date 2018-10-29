// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#import <EarlGrey/EarlGrey.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
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
  if (IsIPadIdiom())
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
  [ChromeEarlGrey waitForWebViewContainingText:responses[startURL]];

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
  [ChromeEarlGrey waitForWebViewContainingText:responses[destinationURL]];
}

// Tests the fix for the regression reported in https://crbug.com/801165.  The
// bug was triggered by opening an HTML file picker and then dismissing it.
- (void)testFixForCrbug801165 {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no action sheet on tablet)");
  }

  std::map<GURL, std::string> responses;
  const GURL testURL = web::test::HttpServer::MakeUrl("http://origin");
  responses[testURL] = "File Picker Test <input id=\"file\" type=\"file\">";
  web::test::SetUpSimpleHttpServer(responses);

  // Load the test page.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebViewContainingText:"File Picker Test"];

  // Invoke the file picker and tap on the "Cancel" button to dismiss the file
  // picker.
  [ChromeEarlGrey tapWebViewElementWithID:@"file"];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

@end
