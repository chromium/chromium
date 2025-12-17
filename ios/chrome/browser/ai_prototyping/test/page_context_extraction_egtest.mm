// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ai_prototyping/test/page_context_app_interface.h"
#import "ios/chrome/browser/ai_prototyping/test/page_context_extraction_data.h"
#import "ios/chrome/browser/ai_prototyping/test/test_args.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

// Test case for page context extraction.
@interface PageContextExtraction : ChromeTestCase

@property(strong, nonatomic) PageContextExtractionConfig* config;
@end

@implementation PageContextExtraction

- (void)setUp {
  [super setUp];
  self.config = [[PageContextExtractionConfig alloc]
      initWithShouldStorePageContextLocally:
          [TestArgs shouldStorePageContextLocallyFromTestArgs]];
}

// Extracts page context for the given `url`.
- (PageContextExtractionResult*)extractPageContextForURL:(const GURL&)url {
  // Navigate to a site and wait for the site to complete loading before
  // capturing page context.
  [ChromeEarlGrey loadURL:url waitForCompletion:YES];
  [PageContextAppInterface triggerPageContextCaptureWithConfig:self.config];

  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Page context extraction completed"
                  block:^BOOL {
                    return
                        [PageContextAppInterface isPageContextCaptureComplete];
                  }];

  BOOL success = [condition waitWithTimeout:30];
  XCTAssertTrue(success, @"Test timed out waiting for page context.");

  return [PageContextAppInterface pageContextResult];
}

// Tests that the page context is extracted correctly.
- (void)testExtractPageContext {
  // TODO(crbug.com/465016086): Add implementation to load list of urls.
  PageContextExtractionResult* result =
      [self extractPageContextForURL:GURL("https://www.youtube.com/")];

  XCTAssertNotNil(result, @"Page context result should not be nil.");
  XCTAssertNil(result.error, @"Page context capture failed with error: %@",
               result.error);
  NSLog(@"Captured Page Context: %@", result.pageContext);
  if (self.config.shouldStorePageContextLocally) {
    NSLog(@"Page Context should be stored on disk.");
    XCTAssertNotNil(result.filePath,
                    @"Page context local file path should not be nil.");
    NSLog(@"Page Context saved to : %@", result.filePath);
  }
}

@end
