// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_
#define GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "gpu/ipc/gl_in_process_context_export.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "ui/gl/gl_share_group.h"

namespace gpu {
class Scheduler;
class SingleTaskSequence;

namespace gles2 {
class ProgramCache;
}  // namespace gles2

class GL_IN_PROCESS_CONTEXT_EXPORT GpuInProcessThreadServiceDelegate {
 public:
  GpuInProcessThreadServiceDelegate();

  GpuInProcessThreadServiceDelegate(const GpuInProcessThreadServiceDelegate&) =
      delete;
  GpuInProcessThreadServiceDelegate& operator=(
      const GpuInProcessThreadServiceDelegate&) = delete;

  virtual ~GpuInProcessThreadServiceDelegate();

  virtual scoped_refptr<SharedContextState> GetSharedContextState() = 0;
  virtual scoped_refptr<gl::GLShareGroup> GetShareGroup() = 0;
};

// Default Service class when no service is specified. GpuInProcessThreadService
// is used by Mus and unit tests.
class GL_IN_PROCESS_CONTEXT_EXPORT GpuInProcessThreadService
    : public CommandBufferTaskExecutor {
 public:
  GpuInProcessThreadService(
      GpuInProcessThreadServiceDelegate* delegate,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      Scheduler* scheduler,
      SyncPointManager* sync_point_manager,
      gl::GLSurfaceFormat share_group_surface_format,
      const GpuFeatureInfo& gpu_feature_info,
      const GpuPreferences& gpu_preferences,
      SharedImageManager* shared_image_manager,
      gles2::ProgramCache* program_cache);

  GpuInProcessThreadService(const GpuInProcessThreadService&) = delete;
  GpuInProcessThreadService& operator=(const GpuInProcessThreadService&) =
      delete;

  ~GpuInProcessThreadService() override;

  // CommandBufferTaskExecutor implementation.
  bool ForceVirtualizedGLContexts() const override;
  bool ShouldCreateMemoryTracker() const override;
  std::unique_ptr<SingleTaskSequence> CreateSequence() override;
  void ScheduleOutOfOrderTask(base::OnceClosure task) override;
  void ScheduleDelayedWork(base::OnceClosure task) override;
  void PostNonNestableToClient(base::OnceClosure callback) override;
  scoped_refptr<SharedContextState> GetSharedContextState() override;
  scoped_refptr<gl::GLShareGroup> GetShareGroup() override;

 private:
  const raw_ptr<GpuInProcessThreadServiceDelegate> delegate_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  raw_ptr<Scheduler> scheduler_;
};

}  // namespace gpu

#endif  // GPU_IPC_GPU_IN_PROCESS_THREAD_SERVICE_H_
