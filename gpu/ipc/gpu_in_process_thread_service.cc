// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gpu_in_process_thread_service.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/ipc/scheduler_sequence.h"

namespace gpu {

GpuInProcessThreadService::GpuInProcessThreadService(
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
    scoped_refptr<SharedContextState> shared_context_state)
    : CommandBufferTaskExecutor(gpu_preferences,
                                gpu_feature_info,
                                sync_point_manager,
                                mailbox_manager,
                                share_group,
                                share_group_surface_format,
                                shared_image_manager,
                                program_cache,
                                std::move(shared_context_state)),
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
  return std::make_unique<SchedulerSequence>(scheduler_);
}

void GpuInProcessThreadService::ScheduleOutOfOrderTask(base::OnceClosure task) {
  task_runner_->PostTask(FROM_HERE, std::move(task));
}

void GpuInProcessThreadService::ScheduleDelayedWork(base::OnceClosure task) {
  task_runner_->PostDelayedTask(FROM_HERE, std::move(task),
                                base::TimeDelta::FromMilliseconds(2));
}

void GpuInProcessThreadService::PostNonNestableToClient(
    base::OnceClosure callback) {
  NOTREACHED();
}

}  // namespace gpu
