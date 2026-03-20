// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/memory_monitor.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import <mach/mach.h>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"

namespace {

BASE_FEATURE(kRestoreLongMemoryMonitorDelay, base::FEATURE_DISABLED_BY_DEFAULT);

// Returns the delay between each invocation of `UpdateMemoryValues`.
base::TimeDelta GetMemoryMonitorDelay() {
  if (base::FeatureList::IsEnabled(kRestoreLongMemoryMonitorDelay)) {
    return base::Seconds(30);
  }
  return base::Seconds(5);
}

// Checks the values of free RAM and free disk space and updates crash keys with
// these values. Also updates available free disk space for PreviousSessionInfo.
void UpdateMemoryValues() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  crash_keys::SetCurrentFreeMemoryInKB(
      base::SysInfo::AmountOfAvailablePhysicalMemory().InKiB());

  task_vm_info_data_t task_vm_info;
  mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
  kern_return_t result =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&task_vm_info), &count);
  if (result == KERN_SUCCESS && count >= TASK_VM_INFO_REV1_COUNT) {
    crash_keys::SetCurrentMemoryLimitBytesRemainingInKB(
        task_vm_info.limit_bytes_remaining / 1024);
  }
}

// Invokes `UpdateMemoryValues` and schedules itself to be called after
// the delay.
void AsynchronousFreeMemoryMonitor() {
  UpdateMemoryValues();
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&AsynchronousFreeMemoryMonitor), GetMemoryMonitorDelay());
}
}  // namespace

void StartFreeMemoryMonitor() {
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             base::BindOnce(&AsynchronousFreeMemoryMonitor));
}
