// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SCHEDULER_SEQUENCE_H_
#define GPU_IPC_SCHEDULER_SEQUENCE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/ipc/gl_in_process_context_export.h"
#include "gpu/ipc/single_task_sequence.h"

namespace viz {
class Display;
class DisplayCompositorMemoryAndTaskController;
class ScopedAllowGpuAccessForDisplayResourceProvider;
class OutputSurfaceProviderImpl;
class OverlayProcessorAndroid;
}  // namespace viz

namespace gpu {
class Scheduler;

// Selectively allow ScheduleTask if DefaultDisallowScheduleTaskOnCurrentThread
// is used for a thread.
class GL_IN_PROCESS_CONTEXT_EXPORT ScopedAllowScheduleGpuTask {
 public:
  ~ScopedAllowScheduleGpuTask();

 private:
  // Only add more friend declarations for classes that Android WebView is
  // guaranteed to be able to support. Talk to boliu@ if in doubt.
  friend class viz::Display;
  friend class viz::DisplayCompositorMemoryAndTaskController;
  friend class viz::ScopedAllowGpuAccessForDisplayResourceProvider;
  friend class viz::OutputSurfaceProviderImpl;
  // Overlay is not supported for WebView. However the initialization and
  // destruction of OverlayProcessor requires posting task to gpu thread, which
  // would trigger DCHECK, even though the task posting would not run on
  // WebView.
  friend class viz::OverlayProcessorAndroid;
  ScopedAllowScheduleGpuTask();

#if DCHECK_IS_ON()
  const bool original_value_;
#endif
  DISALLOW_COPY_AND_ASSIGN(ScopedAllowScheduleGpuTask);
};

// SingleTaskSequence implementation that uses gpu scheduler sequences.
class GL_IN_PROCESS_CONTEXT_EXPORT SchedulerSequence
    : public SingleTaskSequence {
 public:
  // Enable DCHECKs for Android WebView restrictions for ScheduleTask for
  // current thread. Then use ScopedAllowScheduleGpuTask to selectively
  // allow ScheduleTask.
  static void DefaultDisallowScheduleTaskOnCurrentThread();

  explicit SchedulerSequence(Scheduler* scheduler);

  // Note: this drops tasks not executed yet.
  ~SchedulerSequence() override;

  // SingleTaskSequence implementation.
  SequenceId GetSequenceId() override;
  bool ShouldYield() override;
  void ScheduleTask(base::OnceClosure task,
                    std::vector<SyncToken> sync_token_fences) override;
  void ScheduleOrRetainTask(base::OnceClosure task,
                            std::vector<SyncToken> sync_token_fences) override;
  void ContinueTask(base::OnceClosure task) override;

 private:
  Scheduler* const scheduler_;
  const SequenceId sequence_id_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerSequence);
};

}  // namespace gpu

#endif  // GPU_IPC_SCHEDULER_SEQUENCE_H_
