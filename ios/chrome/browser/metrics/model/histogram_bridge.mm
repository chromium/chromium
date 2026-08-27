// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/histogram_bridge.h"

#import "base/metrics/histogram_base.h"
#import "base/metrics/histogram_functions.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/version_info/version_info.h"

@implementation HistogramBridge

+ (BOOL)isCurrentVersionNumber:(NSString*)version {
  if (!version) {
    return NO;
  }
  return base::SysNSStringToUTF8(version) == version_info::GetVersionNumber();
}

+ (void)reportLongDuration:(NSString*)histogramName
               withSeconds:(double)seconds {
  base::UmaHistogramCustomTimes(base::SysNSStringToUTF8(histogramName),
                                base::Seconds(seconds), base::Seconds(1),
                                base::Days(1), 50);
}

+ (void)reportMemoryLargeMB:(NSString*)histogramName
              withMegabytes:(double)megabytes {
  base::UmaHistogramMemoryLargeMB(base::SysNSStringToUTF8(histogramName),
                                  megabytes);
}

+ (void)reportExitReason:(NSString*)histogramName
              withBucket:(NSInteger)bucket
                   count:(NSInteger)count {
  if (count <= 0) {
    return;
  }
  int exclusive_max = static_cast<int>(MetricKitExitReasonMaxValue) + 1;
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      base::SysNSStringToUTF8(histogramName), 1, exclusive_max,
      static_cast<size_t>(exclusive_max + 1),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(base::saturated_cast<int>(bucket),
                      base::saturated_cast<int>(count));
}

+ (void)reportHangTimeBucket:(NSString*)histogramName
                  withSample:(double)sample
                       count:(NSInteger)count {
  if (count <= 0) {
    return;
  }
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      base::SysNSStringToUTF8(histogramName), base::Milliseconds(1),
      base::Minutes(1), 50, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(
      base::saturated_cast<base::HistogramBase::Sample32>(sample),
      base::saturated_cast<int>(count));
}

@end
