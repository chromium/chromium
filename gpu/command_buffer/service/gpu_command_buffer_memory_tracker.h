// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_MEMORY_TRACKER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_MEMORY_TRACKER_H_

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/command_buffer_id.h"

namespace gpu {

// MemoryTracker implementation that also handles recording UMA histograms for
// periodic collection, on shutdown and on memory pressure.
class GPU_GLES2_EXPORT GpuCommandBufferMemoryTracker : public MemoryTracker {
 public:
  GpuCommandBufferMemoryTracker(
      CommandBufferId command_buffer_id,
      uint64_t client_tracing_id,
      ContextType context_type,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      Observer* observer);
  ~GpuCommandBufferMemoryTracker() override;

  // MemoryTracker implementation.
  void TrackMemoryAllocatedChange(uint64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

 private:
  void LogMemoryStatsPeriodic();
  void LogMemoryStatsShutdown();
  void LogMemoryStatsPressure(
      base::MemoryPressureListener::MemoryPressureLevel pressure_level);

  uint64_t size_ = 0;
  const CommandBufferId command_buffer_id_;
  const uint64_t client_tracing_id_;

  // Variables used in memory stat histogram logging.
  const ContextType context_type_;
  base::RepeatingTimer memory_stats_timer_;
  base::MemoryPressureListener memory_pressure_listener_;

  MemoryTracker::Observer* const observer_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferMemoryTracker);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_MEMORY_TRACKER_H_
