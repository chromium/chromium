// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/platform_thread.h"
#import "base/time/time.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::ButtonWithAccessibilityLabelId;

namespace {

// Text appearing on the navigation test page.
const char kPageText[] = "Navigation testing page";

// Waits for EG matcher element to be sufficiently visible. Useful when EG UI
// sync is disabled.
void WaitForMatcherVisible(id<GREYMatcher> matcher,
                           NSString* matcher_description) {
  ConditionBlock wait_for_matcher = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, wait_for_matcher),
      @"Failed to wait %@ to be visible.", matcher_description);
}

// Handler for the infinite pending response.
std::unique_ptr<net::test_server::HttpResponse> HandleInfiniteRequest(
    net::test_server::EmbeddedTestServer* test_server,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == "/infinite") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(
        base::StringPrintf("<p>%s</p><img src='%s'/>", kPageText,
                           test_server->GetURL("/resource").spec().c_str()));
    return response;
  }
  if (request.GetURL().path() == "/resource") {
    return std::make_unique<net::test_server::HungResponse>();
  }
  return nullptr;
}

}  // namespace

// Test case for Stop Loading button.
@interface StopLoadingTestCase : ChromeTestCase
@end

@implementation StopLoadingTestCase

// Tests that tapping "Stop" button stops the loading.
- (void)testStopLoading {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&HandleInfiniteRequest, self.testServer));
  GREYAssertTrue(self.testServer->Start(), @"Server failed to start.");

  // Load a page which never finishes loading.
  GURL infinitePendingURL = self.testServer->GetURL("/infinite");

  // EG synchronizes with WKWebView. Disable synchronization for EG interation
  // during when page is loading.
  ScopedSynchronizationDisabler disabler;

  [ChromeEarlGrey loadURL:infinitePendingURL waitForCompletion:NO];
  // Wait until the page is half loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];
  if (![ChromeEarlGrey isIPadIdiom]) {
    // On iPhone Stop/Reload button is a part of tools menu, so open it.
    [ChromeEarlGreyUI openToolsMenu];
  }
  // Sleep for UI change because synchronization is disabled.
  base::PlatformThread::Sleep(base::Seconds(1));

  // Wait and verify that stop button is visible and reload button is hidden.
  WaitForMatcherVisible(chrome_test_util::StopButton(), @"stop button");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
      assertWithMatcher:grey_notVisible()];

  // Stop the page loading.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::StopButton()]
      performAction:grey_tap()];
  // Sleep for UI change because synchronization is disabled.
  base::PlatformThread::Sleep(base::Seconds(1));
  if (![ChromeEarlGrey isIPadIdiom]) {
    // On iPhone Stop/Reload button is a part of tools menu, so open it.
    [ChromeEarlGreyUI openToolsMenu];
  }

  // Wait and verify that reload button is visible and stop button is hidden.
  WaitForMatcherVisible(chrome_test_util::ReloadButton(), @"reload button");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::StopButton()]
      assertWithMatcher:grey_notVisible()];
}

@end
