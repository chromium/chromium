// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_TASK_SCHEDULER_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_TASK_SCHEDULER_HELPER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/gpu_gles2_export.h"

namespace base {
class TimeTicks;
}

namespace viz {
class DisplayCompositorMemoryAndTaskController;
}

namespace gpu {
class CommandBufferTaskExecutor;
class CommandBufferHelper;
class GLInProcessContext;
class SingleTaskSequence;
class InProcessCommandBuffer;

// This class is a wrapper around a |gpu::SingleTaskSequence|. When we have
// SkiaRenderer enabled, this should behave exactly like a
// |gpu::SingleTaskSequence|. When we have GLRenderer with CommandBuffer, we
// need to initialize this class with a |CommandBufferHelper|. This is because
// when this class is used outside of actual CommandBuffer, we would need to
// make sure the order of post tasks still corresponds to the order that tasks
// are posted to the CommandBuffer.
// This class is per display compositor. When this is used with SkiaRenderer, it
// is created on SkiaOutputSurfaceImpl. Each user of this class would hold a
// reference.
class GPU_GLES2_EXPORT GpuTaskSchedulerHelper {
 public:
  // This constructor is only used for SkiaOutputSurface.
  explicit GpuTaskSchedulerHelper(
      std::unique_ptr<SingleTaskSequence> task_sequence);
  // This constructor is used for command buffer GLOutputSurface.
  explicit GpuTaskSchedulerHelper(
      CommandBufferTaskExecutor* command_buffer_task_executor);

  GpuTaskSchedulerHelper(const GpuTaskSchedulerHelper&) = delete;
  GpuTaskSchedulerHelper& operator=(const GpuTaskSchedulerHelper&) = delete;

  ~GpuTaskSchedulerHelper();

  // This function sets up the |command_buffer_helper| which flushes the command
  // buffer when a user outside of the command buffer shares the same
  // GpuTaskSchedulerHelper. This is only needed for sharing with the command
  // buffer, thus no need to be called when using SkiaRenderer.
  void Initialize(CommandBufferHelper* command_buffer_helper);

  using ReportingCallback =
      base::OnceCallback<void(base::TimeTicks task_ready)>;

  // This is called outside of CommandBuffer and would need to flush the command
  // buffer if the CommandBufferHelper is present. CommandBuffer is a friend of
  // this class and gets a direct pointer to the internal
  // |gpu::SingleTaskSequence|.
  void ScheduleGpuTask(base::OnceClosure task,
                       std::vector<SyncToken> sync_tokens,
                       ReportingCallback report_callback = ReportingCallback());

  // This is only called with SkiaOutputSurface, no need to flush command buffer
  // here.
  void ScheduleOrRetainGpuTask(base::OnceClosure task,
                               std::vector<SyncToken> sync_tokens);
  // This is only called with SkiaOutputSurface.
  SequenceId GetSequenceId();

 private:
  // If |using_command_buffer_| is true, we are using this class with
  // GLOutputSurface. Otherwise we are using this class with
  // SkiaOutputSurface.
  bool using_command_buffer_;

  friend class gpu::GLInProcessContext;
  friend class gpu::InProcessCommandBuffer;
  friend class viz::DisplayCompositorMemoryAndTaskController;
  // Only used for inside CommandBuffer implementation.
  SingleTaskSequence* GetTaskSequence() const;

  // This |task_sequence_| handles task scheduling.
  const std::unique_ptr<SingleTaskSequence> task_sequence_;

  // When this class is used with the command buffer, this bool indicates
  // whether we have initialized the |command_buffer_helper_|. The command
  // buffer requires the |task_sequence_| in order to initialize on gpu thread,
  // but the |command_buffer_helper_| is created after initialization of the
  // command buffer.
  bool initialized_;

  // In the case where the TaskSequence is shared between command buffer and
  // other users, |command_buffer_helper_| is used to flush the command buffer
  // before posting tasks from a different user. This gives the command buffer a
  // chance to post any pending tasks and maintains the ordering between command
  // buffer and other user tasks.
  raw_ptr<CommandBufferHelper> command_buffer_helper_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_TASK_SCHEDULER_HELPER_H_
