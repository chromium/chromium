// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#include <map>

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Matcher for the cancel button on the printer options view.
id<GREYMatcher> PrintOptionsCancelButton() {
  return grey_allOf(grey_accessibilityLabel(@"Cancel"),
                    grey_kindOfClass([UIButton class]), nil);
}
}  // namespace

// Test case for bringing up the print dialog when a web site's JavaScript runs
// "window.print".
@interface JSPrintTestCase : ChromeTestCase
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
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Printer Options")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Clean up and close print dialog.
  [[EarlGrey selectElementWithMatcher:PrintOptionsCancelButton()]
      performAction:grey_tap()];
}

@end
