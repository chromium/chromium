// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_GPU_TASK_SCHEDULER_H_
#define SERVICES_WEBNN_GPU_TASK_SCHEDULER_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"

namespace gpu {
class Scheduler;
}  // namespace gpu

namespace webnn {

// Owns and provides task scheduling for a gpu::Sequence on
// `owning_task_runner`.
class COMPONENT_EXPORT(WEBNN_SERVICE) GpuTaskScheduler {
 public:
  GpuTaskScheduler(
      gpu::Scheduler& scheduler,
      gpu::CommandBufferId command_buffer_id,
      gpu::CommandBufferNamespace namespace_id,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner);

  ~GpuTaskScheduler();

  // Move and copy not allowed.
  GpuTaskScheduler(GpuTaskScheduler&&) = delete;
  GpuTaskScheduler& operator=(GpuTaskScheduler&&) = delete;

  GpuTaskScheduler(const GpuTaskScheduler&) = delete;
  GpuTaskScheduler& operator=(const GpuTaskScheduler&) = delete;

  // Exposes a SequencedTaskRunner which is used to run Mojo messages in this
  // gpu sequence. Lazily creates the underlying gpu::Sequence on
  // `owning_task_runner_`. Does not support nested loops or delayed tasks.
  const scoped_refptr<base::SequencedTaskRunner>& GetTaskRunner();

  // Destroys the underlying gpu::Sequence if it was created. Must be called on
  // `owning_task_runner_`.
  void DestroySequence();

  // Schedules a task to be executed in this gpu sequence after the given fences
  // and releases the given sync token when the task is completed.
  void ScheduleGpuTask(base::OnceClosure task_closure,
                       std::vector<gpu::SyncToken> fences = {},
                       gpu::SyncToken release = gpu::SyncToken());

  gpu::CommandBufferId command_buffer_id() const { return command_buffer_id_; }
  gpu::CommandBufferNamespace namespace_id() const { return namespace_id_; }
  gpu::SequenceId sequence_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return sequence_id_;
  }

 private:
  // Binds to `owning_task_runner_` when the sequence is initialized.
  SEQUENCE_CHECKER(sequence_checker_);

  void ScheduleGpuTaskImpl(base::OnceClosure task_closure,
                           std::vector<gpu::SyncToken> fences,
                           const gpu::SyncToken& release);

  const raw_ref<gpu::Scheduler> scheduler_;
  const gpu::CommandBufferId command_buffer_id_;
  const gpu::CommandBufferNamespace namespace_id_;
  const scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner_;
  gpu::SequenceId sequence_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Marks shutdown of this sequence to prevent tasks from running after
  // destruction.
  base::WeakPtrFactory<GpuTaskScheduler> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_GPU_TASK_SCHEDULER_H_
