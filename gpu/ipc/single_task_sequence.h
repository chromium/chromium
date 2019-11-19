// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SINGLE_TASK_SEQUENCE_H_
#define GPU_IPC_SINGLE_TASK_SEQUENCE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/ipc/gl_in_process_context_export.h"

namespace gpu {
// Represents a single task execution sequence. Tasks posted to a sequence are
// run in order. Tasks across sequences should be synchronized using sync
// tokens. Destroying the sequence will drop tasks which haven't been executed
// yet.
class GL_IN_PROCESS_CONTEXT_EXPORT SingleTaskSequence {
 public:
  virtual ~SingleTaskSequence() {}

  // Returns identifier used for identifying sync tokens with this sequence,
  // and for scheduling.
  virtual SequenceId GetSequenceId() = 0;

  // Returns true if sequence should yield while running its current task.
  virtual bool ShouldYield() = 0;

  // Schedule a task with provided sync token dependencies. The dependencies
  // are hints for sync token waits within the task, and can be ignored by the
  // implementation.
  // For scheduling from viz thread, due to limitations in Android WebView,
  // ScheduleTask is only available to be called inside initialization,
  // teardown, and DrawAndSwap.
  virtual void ScheduleTask(base::OnceClosure task,
                            std::vector<SyncToken> sync_token_fences) = 0;

  // If |ScheduleGpuTask| is available, then this is equivalent to
  // ScheduleGpuTask. Otherwise, the |task| and |sync_tokens| are retained
  // and run when |ScheduleGpuTask| becomes available. Either case, tasks in
  // |ScheduleTask| and |ScheduleOrRetainTask| are sequenced by the call order;
  // calling this instead of |ScheduleTask| can only delay but not reorder
  // tasks.
  virtual void ScheduleOrRetainTask(
      base::OnceClosure task,
      std::vector<SyncToken> sync_token_fences) = 0;

  // Continue running the current task after yielding execution.
  virtual void ContinueTask(base::OnceClosure task) = 0;
};
}  // namespace gpu

#endif  // GPU_IPC_SINGLE_TASK_SEQUENCE_H_
