// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kPDFPath[] = "/complex_document.pdf";

@interface PDFTestCase : ChromeTestCase
@end

@implementation PDFTestCase

// Regression test for crbug/981893. Repro steps: open a PDF in a new
// tab, switch back and forth betweeen the new tab and the old one by
// swiping in the toolbar. The regression is a crash.
- (void)testSwitchToAndFromPDF {
  // Compact width only.
  if (![ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_DISABLED(@"Disabled on iPad -- depends on swiping in the "
                            @"toolbar to change tabs, which is a compact-"
                            @"only feature.");
  }

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load the first page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Open a new Tab to have a tab to switch to.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];

  id<GREYMatcher> toolbar = chrome_test_util::PrimaryToolbar();

  // Swipe to the first page.
  [[EarlGrey selectElementWithMatcher:toolbar]
      performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Swipe back and forth a few times. If this crashes, there may be a new
  // problem with how WKWebView snapshots PDFs.
  for (int i = 0; i < 3; i++) {
    [[EarlGrey selectElementWithMatcher:toolbar]
        performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];
    [[EarlGrey selectElementWithMatcher:toolbar]
        performAction:grey_swipeFastInDirection(kGREYDirectionRight)];
  }

  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

// Regression test for crbug/981893. Repro steps: open PDFs in two tabs.
// Enter and leave the tab grid. Swipe back and forth repeatedly between
// the two tabs in the toolbar. The regressiom is a crash anywhere in this
// process.
- (void)testSwitchBetweenPDFs {
  // Compact width only.
  if (![ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_DISABLED(@"Disabled on iPad -- depends on swiping in the "
                            @"toolbar to change tabs, which is a compact-"
                            @"only feature.");
  }

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load two PDFs in different tabs.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];

  // Enter the tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  // Leave the tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  id<GREYMatcher> toolbar = chrome_test_util::PrimaryToolbar();
  // Swipe back and forth a few times. If this crashes, there may be a new
  // problem with how WKWebView snapshots PDFs.
  for (int i = 0; i < 3; i++) {
    [[EarlGrey selectElementWithMatcher:toolbar]
        performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];
    [[EarlGrey selectElementWithMatcher:toolbar]
        performAction:grey_swipeFastInDirection(kGREYDirectionRight)];
  }
}

// Regression test for crbug/981893. Repro steps: Open a tab, then navigate
// to a PDF in that tab. Enter the tab grid. Wait five seconds. Exit the
// tab switcher. The regression is a crash anywhere in this process.
- (void)testPDFIntoTabGridAndWait {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load a page, then a PDF
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];

  // Enter the tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];

  // Wait five seconds.
  XCTestExpectation* neverFulfilled =
      [[XCTestExpectation alloc] initWithDescription:@"Wait"];
  XCTWaiterResult result = [XCTWaiter waitForExpectations:@[ neverFulfilled ]
                                                  timeout:5.0];

  GREYAssertTrue(result == XCTWaiterResultTimedOut,
                 @"Was not able to complete wait in tab grid with a PDF tab.");

  // Leave the tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}
@end
