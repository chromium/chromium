// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_IN_PROCESS_GPU_THREAD_HOLDER_H_
#define GPU_IPC_IN_PROCESS_GPU_THREAD_HOLDER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"

namespace gpu {
class CommandBufferTaskExecutor;
class Scheduler;
class SharedImageManager;
class SyncPointManager;

// Starts a GPU thread and task executor that runs tasks on the GPU thread. This
// isn't a full GPU thread implementation and should only be used in tests. A
// default GpuPreferences and GpuFeatureInfo will be constructed from the
// command line when this class is first created.
class COMPONENT_EXPORT(GPU_THREAD_HOLDER) InProcessGpuThreadHolder
    : public base::Thread,
      public GpuInProcessThreadServiceDelegate {
 public:
  InProcessGpuThreadHolder();

  InProcessGpuThreadHolder(const InProcessGpuThreadHolder&) = delete;
  InProcessGpuThreadHolder& operator=(const InProcessGpuThreadHolder&) = delete;

  ~InProcessGpuThreadHolder() override;

  // Returns GpuPreferences that can be modified before GetTaskExecutor() is
  // called for the first time.
  GpuPreferences* GetGpuPreferences();

  // Returns GpuFeatureInfo that can be modified before GetTaskExecutor() is
  // called for the first time.
  GpuFeatureInfo* GetGpuFeatureInfo();

  // Returns a task executor that runs commands on the GPU thread. The task
  // executor will be created the first time this is called.
  CommandBufferTaskExecutor* GetTaskExecutor();

  // gpu::GpuInProcessThreadServiceDelegate implementation:
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  scoped_refptr<gl::GLShareGroup> GetShareGroup() override;

 private:
  void InitializeOnGpuThread(base::WaitableEvent* completion);
  void DeleteOnGpuThread();

  GpuPreferences gpu_preferences_;
  GpuFeatureInfo gpu_feature_info_;

  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;

  std::unique_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<SharedImageManager> shared_image_manager_;
  std::unique_ptr<CommandBufferTaskExecutor> task_executor_;
};

}  // namespace gpu

#endif  // GPU_IPC_IN_PROCESS_GPU_THREAD_HOLDER_H_
