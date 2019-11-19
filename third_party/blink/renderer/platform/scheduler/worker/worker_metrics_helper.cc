// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_metrics_helper.h"

#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"

namespace blink {
namespace scheduler {

WorkerMetricsHelper::WorkerMetricsHelper(ThreadType thread_type,
                                         bool has_cpu_timing_for_each_task)
    : MetricsHelper(thread_type, has_cpu_timing_for_each_task),
      dedicated_worker_per_task_type_duration_reporter_(
          "RendererScheduler.TaskDurationPerTaskType2.DedicatedWorker"),
      dedicated_worker_per_task_type_cpu_duration_reporter_(
          "RendererScheduler.TaskCPUDurationPerTaskType2.DedicatedWorker"),
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
    NonMainThreadTaskQueue* queue,
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (ShouldDiscardTask(queue, task, task_timing))
    return;

  MetricsHelper::RecordCommonTaskMetrics(queue, task, task_timing);

  bool backgrounded = internal::ProcessState::Get()->is_process_backgrounded;

  if (thread_type_ == ThreadType::kDedicatedWorkerThread) {
    TaskType task_type = static_cast<TaskType>(task.task_type);
    dedicated_worker_per_task_type_duration_reporter_.RecordTask(
        task_type, task_timing.wall_duration());
    if (task_timing.has_thread_time()) {
      dedicated_worker_per_task_type_cpu_duration_reporter_.RecordTask(
          task_type, task_timing.thread_duration());
    }

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
