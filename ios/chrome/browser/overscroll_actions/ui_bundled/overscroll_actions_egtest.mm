// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using chrome_test_util::NTPCollectionView;
using chrome_test_util::OverscrollSwipe;
using chrome_test_util::WebViewMatcher;

namespace {

// A basic Hello World HTML body.
const char kHTMLHelloWorld[] = "<html><head><title>Hello World</title></head>"
                               "<body>Hello World</body></html>";

// An alternate HTML body to return when the page is reloaded.
const char kHTMLReloaded[] = "<html><head><title>reloaded</title></head>"
                             "<body>reloaded</body></html>";

// Handles HTTP responses for the embedded test server.
std::unique_ptr<net::test_server::HttpResponse> PageHttpResponse(
    const std::string* html,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(*html);
  return std::move(http_response);
}

}  // namespace

@interface OverscrollActionsTestCase : ChromeTestCase
@end

@implementation OverscrollActionsTestCase {
  // Contains the HTML response that the embedded server will return.
  std::string _HTMLResponse;
}

- (void)setUp {
  [super setUp];
  _HTMLResponse = kHTMLHelloWorld;
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&PageHttpResponse, &_HTMLResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Helper method to load a WebView.
- (void)loadWebview {
  GURL url = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Hello World"];
}

#pragma mark - OverscrollActionsTestCase Tests

// Tests that the NTP can be closed using the Overscroll Actions.
- (void)testNTPClose {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Overscroll Actions are only on iPhone.");
  }
  [[EarlGrey selectElementWithMatcher:NTPCollectionView()]
      performAction:OverscrollSwipe(kGREYDirectionRight)];
  [ChromeEarlGrey waitForMainTabCount:0];
}

// Tests that a new tab can be opened via Overscroll Actions on the NTP.
- (void)testNTPAddTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Overscroll Actions are only on iPhone.");
  }
  [[EarlGrey selectElementWithMatcher:NTPCollectionView()]
      performAction:OverscrollSwipe(kGREYDirectionLeft)];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that a WebView can be closed via Overscroll Actions.
// TODO(crbug.com/443702124): Re-enable when fixed.
- (void)DISABLED_testWebViewClose {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Overscroll Actions are only on iPhone.");
  }
  [self loadWebview];
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:OverscrollSwipe(kGREYDirectionRight)];
  [ChromeEarlGrey waitForMainTabCount:0];
}

// Tests that a new tab can be opened via Overscroll Actions on a WebView.
// TODO(crbug.com/443702124): Re-enable when fixed.
- (void)DISABLED_testWebViewAddTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Overscroll Actions are only on iPhone.");
  }
  [self loadWebview];
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:OverscrollSwipe(kGREYDirectionLeft)];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that a WebView can be reloaded via the Overscroll Actions.
// TODO(crbug.com/443702124): Re-enable when fixed.
- (void)DISABLED_testWebViewReload {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Overscroll Actions are only on iPhone.");
  }
  [self loadWebview];
  // Change the HTML body that will be returned on reload.
  _HTMLResponse = kHTMLReloaded;
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:OverscrollSwipe(kGREYDirectionDown)];
  [ChromeEarlGrey waitForWebStateContainingText:"reloaded"];
}

@end
