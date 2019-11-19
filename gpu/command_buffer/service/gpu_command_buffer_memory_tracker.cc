// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "gpu/ipc/common/command_buffer_id.h"

// Macro to reduce code duplication when logging memory in
// GpuCommandBufferMemoryTracker. This is needed as the UMA_HISTOGRAM_* macros
// require a unique call-site per histogram (you can't funnel multiple strings
// into the same call-site).
#define GPU_COMMAND_BUFFER_MEMORY_BLOCK(category)                          \
  do {                                                                     \
    uint64_t mb_used = size_ / (1024 * 1024);                              \
    switch (context_type_) {                                               \
      case CONTEXT_TYPE_WEBGL1:                                            \
      case CONTEXT_TYPE_WEBGL2:                                            \
      case CONTEXT_TYPE_WEBGL2_COMPUTE:                                    \
        UMA_HISTOGRAM_MEMORY_LARGE_MB("GPU.ContextMemory.WebGL." category, \
                                      mb_used);                            \
        break;                                                             \
      case CONTEXT_TYPE_OPENGLES2:                                         \
      case CONTEXT_TYPE_OPENGLES3:                                         \
        UMA_HISTOGRAM_MEMORY_LARGE_MB("GPU.ContextMemory.GLES." category,  \
                                      mb_used);                            \
        break;                                                             \
      case CONTEXT_TYPE_WEBGPU:                                            \
        break;                                                             \
    }                                                                      \
  } while (false)

namespace gpu {

GpuCommandBufferMemoryTracker::GpuCommandBufferMemoryTracker(
    CommandBufferId command_buffer_id,
    uint64_t client_tracing_id,
    ContextType context_type,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Observer* observer)
    : command_buffer_id_(command_buffer_id),
      client_tracing_id_(client_tracing_id),
      context_type_(context_type),
      memory_pressure_listener_(base::BindRepeating(
          &GpuCommandBufferMemoryTracker::LogMemoryStatsPressure,
          base::Unretained(this))),
      observer_(observer) {
  // Set up |memory_stats_timer_| to call LogMemoryPeriodic periodically
  // via the provided |task_runner|.
  memory_stats_timer_.SetTaskRunner(std::move(task_runner));
  memory_stats_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(30), this,
      &GpuCommandBufferMemoryTracker::LogMemoryStatsPeriodic);
}

GpuCommandBufferMemoryTracker::~GpuCommandBufferMemoryTracker() {
  LogMemoryStatsShutdown();
}

void GpuCommandBufferMemoryTracker::TrackMemoryAllocatedChange(uint64_t delta) {
  uint64_t old_size = size_;
  size_ += delta;
  if (observer_)
    observer_->OnMemoryAllocatedChange(command_buffer_id_, old_size, size_);
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

void GpuCommandBufferMemoryTracker::LogMemoryStatsPeriodic() {
  GPU_COMMAND_BUFFER_MEMORY_BLOCK("Periodic");
}

void GpuCommandBufferMemoryTracker::LogMemoryStatsShutdown() {
  GPU_COMMAND_BUFFER_MEMORY_BLOCK("Shutdown");
}

void GpuCommandBufferMemoryTracker::LogMemoryStatsPressure(
    base::MemoryPressureListener::MemoryPressureLevel pressure_level) {
  // Only log on CRITICAL memory pressure.
  if (pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    GPU_COMMAND_BUFFER_MEMORY_BLOCK("Pressure");
  }
}

}  // namespace gpu
