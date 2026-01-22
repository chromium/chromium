// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/mobile_session_crash_helper_metrics_provider.h"

#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "base/version.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

using previous_session_info_constants::DeviceThermalState;

namespace {

// Amount of storage, in kilobytes, considered to be critical enough to
// negatively affect device operation.
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

// Values of the UMA Stability.iOS.UTE.MobileSessionOOMShutdownHint histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MobileSessionOomShutdownHint {
  // There is no additional information for this UTE/XTE.
  NoInformation = 0,
  // Session restoration was in progress before this UTE.
  SessionRestorationUte = 1,
  // Session restoration was in progress before this XTE.
  SessionRestorationXte = 2,
  kMaxValue = SessionRestorationXte
};

// Values of the Stability.iOS.UTE.MobileSessionAppWillTerminateWasReceived
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class MobileSessionAppWillTerminateWasReceived {
  // ApplicationWillTerminate notification was not received for this XTE.
  WasNotReceivedForXte = 0,
  // ApplicationWillTerminate notification was not received for this UTE.
  WasNotReceivedForUte = 1,
  // ApplicationWillTerminate notification was received for this XTE.
  WasReceivedForXte = 2,
  // ApplicationWillTerminate notification was received for this UTE.
  WasReceivedForUte = 3,
  kMaxValue = WasReceivedForUte
};

// Values of the Stability.iOS.UTE.MobileSessionAppState histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MobileSessionAppState {
  // This should not happen and presence of this value likely indicates a bug
  // in Chrome or UIKit code.
  UnknownUte = 0,

  // This should not happen and presence of this value likely indicates a bug
  // in Chrome or UIKit code.
  UnknownXte = 1,

  // App state was UIApplicationStateActive during UTE.
  ActiveUte = 2,

  // App state was UIApplicationStateActive during XTE.
  ActiveXte = 3,

  // App state was UIApplicationStateInactive during UTE.
  InactiveUte = 4,

  // App state was UIApplicationStateInactive during XTE.
  InactiveXte = 5,

  // App state was UIApplicationStateBackground during UTE.
  BackgroundUte = 6,

  // App state was UIApplicationStateBackground during XTE.
  BackgroundXte = 7,
  kMaxValue = BackgroundXte
};

// Returns value to log for Stability.iOS.UTE.MobileSessionOOMShutdownHint
// histogram.
MobileSessionOomShutdownHint GetMobileSessionOomShutdownHint(
    bool has_possible_explanation) {
  if ([PreviousSessionInfo sharedInstance].terminatedDuringSessionRestoration) {
    return has_possible_explanation
               ? MobileSessionOomShutdownHint::SessionRestorationXte
               : MobileSessionOomShutdownHint::SessionRestorationUte;
  }
  return MobileSessionOomShutdownHint::NoInformation;
}

// Returns value to log for Stability.iOS.UTE.MobileSessionAppState
// histogram.
MobileSessionAppState GetMobileSessionAppState(bool has_possible_explanation) {
  if (!PreviousSessionInfo.sharedInstance.applicationState) {
    return has_possible_explanation ? MobileSessionAppState::UnknownXte
                                    : MobileSessionAppState::UnknownUte;
  }

  switch (*PreviousSessionInfo.sharedInstance.applicationState) {
    case UIApplicationStateActive:
      return has_possible_explanation ? MobileSessionAppState::ActiveXte
                                      : MobileSessionAppState::ActiveUte;
    case UIApplicationStateInactive:
      return has_possible_explanation ? MobileSessionAppState::InactiveXte
                                      : MobileSessionAppState::InactiveUte;
    case UIApplicationStateBackground:
      return has_possible_explanation ? MobileSessionAppState::BackgroundXte
                                      : MobileSessionAppState::BackgroundUte;
  }
  NOTREACHED();
}

// Returns value to record for
// Stability.iOS.UTE.MobileSessionAppWillTerminateWasReceived histogram.
MobileSessionAppWillTerminateWasReceived
GetMobileSessionAppWillTerminateWasReceived(bool has_possible_explanation) {
  if (!PreviousSessionInfo.sharedInstance.applicationWillTerminateWasReceived) {
    return has_possible_explanation
               ? MobileSessionAppWillTerminateWasReceived::WasNotReceivedForXte
               : MobileSessionAppWillTerminateWasReceived::WasNotReceivedForUte;
  }

  return has_possible_explanation
             ? MobileSessionAppWillTerminateWasReceived::WasReceivedForXte
             : MobileSessionAppWillTerminateWasReceived::WasReceivedForUte;
}

// Logs `type` in the shutdown type histogram.
void LogShutdownType(MobileSessionShutdownType type) {
  // This histogram will not appear in the initial stability log.
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.MobileSessionShutdownType",
                                      type, MOBILE_SESSION_SHUTDOWN_TYPE_COUNT);
}

// Logs the time which the application was in the background between
// `session_end_time` and now.
void LogApplicationBackgroundedTime(NSDate* session_end_time) {
  NSTimeInterval background_time =
      [[NSDate date] timeIntervalSinceDate:session_end_time];
  UMA_STABILITY_HISTOGRAM_LONG_TIMES(
      "Stability.iOS.UTE.TimeBetweenUTEAndNextLaunch",
      base::Seconds(background_time));
}

// Logs the OS version change between `os_version` and the current os version.
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

// Logs the thermal state of the device.
void LogDeviceThermalState(DeviceThermalState thermal_state) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.iOS.UTE.DeviceThermalState",
                                      thermal_state,
                                      DeviceThermalState::kMaxValue);
}

MobileSessionShutdownType GetLastShutdownType(bool has_reports_to_upload) {
  // If the last app lifetime did not end with a crash, then log it as a normal
  // shutdown while in the background.
  if (GetApplicationContext()->WasLastShutdownClean()) {
    return SHUTDOWN_IN_BACKGROUND;
  }

  // Ignore unclean shutdowns after an upgrade since the data might incorrectly
  // attribute a crash to the next version.
  if ([[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade]) {
    return FIRST_LAUNCH_AFTER_UPGRADE;
  }

  if (has_reports_to_upload) {
    // The cause of the crash is known.
    if ([[PreviousSessionInfo sharedInstance]
            didSeeMemoryWarningShortlyBeforeTerminating]) {
      return SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING;
    }
    return SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING;
  }

  // The cause of the crash is not known. Check the common causes in order of
  // severity and likeliness to have caused the crash.
  if ([[PreviousSessionInfo sharedInstance]
          didSeeMemoryWarningShortlyBeforeTerminating]) {
    return SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING;
  }
  // There is no known cause.
  return SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING;
}

}  // namespace

namespace mobile_session_metrics {

void OnProcessIntermediateDumpsFinished(bool has_new_pending_reports) {
  MobileSessionShutdownType shutdown_type =
      GetLastShutdownType(has_new_pending_reports);
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

  // Log metrics to improve categorization of crashes.
  PreviousSessionInfo* session_info = [PreviousSessionInfo sharedInstance];
  LogApplicationBackgroundedTime(session_info.sessionEndTime);

  NSInteger allTabCount = session_info.tabCount + session_info.OTRTabCount;
  if (shutdown_type == SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING ||
      shutdown_type ==
          SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING) {
    // Log UTE metrics only if the crash was classified as a UTE.

    if (session_info.OSVersion) {
      LogOSVersionChange(base::SysNSStringToUTF8(session_info.OSVersion));
    }
    LogDeviceThermalState(session_info.deviceThermalState);

    UMA_STABILITY_HISTOGRAM_BOOLEAN(
        "Stability.iOS.UTE.OSRestartedAfterPreviousSession",
        session_info.OSRestartedAfterPreviousSession);

    UMA_STABILITY_HISTOGRAM_COUNTS_100("Stability.iOS.TabCountBeforeUTE",
                                       allTabCount);

    bool possible_explanation =
        // Log any of the following cases as a possible explanation for the
        // crash:
        // - device restarted when Chrome was in the foreground (OS was updated,
        // battery died, or iPhone X or newer was powered off)
        (session_info.OSRestartedAfterPreviousSession) ||
        // - storage was critically low
        (session_info.availableDeviceStorage >= 0 &&
         session_info.availableDeviceStorage <= kCriticallyLowDeviceStorage) ||
        // - device in abnormal thermal state
        session_info.deviceThermalState == DeviceThermalState::kCritical ||
        session_info.deviceThermalState == DeviceThermalState::kSerious;
    UMA_STABILITY_HISTOGRAM_BOOLEAN("Stability.iOS.UTE.HasPossibleExplanation",
                                    possible_explanation);

    UMA_STABILITY_HISTOGRAM_ENUMERATION(
        "Stability.iOS.UTE.MobileSessionOOMShutdownHint",
        GetMobileSessionOomShutdownHint(possible_explanation),
        MobileSessionOomShutdownHint::kMaxValue);

    UMA_STABILITY_HISTOGRAM_ENUMERATION(
        "Stability.iOS.UTE.MobileSessionAppState",
        GetMobileSessionAppState(possible_explanation),
        MobileSessionAppState::kMaxValue);

    UMA_STABILITY_HISTOGRAM_ENUMERATION(
        "Stability.iOS.UTE.MobileSessionAppWillTerminateWasReceived",
        GetMobileSessionAppWillTerminateWasReceived(possible_explanation),
        MobileSessionAppWillTerminateWasReceived::kMaxValue);
  } else if (shutdown_type ==
                 SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING ||
             shutdown_type ==
                 SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING) {
    UMA_STABILITY_HISTOGRAM_COUNTS_100(
        "Stability.iOS.TabCountBeforeSignalCrash", allTabCount);
  }
  [session_info resetSessionRestorationFlag];
}

}  // namespace mobile_session_metrics
