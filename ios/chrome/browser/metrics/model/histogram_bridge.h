// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_HISTOGRAM_BRIDGE_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_HISTOGRAM_BRIDGE_H_

#import <Foundation/Foundation.h>

// LINT.IfChange(MetricKitExitReason)
typedef NS_ENUM(NSInteger, MetricKitExitReason) {
  MetricKitExitReasonNormal = 0,
  MetricKitExitReasonAbnormal = 1,
  MetricKitExitReasonWatchdog = 2,
  MetricKitExitReasonCpuLimit = 3,
  MetricKitExitReasonMemoryLimit = 4,
  MetricKitExitReasonMemoryPressure = 5,
  MetricKitExitReasonSuspendedWithLockedFile = 6,
  MetricKitExitReasonBadAccess = 7,
  MetricKitExitReasonIllegalInstruction = 8,
  MetricKitExitReasonBackgroundTaskAssertionTimeout = 9,
  MetricKitExitReasonMaxValue =
      MetricKitExitReasonBackgroundTaskAssertionTimeout,
};
// LINT.ThenChange(tools/metrics/histograms/metadata/ios/enums.xml:MetricKitExitData)

// Bridging class to log MetricKit state-specific histograms using base UMA
// APIs.
@interface HistogramBridge : NSObject

// Returns whether the given application build version matches the current
// application version (`version_info::GetVersionNumber()`).
+ (BOOL)isCurrentVersionNumber:(NSString*)version;

// Logs a duration histogram (custom times, 1s to 1d range).
+ (void)reportLongDuration:(NSString*)histogramName withSeconds:(double)seconds;

// Logs a memory histogram in MBs.
+ (void)reportMemoryLargeMB:(NSString*)histogramName
              withMegabytes:(double)megabytes;

// Logs exit reason using a linear histogram.
+ (void)reportExitReason:(NSString*)histogramName
              withBucket:(NSInteger)bucket
                   count:(NSInteger)count;

// Logs a hang time sample in milliseconds.
+ (void)reportHangTimeBucket:(NSString*)histogramName
                  withSample:(double)sample
                       count:(NSInteger)count;

@end

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_HISTOGRAM_BRIDGE_H_
