// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

const char kPageWithRedColor[] = "/red_page.html";
const char kPageWithGreenAndBlueColor[] = "/green_and_blue_page.html";

// Handler for the test server. Depending of the URL of the page, it returns a
// page filled with different color.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == kPageWithRedColor) {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    result->set_content(R"(<html>
<head>
  <style>
    .red {
      background-color: #ff0000;
      width: 100%;
      height: 100%;
    }
  </style>
</head>
<body>
  <div class='red'>red</div>
</body></html>)");
    return std::move(result);
  }
  if (request.GetURL().path() == kPageWithGreenAndBlueColor) {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    result->set_content(R"(<html>
<head>
  <style>
    .green {
      background-color: #00ff00;
      width: 100%;
      height: 50%;
    }
    .blue {
      background-color: #0000ff;
      width: 100%;
      height: 100%;
    }
  </style>
</head>
<body>
  <div class='green'>green</div>
  <div class='blue'></div>
</body></html>)");
    return std::move(result);
  }
  return nullptr;
}

}  // namespace

@interface SnapshotTestCase : ChromeTestCase
@end

@implementation SnapshotTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests the snapshot of the page filled with one solid color.
- (void)testOneColorSnapshot {
  // Open a page filled with one solid color.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageWithRedColor)];
  [ChromeEarlGrey waitForWebStateContainingText:"red"];
  [ChromeEarlGreyUI openTabGrid];

  // Take a snapshot of the first cell in the tab grid.
  EDORemoteVariable<UIImage*>* tabGridSnapshot =
      [[EDORemoteVariable alloc] init];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_snapshot(tabGridSnapshot)];
  UIImage* image = tabGridSnapshot.object;

  // Use `CGImageGetWidth()` instead of `image.size.width` because the value can
  // be different. CGImage is used in `-getColorAtPoint:`.
  const NSUInteger width = CGImageGetWidth(image.CGImage);
  const NSUInteger height = CGImageGetHeight(image.CGImage);
  const CGPoint center = CGPointMake(width / 2, height / 2);

  // Check a color of the center position in the image.
  CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
  [self getColorAtPoint:center
                  image:image
                    red:&red
                  green:&green
                   blue:&blue
                  alpha:&alpha];

  // The color must be red.
  // The values may not be exactly 0.0 or 1.0 due to the image compression. The
  // test allows more flexible values.
  GREYAssert(red > 0.9, @"A red value should be close to 1.");
  GREYAssert(green < 0.5, @"A green value should be close to 0.");
  GREYAssert(blue < 0.5, @"A blue value should be close to 0.");
  GREYAssert(alpha > 0.9, @"A alpha value should be close to 1.");
}

- (void)testTwoColorsSnapshot {
  // Open a page filled with 2 colors.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageWithGreenAndBlueColor)];
  [ChromeEarlGrey waitForWebStateContainingText:"green"];
  [ChromeEarlGreyUI openTabGrid];

  // Take a snapshot of the first cell in the tab grid.
  EDORemoteVariable<UIImage*>* tabGridSnapshot =
      [[EDORemoteVariable alloc] init];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_snapshot(tabGridSnapshot)];
  UIImage* image = tabGridSnapshot.object;

  // Use `CGImageGetWidth()` instead of `image.size.width` because the value can
  // be different. CGImage is used in `-getColorAtPoint:`.
  const NSUInteger width = CGImageGetWidth(image.CGImage);
  const NSUInteger height = CGImageGetHeight(image.CGImage);

  // Check a color of the upper side in the image.
  {
    const CGPoint pos = CGPointMake(width / 2, height / 2 - 10);
    CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
    [self getColorAtPoint:pos
                    image:image
                      red:&red
                    green:&green
                     blue:&blue
                    alpha:&alpha];

    // The color must be green.
    // The values may not be exactly 0.0 or 1.0 due to the image compression.
    // The test allows more flexible values.
    GREYAssert(red < 0.5, @"A red value should be close to 0.");
    GREYAssert(green > 0.9, @"A green value should be close to 1.");
    GREYAssert(blue < 0.5, @"A blue value should be close to 0.");
    GREYAssert(alpha > 0.9, @"A alpha value should be close to 1.");
  }

  // Check a color of the lower side in the image.
  {
    const CGPoint pos = CGPointMake(width / 2, height - 10);
    CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
    [self getColorAtPoint:pos
                    image:image
                      red:&red
                    green:&green
                     blue:&blue
                    alpha:&alpha];

    // The color must be blue.
    // The values may not be exactly 0.0 or 1.0 due to the image compression.
    // The test allows more flexible values.
    GREYAssert(red < 0.5, @"A red value should be close to 0.");
    GREYAssert(green < 0.5, @"A green value should be close to 0.");
    GREYAssert(blue > 0.9, @"A blue value should be close to 1.");
    GREYAssert(alpha > 0.9, @"A alpha value should be close to 1.");
  }
}

// Tests the snapshot of the page filled with 2 colors. The upper side is green
// and the lower side is blue in the page. A snapshot is taken 2 times with the
// same position before and after scrolling down.
- (void)testSnapshotWithScrollDown {
  // Open a page filled with 2 colors.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageWithGreenAndBlueColor)];
  [ChromeEarlGrey waitForWebStateContainingText:"green"];

  // Take a snapshot of the first cell in the tab grid.
  {
    [ChromeEarlGreyUI openTabGrid];
    EDORemoteVariable<UIImage*>* tabGridSnapshot =
        [[EDORemoteVariable alloc] init];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
        performAction:grey_snapshot(tabGridSnapshot)];
    UIImage* image = tabGridSnapshot.object;

    // Use `CGImageGetWidth()` instead of `image.size.width` because the value
    // can be different. CGImage is used in `-getColorAtPoint:`.
    const NSUInteger width = CGImageGetWidth(image.CGImage);
    const NSUInteger height = CGImageGetHeight(image.CGImage);
    const CGPoint center = CGPointMake(width / 2, height / 2);

    // Check a color of the center position in the image.
    CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
    [self getColorAtPoint:center
                    image:image
                      red:&red
                    green:&green
                     blue:&blue
                    alpha:&alpha];

    // The color must be green. The values may not be exactly 0.0 or 1.0 due to
    // the image compression. The test allows more flexible values.
    GREYAssert(red < 0.5, @"A red value should be close to 0.");
    GREYAssert(green > 0.9, @"A green value should be close to 1.");
    GREYAssert(blue < 0.5, @"A blue value should be close to 0.");
    GREYAssert(alpha > 0.9, @"A alpha value should be close to 1.");
  }

  // Open the same page again and scroll down to change the visible area.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Scroll up a little bit to make the tab grid button visible.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionUp, 50)];

  // Go back to the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  // Take a snapshot of the first cell in the tab grid again. The snapshot
  // should be updated.
  {
    EDORemoteVariable<UIImage*>* tabGridSnapshot =
        [[EDORemoteVariable alloc] init];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
        performAction:grey_snapshot(tabGridSnapshot)];
    UIImage* image = tabGridSnapshot.object;

    // Use `CGImageGetWidth()` instead of `image.size.width` because the value
    // can be different. CGImage is used in `-getColorAtPoint:`.
    const NSUInteger width = CGImageGetWidth(image.CGImage);
    const NSUInteger height = CGImageGetHeight(image.CGImage);
    const CGPoint center = CGPointMake(width / 2, height / 2);

    // Check a color of the center position in the image.
    CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
    [self getColorAtPoint:center
                    image:image
                      red:&red
                    green:&green
                     blue:&blue
                    alpha:&alpha];

    // The color must be blue now because the page was scrolled down.
    // The values may not be exactly 0.0 or 1.0 due to the image compression.
    // The test allows more flexible values.
    GREYAssert(red < 0.5, @"A red value should be close to 0.");
    GREYAssert(green < 0.5, @"A green value should be close to 0.");
    GREYAssert(blue > 0.9, @"A blue value should be close to 1.");
    GREYAssert(alpha > 0.9, @"A alpha value should be close to 1.");
  }
}

#pragma mark - Helper methods

- (void)getColorAtPoint:(CGPoint)point
                  image:(UIImage*)image
                    red:(CGFloat*)red
                  green:(CGFloat*)green
                   blue:(CGFloat*)blue
                  alpha:(CGFloat*)alpha {
  CFDataRef pixelData =
      CGDataProviderCopyData(CGImageGetDataProvider(image.CGImage));
  const UInt8* data = CFDataGetBytePtr(pixelData);

  const NSUInteger bytesPerPixel = CGImageGetBitsPerPixel(image.CGImage) /
                                   CGImageGetBitsPerComponent(image.CGImage);
  const NSUInteger index =
      (CGImageGetWidth(image.CGImage) * point.y + point.x) * bytesPerPixel;

  *red = ((CGFloat)data[index]) / 255.0f;
  *green = ((CGFloat)data[index + 1]) / 255.0f;
  *blue = ((CGFloat)data[index + 2]) / 255.0f;
  *alpha = ((CGFloat)data[index + 3]) / 255.0f;

  // Release CFDataRef explicitly because a Core Foundation object is not
  // released automatically.
  CFRelease(pixelData);
}

@end
