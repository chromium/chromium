// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_helper.h"

#import <UIKit/UIKit.h>
#import <stddef.h>
#import <stdint.h>
#import <sys/stat.h>
#import <sys/sysctl.h>

#import "base/auto_reset.h"
#import "base/debug/crash_logging.h"
#import "base/feature_list.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/crash_key.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/model/crash_report_user_application_state.h"
#import "ios/chrome/browser/crash_report/model/crash_upload_list.h"
#import "ios/chrome/browser/crash_report/model/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/crash_report/crash_helper.h"

namespace crash_helper {

namespace {

// Disable all crash uploading (including during safe mode) if the
// kIOSCrashUploadKillSwitch is enabled. By revoking upload consent Crashpad
// will mark any pending reports as skipped. By disabling UserEnabledUploading
// safe mode crashes will be ignored. This also disables the main thread freeze
// detector.
BASE_FEATURE(kIOSCrashUploadKillSwitch,
             "IOSCrashUploadKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kUptimeAtRestoreInMs[] = "uptime_at_restore_in_ms";
const char kUploadedInRecoveryMode[] = "uploaded_in_recovery_mode";

// This mirrors the logic in MobileSessionShutdownMetricsProvider to avoid a
// dependency loop.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum MobileSessionShutdownType {
  SHUTDOWN_IN_BACKGROUND = 0,
  SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING,
  SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING,
  SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING,
  SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING,
  FIRST_LAUNCH_AFTER_UPGRADE,
  SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN,
  MOBILE_SESSION_SHUTDOWN_TYPE_COUNT,
};

// This mirrors the logic in MobileSessionShutdownMetricsProvider, which
// currently calls crash_helper::HasReportToUpload() before Crashpad calls
// ProcessIntermediateDumps. Experiment with instead calling this later during
// startup, but after Crashpad can process intermediate dumps.
MobileSessionShutdownType GetLastShutdownType() {
  if ([[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade]) {
    return FIRST_LAUNCH_AFTER_UPGRADE;
  }

  // If the last app lifetime did not end with a crash, then log it as a normal
  // shutdown while in the background.
  if (GetApplicationContext()->WasLastShutdownClean()) {
    return SHUTDOWN_IN_BACKGROUND;
  }

  if (crash_helper::HasReportToUpload()) {
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

// Cleaning up the cache is best effort. Ignore removal results and errors.
// Remove this after a few milestones.
void ClearMainThreadFreezeDetectorCache() {
  NSString* cacheDirectory = NSSearchPathForDirectoriesInDomains(
      NSCachesDirectory, NSUserDomainMask, YES)[0];
  // The directory containing old UTE crash reports.
  NSString* UTEDirectory =
      [cacheDirectory stringByAppendingPathComponent:@"UTE"];
  BOOL isDirectory = NO;
  NSError* error = nil;
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if ([fileManager fileExistsAtPath:UTEDirectory isDirectory:&isDirectory] &&
      isDirectory) {
    [fileManager removeItemAtPath:UTEDirectory error:&error];
  }

  // The directory containing old UTE crash reports eligible for crashpad
  // processing.
  NSString* UTEPendingCrashpadDirectory =
      [cacheDirectory stringByAppendingPathComponent:@"UTE_CrashpadPending"];
  isDirectory = NO;
  if ([fileManager fileExistsAtPath:UTEPendingCrashpadDirectory
                        isDirectory:&isDirectory] &&
      isDirectory) {
    [fileManager removeItemAtPath:UTEPendingCrashpadDirectory error:&error];
  }
}

// Tells crashpad to start processing previously created intermediate dumps and
// begin uploading when possible.
void ProcessIntermediateDumps() {
  crash_reporter::ProcessIntermediateDumps();
  crash_reporter::StartProcessingPendingReports();

  // Remove this after a few milestones.
  ClearMainThreadFreezeDetectorCache();

  // Wait until after processing intermediate dumps to record last shutdown
  // type.
  dispatch_async(dispatch_get_main_queue(), ^{
    // This histogram is similar to MobileSessionShutdownType, but will not
    // appear in the initial stability log. Because of this, the stability flag
    // on this histogram doesn't matter. It will be reported like any other
    // metric.
    UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.MobileSessionShutdownType2",
                                        GetLastShutdownType(),
                                        MOBILE_SESSION_SHUTDOWN_TYPE_COUNT);
  });
}

// Returns the uptime, the difference between now and start time.
int64_t GetUptimeMilliseconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  kinfo_proc kern_proc_info;
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t len = sizeof(kern_proc_info);
  if (sysctl(mib, std::size(mib), &kern_proc_info, &len, nullptr, 0) != 0) {
    return 0;
  }
  time_t process_uptime_seconds =
      tv.tv_sec - kern_proc_info.kp_proc.p_starttime.tv_sec;
  return static_cast<const int64_t>(process_uptime_seconds) *
         base::Time::kMillisecondsPerSecond;
}

}  // namespace

void Start() {
  DCHECK(!crash_reporter::IsCrashpadRunning());

  // Notifying the PathService on the location of the crashes so that crashes
  // can be displayed to the user on the about:crashes page.  Use the app group
  // so crashes can be shared by plugins.
  base::PathService::Override(ios::DIR_CRASH_DUMPS,
                              common::CrashpadDumpLocation());
  bool initialized = common::StartCrashpad();
  if (initialized) {
    crash_reporter::SetCrashpadRunning(true);
  }
  UMA_HISTOGRAM_BOOLEAN("Stability.IOS.Crashpad.Initialized", initialized);

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  static crash_reporter::CrashKeyString<4> key("partition_alloc");
  key.Set("yes");
#endif

  if (base::ios::IsApplicationPreWarmed()) {
    static crash_reporter::CrashKeyString<4> prewarmed_key("is_prewarmed");
    prewarmed_key.Set("yes");
  }
}

void SetEnabled(bool enabled) {
  if (base::FeatureList::IsEnabled(kIOSCrashUploadKillSwitch)) {
    enabled = false;
  }
  // Caches the uploading flag in NSUserDefaults, so that we can access the
  // value immediately on startup, such as in safe mode or extensions.
  crash_helper::common::SetUserEnabledUploading(enabled);

  // Don't sync upload consent when the app is backgrounded. Crashpad
  // flocks the settings file, and because Chrome puts this in a shared
  // container, slow reads and writes can lead to watchdog kills.
  if (UIApplication.sharedApplication.applicationState ==
      UIApplicationStateActive) {
    // Posts SetUploadConsent on blocking pool thread because it needs access
    // to IO and cannot work from UI thread.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(^{
          crash_reporter::SetUploadConsent(enabled);
        }));
  }
}

void UploadCrashReports() {
  if (crash_reporter::IsCrashpadRunning()) {
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&ProcessIntermediateDumps));
      return;
    });
  }
}

void ProcessIntermediateReportsForSafeMode() {
  if (crash_reporter::IsCrashpadRunning()) {
    crash_reporter::ProcessIntermediateDumps(
        {{kUploadedInRecoveryMode, "yes"}});
  }
}

int GetPendingCrashReportCount() {
  int count = 0;
  if (crash_reporter::IsCrashpadRunning()) {
    std::vector<crash_reporter::Report> reports;
    crash_reporter::GetReports(&reports);
    for (auto& report : reports) {
      if (report.state == crash_reporter::ReportUploadState::Pending ||
          report.state ==
              crash_reporter::ReportUploadState::Pending_UserRequested) {
        count++;
      }
    }
  }
  return count;
}

bool HasReportToUpload() {
  int pending_reports = GetPendingCrashReportCount();

  // This can get called before crash_reporter::StartProcessingPendingReports()
  // is called, which means we need to look for non-zero length files in
  // common::CrashpadDumpLocation()/ dir. See crbug.com/1365765 for details,
  // but this should be removed once MobileSessionShutdownType2 is validated.
  if (crash_reporter::IsCrashpadRunning()) {
    const base::FilePath path =
        common::CrashpadDumpLocation().Append("pending-serialized-ios-dump");
    NSString* path_ns = base::SysUTF8ToNSString(path.value());
    NSArray<NSString*>* pending_files =
        [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path_ns
                                                            error:nil];
    for (NSString* pending_filename : pending_files) {
      NSString* pending_file =
          [path_ns stringByAppendingPathComponent:pending_filename];
      NSDictionary* fileAttributes =
          [[NSFileManager defaultManager] attributesOfItemAtPath:pending_file
                                                           error:nil];
      if ([[fileAttributes objectForKey:NSFileSize] longLongValue] > 0) {
        pending_reports++;
      }
    }
  }
  return pending_reports > 0;
}

// Records the current process uptime in the kUptimeAtRestoreInMs. This
// will allow engineers to dremel crash logs to find crash whose delta between
// process uptime at crash and process uptime at restore is smaller than X
// seconds and find insta-crashers.
void WillStartCrashRestoration() {
  if (crash_reporter::IsCrashpadRunning()) {
    const int64_t uptime_milliseconds = GetUptimeMilliseconds();
    if (uptime_milliseconds > 0) {
      static crash_reporter::CrashKeyString<16> key(kUptimeAtRestoreInMs);
      key.Set(base::NumberToString((uptime_milliseconds)));
    }
    return;
  }
}

void StartUploadingReportsInRecoveryMode() {
  if (crash_reporter::IsCrashpadRunning()) {
    crash_reporter::StartProcessingPendingReports();
    return;
  }
}

void ClearReportsBetween(base::Time delete_begin, base::Time delete_end) {
  ios::CreateCrashUploadList()->Clear(delete_begin, delete_end);
}

}  // namespace crash_helper
