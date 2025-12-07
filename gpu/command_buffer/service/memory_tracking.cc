// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/memory_tracking.h"

#include "gpu/ipc/common/command_buffer_id.h"

namespace gpu {

MemoryTracker::MemoryTracker(
    CommandBufferId command_buffer_id,
    uint64_t client_tracing_id,
    scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    GpuPeakMemoryAllocationSource source)
    : command_buffer_id_(command_buffer_id),
      client_tracing_id_(client_tracing_id),
      peak_memory_monitor_(std::move(peak_memory_monitor)),
      allocation_source_(source) {}

MemoryTracker::MemoryTracker()
    : command_buffer_id_(0),
      client_tracing_id_(0),
      peak_memory_monitor_(nullptr),
      allocation_source_(GpuPeakMemoryAllocationSource::UNKNOWN) {}

MemoryTracker::~MemoryTracker() {
  DCHECK_EQ(mem_traker_size_, 0u);
}

void MemoryTracker::TrackMemoryAllocatedChange(int64_t delta) {
  base::AutoLock auto_lock(memory_tracker_lock_);
  DCHECK(delta >= 0 || mem_traker_size_ >= static_cast<uint64_t>(-delta));

  uint64_t old_size = mem_traker_size_;
  mem_traker_size_ += delta;

  if (peak_memory_monitor_) {
    peak_memory_monitor_->OnMemoryAllocatedChange(
        command_buffer_id_, old_size, mem_traker_size_, allocation_source_);
  }
}

uint64_t MemoryTracker::GetSize() const {
  base::AutoLock auto_lock(memory_tracker_lock_);
  return mem_traker_size_;
}

uint64_t MemoryTracker::ClientTracingId() const {
  return client_tracing_id_;
}

int MemoryTracker::ClientId() const {
  return ChannelIdFromCommandBufferId(command_buffer_id_);
}

uint64_t MemoryTracker::ContextGroupTracingId() const {
  return command_buffer_id_.GetUnsafeValue();
}

//
// MemoryTypeTracker
//
MemoryTypeTracker::MemoryTypeTracker(
    scoped_refptr<MemoryTracker> memory_tracker)
    : memory_tracker_(std::move(memory_tracker)) {}

MemoryTypeTracker::~MemoryTypeTracker() {
  DCHECK_EQ(mem_represented_, 0u);
}

void MemoryTypeTracker::TrackMemAlloc(size_t bytes) {
  base::AutoLock auto_lock(lock_);
  DCHECK(bytes >= 0);
  mem_represented_ += bytes;

  if (memory_tracker_ && bytes) {
    memory_tracker_->TrackMemoryAllocatedChange(bytes);
  }
}

void MemoryTypeTracker::TrackMemFree(size_t bytes) {
  base::AutoLock auto_lock(lock_);
  DCHECK(bytes >= 0 && bytes <= mem_represented_);
  mem_represented_ -= bytes;

  if (memory_tracker_ && bytes) {
    memory_tracker_->TrackMemoryAllocatedChange(-static_cast<int64_t>(bytes));
  }
}

size_t MemoryTypeTracker::GetMemRepresented() const {
  base::AutoLock auto_lock(lock_);
  return mem_represented_;
}

}  // namespace gpu
