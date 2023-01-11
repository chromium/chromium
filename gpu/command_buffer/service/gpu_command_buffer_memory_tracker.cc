// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_peak_memory.h"

namespace gpu {

GpuCommandBufferMemoryTracker::GpuCommandBufferMemoryTracker(
    CommandBufferId command_buffer_id,
    uint64_t client_tracing_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Observer* observer)
    : command_buffer_id_(command_buffer_id),
      client_tracing_id_(client_tracing_id),
      observer_(observer) {
}

GpuCommandBufferMemoryTracker::~GpuCommandBufferMemoryTracker() = default;

void GpuCommandBufferMemoryTracker::TrackMemoryAllocatedChange(int64_t delta) {
  DCHECK(delta >= 0 || size_ >= static_cast<uint64_t>(-delta));
  uint64_t old_size = size_;
  size_ += delta;
  if (observer_)
    observer_->OnMemoryAllocatedChange(
        command_buffer_id_, old_size, size_,
        GpuPeakMemoryAllocationSource::COMMAND_BUFFER);
}

uint64_t GpuCommandBufferMemoryTracker::GetSize() const {
  return size_;
}

uint64_t GpuCommandBufferMemoryTracker::ClientTracingId() const {
  return client_tracing_id_;
}

int GpuCommandBufferMemoryTracker::ClientId() const {
  return ChannelIdFromCommandBufferId(command_buffer_id_);
}

uint64_t GpuCommandBufferMemoryTracker::ContextGroupTracingId() const {
  return command_buffer_id_.GetUnsafeValue();
}

}  // namespace gpu
