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
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/no_destructor.h"
#import "base/path_service.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/crash_key.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "components/gwp_asan/crash_handler/crash_handler.h"
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
BASE_FEATURE(kIOSCrashUploadKillSwitch, base::FEATURE_DISABLED_BY_DEFAULT);

const char kUptimeAtRestoreInMs[] = "uptime_at_restore_in_ms";
const char kUploadedInRecoveryMode[] = "uploaded_in_recovery_mode";

base::RepeatingCallbackList<void(bool)>&
GetProcessIntermediateDumpsFinishedCallbackList() {
  static base::NoDestructor<base::RepeatingCallbackList<void(bool)>>
      callback_list;
  return *callback_list;
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
  crashpad::UserStreamDataSources user_stream_data_sources;
  user_stream_data_sources.push_back(
      std::make_unique<gwp_asan::UserStreamDataSource>());
  int pending_reports = GetPendingCrashReportCount();
  crash_reporter::ProcessIntermediateDumps({}, &user_stream_data_sources);
  bool has_new_pending_reports = GetPendingCrashReportCount() > pending_reports;
  crash_reporter::StartProcessingPendingReports();

  // Remove this after a few milestones.
  ClearMainThreadFreezeDetectorCache();

  // Wait until after processing intermediate dumps to record last shutdown
  // type.
  dispatch_async(dispatch_get_main_queue(), ^{
    GetProcessIntermediateDumpsFinishedCallbackList().Notify(
        has_new_pending_reports);
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

base::CallbackListSubscription AddProcessIntermediateDumpsFinishedCallback(
    const base::RepeatingCallback<void(bool)>& callback) {
  return GetProcessIntermediateDumpsFinishedCallbackList().Add(callback);
}

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
