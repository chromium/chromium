// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/memory_monitor.h"

#import <Foundation/NSPathUtilities.h>
#import <dispatch/dispatch.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/crash_keys_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IOSStorageCapacity {
  kUnknown = 0,
  k0GB = 1,
  k16GB = 2,
  k32GB = 3,
  k64GB = 4,
  k128GB = 5,
  k256GB = 6,
  k512GB = 7,
  k1TB = 8,
  k2TB = 9,
  kMaxValue = k2TB,
};

// Returns the appropriate storage capacity enum value given the `capacity` in
// GB. An exact match is not required and the halfway points between steps was
// chosen as the separators to account for errors in the calculation.
IOSStorageCapacity StorageCapacityEnumFromGB(int capacity) {
  if (capacity == 0) {
    return IOSStorageCapacity::k0GB;
  }
  if (capacity < 8) {
    return IOSStorageCapacity::kUnknown;
  }
  if (capacity < 24) {
    return IOSStorageCapacity::k16GB;
  }
  if (capacity < 48) {
    return IOSStorageCapacity::k32GB;
  }
  if (capacity < 96) {
    return IOSStorageCapacity::k64GB;
  }
  if (capacity < 192) {
    return IOSStorageCapacity::k128GB;
  }
  if (capacity < 384) {
    return IOSStorageCapacity::k256GB;
  }
  if (capacity < 768) {
    return IOSStorageCapacity::k512GB;
  }
  if (capacity < 1500) {
    return IOSStorageCapacity::k1TB;
  }
  if (capacity < 2500) {
    return IOSStorageCapacity::k2TB;
  }
  return IOSStorageCapacity::kUnknown;
}

// Delay between each invocations of `UpdateMemoryValues`.
constexpr base::TimeDelta kMemoryMonitorDelay = base::Seconds(30);

// Checks the values of free RAM and free disk space and updates crash keys with
// these values. Also updates available free disk space for PreviousSessionInfo.
void UpdateMemoryValues() {
  static bool first_update_since_launch = true;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  const int free_memory =
      static_cast<int>(base::SysInfo::AmountOfAvailablePhysicalMemory() / 1024);

  NSURL* fileURL = [[NSURL alloc] initFileURLWithPath:NSHomeDirectory()];
  NSArray* keys = @[ NSURLVolumeAvailableCapacityForImportantUsageKey ];
  if (first_update_since_launch) {
    keys = @[
      NSURLVolumeAvailableCapacityForImportantUsageKey,
      NSURLVolumeAvailableCapacityForOpportunisticUsageKey,
      NSURLVolumeAvailableCapacityKey,
      NSURLVolumeTotalCapacityKey,
    ];
  }
  NSDictionary* results = [fileURL resourceValuesForKeys:keys error:nil];
  int free_disk_space_kilobytes = -1;
  if (results) {
    NSNumber* available_bytes =
        results[NSURLVolumeAvailableCapacityForImportantUsageKey];
    free_disk_space_kilobytes = [available_bytes integerValue] / 1024;

    if (first_update_since_launch) {
      NSNumber* total_capacity_bytes = results[NSURLVolumeTotalCapacityKey];
      int total_capacity_gigabytes =
          [total_capacity_bytes integerValue] / 1000 / 1000 / 1000;
      base::UmaHistogramEnumeration(
          "IOS.SandboxMetrics.TotalCapacity",
          StorageCapacityEnumFromGB(total_capacity_gigabytes));
      if (total_capacity_gigabytes == 0) {
        return;
      }

      UMA_HISTOGRAM_MEMORY_LARGE_MB(
          "IOS.SandboxMetrics.CapacityForImportantUsage",
          free_disk_space_kilobytes / 1024);
      float important_usage_capacity_percentage =
          [available_bytes floatValue] / [total_capacity_bytes floatValue];
      UMA_HISTOGRAM_PERCENTAGE(
          "IOS.SandboxMetrics.CapacityForImportantUsagePercentage",
          static_cast<int>(100 * important_usage_capacity_percentage));

      NSNumber* opportunistic_bytes =
          results[NSURLVolumeAvailableCapacityForOpportunisticUsageKey];
      int free_disk_space_opportunistic_megabytes =
          [opportunistic_bytes integerValue] / 1024 / 1024;
      UMA_HISTOGRAM_MEMORY_LARGE_MB(
          "IOS.SandboxMetrics.CapacityForOpportunisticUsage",
          free_disk_space_opportunistic_megabytes);
      float opportunistic_usage_capacity_percentage =
          [opportunistic_bytes floatValue] / [total_capacity_bytes floatValue];
      UMA_HISTOGRAM_PERCENTAGE(
          "IOS.SandboxMetrics.CapacityForOpportunisticUsagePercentage",
          static_cast<int>(100 * opportunistic_usage_capacity_percentage));

      NSNumber* available_capacity_bytes =
          results[NSURLVolumeAvailableCapacityKey];
      int available_capacity_megabytes =
          [available_capacity_bytes integerValue] / 1024 / 1024;
      UMA_HISTOGRAM_MEMORY_LARGE_MB("IOS.SandboxMetrics.AvailableCapacity",
                                    available_capacity_megabytes);
      float capacity_percentage = [available_capacity_bytes floatValue] /
                                  [total_capacity_bytes floatValue];
      UMA_HISTOGRAM_PERCENTAGE("IOS.SandboxMetrics.AvailableCapacityPercentage",
                               static_cast<int>(100 * capacity_percentage));

      first_update_since_launch = false;
    }
  }

  // As a workaround to crbug.com/1247282, dispatch back to the main thread.
  dispatch_async(dispatch_get_main_queue(), ^{
    crash_keys::SetCurrentFreeMemoryInKB(free_memory);
    crash_keys::SetCurrentFreeDiskInKB(free_disk_space_kilobytes);
    [[PreviousSessionInfo sharedInstance]
        updateAvailableDeviceStorage:(NSInteger)free_disk_space_kilobytes];
  });
}

// Invokes `UpdateMemoryValues` and schedules itself to be called after
// `kMemoryMonitorDelay`.
void AsynchronousFreeMemoryMonitor() {
  UpdateMemoryValues();
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&AsynchronousFreeMemoryMonitor), kMemoryMonitorDelay);
}
}  // namespace

void StartFreeMemoryMonitor() {
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             base::BindOnce(&AsynchronousFreeMemoryMonitor));
}
