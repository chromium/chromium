// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"

#import "base/debug/debugger.h"
#import "base/files/file_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/crash_key.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "third_party/breakpad/breakpad/src/client/ios/Breakpad.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif
namespace {
// The info contains a dictionary with info about the freeze report.
// See description at `_lastSessionFreezeInfo`.
const char kNsUserDefaultKeyLastSessionInfo[] =
    "MainThreadDetectionLastThreadWasFrozenInfo";

// Clean exit beacon.
NSString* const kLastSessionExitedCleanly = @"LastSessionExitedCleanly";

const NSTimeInterval kFreezeDetectionDelay = 9;

void LogRecoveryTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("IOS.MainThreadFreezeDetection.RecoveredAfter", time);
}

void LogRecordHangGenerationTime(base::TimeTicks start_time) {
  UMA_HISTOGRAM_TIMES("IOS.MainThreadFreezeDetection.RecordGenerationTime",
                      base::TimeTicks::Now() - start_time);
}

// Key indicating that UI thread is frozen.
NSString* const kHangReportKey = @"hang-report";

// Key of the UMA Startup.MobileSessionStartAction histogram.
const char kUMAMainThreadFreezeDetectionNotRunningAfterReport[] =
    "IOS.MainThreadFreezeDetection.NotRunningAfterReport";

// Enum actions for the IOS.MainThreadFreezeDetection.NotRunningAfterReport UMA
// metric. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class IOSMainThreadFreezeDetectionNotRunningAfterReportBlock {
  kAfterBreakpadRef = 0,
  kAfterFileManagerUTEMove = 1,
  kAfterCrashpadDumpWithoutCrash = 2,
  kMaxValue = kAfterCrashpadDumpWithoutCrash,
};

// Only MetricKit reports currently use attachements.
bool IsMetricKitReport(crash_reporter::Report report) {
  return base::ComputeDirectorySize(crash_reporter::GetCrashpadDatabasePath()
                                        .Append("attachments")
                                        .Append(report.local_id)) > 0;
}

}  // namespace

@interface MainThreadFreezeDetector ()
// The callback that is called regularly on main thread.
- (void)runInMainLoop;
// The callback that is called regularly on watchdog thread.
- (void)runInFreezeDetectionQueue;
// These 4 properties will be accessed from both thread. Make them atomic.
// The date at which `runInMainLoop` was last called.
@property(atomic) NSDate* lastSeenMainThread;
// Whether the watchdog should continue running.
@property(atomic) BOOL running;
// Whether a report has been generated. When this is true, the
// FreezeDetectionQueue does not run.
@property(atomic) BOOL reportGenerated;
// The delay in seconds after which main thread will be considered frozen.
@property(atomic) NSInteger delay;
@end

@implementation MainThreadFreezeDetector {
  dispatch_queue_t _freezeDetectionQueue;
  BOOL _enabled;

  // The information on the UTE report that was created on last session.
  // Contains 3 fields:
  // "dump": the file name of the .dmp file in `_UTEDirectory`,
  // "config": the file name of the config file in `_UTEDirectory`,
  // "date": the date at which the UTE file was generated.
  // NOTE: Fields are unused with crashpad.
  NSDictionary* _lastSessionFreezeInfo;
  // The directory containing the UTE crash reports.
  NSString* _UTEDirectory;
  // The directory containing UTE crash reports eligible for crashpad
  // processing.
  NSString* _UTEPendingCrashpadDirectory;
}

+ (instancetype)sharedInstance {
  static MainThreadFreezeDetector* instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[MainThreadFreezeDetector alloc] init];
  });
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    _lastSessionFreezeInfo =
        [defaults dictionaryForKey:@(kNsUserDefaultKeyLastSessionInfo)];

    if (_lastSessionFreezeInfo != nil) {
      // This cannot use WasLastShutdownClean() from the metrics service because
      // MainThreadFreezeDetector starts before metrics. Instead, grab the value
      // directly.
      bool clean = [defaults objectForKey:kLastSessionExitedCleanly] != nil &&
                   [defaults boolForKey:kLastSessionExitedCleanly];
      // Last session exited cleanly, ignore _lastSessionFreezeInfo.
      UMA_HISTOGRAM_BOOLEAN("IOS.MainThreadFreezeDetection.HangWithCleanExit",
                            clean);
      if (clean)
        _lastSessionFreezeInfo = nil;
    }

    _lastSessionEndedFrozen = _lastSessionFreezeInfo != nil;
    [defaults removeObjectForKey:@(kNsUserDefaultKeyLastSessionInfo)];
    _delay = kFreezeDetectionDelay;
    _freezeDetectionQueue = dispatch_queue_create(
        "org.chromium.freeze_detection", DISPATCH_QUEUE_SERIAL);
    NSString* cacheDirectory = NSSearchPathForDirectoriesInDomains(
        NSCachesDirectory, NSUserDomainMask, YES)[0];

    _UTEDirectory = [cacheDirectory stringByAppendingPathComponent:@"UTE"];
    [[[NSFileManager alloc] init] createDirectoryAtPath:_UTEDirectory
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:nil];

    _UTEPendingCrashpadDirectory =
        [cacheDirectory stringByAppendingPathComponent:@"UTE_CrashpadPending"];
    [[[NSFileManager alloc] init]
              createDirectoryAtPath:_UTEPendingCrashpadDirectory
        withIntermediateDirectories:YES
                         attributes:nil
                              error:nil];

    // Like breakpad, the feature is created immediately in the enabled state as
    // the settings are not available yet when it is started.
    _enabled = YES;
  }
  return self;
}

- (void)setEnabled:(BOOL)enabled {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    dispatch_async(_freezeDetectionQueue, ^{
      [self handleLastSessionReport];
    });
    if (_lastSessionEndedFrozen) {
      LogRecoveryTime(base::Seconds(0));
    }
  });
  _enabled = enabled;
  if (_enabled) {
    [self start];
  } else {
    [self stop];
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:@(kNsUserDefaultKeyLastSessionInfo)];
  }
}

- (void)start {
  if (self.delay == 0 || self.running || !_enabled ||
      tests_hook::DisableMainThreadFreezeDetection() ||
      base::debug::BeingDebugged()) {
    return;
  }
  self.running = YES;
  [self runInMainLoop];
  dispatch_async(_freezeDetectionQueue, ^{
    [self runInFreezeDetectionQueue];
  });
}

- (void)stop {
  self.running = NO;
}

- (void)runInMainLoop {
  NSDate* oldLastSeenMainThread = self.lastSeenMainThread;
  self.lastSeenMainThread = [NSDate date];
  if (self.reportGenerated) {
    self.reportGenerated = NO;
    // Remove information about the last session info.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:@(kNsUserDefaultKeyLastSessionInfo)];
    LogRecoveryTime(base::Seconds(
        [[NSDate date] timeIntervalSinceDate:oldLastSeenMainThread]));
    // Restart the freeze detection.
    dispatch_async(_freezeDetectionQueue, ^{
      [self cleanAndRunInFreezeDetectionQueue];
    });
  }
  if (!self.running) {
    return;
  }
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [self runInMainLoop];
      });
}

- (void)cleanAndRunInFreezeDetectionQueue {
  NSFileManager* fileManager = [[NSFileManager alloc] init];
  [fileManager removeItemAtPath:_UTEDirectory error:nil];
  [fileManager createDirectoryAtPath:_UTEDirectory
         withIntermediateDirectories:NO
                          attributes:nil
                               error:nil];
  [self runInFreezeDetectionQueue];
}

- (void)runInFreezeDetectionQueue {
  if (!self.running) {
    return;
  }
  if ([[NSDate date] timeIntervalSinceDate:self.lastSeenMainThread] >
      self.delay) {
    if (crash_reporter::IsCrashpadRunning()) {
      const base::TimeTicks start = base::TimeTicks::Now();
      static crash_reporter::CrashKeyString<4> key("hang-report");
      crash_reporter::ScopedCrashKeyString auto_clear(&key, "yes");
      NSString* intermediate_dump = [_UTEDirectory
          stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
      base::FilePath path(base::SysNSStringToUTF8(intermediate_dump));
      crash_reporter::DumpWithoutCrashAndDeferProcessingAtPath(path);
      if (!self.running) {
        UMA_HISTOGRAM_ENUMERATION(
            kUMAMainThreadFreezeDetectionNotRunningAfterReport,
            IOSMainThreadFreezeDetectionNotRunningAfterReportBlock::
                kAfterCrashpadDumpWithoutCrash);
        return;
      }
      // Fields unused by Crashpad.  Change to bool if Breakpad is deprecated.
      [[NSUserDefaults standardUserDefaults]
          setObject:@{@"dump" : @"", @"config" : @"", @"date" : [NSDate date]}
             forKey:@(kNsUserDefaultKeyLastSessionInfo)];
      self.reportGenerated = YES;
      LogRecordHangGenerationTime(start);
      return;
    }

    [[BreakpadController sharedInstance]
        withBreakpadRef:^(BreakpadRef breakpadRef) {
          const base::TimeTicks start = base::TimeTicks::Now();
          [self recordHangWithBreakpadRef:breakpadRef];
          LogRecordHangGenerationTime(start);
        }];
    return;
  }

  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)),
      _freezeDetectionQueue, ^{
        [self runInFreezeDetectionQueue];
      });
}

- (void)recordHangWithBreakpadRef:(BreakpadRef)breakpadRef {
  if (!self.running) {
    UMA_HISTOGRAM_ENUMERATION(
        kUMAMainThreadFreezeDetectionNotRunningAfterReport,
        IOSMainThreadFreezeDetectionNotRunningAfterReportBlock::
            kAfterBreakpadRef);
    return;
  }
  if (!breakpadRef) {
    return;
  }
  BreakpadAddUploadParameter(breakpadRef, kHangReportKey, @"yes");
  NSDictionary* breakpadReportInfo = BreakpadGenerateReport(breakpadRef, nil);
  BreakpadRemoveUploadParameter(breakpadRef, kHangReportKey);
  if (!breakpadReportInfo) {
    return;
  }
  // The report is always generated in the BreakpadDirectory.
  // As only one report can be uploaded per session, this report is
  // moved out of the Breakpad directory and put in a `UTE` directory.
  NSString* configFile =
      [breakpadReportInfo objectForKey:@BREAKPAD_OUTPUT_CONFIG_FILE];
  NSString* UTEConfigFile = [_UTEDirectory
      stringByAppendingPathComponent:[configFile lastPathComponent]];
  NSString* dumpFile =
      [breakpadReportInfo objectForKey:@BREAKPAD_OUTPUT_DUMP_FILE];
  NSString* UTEDumpFile = [_UTEDirectory
      stringByAppendingPathComponent:[dumpFile lastPathComponent]];
  NSFileManager* fileManager = [[NSFileManager alloc] init];
  [fileManager moveItemAtPath:configFile toPath:UTEConfigFile error:nil];
  [fileManager moveItemAtPath:dumpFile toPath:UTEDumpFile error:nil];
  if (!self.running) {
    UMA_HISTOGRAM_ENUMERATION(
        kUMAMainThreadFreezeDetectionNotRunningAfterReport,
        IOSMainThreadFreezeDetectionNotRunningAfterReportBlock::
            kAfterFileManagerUTEMove);
    return;
  }
  [[NSUserDefaults standardUserDefaults]
      setObject:@{
        @"dump" : [dumpFile lastPathComponent],
        @"config" : [configFile lastPathComponent],
        @"date" : [NSDate date]
      }
         forKey:@(kNsUserDefaultKeyLastSessionInfo)];
  self.reportGenerated = YES;
}

- (void)processIntermediateDumps {
  NSFileManager* fileManager = [[NSFileManager alloc] init];
  NSArray<NSString*>* UTEPendingDirectoryContents =
      [fileManager contentsOfDirectoryAtPath:_UTEPendingCrashpadDirectory
                                       error:NULL];
  if (!UTEPendingDirectoryContents.count)
    return;

  // Get the most recent crash capture_time. -GetReports is already sorted
  // by newest first so just grab the first non-MetricKit report.
  time_t newest_crash = 0;
  std::vector<crash_reporter::Report> reports;
  crash_reporter::GetReports(&reports);
  for (size_t i = 0; i < reports.size(); i++) {
    if (!IsMetricKitReport(reports[i])) {
      newest_crash = reports[i].capture_time;
      break;
    }
  }

  // Process any hang reports that have a modification time newer than the
  // newest crash.
  for (NSString* pendingFile : UTEPendingDirectoryContents) {
    NSString* hang_report = [_UTEPendingCrashpadDirectory
        stringByAppendingPathComponent:pendingFile];
    NSDate* UTECrashDate = [[fileManager attributesOfItemAtPath:hang_report
                                                          error:nil]
        objectForKey:NSFileModificationDate];
    time_t crash_time =
        static_cast<time_t>([UTECrashDate timeIntervalSince1970]);
    if (crash_time > newest_crash) {
      base::FilePath path(base::SysNSStringToUTF8(hang_report));
      crash_reporter::ProcessIntermediateDump(path);
    }
  }
  // Delete the directory when done to clear any un-processed reports.
  [fileManager removeItemAtPath:_UTEPendingCrashpadDirectory error:nil];
}

- (void)restoreLastSessionReportIfNeeded {
  if (!_lastSessionFreezeInfo) {
    return;
  }

  NSFileManager* fileManager = [[NSFileManager alloc] init];
  if (crash_reporter::IsCrashpadRunning()) {
    NSArray<NSString*>* UTEDirectoryContents =
        [fileManager contentsOfDirectoryAtPath:_UTEDirectory error:NULL];
    if (UTEDirectoryContents.count != 1)
      return;

    // Backup hang_report to a new location. See -processIntermediateDumps for
    // why this is necessary.
    NSString* hang_report =
        [_UTEDirectory stringByAppendingPathComponent:UTEDirectoryContents[0]];
    NSString* save_hang_report = [_UTEPendingCrashpadDirectory
        stringByAppendingPathComponent:UTEDirectoryContents[0]];
    [fileManager moveItemAtPath:hang_report toPath:save_hang_report error:nil];
    return;
  }

  // Tests that the dump file still exist.
  NSString* dumpFile = [_lastSessionFreezeInfo objectForKey:@"dump"];
  if (![dumpFile length])
    return;
  NSString* UTEDumpFile =
      [_UTEDirectory stringByAppendingPathComponent:dumpFile];
  NSString* cacheDirectory = NSSearchPathForDirectoriesInDomains(
      NSCachesDirectory, NSUserDomainMask, YES)[0];
  NSString* breakpadDirectory =
      [cacheDirectory stringByAppendingPathComponent:@"Breakpad"];
  NSString* breakpadDumpFile =
      [breakpadDirectory stringByAppendingPathComponent:dumpFile];
  if (!UTEDumpFile || ![fileManager fileExistsAtPath:UTEDumpFile]) {
    return;
  }

  // Tests that the config file still exist.
  NSString* configFile = [_lastSessionFreezeInfo objectForKey:@"config"];
  NSString* UTEConfigFile =
      [_UTEDirectory stringByAppendingPathComponent:configFile];
  NSString* breakpadConfigFile =
      [breakpadDirectory stringByAppendingPathComponent:configFile];
  if (!UTEConfigFile || ![fileManager fileExistsAtPath:UTEConfigFile]) {
    return;
  }

  // Tests that the previous session did not end in a crash with a report.
  NSDirectoryEnumerator* directoryEnumerator =
      [fileManager enumeratorAtURL:[NSURL fileURLWithPath:breakpadDirectory]
          includingPropertiesForKeys:@[ NSURLCreationDateKey ]
                             options:NSDirectoryEnumerationSkipsHiddenFiles
                        errorHandler:nil];
  NSDate* UTECrashDate = [_lastSessionFreezeInfo objectForKey:@"date"];
  if (!UTECrashDate) {
    return;
  }
  for (NSURL* fileURL in directoryEnumerator) {
    if ([[fileURL pathExtension] isEqualToString:@"log"]) {
      // Ignore upload.log
      continue;
    }
    NSDate* crashDate;
    [fileURL getResourceValue:&crashDate forKey:NSURLCreationDateKey error:nil];
    if ([UTECrashDate compare:crashDate] == NSOrderedAscending) {
      return;
    }
  }

  // Restore files.
  [fileManager moveItemAtPath:UTEDumpFile toPath:breakpadDumpFile error:nil];
  [fileManager moveItemAtPath:UTEConfigFile
                       toPath:breakpadConfigFile
                        error:nil];
}

- (void)handleLastSessionReport {
  [self restoreLastSessionReportIfNeeded];
  NSFileManager* fileManager = [[NSFileManager alloc] init];
  // It is possible that this call will delete a report on the current session.
  // But this is unlikely because `handleLastSessionReport` run on the
  // `_freezeDetectionQueue` and is called directly from setEnabled which means
  // that main thread was responding recently.
  [fileManager removeItemAtPath:_UTEDirectory error:nil];
  [fileManager createDirectoryAtPath:_UTEDirectory
         withIntermediateDirectories:NO
                          attributes:nil
                               error:nil];
}

@end
