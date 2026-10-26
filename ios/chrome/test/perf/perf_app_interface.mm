// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/perf/perf_app_interface.h"

#import <mach/mach.h>

#import <optional>

#import "base/process/process_metrics.h"
#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation PerfAppInterface

+ (NSDictionary<NSString*, NSNumber*>*)currentStartupTimingsInMilliseconds {
  NSMutableDictionary<NSString*, NSNumber*>* timings =
      [[NSMutableDictionary alloc] init];
  id<StartupInformation> startupInfo = chrome_test_util::GetMainController();
  if (!startupInfo) {
    return timings;
  }

  const std::optional<base::TimeDelta> preMain = [startupInfo preMainDuration];
  if (preMain.has_value()) {
    timings[@"Startup.ColdStartPreMain"] = @(preMain->InMillisecondsF());
  }

  const base::TimeDelta fromMain = [startupInfo coldStartFromMainDuration];
  if (fromMain.is_positive()) {
    timings[@"Startup.ColdStartFromMain"] = @(fromMain.InMillisecondsF());
  }

  base::LiveTicks now;
  const std::optional<base::TimeDelta> mainToNow =
      [startupInfo appLaunchTimer].Elapsed(&now);
  const base::LiveTicks sceneConnect = [startupInfo firstSceneConnectionTime];
  if (mainToNow.has_value() && !sceneConnect.is_null()) {
    const base::TimeDelta mainToSceneConnect =
        *mainToNow - (now - sceneConnect);
    if (mainToSceneConnect.is_positive()) {
      timings[@"Startup.TimeFromMainToSceneConnection"] =
          @(mainToSceneConnect.InMillisecondsF());
    }
  }

  return timings;
}

+ (double)startupMemoryFootprintBytes {
  id<StartupInformation> startupInfo = chrome_test_util::GetMainController();
  if (!startupInfo) {
    return -1.0;
  }
  const uint64_t footprintBytes = [startupInfo startupMemoryFootprintBytes];
  if (footprintBytes == 0) {
    return -1.0;
  }
  return static_cast<double>(footprintBytes);
}

+ (double)currentProcessPhysFootprintBytes {
  const auto memoryInfo =
      base::ProcessMetrics::CreateCurrentProcessMetrics()->GetMemoryInfo();
  if (!memoryInfo.has_value()) {
    return 0.0;
  }
  return static_cast<double>(memoryInfo->physical_footprint_bytes);
}

+ (double)currentProcessPageIns {
  task_events_info_data_t eventsInfo;
  mach_msg_type_number_t count = TASK_EVENTS_INFO_COUNT;
  kern_return_t kr =
      task_info(mach_task_self(), TASK_EVENTS_INFO,
                reinterpret_cast<task_info_t>(&eventsInfo), &count);
  if (kr != KERN_SUCCESS) {
    return -1.0;
  }
  return static_cast<double>(eventsInfo.pageins);
}

+ (NSError*)recordPerfSamples:(NSArray<NSNumber*>*)samples
                    forMetric:(NSString*)metricName
                         unit:(NSString*)unit
                    benchmark:(NSString*)benchmark {
  if (metricName.length == 0 || unit.length == 0 || samples.count == 0) {
    return [NSError
        errorWithDomain:@"PerfAppInterface"
                   code:1
               userInfo:@{
                 NSLocalizedDescriptionKey :
                     @"metricName, unit, and samples must all be non-empty."
               }];
  }

  NSArray<NSString*>* docDirs = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES);
  if (docDirs.count == 0) {
    return [NSError
        errorWithDomain:@"PerfAppInterface"
                   code:2
               userInfo:@{
                 NSLocalizedDescriptionKey : @"NSDocumentDirectory unavailable."
               }];
  }

  NSString* samplesPath =
      [docDirs.firstObject stringByAppendingPathComponent:@"perf_samples.json"];
  NSMutableArray<NSMutableDictionary*>* entries = [[NSMutableArray alloc] init];

  if ([[NSFileManager defaultManager] fileExistsAtPath:samplesPath]) {
    NSData* existingData = [NSData dataWithContentsOfFile:samplesPath];
    if (existingData.length > 0) {
      id parsed =
          [NSJSONSerialization JSONObjectWithData:existingData
                                          options:NSJSONReadingMutableContainers
                                            error:nil];
      if ([parsed isKindOfClass:[NSArray class]]) {
        for (id item in parsed) {
          if ([item isKindOfClass:[NSDictionary class]]) {
            [entries addObject:[item mutableCopy]];
          }
        }
      }
    }
  }

  NSString* normalizedBenchmark = benchmark.length > 0 ? benchmark : @"";
  NSMutableDictionary* targetEntry = nil;
  for (NSMutableDictionary* entry in entries) {
    NSString* entryBenchmark = entry[@"benchmark"] ?: @"";
    if ([entry[@"name"] isEqualToString:metricName] &&
        [entry[@"unit"] isEqualToString:unit] &&
        [entryBenchmark isEqualToString:normalizedBenchmark]) {
      targetEntry = entry;
      break;
    }
  }

  if (!targetEntry) {
    NSMutableDictionary* newEntry = [@{
      @"name" : metricName,
      @"unit" : unit,
      @"samples" : [[NSMutableArray alloc] init],
    } mutableCopy];
    if (normalizedBenchmark.length > 0) {
      newEntry[@"benchmark"] = normalizedBenchmark;
    }
    targetEntry = newEntry;
    [entries addObject:targetEntry];
  }

  NSMutableArray* mergedSamples = [targetEntry[@"samples"] mutableCopy];
  [mergedSamples addObjectsFromArray:samples];
  targetEntry[@"samples"] = mergedSamples;

  NSError* error = nil;
  NSData* outputData =
      [NSJSONSerialization dataWithJSONObject:entries
                                      options:NSJSONWritingPrettyPrinted
                                        error:&error];
  if (!outputData) {
    return error
               ?: [NSError
                      errorWithDomain:@"PerfAppInterface"
                                 code:3
                             userInfo:@{
                               NSLocalizedDescriptionKey :
                                   @"Failed to serialize perf_samples.json."
                             }];
  }

  [outputData writeToFile:samplesPath options:NSDataWritingAtomic error:&error];
  return error;
}

@end
