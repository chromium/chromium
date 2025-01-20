// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using chrome_test_util::RegularTabGrid;
using chrome_test_util::ScrollToTop;
using chrome_test_util::TabGridCellAtIndex;

const char kPDFPath[] = "/complex_document.pdf";
const char kGreenPDFPath[] = "/green.pdf";

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

  [ChromeEarlGreyUI openTabGrid];

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

  // Load a page, then a PDF.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];

  [ChromeEarlGreyUI openTabGrid];

  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(5));

  // Leave the tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests the center color of the grid tab showing a PDF. (physical device only)
- (void)testCenterColorOfPDFTabGrid {
#if TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_SKIPPED(@"The API to take a snapshot is not working correctly "
                         @"and it becomes black on simulator.");
#endif

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load a page, then a PDF.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kGreenPDFPath)];

  // Open more than 6 pages to scroll down/up in a tab grid.
  for (int i = 0; i < 10; i++) {
    [ChromeEarlGreyUI openNewTab];
    [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  }

  // Open a tab grid and then scroll up to make a tab grid cell recycled and
  // re-created.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:RegularTabGrid()]
      performAction:ScrollToTop()];

  // Take a snapshot of the tab grid showing a PDF.
  EDORemoteVariable<UIImage*>* tabGridSnapshot =
      [[EDORemoteVariable alloc] init];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(0)]
      performAction:grey_snapshot(tabGridSnapshot)];

  // Get a color of the center position in a tab grid.
  CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
  [self getCenterColor:tabGridSnapshot.object
                   red:&red
                 green:&green
                  blue:&blue
                 alpha:&alpha];

  // The values may not be exactly 0.0 or 1.0 due to the image compression. The
  // test allows more flexible values.
  GREYAssert(
      red < 0.1,
      @"A red value of the center color in a tab grid should be close to 0.");
  GREYAssert(
      green > 0.9,
      @"A green value of the center color in a tab grid should be close to 1.");
  GREYAssert(
      blue < 0.1,
      @"A blue value of the center color in a tab grid should be close to 0.");
  GREYAssert(
      alpha > 0.9,
      @"A alpha value of the center color in a tab grid should be close to 1.");
}

#pragma mark - Helper methods

- (void)getCenterColor:(UIImage*)image
                   red:(CGFloat*)red
                 green:(CGFloat*)green
                  blue:(CGFloat*)blue
                 alpha:(CGFloat*)alpha {
  CGImageRef imageRef = [image CGImage];

  NSUInteger width = CGImageGetWidth(imageRef);
  NSUInteger height = CGImageGetHeight(imageRef);
  NSUInteger x = width / 2;
  NSUInteger y = height / 2;

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  unsigned char* data =
      (unsigned char*)calloc(height * width * 4, sizeof(unsigned char));
  NSUInteger bytesPerPixel = 4;
  NSUInteger bytesPerRow = bytesPerPixel * width;
  NSUInteger bitsPerComponent = 8;
  CGContextRef context =
      CGBitmapContextCreate(data, width, height, bitsPerComponent, bytesPerRow,
                            colorSpace, kCGImageAlphaPremultipliedLast);
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), imageRef);

  NSUInteger index = (bytesPerRow * y) + x * bytesPerPixel;
  *red = ((CGFloat)data[index]) / 255.0f;
  *green = ((CGFloat)data[index + 1]) / 255.0f;
  *blue = ((CGFloat)data[index + 2]) / 255.0f;
  *alpha = ((CGFloat)data[index + 3]) / 255.0f;

  CGColorSpaceRelease(colorSpace);
  CGContextRelease(context);
  free(data);
}

@end
