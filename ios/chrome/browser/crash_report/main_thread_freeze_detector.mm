// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/crash_report/crash_report_flags.h"
#import "third_party/breakpad/breakpad/src/client/ios/Breakpad.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif
namespace {
// The info contains a dictionary with info about the freeze report.
// See description at |_lastSessionFreezeInfo|.
const char kNsUserDefaultKeyLastSessionInfo[] =
    "MainThreadDetectionLastThreadWasFrozenInfo";
// The delay after which a UTE report is generated. It is a cache of the
// Variations value to use when variations is not available yet
const char kNsUserDefaultKeyDelay[] = "MainThreadDetectionDelay";

void LogRecoveryTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("IOS.MainThreadFreezeDetection.RecoveredAfter", time);
}

}

@interface MainThreadFreezeDetector ()
// The callback that is called regularly on main thread.
- (void)runInMainLoop;
// The callback that is called regularly on watchdog thread.
- (void)runInFreezeDetectionQueue;
// These 4 properties will be accessed from both thread. Make them atomic.
// The date at which |runInMainLoop| was last called.
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
  // "dump": the file name of the .dmp file in |_UTEDirectory|,
  // "config": the file name of the config file in |_UTEDirectory|,
  // "date": the date at which the UTE file was generated.
  NSDictionary* _lastSessionFreezeInfo;
  // The directory containing the UTE crash reports.
  NSString* _UTEDirectory;
  // The block to call (on main thread) once the UTE report is restored in the
  // breakpad directory.
  ProceduralBlock _restorationCompletion;
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
    _lastSessionFreezeInfo = [[NSUserDefaults standardUserDefaults]
        dictionaryForKey:@(kNsUserDefaultKeyLastSessionInfo)];
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:@(kNsUserDefaultKeyLastSessionInfo)];
    _delay = [[NSUserDefaults standardUserDefaults]
        integerForKey:@(kNsUserDefaultKeyDelay)];
    _freezeDetectionQueue = dispatch_queue_create(
        "org.chromium.freeze_detection", DISPATCH_QUEUE_SERIAL);
    NSString* cacheDirectory = NSSearchPathForDirectoriesInDomains(
        NSCachesDirectory, NSUserDomainMask, YES)[0];

    _UTEDirectory = [cacheDirectory stringByAppendingPathComponent:@"UTE"];
    // Like breakpad, the feature is created immediately in the enabled state as
    // the settings are not available yet when it is started.
    _enabled = YES;
  }
  return self;
}

- (void)setEnabled:(BOOL)enabled {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    // The first time |setEnabled| is called is the first occasion to update
    // the config based on new finch experiment and settings.
    int newDelay = crash_report::TimeoutForMainThreadFreezeDetection();
    self.delay = newDelay;
    [[NSUserDefaults standardUserDefaults]
        setInteger:newDelay
            forKey:@(kNsUserDefaultKeyDelay)];
    if (_lastSessionEndedFrozen) {
      LogRecoveryTime(base::TimeDelta::FromSeconds(0));
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
  if (self.delay == 0 || self.running || !_enabled) {
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
  if (self.reportGenerated) {
    self.reportGenerated = NO;
    // Remove information about the last session info.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:@(kNsUserDefaultKeyLastSessionInfo)];
    LogRecoveryTime(base::TimeDelta::FromSecondsD(
        [[NSDate date] timeIntervalSinceDate:self.lastSeenMainThread]));
    // Restart the freeze detection.
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)),
        _freezeDetectionQueue, ^{
          [self cleanAndRunInFreezeDetectionQueue];
        });
  }
  if (!self.running) {
    return;
  }
  self.lastSeenMainThread = [NSDate date];
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [self runInMainLoop];
      });
}

- (void)cleanAndRunInFreezeDetectionQueue {
  if (_canUploadBreakpadCrashReports) {
    // If the prevous session is not processed yet, do not delete the directory.
    // It will be cleared on completion of processing the previous session.
    NSFileManager* fileManager = [[NSFileManager alloc] init];
    [fileManager removeItemAtPath:_UTEDirectory error:nil];
  }
  [self runInFreezeDetectionQueue];
}

- (void)runInFreezeDetectionQueue {
  if (!self.running) {
    return;
  }
  if ([[NSDate date] timeIntervalSinceDate:self.lastSeenMainThread] >
      self.delay) {
    [[BreakpadController sharedInstance]
        withBreakpadRef:^(BreakpadRef breakpadRef) {
          if (!breakpadRef) {
            return;
          }
          breakpad_helper::SetHangReport(true);
          NSDictionary* breakpadReportInfo =
              BreakpadGenerateReport(breakpadRef, nil);
          if (!breakpadReportInfo) {
            return;
          }
          // The report is always generated in the BreakpadDirectory.
          // As only one report can be uploaded per session, this report is
          // moved out of the Breakpad directory and put in a |UTE| directory.
          NSString* configFile =
              [breakpadReportInfo objectForKey:@BREAKPAD_OUTPUT_CONFIG_FILE];
          NSString* UTEConfigFile = [_UTEDirectory
              stringByAppendingPathComponent:[configFile lastPathComponent]];
          NSString* dumpFile =
              [breakpadReportInfo objectForKey:@BREAKPAD_OUTPUT_DUMP_FILE];
          NSString* UTEDumpFile = [_UTEDirectory
              stringByAppendingPathComponent:[dumpFile lastPathComponent]];
          NSFileManager* fileManager = [[NSFileManager alloc] init];

          // Clear previous reports if they exist.
          [fileManager createDirectoryAtPath:_UTEDirectory
                 withIntermediateDirectories:NO
                                  attributes:nil
                                       error:nil];
          [fileManager moveItemAtPath:configFile
                               toPath:UTEConfigFile
                                error:nil];
          [fileManager moveItemAtPath:dumpFile toPath:UTEDumpFile error:nil];
          [[NSUserDefaults standardUserDefaults]
              setObject:@{
                @"dump" : [dumpFile lastPathComponent],
                @"config" : [configFile lastPathComponent],
                @"date" : [NSDate date]
              }
                 forKey:@(kNsUserDefaultKeyLastSessionInfo)];
          self.reportGenerated = YES;
        }];
    return;
  }
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)),
                 _freezeDetectionQueue, ^{
                   [self runInFreezeDetectionQueue];
                 });
}

- (void)prepareCrashReportsForUpload:(ProceduralBlock)completion {
  DCHECK(completion);
  _restorationCompletion = completion;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    dispatch_async(_freezeDetectionQueue, ^{
      [self handleLastSessionReport];
    });
  });
}

- (void)restoreLastSessionReportIfNeeded {
  NSString* cacheDirectory = NSSearchPathForDirectoriesInDomains(
      NSCachesDirectory, NSUserDomainMask, YES)[0];
  NSString* breakpadDirectory =
      [cacheDirectory stringByAppendingPathComponent:@"Breakpad"];
  if (!_lastSessionFreezeInfo) {
    return;
  }

  // Tests that the dump file still exist.
  NSFileManager* fileManager = [[NSFileManager alloc] init];
  NSString* dumpFile = [_lastSessionFreezeInfo objectForKey:@"dump"];
  NSString* UTEDumpFile =
      [_UTEDirectory stringByAppendingPathComponent:dumpFile];
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
  // But this is unlikely because |handleLastSessionReport| run on the
  // |_freezeDetectionQueue| and is called directly from the main thread
  // |prepareToUpload| which mean that main thread was responding recently.
  [fileManager removeItemAtPath:_UTEDirectory error:nil];
  dispatch_async(dispatch_get_main_queue(), ^{
    _canUploadBreakpadCrashReports = YES;
    DCHECK(_restorationCompletion);
    _restorationCompletion();
  });
}

@end
