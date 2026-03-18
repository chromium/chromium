// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <memory>

#import "base/functional/bind.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {
const char kPrintPath[] = "/printpage";

// Matcher for the cancel button on the printer options view.
id<GREYMatcher> PrintOptionsCancelButton() {
  if (@available(iOS 26, *)) {
    return grey_allOf(grey_accessibilityLabel(@"Close"),
                      grey_kindOfClassName(@"_UIModernBarButton"),
                      grey_kindOfClass([UIButton class]), nil);
  }
  return grey_allOf(grey_accessibilityLabel(@"Cancel"),
                    grey_kindOfClass([UIButton class]), nil);
}

// A request handler that provides the print page HTML.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPrintPath) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_NOT_FOUND);
    return response;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(
      "<html><body>"
      "<input onclick='window.print();' type='button' id=\"printButton\" "
      "value='Print Page' />"
      "</body></html>");
  return std::move(response);
}
}  // namespace

// Test case for bringing up the print dialog when a web site's JavaScript runs
// "window.print".
@interface JSPrintTestCase : ChromeTestCase
@end

@implementation JSPrintTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(),
                 @"EmbeddedTestServer failed to start.");
}

// Tests that tapping a button with onclick='window.print' brings up the
// print dialog.
- (void)testWebPrintButton {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPrintPath)];

  // Tap print button.
  [ChromeEarlGrey tapWebStateElementWithID:@"printButton"];

  // Test if print dialog appeared.
  NSString* dialogTitle = @"Options";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dialogTitle)]
      assertWithMatcher:grey_minimumVisiblePercent(0.7)];

  // Clean up and close print dialog.
  [[EarlGrey selectElementWithMatcher:PrintOptionsCancelButton()]
      performAction:grey_tap()];
}

@end
