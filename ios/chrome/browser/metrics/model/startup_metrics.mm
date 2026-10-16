// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/startup_metrics.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/timer/elapsed_timer.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/signin/model/signin_util.h"

std::string_view IOSStartupTemperatureToString(
    IOSStartupTemperature temperature) {
  switch (temperature) {
    case IOSStartupTemperature::kNotPrewarmed:
      return ".NotPrewarmed";
    case IOSStartupTemperature::kActivePrewarm:
      return ".ActivePrewarm";
    case IOSStartupTemperature::kSuspicious:
      NOTREACHED();
    case IOSStartupTemperature::kLaunchedInBackground:
      return ".LaunchedInBackground";
  }
}

std::string_view IOSColdStartTypeToString(ColdStartType type) {
  switch (type) {
    case ColdStartType::kRegular:
      return ".Regular";
    case ColdStartType::kFirstRun:
      return ".FirstRun";
    case ColdStartType::kAfterDeviceRestore:
      return ".AfterDeviceRestore";
    case ColdStartType::kAfterChromeUpgrade:
      return ".AfterChromeUpgrade";
    case ColdStartType::kAfterDeviceRestoreAndChromeUpgrade:
      return ".AfterDeviceRestoreAndChromeUpgrade";
    case ColdStartType::kUnknownDeviceRestore:
      return ".UnknownDeviceRestore";
    case ColdStartType::kUnknownDeviceRestoreAndChromeUpgrade:
      return ".UnknownDeviceRestoreAndChromeUpgrade";
  }
}

IOSStartupTemperature GetStartupTemperature(
    id<StartupInformation> startup_information) {
  switch (*startup_information.launchReason) {
    case IOSLaunchReason::kForeground:
      return IOSStartupTemperature::kNotPrewarmed;
    case IOSLaunchReason::kPreWarming:
      return IOSStartupTemperature::kActivePrewarm;
    case IOSLaunchReason::kSuspicious:
      return IOSStartupTemperature::kSuspicious;
    case IOSLaunchReason::kBackgroundRefresh:
    case IOSLaunchReason::kBackgroundURLSession:
    case IOSLaunchReason::kBackgroundSilentNotification:
      return IOSStartupTemperature::kLaunchedInBackground;
    case IOSLaunchReason::kForegroundWithSystemSuspension:
      // Metrics that depend on startup temperature are not logged for this
      // reason.
      NOTREACHED();
  }
}

ColdStartType GetColdStartType(id<StartupInformation> startup_information) {
  if (startup_information.isFirstRun) {
    return ColdStartType::kFirstRun;
  }
  const bool after_upgrade =
      [PreviousSessionInfo sharedInstance].isFirstSessionAfterUpgrade;
  const signin::Tribool device_restore = IsFirstSessionAfterDeviceRestore();
  switch (device_restore) {
    case signin::Tribool::kUnknown:
      return after_upgrade
                 ? ColdStartType::kUnknownDeviceRestoreAndChromeUpgrade
                 : ColdStartType::kUnknownDeviceRestore;
    case signin::Tribool::kTrue:
      return after_upgrade ? ColdStartType::kAfterDeviceRestoreAndChromeUpgrade
                           : ColdStartType::kAfterDeviceRestore;
    case signin::Tribool::kFalse:
      return after_upgrade ? ColdStartType::kAfterChromeUpgrade
                           : ColdStartType::kRegular;
  }
}

void LogStartupDurationWithTemperatureAndType(
    std::string_view histogram_name,
    base::TimeDelta duration,
    std::optional<IOSStartupTemperature> temperature,
    ColdStartType cold_start_type) {
  const std::string_view temp_str =
      temperature ? IOSStartupTemperatureToString(*temperature)
                  : std::string_view();
  const std::string_view type_str = IOSColdStartTypeToString(cold_start_type);
  base::UmaHistogramTimes(histogram_name, duration);
  if (temperature.has_value()) {
    base::UmaHistogramTimes(base::StrCat({histogram_name, temp_str}), duration);
    base::UmaHistogramTimes(base::StrCat({histogram_name, temp_str, type_str}),
                            duration);
  } else {
    base::UmaHistogramTimes(base::StrCat({histogram_name, type_str}), duration);
  }
}

std::optional<base::TimeDelta> ComputeStartupLatency(
    id<StartupInformation> startup_information,
    std::optional<base::TimeDelta> main_to_now_time,
    base::TimeDelta scene_connection_to_now_time,
    IOSStartupTemperature temperature) {
  switch (temperature) {
    case IOSStartupTemperature::kNotPrewarmed: {
      const std::optional<base::TimeDelta> pre_main_duration =
          [startup_information preMainDuration];
      if (main_to_now_time.has_value() && pre_main_duration.has_value() &&
          pre_main_duration->is_positive()) {
        return *main_to_now_time + *pre_main_duration;
      }
      return std::nullopt;
    }
    case IOSStartupTemperature::kActivePrewarm:
      return main_to_now_time;
    case IOSStartupTemperature::kLaunchedInBackground:
      return scene_connection_to_now_time;
    case IOSStartupTemperature::kSuspicious:
      return std::nullopt;
  }
}

void RecordStartupLatencyMetric(std::string_view metric_name,
                                id<StartupInformation> startup_information,
                                std::optional<base::TimeDelta> main_to_now_time,
                                base::TimeDelta scene_connection_to_now_time) {
  if (!startup_information || !startup_information.isColdStart) {
    return;
  }

  if (![startup_information isLaunchedInBackground]) {
    if (!main_to_now_time.has_value()) {
      [startup_information
          maybeSetLaunchReason:IOSLaunchReason::
                                   kForegroundWithSystemSuspension];
    } else if (*main_to_now_time > kLaunchReasonTimeout) {
      [startup_information maybeSetLaunchReason:IOSLaunchReason::kSuspicious];
    } else if (base::ios::IsApplicationPreWarmed()) {
      [startup_information maybeSetLaunchReason:IOSLaunchReason::kPreWarming];
    } else {
      [startup_information maybeSetLaunchReason:IOSLaunchReason::kForeground];
    }
  }

  if ([startup_information launchReason] ==
      IOSLaunchReason::kForegroundWithSystemSuspension) {
    return;
  }

  const IOSStartupTemperature temperature =
      GetStartupTemperature(startup_information);
  const ColdStartType cold_start_type = GetColdStartType(startup_information);

  std::optional<base::TimeDelta> latency =
      ComputeStartupLatency(startup_information, main_to_now_time,
                            scene_connection_to_now_time, temperature);

  if (latency.has_value() && latency->is_positive()) {
    LogStartupDurationWithTemperatureAndType(metric_name, *latency, temperature,
                                             cold_start_type);
  }
}
