// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/optimization_guide/core/feature_registry/feature_registration.h"
#import "ios/chrome/browser/ai_prototyping/test/page_context_app_interface.h"
#import "ios/chrome/browser/ai_prototyping/test/page_context_extraction_data.h"
#import "ios/chrome/browser/ai_prototyping/test/test_args.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

namespace {
constexpr base::TimeDelta kWaitForPageLoadTimeout = base::Seconds(60);
// Additional time to wait after page load before starting context extraction.
constexpr base::TimeDelta kPageContextExtractionAdditionalWaitTime =
    base::Seconds(5);
// Timeout for page context extraction.
NSInteger kPageContextExtractionTimeout = 10;
NSInteger kPageContextExtractionTimeoutErrorCode = 0;
NSString* kErrorDomain = @"PageContextExtraction";
}  // namespace

// Test case for page context extraction.
@interface PageContextExtraction : ChromeTestCase

@property(nonatomic, assign) BOOL shouldStorePageContextLocally;
@property(nonatomic, copy) NSString* outputDir;
@property(nonatomic, assign) BOOL shouldUploadToMQLS;
@property(nonatomic, copy) NSString* mqlsLoggingTag;
@property(nonatomic, copy) NSString* modelQuery;
@property(strong, nonatomic)
    NSMutableArray<PageContextExtractionResult*>* results;
@property(nonatomic, assign) BOOL loadExternalUrl;
@end

@interface PageContextExtraction ()
- (BOOL)processAndLogResultsForUrls:(NSArray<NSString*>*)urlsArray;
- (BOOL)hasAnyError:(PageContextExtractionResult*)result;
@end

@implementation PageContextExtraction

- (void)setUp {
  [super setUp];
  // Check if this execution will load external sites. This only occur when it
  // is trigger manually with additional arguments. On bots, no external site
  // should be loaded and the timeout duration will be kept minimal.
  self.loadExternalUrl = [TestArgs readUrlListFilePathTestArgs] ? YES : NO;
  self.shouldStorePageContextLocally =
      [TestArgs shouldStorePageContextLocallyFromTestArgs];
  self.outputDir = [TestArgs readOutputDirNameFromTestArgs];
  self.shouldUploadToMQLS = [TestArgs shouldUploadToMQLSFromTestArgs];
  self.mqlsLoggingTag = [TestArgs readMQLSLoggingTagFromTestArgs];
  self.modelQuery = [TestArgs readModelQueryFromTestArgs];
  self.results = [NSMutableArray array];
  if (self.shouldUploadToMQLS) {
    [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
    // Metrics consent must be granted to upload to MQLS.
    GREYAssertFalse(
        [MetricsAppInterface setMetricsAndCrashReportingForTesting:YES],
        @"User consent has already been granted.");
  }
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([TestArgs shouldUploadToMQLSFromTestArgs]) {
    config.features_enabled.push_back(
        optimization_guide::features::kBlingPrototypingMqlsLogging);
  }
  return config;
}

- (void)tearDownHelper {
  // Metrics consent may have been overwritten if for MQLS upload.
  // Revoke metrics consent and update MetricsServicesManager.
  GREYAssert([MetricsAppInterface setMetricsAndCrashReportingForTesting:NO],
             @"Unpaired set/reset of user consent.");
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  [super tearDownHelper];
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
- (PageContextExtractionResult*)
    triggerPageContextExtractionAndWaitForResult:(NSString*)url
                                          config:(PageContextExtractionConfig*)
                                                     config {
  // Navigate to a site, waiting for it to finish loading.
  NSError* loadError =
      [ChromeEarlGrey loadURL:GURL(base::SysNSStringToUTF8(url))
             timeoutWithError:kWaitForPageLoadTimeout];
  if (loadError) {
    NSDictionary* userInfo = @{
      NSLocalizedDescriptionKey :
          [NSString stringWithFormat:@"Error loading url: %@ reason: %@", url,
                                     loadError.localizedDescription]
    };
    NSError* error =
        [NSError errorWithDomain:kErrorDomain
                            code:kPageContextExtractionTimeoutErrorCode
                        userInfo:userInfo];
    return [[PageContextExtractionResult alloc] initWithPageContext:nil
                                                       wrapperError:error
                                                         storeError:nil
                                                          mqlsError:nil
                                                           filePath:nil];
  }
  if (self.loadExternalUrl) {
    // Wait for longer after website is loaded as some sites may still be
    // populating content.
    base::test::ios::SpinRunLoopWithMinDelay(
        kPageContextExtractionAdditionalWaitTime);
  }

  [PageContextAppInterface triggerPageContextCaptureWithConfig:config url:url];

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
                                                       wrapperError:error
                                                         storeError:nil
                                                          mqlsError:nil
                                                           filePath:nil];
  }

  return [PageContextAppInterface pageContextResult];
}

// Extract page context for `url` and store the result.
- (void)handlePageContextExtractionForUrl:(NSString*)url
                                   config:(PageContextExtractionConfig*)config {
  PageContextExtractionResult* result =
      [self triggerPageContextExtractionAndWaitForResult:url config:config];

  if ([self hasAnyError:result]) {
    NSLog(@"[PageContextExtraction] Failed to extract page context for url:%@ "
          @"with errors: wrapperError: %@, storeError: %@, mqlsError: %@",
          url, result.wrapperError, result.storeError, result.mqlsError);
  } else {
    NSLog(@"[PageContextExtraction] Successfully extracted page context for "
          @"url:%@",
          url);
  }
  [self.results addObject:result];
}

- (BOOL)hasAnyError:(PageContextExtractionResult*)result {
  return result.wrapperError != nil || result.storeError != nil ||
         result.mqlsError != nil;
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
    urlsArray = @[ @"about:blank" ];
  }
  NSUInteger urlsSize = [urlsArray count];
  [urlsArray enumerateObjectsUsingBlock:^(NSString* url, NSUInteger index,
                                          BOOL*) {
    NSLog(
        @"[PageContextExtraction] extracting page context for url:%@ (%lu/%lu)",
        url, (unsigned long)index + 1, (unsigned long)urlsSize);
    PageContextExtractionConfig* perUrlConfig = [[PageContextExtractionConfig
        alloc]
        initWithShouldStorePageContextLocally:self.shouldStorePageContextLocally
                                    outputDir:self.outputDir
                                   filePrefix:[NSString
                                                  stringWithFormat:
                                                      @"%lu",
                                                      (unsigned long)index + 1]
                           shouldUploadToMQLS:self.shouldUploadToMQLS
                               mqlsLoggingTag:self.mqlsLoggingTag
                                   modelQuery:self.modelQuery];
    [self handlePageContextExtractionForUrl:url config:perUrlConfig];
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
    [summaryLog
        appendFormat:@"%lu. URL: %@\n", (unsigned long)index + 1, urlString];

    if (![self hasAnyError:(result)]) {
      [summaryLog appendString:@"   - Success\n"];
    } else {
      hasErrors = YES;
      [summaryLog appendString:@"   - Fail\n"];
      if (result.wrapperError) {
        [summaryLog
            appendFormat:@"     - Wrapper Failure: %@\n", result.wrapperError];
      }
      if (result.storeError) {
        [summaryLog
            appendFormat:@"     - Store Failure: %@\n", result.storeError];
      }
      if (result.mqlsError) {
        [summaryLog
            appendFormat:@"     - MQLS Failure: %@\n", result.mqlsError];
      }
    }

    if (self.shouldStorePageContextLocally) {
      if (result.filePath) {
        [summaryLog appendFormat:@"     - Saved to: %@\n", result.filePath];
      } else {
        hasErrors = YES;
        [summaryLog appendString:@"     - Failure: File path is nil when it "
                                 @"should be saved.\n"];
      }
    }

    if (self.shouldUploadToMQLS && !result.mqlsError) {
      if ([self.mqlsLoggingTag length] > 0) {
        [summaryLog
            appendFormat:@"     - Uploaded to MQLS with logging tag: %@\n",
                         self.mqlsLoggingTag];
      } else {
        [summaryLog appendString:@"     - Uploaded to MQLS\n"];
      }
    }
    [summaryLog appendString:@" -----------------------\n"];
  }];

  [summaryLog appendString:@"--- End of Summary ---\n"];
  NSLog(@"%@", summaryLog);

  return hasErrors;
}

@end
