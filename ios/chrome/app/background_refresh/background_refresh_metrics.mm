// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/background_refresh_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"

const char kInitStageDuringBackgroundRefreshHistogram[] =
    "IOS.BackgroundRefresh.InitStage";

const char kBGTaskSchedulerErrorHistogram[] =
    "IOS.BackgroundRefresh.BGTaskSchedulerError";

const char kLaunchTypeForBackgroundRefreshHistogram[] =
    "IOS.BackgroundRefresh.LaunchType";

const char kExecutionDurationHistogram[] =
    "IOS.BackgroundRefresh.ExecutionDuration";

const char kExecutionDurationTimeoutHistogram[] =
    "IOS.BackgroundRefresh.ExecutionDuration.Timeout";

const char kActiveProviderCountAtTimeoutHistogram[] =
    "IOS.BackgroundRefresh.Timeout.ActiveProviderCount";

const char kTotalProviderCountAtTimeoutHistogram[] =
    "IOS.BackgroundRefresh.Timeout.TotalProviderCount";

void RecordProviderExecutionDuration(NSString* provider_identifier,
                                     base::TimeDelta duration) {
  std::string histogram_name = "IOS.BackgroundRefresh.Provider.Duration.";
  histogram_name += base::SysNSStringToUTF8(provider_identifier);
  base::UmaHistogramMediumTimes(histogram_name, duration);
}
