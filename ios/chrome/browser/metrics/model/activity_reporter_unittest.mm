// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/activity_reporter.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/metrics/model/histogram_bridge.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns whether MetricKitReportSubscriber is compiled with full SDK 27+
// support and running on iOS 27+.
bool IsMetricKitSubscriberSupported() {
#if defined(__IPHONE_27_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_0
  return true;
#else
  return false;
#endif
}

NSString* GetReportJsonWithEnvironment(BOOL includes_multiple_versions,
                                       NSString* app_build_version) {
  return [NSString
      stringWithFormat:
          @"{\"timeRange\": {\"begin\": 1719163200, \"end\": 1719249600},"
           "\"environment\": {"
           "  \"regionFormat\": \"US\","
           "  \"osVersion\": {"
           "    \"platform\": \"iOS\","
           "    \"number\": \"27.0\","
           "    \"buildNumber\": \"12345\""
           "  },"
           "  \"deviceType\": \"iPhone\","
           "  \"platformArchitecture\": \"arm64\","
           "  \"lowPowerModeEnabled\": false,"
           "  \"isTestFlightApp\": false,"
           "  \"bundleIdentifier\": \"com.google.chrome.ios\","
           "  \"applicationBuildVersion\": \"%@\","
           "  \"latestApplicationVersion\": \"128.0.6613.0\","
           "  \"includesMultipleApplicationVersions\": %@,"
           "  \"hasExceededStateLimit\": false"
           "},"
           "\"stateEntries\": {"
           "  \"com.google.chrome.ios.tab\": [{"
           "    \"state\": {"
           "      \"label\": \"Active\","
           "      \"domain\": \"com.google.chrome.ios.tab\","
           "      \"duration\": {\"value\": 120.0, \"unit\": \"s\"},"
           "      \"stableMetadata\": {}"
           "    },"
           "    \"values\": ["
           "      {\"totalForegroundTimeMetric\": {\"value\": {\"value\": "
           "45.0, "
           "\"unit\": \"s\"}}}"
           "    ]"
           "  }]"
           "},"
           "\"intervalEntries\": []}",
          app_build_version, includes_multiple_versions ? @"true" : @"false"];
}

}  // namespace

using ActivityReporterTest = PlatformTest;

// Tests that the ActivityReporter init and reporting APIs do not crash.
TEST_F(ActivityReporterTest, SmokeTest) {
  ActivityReporter* reporter =
      [[ActivityReporter alloc] initWithDomain:ActivityReportDomainTest];
  EXPECT_NE(reporter, nil);
  [reporter reportActive];
  [reporter reportInactive];
}

// Tests that the ActivityReporterWithIncognito init and reporting APIs do not
// crash.
TEST_F(ActivityReporterTest, WithIncognitoSmokeTest) {
  ActivityReporterWithIncognito* reporter =
      [[ActivityReporterWithIncognito alloc]
          initWithDomain:ActivityReportDomainTestWithIncognito];
  EXPECT_NE(reporter, nil);
  [reporter reportActiveWithIncognito:YES];
  [reporter reportActiveWithIncognito:NO];
  [reporter reportInactive];
}

// Tests that the MetricKitReportSubscriber can be enabled and disabled.
TEST_F(ActivityReporterTest, ReportSubscriberSmokeTest) {
  if (!IsMetricKitSubscriberSupported()) {
    GTEST_SKIP() << "MetricKitReportSubscriber is not supported.";
  }
  if (@available(iOS 27.0, *)) {
    MetricKitReportSubscriber* subscriber =
        [MetricKitReportSubscriber sharedInstance];
    EXPECT_NE(subscriber, nil);
    [subscriber setEnabled:YES];
    [subscriber setEnabled:NO];
  }
}

// Tests that the MetricKitReportSubscriber decodes reports and logs all
// expected histograms correctly.
TEST_F(ActivityReporterTest, ReportSubscriberHistogramsTest) {
  if (!IsMetricKitSubscriberSupported()) {
    GTEST_SKIP() << "MetricKitReportSubscriber is not supported.";
  }
  if (@available(iOS 27.0, *)) {
    base::HistogramTester histogram_tester;

    NSString* report_json =
        @"{\"timeRange\": {\"begin\": 1719163200, \"end\": 1719249600},"
         "\"stateEntries\": {"
         "  \"com.google.chrome.ios.tab\": [{"
         "    \"state\": {"
         "      \"label\": \"Active\","
         "      \"domain\": \"com.google.chrome.ios.tab\","
         "      \"duration\": {\"value\": 120.0, \"unit\": \"s\"},"
         "      \"stableMetadata\": {}"
         "    },"
         "    \"values\": ["
         "      {\"totalForegroundTimeMetric\": {\"value\": {\"value\": 45.0, "
         "\"unit\": \"s\"}}},"
         "      {\"totalBackgroundTimeMetric\": {\"value\": {\"value\": 30.0, "
         "\"unit\": \"s\"}}},"
         "      {\"cpuTimeMetric\": {\"value\": {\"value\": 15.0, \"unit\": "
         "\"s\"}}},"
         "      {\"suspendedMemoryMetric\": {\"value\": {\"average\": "
         "{\"value\": 120.0, \"unit\": \"MB\"}, \"count\": 10}}},"
         "      {\"peakMemoryMetric\": {\"value\": {\"value\": 350.0, "
         "\"unit\": \"MB\"}}},"
         "      {\"hangTimeMetric\": {\"histogram\": {\"buckets\": "
         "[{\"lowerBound\": {\"value\": 100.0, \"unit\": \"ms\"}, "
         "\"upperBound\": {\"value\": 200.0, \"unit\": \"ms\"}, \"count\": "
         "5}]}}},"
         "      {\"foregroundTerminationMetric\": {"
         "        \"normalTerminationCount\": 1,"
         "        \"memoryLimitTerminationCount\": 2,"
         "        \"badAccessTerminationCount\": 3,"
         "        \"abnormalTerminationCount\": 4,"
         "        \"watchdogTerminationCount\": 5,"
         "        \"illegalInstructionTerminationCount\": 6"
         "      }},"
         "      {\"backgroundTerminationMetric\": {"
         "        \"normalTerminationCount\": 1,"
         "        \"memoryLimitTerminationCount\": 2,"
         "        \"badAccessTerminationCount\": 3,"
         "        \"abnormalTerminationCount\": 4,"
         "        \"watchdogTerminationCount\": 5,"
         "        \"illegalInstructionTerminationCount\": 6,"
         "        \"highCPUTerminationCount\": 7,"
         "        \"systemPressureTerminationCount\": 8,"
         "        \"fileLockTerminationCount\": 9,"
         "        \"taskTimeoutTerminationCount\": 10"
         "      }}"
         "    ]"
         "  }]"
         "},"
         "\"intervalEntries\": []}";

    NSData* data = [report_json dataUsingEncoding:NSUTF8StringEncoding];
    [[MetricKitReportSubscriber sharedInstance] processReportForTesting:data];

    // Assert that the expected histograms were logged!
    histogram_tester.ExpectUniqueTimeSample(
        "IOS.MetricKit.IncludingMismatch.ForegroundTimePerDay.Tab",
        base::Seconds(45), 1);
    histogram_tester.ExpectUniqueTimeSample(
        "IOS.MetricKit.IncludingMismatch.BackgroundTimePerDay.Tab",
        base::Seconds(30), 1);
    histogram_tester.ExpectUniqueTimeSample(
        "IOS.MetricKit.IncludingMismatch.CPUTimePerDay.Tab", base::Seconds(15),
        1);

    histogram_tester.ExpectUniqueSample(
        "IOS.MetricKit.IncludingMismatch.AverageSuspendedMemory.Tab", 120, 1);

    histogram_tester.ExpectUniqueSample(
        "IOS.MetricKit.IncludingMismatch.PeakMemoryUsage.Tab", 350, 1);

    // HangTime bucket sample is the average of lower/upper bounds = 150ms.
    histogram_tester.ExpectUniqueSample(
        "IOS.MetricKit.IncludingMismatch.ApplicationHangTime.Tab", 150, 5);

    // Foreground terminations exit reasons
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.ForegroundExitData.Tab",
        MetricKitExitReasonNormal, 1);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.ForegroundExitData.Tab",
        MetricKitExitReasonMemoryLimit, 2);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.ForegroundExitData.Tab",
        MetricKitExitReasonBadAccess, 3);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.ForegroundExitData.Tab",
        MetricKitExitReasonAbnormal, 4);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.ForegroundExitData.Tab",
        MetricKitExitReasonWatchdog, 5);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.ForegroundExitData.Tab",
        MetricKitExitReasonIllegalInstruction, 6);

    // Background terminations exit reasons
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonNormal, 1);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonMemoryLimit, 2);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonBadAccess, 3);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonAbnormal, 4);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonWatchdog, 5);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonIllegalInstruction, 6);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonCpuLimit, 7);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonMemoryPressure, 8);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonSuspendedWithLockedFile, 9);
    histogram_tester.ExpectBucketCount(
        "IOS.MetricKit.IncludingMismatch.BackgroundExitData.Tab",
        MetricKitExitReasonBackgroundTaskAssertionTimeout, 10);
  }
}

// Tests that the MetricKitReportSubscriber respects version matching and
// `includesMultipleApplicationVersions` when logging to standard vs
// `IncludingMismatch.` histograms.
TEST_F(ActivityReporterTest, ReportSubscriberVersionCheckTest) {
  if (!IsMetricKitSubscriberSupported()) {
    GTEST_SKIP() << "MetricKitReportSubscriber is not supported.";
  }
  if (@available(iOS 27.0, *)) {
    // 1. Matches current version and `includesMultipleApplicationVersions` is
    // false: logs to both prefix and `IncludingMismatch.` prefix.
    {
      base::HistogramTester histogram_tester;
      NSString* current_version =
          base::SysUTF8ToNSString(version_info::GetVersionNumber());
      NSString* report_json = GetReportJsonWithEnvironment(
          /*includes_multiple_versions=*/NO, current_version);
      NSData* data = [report_json dataUsingEncoding:NSUTF8StringEncoding];
      [[MetricKitReportSubscriber sharedInstance] processReportForTesting:data];

      histogram_tester.ExpectUniqueTimeSample(
          "IOS.MetricKit.IncludingMismatch.ForegroundTimePerDay.Tab",
          base::Seconds(45), 1);
      histogram_tester.ExpectUniqueTimeSample(
          "IOS.MetricKit.ForegroundTimePerDay.Tab", base::Seconds(45), 1);
    }

    // 2. Mismatched version: logs only to `IncludingMismatch.` prefix.
    {
      base::HistogramTester histogram_tester;
      NSString* report_json = GetReportJsonWithEnvironment(
          /*includes_multiple_versions=*/NO, @"0.0.0.0");
      NSData* data = [report_json dataUsingEncoding:NSUTF8StringEncoding];
      [[MetricKitReportSubscriber sharedInstance] processReportForTesting:data];

      histogram_tester.ExpectUniqueTimeSample(
          "IOS.MetricKit.IncludingMismatch.ForegroundTimePerDay.Tab",
          base::Seconds(45), 1);
      histogram_tester.ExpectTotalCount(
          "IOS.MetricKit.ForegroundTimePerDay.Tab", 0);
    }

    // 3. `includesMultipleApplicationVersions` is true: logs only to
    // `IncludingMismatch.` prefix.
    {
      base::HistogramTester histogram_tester;
      NSString* current_version =
          base::SysUTF8ToNSString(version_info::GetVersionNumber());
      NSString* report_json = GetReportJsonWithEnvironment(
          /*includes_multiple_versions=*/YES, current_version);
      NSData* data = [report_json dataUsingEncoding:NSUTF8StringEncoding];
      [[MetricKitReportSubscriber sharedInstance] processReportForTesting:data];

      histogram_tester.ExpectUniqueTimeSample(
          "IOS.MetricKit.IncludingMismatch.ForegroundTimePerDay.Tab",
          base::Seconds(45), 1);
      histogram_tester.ExpectTotalCount(
          "IOS.MetricKit.ForegroundTimePerDay.Tab", 0);
    }
  }
}
