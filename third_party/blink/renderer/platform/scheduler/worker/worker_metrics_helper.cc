// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_metrics_helper.h"

#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"

namespace blink {
namespace scheduler {

WorkerMetricsHelper::WorkerMetricsHelper(ThreadType thread_type,
                                         bool has_cpu_timing_for_each_task)
    : MetricsHelper(thread_type, has_cpu_timing_for_each_task),
      dedicated_worker_per_parent_frame_status_duration_reporter_(
          "RendererScheduler.TaskDurationPerFrameOriginType2.DedicatedWorker"),
      background_dedicated_worker_per_parent_frame_status_duration_reporter_(
          "RendererScheduler.TaskDurationPerFrameOriginType2.DedicatedWorker."
          "Background") {}

WorkerMetricsHelper::~WorkerMetricsHelper() {}

void WorkerMetricsHelper::SetParentFrameType(FrameOriginType frame_type) {
  parent_frame_type_ = frame_type;
}

void WorkerMetricsHelper::RecordTaskMetrics(
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (ShouldDiscardTask(task, task_timing))
    return;

  bool backgrounded = internal::ProcessState::Get()->is_process_backgrounded;

  if (thread_type_ == ThreadType::kDedicatedWorkerThread) {
    if (parent_frame_type_) {
      dedicated_worker_per_parent_frame_status_duration_reporter_.RecordTask(
          parent_frame_type_.value(), task_timing.wall_duration());

      if (backgrounded) {
        background_dedicated_worker_per_parent_frame_status_duration_reporter_
            .RecordTask(parent_frame_type_.value(),
                        task_timing.wall_duration());
      }
    }
  }
}

}  // namespace scheduler
}  // namespace blink
