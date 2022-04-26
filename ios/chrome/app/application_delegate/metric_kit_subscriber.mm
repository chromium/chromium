// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/reporter_running_ios.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/crash_report/features.h"
#include "ios/chrome/browser/crash_report/synthetic_crash_report_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kChromeMetricKitPayloadsDirectory = @"ChromeMetricKitPayloads";

// The different causes of app exit as reported by MetricKit.
// This enum is used in UMA. Do not change the order.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum MetricKitExitReason {
  kNormalAppExit = 0,
  kAbnormalAppExit = 1,
  kWatchdogExit = 2,
  kCPUResourceLimitExit = 3,
  kMemoryResourceLimitExit = 4,
  kMemoryPressureExit = 5,
  kSuspendedWithLockedFileExit = 6,
  kBadAccessExit = 7,
  kIllegalInstructionExit = 8,
  kBackgroundTaskAssertionTimeoutExit = 9,

  // Must be the last enum entries.
  kMetricKitExitReasonMaxValue = kBackgroundTaskAssertionTimeoutExit,
  kMetricKitExitReasonCount = kMetricKitExitReasonMaxValue + 1
};

namespace {

NSString* const kEnableMetricKit = @"EnableMetricKit";

void ReportExitReason(base::HistogramBase* histogram,
                      MetricKitExitReason bucket,
                      NSUInteger count) {
  if (!count) {
    return;
  }
  histogram->AddCount(bucket, count);
}

void ReportLongDuration(const char* histogram_name,
                        NSMeasurement* measurement) {
  if (!measurement) {
    return;
  }
  double value =
      [measurement measurementByConvertingToUnit:NSUnitDuration.seconds]
          .doubleValue;
  base::UmaHistogramCustomTimes(histogram_name, base::Seconds(value),
                                base::Seconds(1),
                                base::Seconds(86400 /* secs per day */), 50);
}

void ReportMemory(const char* histogram_name, NSMeasurement* measurement) {
  if (!measurement) {
    return;
  }
  double value =
      [measurement
          measurementByConvertingToUnit:NSUnitInformationStorage.megabytes]
          .doubleValue;
  base::UmaHistogramMemoryLargeMB(histogram_name, value);
}

void WriteMetricPayloads(NSArray<MXMetricPayload*>* payloads) {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString* documents_directory = [paths objectAtIndex:0];
  NSString* metric_kit_report_directory = [documents_directory
      stringByAppendingPathComponent:kChromeMetricKitPayloadsDirectory];
  base::FilePath metric_kit_report_path(
      base::SysNSStringToUTF8(metric_kit_report_directory));
  if (!base::CreateDirectory(metric_kit_report_path)) {
    return;
  }
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
  [formatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
  for (MXMetricPayload* payload : payloads) {
    NSDate* end_date = payload.timeStampEnd;
    NSString* file_name =
        [NSString stringWithFormat:@"Metrics-%@.json",
                                   [formatter stringFromDate:end_date]];
    base::FilePath file_path(
        base::SysNSStringToUTF8([metric_kit_report_directory
            stringByAppendingPathComponent:file_name]));
    NSData* file_data = payload.JSONRepresentation;
    base::WriteFile(file_path, static_cast<const char*>(file_data.bytes),
                    file_data.length);
  }
}

void WriteDiagnosticPayloads(NSArray<MXDiagnosticPayload*>* payloads)
    API_AVAILABLE(ios(14.0)) {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString* documents_directory = [paths objectAtIndex:0];
  NSString* metric_kit_report_directory = [documents_directory
      stringByAppendingPathComponent:kChromeMetricKitPayloadsDirectory];
  base::FilePath metric_kit_report_path(
      base::SysNSStringToUTF8(metric_kit_report_directory));
  if (!base::CreateDirectory(metric_kit_report_path)) {
    return;
  }
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
  [formatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
  for (MXDiagnosticPayload* payload : payloads) {
    NSDate* end_date = payload.timeStampEnd;
    NSString* file_name =
        [NSString stringWithFormat:@"Diagnostic-%@.json",
                                   [formatter stringFromDate:end_date]];
    base::FilePath file_path(
        base::SysNSStringToUTF8([metric_kit_report_directory
            stringByAppendingPathComponent:file_name]));
    NSData* file_data = payload.JSONRepresentation;
    base::WriteFile(file_path, static_cast<const char*>(file_data.bytes),
                    file_data.length);
  }
}

void SendDiagnostic(MXDiagnostic* diagnostic, const std::string& type)
    API_AVAILABLE(ios(14.0)) {
  base::FilePath cache_dir_path;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_dir_path)) {
    return;
  }
  NSDictionary* info_dict = NSBundle.mainBundle.infoDictionary;
  NSError* error = nil;
  // Deflate the payload.
  NSData* payload = [diagnostic.JSONRepresentation
      compressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib
                             error:&error];
  if (!payload) {
    return;
  }

  if (crash_reporter::IsBreakpadRunning()) {
    std::string stringpayload(reinterpret_cast<const char*>(payload.bytes),
                              payload.length);
    CreateSyntheticCrashReportForMetrickit(
        cache_dir_path.Append(FILE_PATH_LITERAL("Breakpad")),
        base::SysNSStringToUTF8(info_dict[@"BreakpadProductDisplay"]),
        base::SysNSStringToUTF8([NSString
            stringWithFormat:@"%@_MetricKit", info_dict[@"BreakpadProduct"]]),
        base::SysNSStringToUTF8(diagnostic.metaData.applicationBuildVersion),
        base::SysNSStringToUTF8(info_dict[@"BreakpadURL"]), type,
        stringpayload);
  } else {
    base::span<const uint8_t> spanpayload(
        reinterpret_cast<const uint8_t*>(payload.bytes), payload.length);
    crash_reporter::ProcessExternalDump(
        "MetricKit", spanpayload,
        {{"ver",
          base::SysNSStringToUTF8(diagnostic.metaData.applicationBuildVersion)},
         {"metrickit", "true"},
         {"metrickit_type", type}});
  }
}

void SendDiagnosticPayloads(NSArray<MXDiagnosticPayload*>* payloads)
    API_AVAILABLE(ios(14.0)) {
  for (MXDiagnosticPayload* payload in payloads) {
    for (MXCrashDiagnostic* diagnostic in payload.crashDiagnostics) {
      SendDiagnostic(diagnostic, "crash");
    }
    for (MXCPUExceptionDiagnostic* diagnostic in payload
             .cpuExceptionDiagnostics) {
      SendDiagnostic(diagnostic, "cpu-exception");
    }
    for (MXHangDiagnostic* diagnostic in payload.hangDiagnostics) {
      SendDiagnostic(diagnostic, "hang");
    }
    for (MXDiskWriteExceptionDiagnostic* diagnostic in payload
             .diskWriteExceptionDiagnostics) {
      SendDiagnostic(diagnostic, "diskwrite-exception");
    }
  }
}

void ProcessDiagnosticPayloads(NSArray<MXDiagnosticPayload*>* payloads,
                               bool write_payloads,
                               bool send_payloads) API_AVAILABLE(ios(14.0)) {
  if (write_payloads) {
    WriteDiagnosticPayloads(payloads);
  }
  if (send_payloads) {
    SendDiagnosticPayloads(payloads);
  }
}

}  // namespace

@implementation MetricKitSubscriber

+ (instancetype)sharedInstance {
  static MetricKitSubscriber* instance = [[MetricKitSubscriber alloc] init];
  return instance;
}

- (void)setEnabled:(BOOL)enable {
  if (enable == _enabled) {
    return;
  }
  _enabled = enable;
  if (enable) {
    [[MXMetricManager sharedManager] addSubscriber:self];
  } else {
    [[MXMetricManager sharedManager] removeSubscriber:self];
  }
}

- (void)didReceiveMetricPayloads:(NSArray<MXMetricPayload*>*)payloads {
  NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
  if ([standard_defaults boolForKey:kEnableMetricKit]) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
         base::ThreadPolicy::PREFER_BACKGROUND, base::MayBlock()},
        base::BindOnce(WriteMetricPayloads, payloads));
  }
  for (MXMetricPayload* payload : payloads) {
    [self processPayload:payload];
  }
}

- (void)logStartupDurationMXHistogram:(MXHistogram*)histogram
                       toUMAHistogram:(const char*)histogramUMAName {
  if (!histogram || !histogram.totalBucketCount) {
    return;
  }
  // It should take less than 1 minute to startup.
  // Histogram is defined in millisecond granularity.
  base::HistogramBase* histogramUMA = base::Histogram::FactoryTimeGet(
      histogramUMAName, base::Milliseconds(1), base::Minutes(1), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  MXHistogramBucket* bucket;
  NSEnumerator* enumerator = [histogram bucketEnumerator];
  while (bucket = [enumerator nextObject]) {
    // MXHistogram structure is linear and the bucket size is not guaranteed to
    // never change. As the granularity is small in the current iOS version,
    // (10ms) they are reported using a representative value of the bucket.
    // DCHECK on the size of the bucket to detect if the resolution decrease.

    // Time based MXHistogram report their values using |UnitDuration| which has
    // seconds as base unit. Hence, start and end are given in seconds.
    double start =
        [bucket.bucketStart
            measurementByConvertingToUnit:NSUnitDuration.milliseconds]
            .doubleValue;
    double end = [bucket.bucketEnd
                     measurementByConvertingToUnit:NSUnitDuration.milliseconds]
                     .doubleValue;
    // DCHECKS that resolution is less than 10ms.
    // Note: Real paylods use 10ms resolution but the simulated payload in XCode
    // uses 100ms resolution so it will trigger this DCHECK.
    DCHECK_LE(end - start, 10);
    double sample = (end + start) / 2;
    histogramUMA->AddCount(
        base::saturated_cast<base::HistogramBase::Sample>(sample),
        bucket.bucketCount);
  }
}

- (void)logForegroundExit:(MXForegroundExitData*)exitData
    API_AVAILABLE(ios(14.0)) {
  base::HistogramBase* histogramUMA = base::LinearHistogram::FactoryGet(
      "IOS.MetricKit.ForegroundExitData", 1, kMetricKitExitReasonCount,
      kMetricKitExitReasonCount + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  ReportExitReason(histogramUMA, kNormalAppExit,
                   exitData.cumulativeNormalAppExitCount);
  ReportExitReason(histogramUMA, kAbnormalAppExit,
                   exitData.cumulativeAbnormalExitCount);
  ReportExitReason(histogramUMA, kWatchdogExit,
                   exitData.cumulativeAppWatchdogExitCount);
  ReportExitReason(histogramUMA, kMemoryResourceLimitExit,
                   exitData.cumulativeMemoryResourceLimitExitCount);
  ReportExitReason(histogramUMA, kBadAccessExit,
                   exitData.cumulativeBadAccessExitCount);
  ReportExitReason(histogramUMA, kIllegalInstructionExit,
                   exitData.cumulativeIllegalInstructionExitCount);
}

- (void)logBackgroundExit:(MXBackgroundExitData*)exitData
    API_AVAILABLE(ios(14.0)) {
  base::HistogramBase* histogramUMA = base::LinearHistogram::FactoryGet(
      "IOS.MetricKit.BackgroundExitData", 1, kMetricKitExitReasonCount,
      kMetricKitExitReasonCount + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  ReportExitReason(histogramUMA, kNormalAppExit,
                   exitData.cumulativeNormalAppExitCount);
  ReportExitReason(histogramUMA, kAbnormalAppExit,
                   exitData.cumulativeAbnormalExitCount);
  ReportExitReason(histogramUMA, kWatchdogExit,
                   exitData.cumulativeAppWatchdogExitCount);
  ReportExitReason(histogramUMA, kCPUResourceLimitExit,
                   exitData.cumulativeCPUResourceLimitExitCount);
  ReportExitReason(histogramUMA, kMemoryResourceLimitExit,
                   exitData.cumulativeMemoryResourceLimitExitCount);
  ReportExitReason(histogramUMA, kMemoryPressureExit,
                   exitData.cumulativeMemoryPressureExitCount);
  ReportExitReason(histogramUMA, kSuspendedWithLockedFileExit,
                   exitData.cumulativeSuspendedWithLockedFileExitCount);
  ReportExitReason(histogramUMA, kBadAccessExit,
                   exitData.cumulativeBadAccessExitCount);
  ReportExitReason(histogramUMA, kIllegalInstructionExit,
                   exitData.cumulativeIllegalInstructionExitCount);
  ReportExitReason(histogramUMA, kBackgroundTaskAssertionTimeoutExit,
                   exitData.cumulativeBackgroundTaskAssertionTimeoutExitCount);
}

- (void)processPayload:(MXMetricPayload*)payload {
  // TODO(crbug.com/1140474): See related bug for why |bundleVersion| comes from
  // mainBundle instead of from version_info::GetVersionNumber(). Remove once
  // iOS 14.2 reaches mass adoption.
  NSString* bundleVersion =
      [[NSBundle mainBundle] infoDictionary][(NSString*)kCFBundleVersionKey];
  if (payload.includesMultipleApplicationVersions ||
      base::SysNSStringToUTF8(payload.metaData.applicationBuildVersion) !=
          base::SysNSStringToUTF8(bundleVersion)) {
    // The metrics will be reported on the current version of Chrome.
    // Ignore any report that contains data from another version to avoid
    // confusion.
    return;
  }

  ReportLongDuration("IOS.MetricKit.ForegroundTimePerDay",
                     payload.applicationTimeMetrics.cumulativeForegroundTime);
  ReportLongDuration("IOS.MetricKit.BackgroundTimePerDay",
                     payload.applicationTimeMetrics.cumulativeBackgroundTime);
  ReportMemory("IOS.MetricKit.AverageSuspendedMemory",
               payload.memoryMetrics.averageSuspendedMemory.averageMeasurement);
  ReportMemory("IOS.MetricKit.PeakMemoryUsage",
               payload.memoryMetrics.peakMemoryUsage);

  MXHistogram* histogrammedApplicationResumeTime =
      payload.applicationLaunchMetrics.histogrammedApplicationResumeTime;
  [self logStartupDurationMXHistogram:histogrammedApplicationResumeTime
                       toUMAHistogram:"IOS.MetricKit.ApplicationResumeTime"];

  MXHistogram* histogrammedTimeToFirstDraw =
      payload.applicationLaunchMetrics.histogrammedTimeToFirstDraw;
  [self logStartupDurationMXHistogram:histogrammedTimeToFirstDraw
                       toUMAHistogram:"IOS.MetricKit.TimeToFirstDraw"];

  MXHistogram* histogrammedApplicationHangTime =
      payload.applicationResponsivenessMetrics.histogrammedApplicationHangTime;
  [self logStartupDurationMXHistogram:histogrammedApplicationHangTime
                       toUMAHistogram:"IOS.MetricKit.ApplicationHangTime"];

  [self logForegroundExit:payload.applicationExitMetrics.foregroundExitData];
  [self logBackgroundExit:payload.applicationExitMetrics.backgroundExitData];
}

- (void)didReceiveDiagnosticPayloads:(NSArray<MXDiagnosticPayload*>*)payloads
    API_AVAILABLE(ios(14.0)) {
  NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
  BOOL writePayloadsToDisk = [standard_defaults boolForKey:kEnableMetricKit];
  bool sendPayloads = base::FeatureList::IsEnabled(kMetrickitCrashReport);

  if (writePayloadsToDisk || sendPayloads) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
         base::ThreadPolicy::PREFER_BACKGROUND, base::MayBlock()},
        base::BindOnce(ProcessDiagnosticPayloads, payloads, writePayloadsToDisk,
                       sendPayloads));
  }
}

@end
