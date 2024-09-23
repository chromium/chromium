// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gpu_in_process_thread_service.h"

#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"

namespace gpu {

GpuInProcessThreadServiceDelegate::GpuInProcessThreadServiceDelegate() =
    default;
GpuInProcessThreadServiceDelegate::~GpuInProcessThreadServiceDelegate() =
    default;

GpuInProcessThreadService::GpuInProcessThreadService(
    GpuInProcessThreadServiceDelegate* delegate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Scheduler* scheduler,
    SyncPointManager* sync_point_manager,
    gl::GLSurfaceFormat share_group_surface_format,
    const GpuFeatureInfo& gpu_feature_info,
    const GpuPreferences& gpu_preferences,
    SharedImageManager* shared_image_manager,
    gles2::ProgramCache* program_cache)
    : CommandBufferTaskExecutor(gpu_preferences,
                                gpu_feature_info,
                                sync_point_manager,
                                share_group_surface_format,
                                shared_image_manager,
                                program_cache),
      delegate_(delegate),
      task_runner_(task_runner),
      scheduler_(scheduler) {}

GpuInProcessThreadService::~GpuInProcessThreadService() = default;

bool GpuInProcessThreadService::ForceVirtualizedGLContexts() const {
  return false;
}

bool GpuInProcessThreadService::ShouldCreateMemoryTracker() const {
  return true;
}

std::unique_ptr<SingleTaskSequence>
GpuInProcessThreadService::CreateSequence() {
  return std::make_unique<SchedulerSequence>(scheduler_, task_runner_);
}

void GpuInProcessThreadService::ScheduleOutOfOrderTask(base::OnceClosure task) {
  task_runner_->PostTask(FROM_HERE, std::move(task));
}

void GpuInProcessThreadService::ScheduleDelayedWork(base::OnceClosure task) {
  task_runner_->PostDelayedTask(FROM_HERE, std::move(task),
                                base::Milliseconds(2));
}

void GpuInProcessThreadService::PostNonNestableToClient(
    base::OnceClosure callback) {
  NOTREACHED_IN_MIGRATION();
}

scoped_refptr<SharedContextState>
GpuInProcessThreadService::GetSharedContextState() {
  return delegate_->GetSharedContextState();
}

scoped_refptr<gl::GLShareGroup> GpuInProcessThreadService::GetShareGroup() {
  return delegate_->GetShareGroup();
}

}  // namespace gpu
