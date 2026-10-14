// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/tracing/test_trace_processor_impl.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface TracingTestCase : ChromeTestCase
@end

@implementation TracingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  if ([self isRunningTest:@selector(testStartupTracing)]) {
    config.additional_args.push_back("--trace-startup");
    config.additional_args.push_back("--trace-startup-duration=0");
  }
  return config;
}

// Tests startup trace recording and stopping with stopStartupTracing.
- (void)testStartupTracing {
  // Perform UI interaction to generate trace events.
  [ChromeEarlGrey openNewTab];

  // Stop startup tracing and get the trace file path.
  NSString* traceFilePathString = [ChromeEarlGrey stopStartupTracing];
  GREYAssertTrue(traceFilePathString.length > 0, @"Trace file path is empty.");

  // Wait until the trace file is generated and not empty.
  ConditionBlock condition = ^{
    return [ChromeEarlGrey isTraceFileReadyAtPath:traceFilePathString];
  };
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                     base::test::ios::kWaitForActionTimeout, condition),
                 @"Startup trace file was not generated or is empty.");

  // Read and parse the trace file via EarlGrey EDO.
  NSData* traceData = [ChromeEarlGrey traceDataForPath:traceFilePathString];
  GREYAssertTrue(traceData && traceData.length > 0,
                 @"Failed to read trace file contents.");

  auto trace_span = base::as_chars(base::apple::NSDataToSpan(traceData));
  std::vector<char> rawData(trace_span.begin(), trace_span.end());

  base::test::TestTraceProcessorImpl ttp;
  auto status = ttp.ParseTrace(rawData);
  NSString* parseErrorMsg =
      [NSString stringWithFormat:@"Failed to parse trace file: %s",
                                 std::string(status.message()).c_str()];
  GREYAssertTrue(status.ok(), parseErrorMsg);

  // Query the trace file for the number of slices.
  auto result = ttp.ExecuteQuery("SELECT count(*) FROM slice");
  NSString* queryErrorMsg =
      [NSString stringWithFormat:@"Failed to query slices from trace file: %s",
                                 result.error().c_str()];
  GREYAssertTrue(result.ok() && result.result().size() >= 2, queryErrorMsg);

  int count = 0;
  GREYAssertTrue(base::StringToInt(result.result()[1][0], &count),
                 @"Failed to parse slice count query result.");

  GREYAssertTrue(count > 0, @"Trace file contains no slices.");

  // Clean up trace file.
  GREYAssertTrue([ChromeEarlGrey removeTraceFileAtPath:traceFilePathString],
                 @"Failed to remove startup trace file.");
}

// Tests manual trace recording and stopping with startManualTracing and
// stopManualTracing.
- (void)testManualTracing {
  // Start manual tracing.
  GREYAssertTrue([ChromeEarlGrey startManualTracing],
                 @"Failed to start manual tracing.");

  // Perform UI interaction to generate trace events.
  [ChromeEarlGrey openNewTab];

  // Stop manual tracing and get trace file path.
  NSString* traceFilePathString = [ChromeEarlGrey stopManualTracing];
  GREYAssertTrue(traceFilePathString.length > 0, @"Trace file path is empty.");

  // Wait until the trace file is generated and non-empty.
  ConditionBlock condition = ^{
    return [ChromeEarlGrey isTraceFileReadyAtPath:traceFilePathString];
  };
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                     base::test::ios::kWaitForActionTimeout, condition),
                 @"Manual trace file was not generated or is empty.");

  // Read and parse the trace file via EarlGrey EDO.
  NSData* traceData = [ChromeEarlGrey traceDataForPath:traceFilePathString];
  GREYAssertTrue(traceData && traceData.length > 0,
                 @"Failed to read trace file contents.");

  auto trace_span = base::as_chars(base::apple::NSDataToSpan(traceData));
  std::vector<char> rawData(trace_span.begin(), trace_span.end());

  base::test::TestTraceProcessorImpl ttp;
  auto status = ttp.ParseTrace(rawData);
  NSString* parseErrorMsg =
      [NSString stringWithFormat:@"Failed to parse trace file: %s",
                                 std::string(status.message()).c_str()];
  GREYAssertTrue(status.ok(), parseErrorMsg);

  // Query the trace file for the number of slices.
  auto result = ttp.ExecuteQuery("SELECT count(*) FROM slice");
  NSString* queryErrorMsg =
      [NSString stringWithFormat:@"Failed to query slices from trace file: %s",
                                 result.error().c_str()];
  GREYAssertTrue(result.ok() && result.result().size() >= 2, queryErrorMsg);

  int count = 0;
  GREYAssertTrue(base::StringToInt(result.result()[1][0], &count),
                 @"Failed to parse slice count query result.");

  GREYAssertTrue(count > 0, @"Trace file contains no slices.");

  // Clean up trace file.
  GREYAssertTrue([ChromeEarlGrey removeTraceFileAtPath:traceFilePathString],
                 @"Failed to remove manual trace file.");
}

@end
