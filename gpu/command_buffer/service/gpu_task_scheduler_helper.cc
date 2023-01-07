// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_task_scheduler_helper.h"

#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/command_buffer/service/single_task_sequence.h"

namespace gpu {

GpuTaskSchedulerHelper::GpuTaskSchedulerHelper(
    std::unique_ptr<SingleTaskSequence> task_sequence)
    : using_command_buffer_(false),
      task_sequence_(std::move(task_sequence)),
      initialized_(true) {}

GpuTaskSchedulerHelper::GpuTaskSchedulerHelper(
    gpu::CommandBufferTaskExecutor* command_buffer_task_executor)
    : using_command_buffer_(true),
      task_sequence_(command_buffer_task_executor->CreateSequence()),
      initialized_(false) {}

GpuTaskSchedulerHelper::~GpuTaskSchedulerHelper() = default;

void GpuTaskSchedulerHelper::Initialize(
    gpu::CommandBufferHelper* command_buffer_helper) {
  DCHECK(using_command_buffer_);
  DCHECK(!initialized_);
  DCHECK(command_buffer_helper);
  command_buffer_helper_ = command_buffer_helper;
  initialized_ = true;
}

void GpuTaskSchedulerHelper::ScheduleGpuTask(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens,
    SingleTaskSequence::ReportingCallback report_callback) {
  // There are two places where this function is called: inside
  // SkiaOutputSurface, where |using_command_buffer_| is false, or by other
  // users when sharing with command buffer, where we should ahve
  // |command_buffer_helper_| already set up.
  DCHECK(!using_command_buffer_ || command_buffer_helper_);
  DCHECK(initialized_);
  if (command_buffer_helper_)
    command_buffer_helper_->Flush();

  task_sequence_->ScheduleTask(std::move(callback), std::move(sync_tokens),
                               std::move(report_callback));
}

void GpuTaskSchedulerHelper::ScheduleOrRetainGpuTask(
    base::OnceClosure task,
    std::vector<SyncToken> sync_tokens) {
  DCHECK(!using_command_buffer_);
  DCHECK(!command_buffer_helper_);
  task_sequence_->ScheduleOrRetainTask(std::move(task), sync_tokens);
}

SequenceId GpuTaskSchedulerHelper::GetSequenceId() {
  DCHECK(!using_command_buffer_);
  DCHECK(!command_buffer_helper_);
  return task_sequence_->GetSequenceId();
}

gpu::SingleTaskSequence* GpuTaskSchedulerHelper::GetTaskSequence() const {
  // The are two places this function is called: inside command buffer or during
  // start up or tear down.
  return task_sequence_.get();
}

}  // namespace gpu
