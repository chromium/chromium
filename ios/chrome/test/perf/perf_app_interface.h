// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PERF_PERF_APP_INTERFACE_H_
#define IOS_CHROME_TEST_PERF_PERF_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App-side helper interface for EG2 performance benchmarks (`PerfTestCase`).
// Lives under `//ios/chrome/test/perf` so it can access `StartupInformation`
// on `MainController` without violating `//ios/chrome/browser` layering rules.
@interface PerfAppInterface : NSObject

// Returns unbucketed cold startup timings in milliseconds read from
// `MainController`'s `StartupInformation`:
// - `Startup.ColdStartPreMain`: process creation to `main()`
// - `Startup.ColdStartFromMain`: `main()` to initial UI readiness
//   (`logStartupDuration:`)
// - `Startup.TimeFromMainToSceneConnection`: `main()` to first scene connection
+ (NSDictionary<NSString*, NSNumber*>*)currentStartupTimingsInMilliseconds;

// Returns the browser process Mach `TASK_VM_INFO` `phys_footprint` in bytes
// captured by the `TakeStartupMemorySnapshot` deferred initialization block, or
// `-1.0` if the snapshot has not been captured yet.
+ (double)startupMemoryFootprintBytes;

// Returns the browser process Mach `TASK_VM_INFO` `phys_footprint` in bytes,
// or `0.0` on error.
+ (double)currentProcessPhysFootprintBytes;

// Returns cumulative kernel `TASK_EVENTS_INFO` `pageins` for the current
// browser process since launch, or `-1.0` on error.
+ (double)currentProcessPageIns;

// Appends raw numeric `samples` under `(metricName, unit, benchmark)` into
// `<NSDocumentDirectory>/perf_samples.json` for host collection and Catapult
// `HistogramSet` conversion. Returns `nil` on success, or an `NSError*` if any
// argument is empty or serialization fails.
+ (NSError*)recordPerfSamples:(NSArray<NSNumber*>*)samples
                    forMetric:(NSString*)metricName
                         unit:(NSString*)unit
                    benchmark:(NSString*)benchmark [[nodiscard]];

// Opens `count` regular tabs in the background via `SceneCommands` without
// waiting for navigation, used to populate the Tab Grid for scroll benchmarks.
+ (void)openBackgroundNewTabs:(int)count;

// Schedules a repeating `NSTimer` on the main runloop in `NSRunLoopCommonModes`
// that sleeps the main thread for `durationMs` every `intervalMs`.
+ (void)startMainThreadStallWithIntervalMs:(int)intervalMs
                                durationMs:(int)durationMs;

// Schedules a `CADisplayLink` on the main runloop in `NSRunLoopCommonModes`
// that sleeps the main thread for `durationMs` every `stride` frames.
+ (void)startCADisplayLinkStallWithStride:(int)stride
                               durationMs:(int)durationMs;

// Invalidates and clears any active main-thread stall timer or display link.
+ (void)stopMainThreadStall;

@end

#endif  // IOS_CHROME_TEST_PERF_PERF_APP_INTERFACE_H_
