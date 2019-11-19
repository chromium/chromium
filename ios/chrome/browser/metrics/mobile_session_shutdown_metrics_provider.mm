// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/mobile_session_shutdown_metrics_provider.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "components/metrics/metrics_service.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using previous_session_info_constants::DeviceBatteryState;
using previous_session_info_constants::DeviceThermalState;

namespace {

// Amount of storage, in kilobytes, considered to be critical enough to
// negatively effect device operation.
const int kCriticallyLowDeviceStorage = 1024 * 5;

// An enum representing the difference between two version numbers.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VersionComparison {
  kSameVersion = 0,
  kMinorVersionChange = 1,
  kMajorVersionChange = 2,
  kMaxValue = kMajorVersionChange,
};

// Logs |type| in the shutdown type histogram.
void LogShutdownType(MobileSessionShutdownType type) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.MobileSessionShutdownType",
                                      type, MOBILE_SESSION_SHUTDOWN_TYPE_COUNT);
}

// Logs the time which the application was in the background between
// |session_end_time| and now.
void LogApplicationBackgroundedTime(NSDate* session_end_time) {
  NSTimeInterval background_time =
      [[NSDate date] timeIntervalSinceDate:session_end_time];
  UMA_STABILITY_HISTOGRAM_LONG_TIMES(
      "Stability.iOS.UTE.TimeBetweenUTEAndNextLaunch",
      base::TimeDelta::FromSeconds(background_time));
}

// Logs the device |battery_level| as a UTE stability metric.
void LogBatteryCharge(float battery_level) {
  int battery_charge = static_cast<int>(battery_level * 100);
  UMA_STABILITY_HISTOGRAM_PERCENTAGE("Stability.iOS.UTE.BatteryCharge",
                                     battery_charge);
}

// Logs the device's |available_storage| as a UTE stability metric.
void LogAvailableStorage(NSInteger available_storage) {
  UMA_STABILITY_HISTOGRAM_CUSTOM_COUNTS("Stability.iOS.UTE.AvailableStorage",
                                        available_storage, 1, 200000, 100);
}

// Logs the OS version change between |os_version| and the current os version.
// Records whether the version is the same, if a minor version change occurred,
// or if a major version change occurred.
void LogOSVersionChange(std::string os_version) {
  base::Version previous_os_version = base::Version(os_version);
  base::Version current_os_version =
      base::Version(base::SysInfo::OperatingSystemVersion());

  VersionComparison difference = VersionComparison::kSameVersion;
  if (previous_os_version.CompareTo(current_os_version) != 0) {
    const std::vector<uint32_t> previous_os_version_components =
        previous_os_version.components();
    const std::vector<uint32_t> current_os_version_components =
        current_os_version.components();
    if (previous_os_version_components.size() > 0 &&
        current_os_version_components.size() > 0 &&
        previous_os_version_components[0] != current_os_version_components[0]) {
      difference = VersionComparison::kMajorVersionChange;
    } else {
      difference = VersionComparison::kMinorVersionChange;
    }
  }

  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.iOS.UTE.OSVersion", difference,
                                      VersionComparison::kMaxValue);
}

// Logs wether or not low power mode is enabled.
void LogLowPowerMode(bool low_power_mode_enabled) {
  UMA_STABILITY_HISTOGRAM_BOOLEAN("Stability.iOS.UTE.LowPowerModeEnabled",
                                  low_power_mode_enabled);
}

// Logs the thermal state of the device.
void LogDeviceThermalState(DeviceThermalState thermal_state) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.iOS.UTE.DeviceThermalState",
                                      thermal_state,
                                      DeviceThermalState::kMaxValue);
}
}  // namespace

const float kCriticallyLowBatteryLevel = 0.01;

MobileSessionShutdownMetricsProvider::MobileSessionShutdownMetricsProvider(
    metrics::MetricsService* metrics_service)
    : metrics_service_(metrics_service) {
  DCHECK(metrics_service_);
}

MobileSessionShutdownMetricsProvider::~MobileSessionShutdownMetricsProvider() {}

bool MobileSessionShutdownMetricsProvider::HasPreviousSessionData() {
  return true;
}

void MobileSessionShutdownMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  MobileSessionShutdownType shutdown_type = GetLastShutdownType();
  LogShutdownType(shutdown_type);

  // If app was upgraded since the last session, even if the previous session
  // ended in an unclean shutdown (crash, may or may not be UTE), this should
  // *not* be logged into one of the Foreground* or Background* states of
  // MobileSessionShutdownType. The crash is really from the pre-upgraded
  // version of app. Logging it now will incorrectly inflate the current
  // version's crash count with a crash that happened in a previous version of
  // the app.
  //
  // Not counting first run after upgrade does *not* bias the distribution of
  // the 4 Foreground* termination states because the reason of a crash would
  // not be affected by an imminent upgrade of Chrome app. Thus, the ratio of
  // Foreground shutdowns w/ crash log vs. w/o crash log is expected to be the
  // same regardless of whether First Launch after Upgrade is considered or not.
  if (shutdown_type == FIRST_LAUNCH_AFTER_UPGRADE) {
    return;
  }

  // Do not log UTE metrics if the application terminated cleanly.
  if (shutdown_type == SHUTDOWN_IN_BACKGROUND) {
    return;
  }

  PreviousSessionInfo* session_info = [PreviousSessionInfo sharedInstance];
  // Log metrics to improve categorization of crashes.
  LogApplicationBackgroundedTime(session_info.sessionEndTime);

  if (session_info.deviceBatteryState == DeviceBatteryState::kUnplugged) {
    LogBatteryCharge(session_info.deviceBatteryLevel);
  }
  if (session_info.availableDeviceStorage >= 0) {
    LogAvailableStorage(session_info.availableDeviceStorage);
  }
  if (session_info.OSVersion) {
    LogOSVersionChange(base::SysNSStringToUTF8(session_info.OSVersion));
  }
  LogLowPowerMode(session_info.deviceWasInLowPowerMode);
  LogDeviceThermalState(session_info.deviceThermalState);

  UMA_STABILITY_HISTOGRAM_BOOLEAN(
      "Stability.iOS.UTE.OSRestartedAfterPreviousSession",
      session_info.OSRestartedAfterPreviousSession);

  bool possible_explanation =
      // Log any of the following cases as a possible explanation for the
      // crash:
      // - device restarted while the battery was critically low
      (session_info.deviceBatteryState == DeviceBatteryState::kUnplugged &&
       session_info.deviceBatteryLevel <= kCriticallyLowBatteryLevel &&
       session_info.OSRestartedAfterPreviousSession) ||
      // - storage was critically low
      (session_info.availableDeviceStorage >= 0 &&
       session_info.availableDeviceStorage <= kCriticallyLowDeviceStorage) ||
      // - OS version changed
      session_info.isFirstSessionAfterOSUpgrade ||
      // - device in abnormal thermal state
      session_info.deviceThermalState == DeviceThermalState::kCritical ||
      session_info.deviceThermalState == DeviceThermalState::kSerious;
  UMA_STABILITY_HISTOGRAM_BOOLEAN("Stability.iOS.UTE.HasPossibleExplanation",
                                  possible_explanation);
}

MobileSessionShutdownType
MobileSessionShutdownMetricsProvider::GetLastShutdownType() {
  if (IsFirstLaunchAfterUpgrade()) {
    return FIRST_LAUNCH_AFTER_UPGRADE;
  }

  // If the last app lifetime did not end with a crash, then log it as a normal
  // shutdown while in the background.
  if (metrics_service_->WasLastShutdownClean()) {
    return SHUTDOWN_IN_BACKGROUND;
  }

  // If the last app lifetime ended with main thread not responding, log it as
  // main thread frozen shutdown.
  if (LastSessionEndedFrozen()) {
    return SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN;
  }

  // If the last app lifetime ended in a crash, log the type of crash.
  if (ReceivedMemoryWarningBeforeLastShutdown()) {
    if (HasCrashLogs()) {
      return SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING;
    }
    return SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING;
  }

  if (HasCrashLogs()) {
    return SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING;
  }
  return SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING;
}

bool MobileSessionShutdownMetricsProvider::IsFirstLaunchAfterUpgrade() {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}

bool MobileSessionShutdownMetricsProvider::HasCrashLogs() {
  return breakpad_helper::HasReportToUpload();
}

bool MobileSessionShutdownMetricsProvider::LastSessionEndedFrozen() {
  return [MainThreadFreezeDetector sharedInstance].lastSessionEndedFrozen;
}

bool MobileSessionShutdownMetricsProvider::
    ReceivedMemoryWarningBeforeLastShutdown() {
  return [[PreviousSessionInfo sharedInstance]
      didSeeMemoryWarningShortlyBeforeTerminating];
}
