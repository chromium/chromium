// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/memory_monitor.h"

#import <Foundation/NSPathUtilities.h>
#import <dispatch/dispatch.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
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

// Delay between each invocations of `UpdateMemoryValues`.
constexpr base::TimeDelta kMemoryMonitorDelay = base::Seconds(30);

// Checks the values of free RAM and free disk space and updates crash keys with
// these values. Also updates available free disk space for PreviousSessionInfo.
void UpdateMemoryValues() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  const int free_memory =
      static_cast<int>(base::SysInfo::AmountOfAvailablePhysicalMemory() / 1024);
    crash_keys::SetCurrentFreeMemoryInKB(free_memory);
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
