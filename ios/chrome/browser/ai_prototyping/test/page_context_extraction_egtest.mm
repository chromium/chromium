// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
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

namespace {
// Timeout for page load + duration of page context extraction.
NSInteger kPageContextExtractionTimeout = 40;
NSInteger kPageContextExtractionTimeoutErrorCode = 0;
NSString* kErrorDomain = @"PageContextExtraction";
}  // namespace

// Test case for page context extraction.
@interface PageContextExtraction : ChromeTestCase

@property(strong, nonatomic) PageContextExtractionConfig* config;
@property(strong, nonatomic)
    NSMutableArray<PageContextExtractionResult*>* results;
@end

@interface PageContextExtraction ()
- (BOOL)processAndLogResultsForUrls:(NSArray<NSString*>*)urlsArray;
@end

@implementation PageContextExtraction

- (void)setUp {
  [super setUp];
  self.config = [[PageContextExtractionConfig alloc]
      initWithShouldStorePageContextLocally:
          [TestArgs shouldStorePageContextLocallyFromTestArgs]
                                  outputDir:[TestArgs
                                                readOutputDirNameFromTestArgs]];
  self.results = [NSMutableArray array];
}

- (NSArray<NSString*>*)urlsFromInputFile:(NSString*)filePath {
  NSLog(@"[PageContextExtraction] Reading list of urls to extract page context "
        @"from file: %@",
        filePath);
  NSError* error = nil;
  NSString* inputFileContent =
      [NSString stringWithContentsOfFile:filePath
                                encoding:NSUTF8StringEncoding
                                   error:&error];
  if (error) {
    XCTAssertNil(error,
                 @"[PageContextExtraction] Error reading urls from file: %@.",
                 error);
  }
  NSArray<NSString*>* linesArray = [inputFileContent
      componentsSeparatedByCharactersInSet:[NSCharacterSet
                                               newlineCharacterSet]];
  // Filter out empty lines.
  NSPredicate* nonEmptyPredicate =
      [NSPredicate predicateWithFormat:@"length > 0"];
  NSArray<NSString*>* urlsArray =
      [linesArray filteredArrayUsingPredicate:nonEmptyPredicate];
  return urlsArray;
}

// Trigger page context extraction for `url` and wait for result.
- (PageContextExtractionResult*)triggerPageContextExtractionAndWaitForResult:
    (NSString*)url {
  // Navigate to a site. Intentionally not wait for loading as default timeout
  // could be too short for some sites.
  [ChromeEarlGrey loadURL:GURL(base::SysNSStringToUTF8(url))];
  [PageContextAppInterface triggerPageContextCaptureWithConfig:self.config
                                                           url:url];

  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Page context extraction completed"
                  block:^BOOL {
                    return
                        [PageContextAppInterface isPageContextCaptureComplete];
                  }];

  BOOL success = [condition waitWithTimeout:kPageContextExtractionTimeout];
  if (!success) {
    NSDictionary* userInfo = @{
      NSLocalizedDescriptionKey :
          @"Timeout waiting for page context extraction. It is possible that "
          @"site takes too long to load"
    };
    NSError* error =
        [NSError errorWithDomain:kErrorDomain
                            code:kPageContextExtractionTimeoutErrorCode
                        userInfo:userInfo];
    return [[PageContextExtractionResult alloc] initWithPageContext:nil
                                                              error:error
                                                           filePath:nil];
  }

  return [PageContextAppInterface pageContextResult];
}

// Extract page context for `url` and store the result.
- (void)handlePageContextExtractionForUrl:(NSString*)url {
  PageContextExtractionResult* result =
      [self triggerPageContextExtractionAndWaitForResult:url];

  if (result.error) {
    NSLog(@"[PageContextExtraction] Failed to extract page context for url:%@ "
          @"with error: %@",
          url, result.error);
  } else {
    NSLog(@"[PageContextExtraction] Successfully extracted page context for "
          @"url:%@",
          url);
  }
  [self.results addObject:result];
}

// Extract page context for input urls.
- (void)testExtractPageContext {
  NSString* inputFilePath = [TestArgs readUrlListFilePathTestArgs];
  NSArray<NSString*>* urlsArray = nil;
  if (inputFilePath != nil) {
    urlsArray = [self urlsFromInputFile:inputFilePath];
  } else {
    NSLog(@"[PageContextExtraction] input file for url not specified");
    // Use place holder url when input file is not given. This is the case when
    // this test is triggered by trybots.
    urlsArray = @[ @"https://www.youtube.com/" ];
  }
  NSUInteger urlsSize = [urlsArray count];
  [urlsArray enumerateObjectsUsingBlock:^(NSString* url, NSUInteger index,
                                          BOOL*) {
    NSLog(
        @"[PageContextExtraction] extracting page context for url:%@ (%tu/%tu)",
        url, index + 1, urlsSize);
    [self handlePageContextExtractionForUrl:url];
  }];

  BOOL hasErrors = [self processAndLogResultsForUrls:urlsArray];

  XCTAssertFalse(hasErrors,
                 @"One or more page context extractions failed. See summary "
                 @"log for details.");
}

- (BOOL)processAndLogResultsForUrls:(NSArray<NSString*>*)urlsArray {
  NSMutableString* summaryLog =
      [NSMutableString stringWithString:@"\n--- Page Context Extraction "
                                        @"Summary ---\n"];
  __block BOOL hasErrors = NO;

  [self.results enumerateObjectsUsingBlock:^(
                    PageContextExtractionResult* result, NSUInteger index,
                    BOOL*) {
    NSString* urlString = urlsArray[index];
    [summaryLog appendFormat:@"%tu. URL: %@\n", index + 1, urlString];

    if (result.error) {
      hasErrors = YES;
      [summaryLog appendFormat:@"   - Failure: %@\n", result.error];
    } else {
      [summaryLog appendString:@"   - Success\n"];
      if (self.config.shouldStorePageContextLocally) {
        if (result.filePath) {
          [summaryLog appendFormat:@"     - Saved to: %@\n", result.filePath];
        } else {
          hasErrors = YES;
          [summaryLog appendString:@"     - Failure: File path is nil when it "
                                   @"should be saved.\n"];
        }
      }
    }
    [summaryLog appendString:@" -----------------------\n"];
  }];

  [summaryLog appendString:@"--- End of Summary ---\n"];
  NSLog(@"%@", summaryLog);

  return hasErrors;
}

@end
