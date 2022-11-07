// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_USER_INPUT_MONITOR_H_
#define MEDIA_BASE_USER_INPUT_MONITOR_H_

#include <stddef.h>

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// Utility functions for correctly and atomically reading from/writing to a
// shared memory mapping containing key press count.
uint32_t MEDIA_EXPORT
ReadKeyPressMonitorCount(const base::ReadOnlySharedMemoryMapping& shmem);
void MEDIA_EXPORT
WriteKeyPressMonitorCount(const base::WritableSharedMemoryMapping& shmem,
                          uint32_t count);

// Base class for audio:: and media:: UserInputMonitor implementations.
class MEDIA_EXPORT UserInputMonitor {
 public:
  UserInputMonitor();

  UserInputMonitor(const UserInputMonitor&) = delete;
  UserInputMonitor& operator=(const UserInputMonitor&) = delete;

  virtual ~UserInputMonitor();

  // Creates a platform-specific instance of UserInputMonitorBase.
  // |io_task_runner| is the task runner for an IO thread.
  // |ui_task_runner| is the task runner for a UI thread.
  static std::unique_ptr<UserInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  virtual void EnableKeyPressMonitoring() = 0;
  virtual void DisableKeyPressMonitoring() = 0;

  // Returns the number of keypresses. The starting point from when it is
  // counted is not guaranteed, but consistent within the pair of calls of
  // EnableKeyPressMonitoring and DisableKeyPressMonitoring. So a caller can
  // use the difference between the values returned at two times to get the
  // number of keypresses happened within that time period, but should not make
  // any assumption on the initial value.
  virtual uint32_t GetKeyPressCount() const = 0;
};

// Monitors and notifies about keyboard events.
class MEDIA_EXPORT UserInputMonitorBase : public UserInputMonitor {
 public:
  UserInputMonitorBase();

  UserInputMonitorBase(const UserInputMonitorBase&) = delete;
  UserInputMonitorBase& operator=(const UserInputMonitorBase&) = delete;

  ~UserInputMonitorBase() override;

  // A caller must call EnableKeyPressMonitoring(WithMapping) and
  // DisableKeyPressMonitoring in pair on the same sequence.
  void EnableKeyPressMonitoring() override;
  void DisableKeyPressMonitoring() override;

  // Initializes a MappedReadOnlyRegion storing key press count. Returns a
  // readonly region to the mapping and passes the writable mapping to platform
  // specific implementation, to update key press count. If monitoring is
  // already enabled, it only returns a handle to readonly region.
  base::ReadOnlySharedMemoryRegion EnableKeyPressMonitoringWithMapping();

 private:
  virtual void StartKeyboardMonitoring() = 0;
  virtual void StartKeyboardMonitoring(
      base::WritableSharedMemoryMapping mapping) = 0;
  virtual void StopKeyboardMonitoring() = 0;

  size_t references_ = 0;
  base::ReadOnlySharedMemoryRegion key_press_count_region_;

  SEQUENCE_CHECKER(owning_sequence_);
};

}  // namespace media

#endif  // MEDIA_BASE_USER_INPUT_MONITOR_H_
