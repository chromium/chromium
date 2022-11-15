// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_helper.h"

#import <UIKit/UIKit.h>
#import <stddef.h>
#import <stdint.h>
#import <sys/stat.h>
#import <sys/sysctl.h>

#import "base/auto_reset.h"
#import "base/bind.h"
#import "base/debug/crash_logging.h"
#import "base/feature_list.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
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
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"
#import "ios/chrome/browser/crash_report/crash_upload_list.h"
#import "ios/chrome/browser/crash_report/features.h"
#import "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace crash_helper {

namespace {

const char kUptimeAtRestoreInMs[] = "uptime_at_restore_in_ms";
const char kUploadedInRecoveryMode[] = "uploaded_in_recovery_mode";

// Delete breakpad reports after 60 days.
void DeleteOldReportsInDirectory(base::FilePath directory) {
  base::FileEnumerator enumerator(directory, false,
                                  base::FileEnumerator::FILES);
  base::FilePath cur_file;
  while (!(cur_file = enumerator.Next()).value().empty()) {
    if (cur_file.BaseName().value() != kReporterLogFilename) {
      time_t now = time(nullptr);
      struct stat st;
      if (lstat(cur_file.value().c_str(), &st) != 0) {
        continue;
      }

      // 60 days.
      constexpr time_t max_breakpad_report_age_sec = 60 * 60 * 24 * 60;
      if (st.st_mtime <= now - max_breakpad_report_age_sec) {
        base::DeleteFile(cur_file);
      }
    }
  }
}

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
  if ([MainThreadFreezeDetector sharedInstance].lastSessionEndedFrozen) {
    return SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN;
  }
  if ([[PreviousSessionInfo sharedInstance]
          didSeeMemoryWarningShortlyBeforeTerminating]) {
    return SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING;
  }
  // There is no known cause.
  return SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING;
}

// Tells crashpad to start processing previously created intermediate dumps and
// begin uploading when possible.
void ProcessIntermediateDumps() {
  crash_reporter::ProcessIntermediateDumps();
  [[MainThreadFreezeDetector sharedInstance] processIntermediateDumps];
  crash_reporter::StartProcessingPendingReports();
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
  if (sysctl(mib, std::size(mib), &kern_proc_info, &len, nullptr, 0) != 0)
    return 0;
  time_t process_uptime_seconds =
      tv.tv_sec - kern_proc_info.kp_proc.p_starttime.tv_sec;
  return static_cast<const int64_t>(process_uptime_seconds) *
         base::Time::kMillisecondsPerSecond;
}

}  // namespace

void SyncCrashpadEnabledOnNextRun() {
  [app_group::GetGroupUserDefaults()
      setBool:base::FeatureList::IsEnabled(kCrashpadIOS) ? YES : NO
       forKey:base::SysUTF8ToNSString(common::kCrashpadStartOnNextRun)];
}

void Start() {
  DCHECK(!crash_reporter::IsBreakpadRunning());
  DCHECK(!crash_reporter::IsCrashpadRunning());

  // Notifying the PathService on the location of the crashes so that crashes
  // can be displayed to the user on the about:crashes page.  Use the app group
  // so crashes can be shared by plugins.
  if (common::CanUseCrashpad()) {
    base::PathService::Override(ios::DIR_CRASH_DUMPS,
                                common::CrashpadDumpLocation());
    bool initialized = common::StartCrashpad();
    if (initialized) {
      crash_reporter::SetCrashpadRunning(true);
    }
    UMA_HISTOGRAM_BOOLEAN("Stability.IOS.Crashpad.Initialized", initialized);
  } else {
    NSArray* cachesDirectories = NSSearchPathForDirectoriesInDomains(
        NSCachesDirectory, NSUserDomainMask, YES);
    NSString* cachePath = [cachesDirectories objectAtIndex:0];
    NSString* dumpDirectory =
        [cachePath stringByAppendingPathComponent:@kDefaultLibrarySubdirectory];
    base::PathService::Override(
        ios::DIR_CRASH_DUMPS,
        base::FilePath(base::SysNSStringToUTF8(dumpDirectory)));
    [[BreakpadController sharedInstance] start:YES];
    crash_reporter::SetBreakpadRunning(true);

    // Register channel information (Breakpad only, crashpad does this
    // automatically).
    std::string channel_name = GetChannelString();
    if (channel_name.length()) {
      static crash_reporter::CrashKeyString<64> key("channel");
      key.Set(channel_name);
    }
  }

  crash_reporter::InitializeCrashKeys();

  // Don't start MTFD when prewarmed, the check thread will just get confused.
  if (!base::ios::IsApplicationPreWarmed()) {
    [[MainThreadFreezeDetector sharedInstance] start];
  }
}

void SetEnabled(bool enabled) {
  // Caches the uploading flag in NSUserDefaults, so that we can access the
  // value immediately on startup, such as in safe mode or extensions.
  crash_helper::common::SetUserEnabledUploading(enabled);

  // It is necessary to always call `MainThreadFreezeDetector setEnabled` as
  // the function will update its preference based on finch.
  [[MainThreadFreezeDetector sharedInstance] setEnabled:enabled];

  // Crashpad is always running, don't shut it off. Using CanUseCrashpad()
  // here, because if Crashpad fails to init, do not unintentionally enable
  // breakpad.
  if (common::CanUseCrashpad()) {
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
    return;
  }

  if (crash_reporter::IsBreakpadRunning() == enabled)
    return;
  crash_reporter::SetBreakpadRunning(enabled);
  if (enabled) {
    [[BreakpadController sharedInstance] start:NO];
  } else {
    [[BreakpadController sharedInstance] stop];
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

  if (crash_reporter::IsBreakpadRunning()) {
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
      // Clean old breakpad files here. Breakpad-only as Crashpad has it's own
      // database cleaner.
      base::FilePath crash_directory;
      base::PathService::Get(ios::DIR_CRASH_DUMPS, &crash_directory);
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&DeleteOldReportsInDirectory, crash_directory));
    });
    [[BreakpadController sharedInstance] setUploadingEnabled:YES];
  }
}

void PauseBreakpadUploads() {
  if (crash_reporter::IsBreakpadRunning()) {
    [[BreakpadController sharedInstance] setUploadingEnabled:NO];
  }
}

void ProcessIntermediateReportsForSafeMode() {
  if (crash_reporter::IsCrashpadRunning()) {
    crash_reporter::ProcessIntermediateDumps(
        {{kUploadedInRecoveryMode, "yes"}});
  }
}

int GetPendingCrashReportCount() {
  if (crash_reporter::IsCrashpadRunning()) {
    int count = 0;
    std::vector<crash_reporter::Report> reports;
    crash_reporter::GetReports(&reports);
    for (auto& report : reports) {
      if (report.state == crash_reporter::ReportUploadState::Pending ||
          report.state ==
              crash_reporter::ReportUploadState::Pending_UserRequested) {
        count++;
      }
    }
    return count;
  }

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block int outerCrashReportCount = 0;
  [[BreakpadController sharedInstance] getCrashReportCount:^(int count) {
    outerCrashReportCount = count;
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  return outerCrashReportCount;
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
  if (!crash_reporter::IsBreakpadRunning())
    return;
  // We use gettimeofday and BREAKPAD_PROCESS_START_TIME to compute the
  // uptime to stay as close as possible as how breakpad computes the
  // "ProcessUptime" in order to have meaningful comparison in dremel.
  struct timeval tv;
  gettimeofday(&tv, NULL);
  // The values stored in the breakpad log are only accessible through a
  // BreakpadRef. To record the process uptime at restore, the value of
  // BREAKPAD_PROCESS_START_TIME is required to compute the delta.
  [[BreakpadController sharedInstance] withBreakpadRef:^(BreakpadRef ref) {
    if (!ref)
      return;
    NSString* processStartTimeSecondsString =
        BreakpadKeyValue(ref, @BREAKPAD_PROCESS_START_TIME);
    if (!processStartTimeSecondsString)
      return;

    time_t processStartTimeSeconds =
        [processStartTimeSecondsString longLongValue];
    time_t processUptimeSeconds = tv.tv_sec - processStartTimeSeconds;
    const int64_t processUptimeMilliseconds =
        static_cast<const int64_t>(processUptimeSeconds) *
        base::Time::kMillisecondsPerSecond;
    BreakpadAddUploadParameter(
        ref, base::SysUTF8ToNSString(kUptimeAtRestoreInMs),
        [NSString stringWithFormat:@"%llu", processUptimeMilliseconds]);
  }];
}

void StartUploadingReportsInRecoveryMode() {
  if (crash_reporter::IsCrashpadRunning()) {
    crash_reporter::StartProcessingPendingReports();
    return;
  }

  if (!crash_reporter::IsBreakpadRunning())
    return;
  [[BreakpadController sharedInstance] stop];
  [[BreakpadController sharedInstance] setParametersToAddAtUploadTime:@{
    base::SysUTF8ToNSString(kUploadedInRecoveryMode) : @"yes"
  }];
  [[BreakpadController sharedInstance] setUploadInterval:1];
  [[BreakpadController sharedInstance] start:NO];
  [[BreakpadController sharedInstance] setUploadingEnabled:YES];
}

void RestoreDefaultConfiguration() {
  if (!crash_reporter::IsBreakpadRunning()) {
    return;
  }

  [[BreakpadController sharedInstance] stop];
  [[BreakpadController sharedInstance] resetConfiguration];
  [[BreakpadController sharedInstance] start:NO];
  [[BreakpadController sharedInstance] setUploadingEnabled:NO];
}

void ClearReportsBetween(base::Time delete_begin, base::Time delete_end) {
  ios::CreateCrashUploadList()->Clear(delete_begin, delete_end);
}

}  // namespace crash_helper
