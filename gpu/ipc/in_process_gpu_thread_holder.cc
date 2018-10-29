// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/in_process_gpu_thread_holder.h"

#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"

namespace gpu {

InProcessGpuThreadHolder::InProcessGpuThreadHolder()
    : base::Thread("GpuThread") {
  DCHECK(base::CommandLine::InitializedForCurrentProcess());
  auto* command_line = base::CommandLine::ForCurrentProcess();
  gpu_preferences_ = gles2::ParseGpuPreferences(command_line);
  gpu_preferences_.texture_target_exception_list =
      CreateBufferUsageAndFormatExceptionList();

  gpu::GPUInfo gpu_info;
  gpu::CollectGraphicsInfoForTesting(&gpu_info);
  gpu::GpuFeatureInfo gpu_feature_info = gpu::ComputeGpuFeatureInfo(
      gpu_info, gpu_preferences_, command_line, nullptr);

  Start();
}

InProcessGpuThreadHolder::~InProcessGpuThreadHolder() {
  // Ensure members created on GPU thread are destroyed there too.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&InProcessGpuThreadHolder::DeleteOnGpuThread,
                                base::Unretained(this)));
  Stop();
}

GpuPreferences* InProcessGpuThreadHolder::GetGpuPreferences() {
  DCHECK(!task_executor_);
  return &gpu_preferences_;
}

GpuFeatureInfo* InProcessGpuThreadHolder::GetGpuFeatureInfo() {
  DCHECK(!task_executor_);
  return &gpu_feature_info_;
}

void InProcessGpuThreadHolder::SetGpuFeatureInfo(
    const GpuFeatureInfo& gpu_feature_info) {
  DCHECK(!task_executor_);
  gpu_feature_info_ = gpu_feature_info;
}

scoped_refptr<CommandBufferTaskExecutor>
InProcessGpuThreadHolder::GetTaskExecutor() {
  if (!task_executor_) {
    base::WaitableEvent completion;
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&InProcessGpuThreadHolder::InitializeOnGpuThread,
                       base::Unretained(this), &completion));
    completion.Wait();
  }
  return task_executor_;
}

void InProcessGpuThreadHolder::InitializeOnGpuThread(
    base::WaitableEvent* completion) {
  sync_point_manager_ = std::make_unique<SyncPointManager>();
  scheduler_ =
      std::make_unique<Scheduler>(task_runner(), sync_point_manager_.get());
  task_executor_ = base::MakeRefCounted<GpuInProcessThreadService>(
      task_runner(), scheduler_.get(), sync_point_manager_.get(), nullptr,
      nullptr, gl::GLSurfaceFormat(), gpu_feature_info_, gpu_preferences_);

  completion->Signal();
}

void InProcessGpuThreadHolder::DeleteOnGpuThread() {
  task_executor_.reset();
  scheduler_.reset();
  sync_point_manager_.reset();
}

}  // namespace gpu
