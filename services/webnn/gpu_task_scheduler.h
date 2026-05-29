// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_GPU_TASK_SCHEDULER_H_
#define SERVICES_WEBNN_GPU_TASK_SCHEDULER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"

namespace gpu {
class Scheduler;
class SchedulerTaskRunner;
}  // namespace gpu

namespace webnn {

// Non-owning wrapper providing task scheduling for a gpu::Sequence.
// GpuTaskScheduler must not outlive the gpu::Sequence managed by the provider,
// except after context destruction where ShutDown() prevents execution.
class COMPONENT_EXPORT(WEBNN_SERVICE) GpuTaskScheduler {
 public:
  GpuTaskScheduler(gpu::Scheduler& scheduler,
                   gpu::CommandBufferId command_buffer_id,
                   gpu::SequenceId sequence_id,
                   gpu::CommandBufferNamespace namespace_id);

  ~GpuTaskScheduler();

  // Move and copy not allowed.
  GpuTaskScheduler(GpuTaskScheduler&&) = delete;
  GpuTaskScheduler& operator=(GpuTaskScheduler&&) = delete;

  GpuTaskScheduler(const GpuTaskScheduler&) = delete;
  GpuTaskScheduler& operator=(const GpuTaskScheduler&) = delete;

  // Exposes a SequencedTaskRunner which is used to run Mojo messages in this
  // gpu sequence. Does not support nested loops or delayed tasks.
  const scoped_refptr<gpu::SchedulerTaskRunner>& scheduler_task_runner() const {
    return scheduler_task_runner_;
  }

  // Schedules a task to be executed in this gpu sequence after the given fence
  // and releases the given sync token when the task is completed.
  void ScheduleGpuTask(base::OnceClosure task_closure,
                       gpu::SyncToken fence = gpu::SyncToken(),
                       gpu::SyncToken release = gpu::SyncToken());

  gpu::CommandBufferId command_buffer_id() const { return command_buffer_id_; }
  gpu::CommandBufferNamespace namespace_id() const { return namespace_id_; }
  gpu::SequenceId sequence_id() const { return sequence_id_; }

 private:
  // Binds to the sequence that first schedules GPU work. All accesses to
  // `weak_factory_` and destruction of this object must happen on that same
  // sequence, which requires `scheduler_task_runner_` to be shut down on that
  // sequence before destruction.
  SEQUENCE_CHECKER(sequence_checker_);

  void ScheduleGpuTaskImpl(base::OnceClosure task_closure,
                           std::vector<gpu::SyncToken> fences,
                           const gpu::SyncToken& release);

  const raw_ref<gpu::Scheduler> scheduler_;
  const gpu::CommandBufferId command_buffer_id_;
  const gpu::CommandBufferNamespace namespace_id_;
  const gpu::SequenceId sequence_id_;
  const scoped_refptr<gpu::SchedulerTaskRunner> scheduler_task_runner_;

  // Marks shutdown of this sequence to prevent tasks from running after
  // destruction.
  base::WeakPtrFactory<GpuTaskScheduler> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_GPU_TASK_SCHEDULER_H_
