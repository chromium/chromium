// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ai_prototyping/test/page_context_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

// Test case for page context extraction.
@interface PageContextExtraction : ChromeTestCase
@end

@implementation PageContextExtraction

// Tests that the page context is extracted correctly.
- (void)testExtractPageContext {
  // TODO(crbug.com/465016086): Add implementation to load list of urls.

  // Navigate to a site and wait for the site to complete loading before
  // capturing page context.
  [ChromeEarlGrey loadURL:GURL("https://www.youtube.com/")
        waitForCompletion:YES];
  [PageContextAppInterface triggerPageContextCapture];

  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Page context extraction completed"
                  block:^BOOL {
                    return
                        [PageContextAppInterface isPageContextCaptureComplete];
                  }];

  BOOL success = [condition waitWithTimeout:30];
  XCTAssertTrue(success, @"Test timed out waiting for page context.");

  NSString* pageContextString = [PageContextAppInterface pageContextResult];
  NSLog(@"Captured Page Context: %@", pageContextString);
}

@end
