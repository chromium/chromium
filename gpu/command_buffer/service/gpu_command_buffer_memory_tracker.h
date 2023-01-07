// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_MEMORY_TRACKER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_MEMORY_TRACKER_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
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
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      Observer* observer);

  GpuCommandBufferMemoryTracker(const GpuCommandBufferMemoryTracker&) = delete;
  GpuCommandBufferMemoryTracker& operator=(
      const GpuCommandBufferMemoryTracker&) = delete;

  ~GpuCommandBufferMemoryTracker() override;

  // MemoryTracker implementation.
  void TrackMemoryAllocatedChange(int64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

 private:
  uint64_t size_ = 0;
  const CommandBufferId command_buffer_id_;
  const uint64_t client_tracing_id_;

  const raw_ptr<MemoryTracker::Observer> observer_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_COMMAND_BUFFER_MEMORY_TRACKER_H_
