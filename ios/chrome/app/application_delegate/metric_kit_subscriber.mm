// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/metrics/histogram_base.h"
#import "base/metrics/histogram_functions.h"
#import "base/numerics/safe_conversions.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/version.h"
#import "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/crash_report/model/features.h"

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

// Task identifier for tracking startup until the app becomes interactive.
NSString* const kMainLaunchTaskId = @"MainLaunchTask";

void ReportExitReason(base::HistogramBase* histogram,
                      MetricKitExitReason bucket,
                      NSUInteger count) {
  if (!count) {
    return;
  }
  histogram->AddCount(bucket, count);
}

void ReportLongDuration(const std::string& histogram_name,
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

void ReportMemory(const std::string& histogram_name,
                  NSMeasurement* measurement) {
  if (!measurement) {
    return;
  }
  double value =
      [measurement
          measurementByConvertingToUnit:NSUnitInformationStorage.megabytes]
          .doubleValue;
  base::UmaHistogramMemoryLargeMB(histogram_name, value);
}

void SendDiagnostic(MXDiagnostic* diagnostic, const std::string& type) {
  base::FilePath cache_dir_path;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_dir_path)) {
    return;
  }

  // Deflate the payload.
  NSError* error = nil;
  NSData* payload = [diagnostic.JSONRepresentation
      compressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib
                             error:&error];
  if (!payload) {
    return;
  }

  if (crash_reporter::IsCrashpadRunning()) {
    base::span<const uint8_t> spanpayload = base::apple::NSDataToSpan(payload);

    std::map<std::string, std::string> override_annotations = {
        {"ver",
         base::SysNSStringToUTF8(diagnostic.metaData.applicationBuildVersion)},
        {"metrickit", "true"},
        {"metrickit_type", type}};
    PreviousSessionInfo* previous_session =
        [PreviousSessionInfo sharedInstance];
    for (NSString* key in previous_session.reportParameters.allKeys) {
      override_annotations.insert(
          {base::SysNSStringToUTF8(key),
           base::SysNSStringToUTF8(previous_session.reportParameters[key])});
    }
    if (previous_session.breadcrumbs) {
      override_annotations.insert(
          {"breadcrumbs",
           base::SysNSStringToUTF8(previous_session.breadcrumbs)});
    }
    crash_reporter::ProcessExternalDump("MetricKit", spanpayload,
                                        override_annotations);
  }
}

void ProcessDiagnosticPayloads(NSArray<MXDiagnosticPayload*>* payloads) {
  for (MXDiagnosticPayload* payload in payloads) {
    for (MXCrashDiagnostic* diagnostic in payload.crashDiagnostics) {
      SendDiagnostic(diagnostic, "crash");
    }
    if (base::FeatureList::IsEnabled(kMetrickitNonCrashReport)) {
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
}

// Record MXPayload data even when the version is mismatched.
const char kHistogramPrefixIncludingMismatch[] =
    "IOS.MetricKit.IncludingMismatch.";
const char kHistogramPrefix[] = "IOS.MetricKit.";

std::string HistogramPrefix(bool include_mismatch) {
  return include_mismatch ? kHistogramPrefixIncludingMismatch
                          : kHistogramPrefix;
}

}  // namespace

@implementation MetricKitSubscriber

+ (instancetype)sharedInstance {
  static MetricKitSubscriber* instance = [[MetricKitSubscriber alloc] init];
  return instance;
}

+ (void)createExtendedLaunchTask {
  [MXMetricManager extendLaunchMeasurementForTaskID:kMainLaunchTaskId
                                              error:nil];
}

+ (void)endExtendedLaunchTask {
  [MXMetricManager finishExtendedLaunchMeasurementForTaskID:kMainLaunchTaskId
                                                      error:nil];
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
  for (MXMetricPayload* payload : payloads) {
    [self processPayload:payload];
  }
}

- (void)logStartupDurationMXHistogram:(MXHistogram*)histogram
                       toUMAHistogram:(const std::string&)histogramUMAName {
  if (!histogram || !histogram.totalBucketCount) {
    return;
  }
  // It should take less than 1 minute to startup.
  // Histogram is defined in millisecond granularity.
  base::HistogramBase* histogramUMA = base::Histogram::FactoryTimeGet(
      histogramUMAName, base::Milliseconds(1), base::Minutes(1), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  for (MXHistogramBucket* bucket in [histogram bucketEnumerator]) {
    // MXHistogram structure is linear and the bucket size is not guaranteed to
    // never change. As the granularity is small in the current iOS version,
    // (10ms) they are reported using a representative value of the bucket.
    // DCHECK on the size of the bucket to detect if the resolution decrease.

    // Time based MXHistogram report their values using `UnitDuration` which has
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
          histogramPrefix:(const std::string&)prefix {
  base::HistogramBase* histogramUMA = base::LinearHistogram::FactoryGet(
      prefix + "ForegroundExitData", 1, kMetricKitExitReasonCount,
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
          histogramPrefix:(const std::string&)prefix {
  base::HistogramBase* histogramUMA = base::LinearHistogram::FactoryGet(
      prefix + "BackgroundExitData", 1, kMetricKitExitReasonCount,
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
  if (!payload.includesMultipleApplicationVersions &&
      base::SysNSStringToUTF8(payload.metaData.applicationBuildVersion) ==
          version_info::GetVersionNumber()) {
    [self processPayload:payload withHistogramPrefix:HistogramPrefix(false)];
  }
  [self processPayload:payload withHistogramPrefix:HistogramPrefix(true)];
}

- (void)processPayload:(MXMetricPayload*)payload
    withHistogramPrefix:(const std::string&)prefix {
  ReportLongDuration(prefix + "ForegroundTimePerDay",
                     payload.applicationTimeMetrics.cumulativeForegroundTime);
  ReportLongDuration(prefix + "BackgroundTimePerDay",
                     payload.applicationTimeMetrics.cumulativeBackgroundTime);
  ReportLongDuration(prefix + "CPUTimePerDay",
                     payload.cpuMetrics.cumulativeCPUTime);
  ReportMemory(prefix + "AverageSuspendedMemory",
               payload.memoryMetrics.averageSuspendedMemory.averageMeasurement);
  ReportMemory(prefix + "PeakMemoryUsage",
               payload.memoryMetrics.peakMemoryUsage);

  MXHistogram* histogrammedApplicationResumeTime =
      payload.applicationLaunchMetrics.histogrammedApplicationResumeTime;
  [self logStartupDurationMXHistogram:histogrammedApplicationResumeTime
                       toUMAHistogram:prefix + "ApplicationResumeTime"];

  MXHistogram* histogrammedTimeToFirstDraw =
      payload.applicationLaunchMetrics.histogrammedTimeToFirstDraw;
  [self logStartupDurationMXHistogram:histogrammedTimeToFirstDraw
                       toUMAHistogram:prefix + "TimeToFirstDraw"];

  MXHistogram* histogrammedOptimizedTimeToFirstDraw =
      payload.applicationLaunchMetrics.histogrammedOptimizedTimeToFirstDraw;
  [self logStartupDurationMXHistogram:histogrammedOptimizedTimeToFirstDraw
                       toUMAHistogram:prefix + "OptimizedTimeToFirstDraw"];

  MXHistogram* histogrammedExtendedLaunch =
      payload.applicationLaunchMetrics.histogrammedExtendedLaunch;
  [self logStartupDurationMXHistogram:histogrammedExtendedLaunch
                       toUMAHistogram:prefix + "ExtendedLaunch"];

  MXHistogram* histogrammedApplicationHangTime =
      payload.applicationResponsivenessMetrics.histogrammedApplicationHangTime;
  [self logStartupDurationMXHistogram:histogrammedApplicationHangTime
                       toUMAHistogram:prefix + "ApplicationHangTime"];

  [self logForegroundExit:payload.applicationExitMetrics.foregroundExitData
          histogramPrefix:prefix];
  [self logBackgroundExit:payload.applicationExitMetrics.backgroundExitData
          histogramPrefix:prefix];
}

- (void)didReceiveDiagnosticPayloads:(NSArray<MXDiagnosticPayload*>*)payloads {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
       base::ThreadPolicy::PREFER_BACKGROUND, base::MayBlock()},
      base::BindOnce(ProcessDiagnosticPayloads, payloads));
}

@end
