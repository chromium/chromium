// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <functional>
#import <string>

#import <TargetConditionals.h>

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

namespace {
// Returns ERR_CONNECTION_CLOSED error message.
std::string GetErrorMessage() {
  return net::ErrorToShortString(net::ERR_CONNECTION_CLOSED);
}

const std::string kRedirectPage = "/redirect-page.html";

// Provides responses for the different pages.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == kRedirectPage) {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_code(net::HTTP_MOVED_PERMANENTLY);
    result->AddCustomHeader("Location", "data:text/plain,Hello World");
    return std::move(result);
  }

  return nullptr;
}

}  // namespace

// Tests critical user journeys reloated to page load errors.
@interface ErrorPageTestCase : ChromeTestCase
// YES if test server is replying with valid HTML content (URL query). NO if
// test server closes the socket.
@property(atomic) bool serverRespondsWithContent;
@end

@implementation ErrorPageTestCase
@synthesize serverRespondsWithContent = _serverRespondsWithContent;

- (void)setUp {
  [super setUp];

  RegisterDefaultHandlers(self.testServer);
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/echo-query",
      base::BindRepeating(&testing::HandleEchoQueryOrCloseSocket,
                          std::cref(_serverRespondsWithContent))));
  self.testServer->RegisterDefaultHandler(
      base::BindRepeating(&StandardResponse));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the error page is correctly displayed after navigating back to it
// multiple times. See http://crbug.com/944037 .
// TODO:(crbug.com/1185639): Re-enable this test on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testBackForwardErrorPage FLAKY_testBackForwardErrorPage
#else
#define MAYBE_testBackForwardErrorPage testBackForwardErrorPage
#endif
- (void)MAYBE_testBackForwardErrorPage {
  // TODO(crbug.com/40159013): Going back/forward on the same host is failing.
  // Use chrome:// to have a different hosts.
  std::string errorText = net::ErrorToShortString(net::ERR_INVALID_URL);
  self.serverRespondsWithContent = YES;

  [ChromeEarlGrey loadURL:GURL("chrome://invalid")];
  [ChromeEarlGrey waitForWebStateContainingText:errorText];
  // Add some delay otherwise the back/forward navigations are occurring too
  // fast.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.2));

  // Navigate to a page which responds.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?bar")];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.2));

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:errorText];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.2));

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.2));

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:errorText];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.2));

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];
}

// Loads the URL which fails to load, then sucessfully navigates back/forward to
// the page.
// TODO:(crbug.com/1185639): Re-enable this test on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testNavigateForwardToErrorPage \
  FLAKY_testNavigateForwardToErrorPage
#else
#define MAYBE_testNavigateForwardToErrorPage testNavigateForwardToErrorPage
#endif
- (void)MAYBE_testNavigateForwardToErrorPage {
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?bar")];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];

  // No response leads to ERR_CONNECTION_CLOSED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];

  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];

  // Navigate forward to the error page, which should load without errors.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];
}

// Loads the URL which fails to load, then sucessfully reloads the page.
- (void)testReloadErrorPage {
  // No response leads to ERR_CONNECTION_CLOSED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];

  // Reload the page, which should load without errors.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];
}

// Sucessfully loads the page, stops the server and reloads the page.
- (void)testReloadPageAfterServerIsDown {
  // Sucessfully load the page.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];

  // Reload the page, no response leads to ERR_CONNECTION_CLOSED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];
}

// Loads a URL then restore the session and fail during the reload
- (void)testRestoreErrorPage {
  // Load the page.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];
  GREYAssertEqual(1, [ChromeEarlGrey navigationBackListItemsCount],
                  @"The navigation back list should have only 1 entries before "
                  @"the restoration.");

  // Restore the session but with the page no longer loading.
  self.serverRespondsWithContent = NO;
  [self triggerRestoreByRestartingApplication];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];

  GREYAssertEqual(1, [ChromeEarlGrey navigationBackListItemsCount],
                  @"The navigation back list should still have only 1 entries "
                  @"after the restoration.");
}

// Loads a URL which redirect to a data URL and check that the navigation is
// blocked on the first URL.
- (void)testRedirectToData {
  // Disable the test on iOS 16.4 as WKWebView handles this internally now as of
  // https://bugs.webkit.org/show_bug.cgi?id=230158
  // TODO(crbug.com/40267045): Remove redirect logic completely when dropping
  // iOS 16.
  if (@available(iOS 16.4, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 16.4.");
  }
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kRedirectPage)];
  [ChromeEarlGrey waitForWebStateContainingText:net::ErrorToShortString(
                                                    net::ERR_UNSAFE_REDIRECT)];
  [ChromeEarlGrey waitForWebStateContainingText:kRedirectPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kRedirectPage)];
}

@end
