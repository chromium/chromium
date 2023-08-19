// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#import <map>

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"

namespace {
// Matcher for the cancel button on the printer options view.
id<GREYMatcher> PrintOptionsCancelButton() {
  return grey_allOf(grey_accessibilityLabel(@"Cancel"),
                    grey_kindOfClass([UIButton class]), nil);
}
}  // namespace

// Test case for bringing up the print dialog when a web site's JavaScript runs
// "window.print".
@interface JSPrintTestCase : WebHttpServerChromeTestCase
@end

@implementation JSPrintTestCase

// Tests that tapping a button with onclick='window.print' brings up the
// print dialog.
- (void)testWebPrintButton {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL testURL = web::test::HttpServer::MakeUrl("http://printpage");

  // Page containing button with onclick attribute calling window.print.
  responses[testURL] =
      "<input onclick='window.print();' type='button' id=\"printButton\" "
      "value='Print Page' />";

  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:testURL];

  // Tap print button.
  [ChromeEarlGrey tapWebStateElementWithID:@"printButton"];

  // Test if print dialog appeared.
  NSString* dialogTitle = @"Print Options";
  if (@available(iOS 17, *)) {
    dialogTitle = @"Options";
  }
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dialogTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Clean up and close print dialog.
  [[EarlGrey selectElementWithMatcher:PrintOptionsCancelButton()]
      performAction:grey_tap()];
}

@end
