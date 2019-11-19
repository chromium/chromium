// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/shell_matchers.h"
#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kLongPage1[] = "/ios/testing/data/http_server_files/tall_page.html";
const char kLongPage2[] =
    "/ios/testing/data/http_server_files/tall_page.html?2";

// Test scroll offsets.
const CGFloat kOffset1 = 20.0f;
const CGFloat kOffset2 = 40.0f;

// Waits for the web view scroll view is scrolled to |y_offset|.
void WaitForOffset(CGFloat y_offset) {
  CGPoint offset = CGPointMake(0.0, y_offset);
  NSString* content_offset_string = NSStringFromCGPoint(offset);
  NSString* name =
      [NSString stringWithFormat:@"Wait for scroll view to scroll to %@.",
                                 content_offset_string];
  GREYCondition* condition = [GREYCondition
      conditionWithName:name
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:web::WebViewScrollView()]
                        assertWithMatcher:grey_scrollViewContentOffset(offset)
                                    error:&error];
                    return (error == nil);
                  }];
  NSString* error_text =
      [NSString stringWithFormat:@"Scroll view did not scroll to %@",
                                 content_offset_string];
  GREYAssert([condition waitWithTimeout:10], error_text);
}

// Loads the long page at |url|, scrolls to the top, and waits for the offset to
// be {0, 0} before returning.
void ScrollLongPageToTop(const GURL& url) {
  // Load the page and swipe down.
  [ShellEarlGrey loadURL:url];
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
  // Waits for the {0, 0} offset.
  WaitForOffset(0.0);
}

}  // namespace

// Page state test cases for the web shell.
@interface PageStateTestCase : WebShellTestCase {
  net::EmbeddedTestServer _server;
}
@end

@implementation PageStateTestCase

- (void)setUp {
  [super setUp];

  NSString* bundlePath = [NSBundle bundleForClass:[self class]].resourcePath;
  _server.ServeFilesFromDirectory(
      base::FilePath(base::SysNSStringToUTF8(bundlePath)));
  GREYAssert(_server.Start(), @"EmbeddedTestServer failed to start.");
}

// Tests that page scroll position of a page is restored upon returning to the
// page via the back/forward buttons.
- (void)testScrollPositionRestoring {
  // grey_scrollInDirection scrolls incorrect distance on iOS 13.
  // TODO(crbug.com/983144): Enable this test on iOS 13.
  if (@available(iOS 13, *)) {
    return;
  }

  // Scroll the first page and verify the offset.
  ScrollLongPageToTop(_server.GetURL(kLongPage1));
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kOffset1)];
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      assertWithMatcher:grey_scrollViewContentOffset(CGPointMake(0, kOffset1))];

  // Scroll the second page and verify the offset.
  ScrollLongPageToTop(_server.GetURL(kLongPage2));
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kOffset2)];
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      assertWithMatcher:grey_scrollViewContentOffset(CGPointMake(0, kOffset2))];

  // Go back and verify that the first page offset has been restored.
  [[EarlGrey selectElementWithMatcher:web::BackButton()]
      performAction:grey_tap()];
  WaitForOffset(kOffset1);

  // Go forward and verify that the second page offset has been restored.
  [[EarlGrey selectElementWithMatcher:web::ForwardButton()]
      performAction:grey_tap()];
  WaitForOffset(kOffset2);
}

// Tests that the content offset of the webview scroll view is {0, 0} after a
// load.
- (void)testZeroContentOffsetAfterLoad {
  // Set up the file-based server to load the tall page.
  const GURL baseURL = _server.GetURL(kLongPage1);
  [ShellEarlGrey loadURL:baseURL];

  // Scroll the page and load again to verify that the new page's scroll offset
  // is reset to {0, 0}.
  const CGFloat kOffsetIncrement = 20.0;
  for (NSInteger i = 0; i < 10; ++i) {
    // Scroll down the page a bit before re-loading the URL.
    CGFloat offset = (i + 1) * kOffsetIncrement;
    [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
        performAction:grey_scrollInDirection(kGREYDirectionDown, offset)];
    // Add a query parameter so the next load creates another NavigationItem.
    GURL::Replacements replacements;
    replacements.SetQueryStr(base::NumberToString(i));
    [ShellEarlGrey loadURL:baseURL.ReplaceComponents(replacements)];
    // Wait for the content offset to be set to {0, 0}.
    WaitForOffset(0.0);
  }
}

@end
