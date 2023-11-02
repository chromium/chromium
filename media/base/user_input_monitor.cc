// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <utility>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace media {

uint32_t ReadKeyPressMonitorCount(
    const base::ReadOnlySharedMemoryMapping& readonly_mapping) {
  if (!readonly_mapping.IsValid())
    return 0;

  // No ordering constraints between Load/Store operations, a temporary
  // inconsistent value is fine.
  return base::subtle::NoBarrier_Load(
      reinterpret_cast<const base::subtle::Atomic32*>(
          readonly_mapping.memory()));
}

void WriteKeyPressMonitorCount(
    const base::WritableSharedMemoryMapping& writable_mapping,
    uint32_t count) {
  if (!writable_mapping.IsValid())
    return;

  // No ordering constraints between Load/Store operations, a temporary
  // inconsistent value is fine.
  base::subtle::NoBarrier_Store(
      static_cast<base::subtle::Atomic32*>(writable_mapping.memory()), count);
}

#ifdef DISABLE_USER_INPUT_MONITOR
// static
std::unique_ptr<UserInputMonitor> UserInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return nullptr;
}
#endif  // DISABLE_USER_INPUT_MONITOR
UserInputMonitor::UserInputMonitor() = default;

UserInputMonitor::~UserInputMonitor() = default;

UserInputMonitorBase::UserInputMonitorBase() {
  DETACH_FROM_SEQUENCE(owning_sequence_);
}

UserInputMonitorBase::~UserInputMonitorBase() {
  // |references_| may be non-zero as it's decremented due to Mojo messages from
  // the renderer, and they may not reach the browser always in tests.
}

void UserInputMonitorBase::EnableKeyPressMonitoring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (++references_ == 1) {
    StartKeyboardMonitoring();
    DVLOG(2) << "Started keyboard monitoring.";
  }
}

base::ReadOnlySharedMemoryRegion
UserInputMonitorBase::EnableKeyPressMonitoringWithMapping() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (++references_ == 1) {
    base::MappedReadOnlyRegion shmem =
        base::ReadOnlySharedMemoryRegion::Create(sizeof(uint32_t));
    if (!shmem.IsValid()) {
      DVLOG(2) << "Error mapping key press count shmem.";
      return base::ReadOnlySharedMemoryRegion();
    }

    key_press_count_region_ =
        base::ReadOnlySharedMemoryRegion(std::move(shmem.region));
    WriteKeyPressMonitorCount(shmem.mapping, 0u);
    StartKeyboardMonitoring(std::move(shmem.mapping));
    DVLOG(2) << "Started keyboard monitoring.";
  }

  return key_press_count_region_.Duplicate();
}

void UserInputMonitorBase::DisableKeyPressMonitoring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK_NE(references_, 0u);
  if (--references_ == 0) {
    key_press_count_region_ = base::ReadOnlySharedMemoryRegion();
    StopKeyboardMonitoring();
    DVLOG(2) << "Stopped keyboard monitoring.";
  }
}

}  // namespace media
