// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_
#define GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/gl_in_process_context_export.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/single_task_sequence.h"
#include "ui/gl/gl_share_group.h"

namespace gpu {
class Scheduler;
class SingleTaskSequence;

namespace gles2 {
class ProgramCache;
}  // namespace gles2

// Default Service class when no service is specified. GpuInProcessThreadService
// is used by Mus and unit tests.
class GL_IN_PROCESS_CONTEXT_EXPORT GpuInProcessThreadService
    : public CommandBufferTaskExecutor {
 public:
  GpuInProcessThreadService(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      Scheduler* scheduler,
      SyncPointManager* sync_point_manager,
      MailboxManager* mailbox_manager,
      scoped_refptr<gl::GLShareGroup> share_group,
      gl::GLSurfaceFormat share_group_surface_format,
      const GpuFeatureInfo& gpu_feature_info,
      const GpuPreferences& gpu_preferences,
      SharedImageManager* shared_image_manager,
      gles2::ProgramCache* program_cache,
      scoped_refptr<SharedContextState> shared_context_state);
  ~GpuInProcessThreadService() override;

  // CommandBufferTaskExecutor implementation.
  bool ForceVirtualizedGLContexts() const override;
  bool ShouldCreateMemoryTracker() const override;
  std::unique_ptr<SingleTaskSequence> CreateSequence() override;
  void ScheduleOutOfOrderTask(base::OnceClosure task) override;
  void ScheduleDelayedWork(base::OnceClosure task) override;
  void PostNonNestableToClient(base::OnceClosure callback) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Scheduler* scheduler_;

  DISALLOW_COPY_AND_ASSIGN(GpuInProcessThreadService);
};

}  // namespace gpu

#endif  // GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_
