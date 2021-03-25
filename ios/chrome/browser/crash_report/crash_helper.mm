// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_helper.h"

#import <UIKit/UIKit.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/crash/core/app/crashpad.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/reporter_running_ios.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/crash_report/chrome_crash_reporter_client.h"
#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"
#include "ios/chrome/browser/crash_report/features.h"
#import "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"
#include "ios/chrome/common/channel_info.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace crash_helper {

namespace {

// Key in NSUserDefaults for a Boolean value that stores whether to upload
// crash reports.
NSString* const kCrashReportsUploadingEnabledKey =
    @"CrashReportsUploadingEnabled";

// Key in NSUserDefaults for a Boolean value that stores the last feature
// value of kCrashpadIOS.
NSString* const kCrashpadStartOnNextRun = @"CrashpadStartOnNextRun";

const char kUptimeAtRestoreInMs[] = "uptime_at_restore_in_ms";
const char kUploadedInRecoveryMode[] = "uploaded_in_recovery_mode";

void DeleteAllReportsInDirectory(base::FilePath directory) {
  base::FileEnumerator enumerator(directory, false,
                                  base::FileEnumerator::FILES);
  base::FilePath cur_file;
  while (!(cur_file = enumerator.Next()).value().empty()) {
    if (cur_file.BaseName().value() != kReporterLogFilename)
      base::DeleteFile(cur_file);
  }
}

// Tells crashpad to start processing previously created intermediate dumps and
// begin uploading when possible.
void ProcessIntermediateDumps() {
  crash_reporter::ProcessIntermediateDumps();
  crash_reporter::StartProcesingPendingReports();
}

// Callback for logging::SetLogMessageHandler
bool FatalMessageHandler(int severity,
                         const char* file,
                         int line,
                         size_t message_start,
                         const std::string& str) {
  // Do not handle non-FATAL.
  if (severity != logging::LOG_FATAL)
    return false;

  // In case of OOM condition, this code could be reentered when
  // constructing and storing the key.  Using a static is not
  // thread-safe, but if multiple threads are in the process of a
  // fatal crash at the same time, this should work.
  static bool guarded = false;
  if (guarded)
    return false;

  base::AutoReset<bool> guard(&guarded, true);

  // Only log last path component.  This matches logging.cc.
  if (file) {
    const char* slash = strrchr(file, '/');
    if (slash)
      file = slash + 1;
  }

  NSString* fatal_value = [NSString
      stringWithFormat:@"%s:%d: %s", file, line, str.c_str() + message_start];
  static crash_reporter::CrashKeyString<2550> key("LOG_FATAL");
  key.Set(base::SysNSStringToUTF8(fatal_value));

  // Rather than including the code to force the crash here, allow the
  // caller to do it.
  return false;
}

// Called after Breakpad finishes uploading each report.
void UploadResultHandler(NSString* report_id, NSError* error) {
  base::UmaHistogramSparse("CrashReport.BreakpadIOSUploadOutcome", error.code);
}

// Check and cache the NSUserDefault value synced from the associated
// kCrashpadIOS feature.
bool CanCrashpadStart() {
  static bool can_crashpad_start = [[NSUserDefaults standardUserDefaults]
      boolForKey:kCrashpadStartOnNextRun];
  return can_crashpad_start;
}

// Returns the uptime, the difference between now and start time.
int64_t GetUptimeMilliseconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  kinfo_proc kern_proc_info;
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t len = sizeof(kern_proc_info);
  if (sysctl(mib, base::size(mib), &kern_proc_info, &len, nullptr, 0) != 0)
    return 0;
  time_t process_uptime_seconds =
      tv.tv_sec - kern_proc_info.kp_proc.p_starttime.tv_sec;
  return static_cast<const int64_t>(process_uptime_seconds) *
         base::Time::kMillisecondsPerSecond;
}

}  // namespace

void SyncCrashpadEnabledOnNextRun() {
  [[NSUserDefaults standardUserDefaults]
      setBool:base::FeatureList::IsEnabled(kCrashpadIOS) ? YES : NO
       forKey:kCrashpadStartOnNextRun];
}

void Start() {
  DCHECK(!crash_reporter::IsBreakpadRunning());
  DCHECK(!crash_reporter::IsCrashpadRunning());

  // Notifying the PathService on the location of the crashes so that crashes
  // can be displayed to the user on the about:crashes page.
  NSArray* cachesDirectories = NSSearchPathForDirectoriesInDomains(
      NSCachesDirectory, NSUserDomainMask, YES);
  NSString* cachePath = [cachesDirectories objectAtIndex:0];
  NSString* dumpDirectory =
      [cachePath stringByAppendingPathComponent:@kDefaultLibrarySubdirectory];
  base::PathService::Override(
      ios::DIR_CRASH_DUMPS,
      base::FilePath(base::SysNSStringToUTF8(dumpDirectory)));

  logging::SetLogMessageHandler(&FatalMessageHandler);
  if (CanCrashpadStart()) {
    ChromeCrashReporterClient::Create();
    crash_reporter::InitializeCrashpad(true, "");
    crash_reporter::SetCrashpadRunning(true);
  } else {
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
  [[MainThreadFreezeDetector sharedInstance] start];
}

void SetEnabled(bool enabled) {
  // It is necessary to always call |MainThreadFreezeDetector setEnabled| as
  // the function will update its preference based on finch.
  [[MainThreadFreezeDetector sharedInstance] setEnabled:enabled];

  // Crashpad is always running, don't shut it off.
  if (crash_reporter::IsCrashpadRunning()) {
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

// Breakpad only.
void SetBreakpadUploadingEnabled(bool enabled) {
  if (!crash_reporter::IsBreakpadRunning())
    return;
  if (enabled) {
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
      [[BreakpadController sharedInstance]
          setUploadCallback:UploadResultHandler];
    });
  }
  [[BreakpadController sharedInstance] setUploadingEnabled:enabled];
}

// Caches the uploading flag in NSUserDefaults, so that we can access the value
// in safe mode.
void SetUserEnabledUploading(bool uploading_enabled) {
  [[NSUserDefaults standardUserDefaults]
      setBool:uploading_enabled ? YES : NO
       forKey:kCrashReportsUploadingEnabledKey];
}

void SetUploadingEnabled(bool enabled) {
  if (enabled && [UIApplication sharedApplication].applicationState ==
                     UIApplicationStateInactive) {
    return;
  }

  if (crash_reporter::IsCrashpadRunning()) {
    crash_reporter::SetUploadConsent(enabled);
    return;
  }

  if ([MainThreadFreezeDetector sharedInstance].canUploadBreakpadCrashReports) {
    SetBreakpadUploadingEnabled(enabled);
  } else {
    [[MainThreadFreezeDetector sharedInstance]
        prepareCrashReportsForUpload:^() {
          SetBreakpadUploadingEnabled(enabled);
        }];
  }
}

bool UserEnabledUploading() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kCrashReportsUploadingEnabledKey];
}

void CleanupCrashReports(BOOL after_upgrade) {
  if (crash_reporter::IsCrashpadRunning()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&ProcessIntermediateDumps));
    return;
  }

  if (after_upgrade) {
    base::FilePath crash_directory;
    base::PathService::Get(ios::DIR_CRASH_DUMPS, &crash_directory);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DeleteAllReportsInDirectory, crash_directory));
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

void GetCrashReportCount(void (^callback)(int)) {
  if (crash_reporter::IsCrashpadRunning()) {
    callback(GetPendingCrashReportCount());
  }

  [[BreakpadController sharedInstance] getCrashReportCount:callback];
}

bool HasReportToUpload() {
  return GetPendingCrashReportCount() > 0;
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
    crash_reporter::StartProcesingPendingReports();
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
  if (crash_reporter::IsCrashpadRunning()) {
    return;
  }

  if (!crash_reporter::IsBreakpadRunning())
    return;
  [[BreakpadController sharedInstance] stop];
  [[BreakpadController sharedInstance] resetConfiguration];
  [[BreakpadController sharedInstance] start:NO];
  [[BreakpadController sharedInstance] setUploadingEnabled:NO];
}

}  // namespace crash_helper
