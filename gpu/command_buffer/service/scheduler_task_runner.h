// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_TASK_RUNNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_TASK_RUNNER_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "gpu/command_buffer/service/gpu_command_buffer_service_export.h"
#include "gpu/command_buffer/service/sequence_id.h"

namespace gpu {

class Scheduler;

// A SequencedTaskRunner implementation to abstract task execution for a
// specific SequenceId on the GPU Scheduler. This object does not support
// delayed tasks, because the underlying Scheduler implementation does not
// support scheduling delayed tasks. Also note that tasks run by this object do
// not support running nested RunLoops.
class GPU_COMMAND_BUFFER_SERVICE_EXPORT SchedulerTaskRunner
    : public base::SequencedTaskRunner {
 public:
  // Constructs a SchedulerTaskRunner that runs tasks on `scheduler`, on the
  // sequence identified by `sequence_id`. `ShutDown()` must be called before
  // `scheduler` is destroyed.
  SchedulerTaskRunner(Scheduler& scheduler, SequenceId sequence_id);

  // Once this is called, all subsequent tasks will be rejected and `scheduler_`
  // is cleared so refs to this task runner may safely outlive `scheduler`.
  void ShutDown();

  // base::SequencedTaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;
  bool RunsTasksInCurrentSequence() const override;

 private:
  ~SchedulerTaskRunner() override;

  void RunTask(base::OnceClosure task);

  base::Lock lock_;
  raw_ptr<Scheduler> scheduler_ GUARDED_BY(lock_);
  const SequenceId sequence_id_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_TASK_RUNNER_H_
