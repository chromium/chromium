// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_SEQUENCE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_SEQUENCE_H_

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "gpu/gpu_gles2_export.h"

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
class GPU_GLES2_EXPORT [[maybe_unused, nodiscard]] ScopedAllowScheduleGpuTask {
 public:
  ScopedAllowScheduleGpuTask(const ScopedAllowScheduleGpuTask&) = delete;
  ScopedAllowScheduleGpuTask& operator=(const ScopedAllowScheduleGpuTask&) =
      delete;

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
  const base::AutoReset<bool> resetter_;
#endif
};

// SingleTaskSequence implementation that uses gpu scheduler sequences.
class GPU_GLES2_EXPORT SchedulerSequence : public SingleTaskSequence {
 public:
  // Enable DCHECKs for Android WebView restrictions for ScheduleTask for
  // current thread. Then use ScopedAllowScheduleGpuTask to selectively
  // allow ScheduleTask.
  //
  // Context: in WebView, display compositor tasks are scheduled on thread
  // created by Android framework, so we cannot post tasks to it at arbitrary
  // times. Calling this function signifies that by default, we should only
  // allow |ScheduleTask()| calls during specific moments (namely, when an
  // instance of `ScopedAllowScheduleGpuTask` is alive). If you are creating a
  // `SchedulerSequence` using a task runner that does not have any posting
  // restrictions, you can suppress the DCHECK by setting the
  // |target_thread_is_always_available| to `true` in the constructor.
  static void DefaultDisallowScheduleTaskOnCurrentThread();

  // Set |target_thread_is_always_available| to true to communicate that
  // ScheduleTask is always possible. This will suppress the DCHECKs enabled by
  // |DefaultDisallowScheduleTaskOnCurrentThread()|.
  SchedulerSequence(Scheduler* scheduler,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    bool target_thread_is_always_available = false);

  SchedulerSequence(const SchedulerSequence&) = delete;
  SchedulerSequence& operator=(const SchedulerSequence&) = delete;

  // Note: this drops tasks not executed yet.
  ~SchedulerSequence() override;

  // SingleTaskSequence implementation.
  SequenceId GetSequenceId() override;
  bool ShouldYield() override;
  void ScheduleTask(
      base::OnceClosure task,
      std::vector<SyncToken> sync_token_fences,
      ReportingCallback report_callback = ReportingCallback()) override;
  void ScheduleOrRetainTask(
      base::OnceClosure task,
      std::vector<SyncToken> sync_token_fences,
      ReportingCallback report_callback = ReportingCallback()) override;
  void ContinueTask(base::OnceClosure task) override;

 private:
  const raw_ptr<Scheduler> scheduler_;
  const SequenceId sequence_id_;
  const bool target_thread_is_always_available_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_SEQUENCE_H_
