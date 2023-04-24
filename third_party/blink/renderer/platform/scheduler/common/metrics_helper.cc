// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/metrics_helper.h"

#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"

namespace blink {
namespace scheduler {

namespace {

// Threshold for discarding ultra-long tasks. It is assumed that ultra-long
// tasks are reporting glitches (e.g. system falling asleep on the middle of the
// task).
constexpr base::TimeDelta kLongTaskDiscardingThreshold = base::Seconds(30);

}  // namespace

MetricsHelper::MetricsHelper(ThreadType thread_type,
                             bool has_cpu_timing_for_each_task)
    : thread_type_(thread_type),
      thread_task_cpu_duration_reporter_(
          "RendererScheduler.TaskCPUDurationPerThreadType2"),
      foreground_thread_task_cpu_duration_reporter_(
          "RendererScheduler.TaskCPUDurationPerThreadType2.Foreground"),
      background_thread_task_cpu_duration_reporter_(
          "RendererScheduler.TaskCPUDurationPerThreadType2.Background") {}

MetricsHelper::~MetricsHelper() {}

bool MetricsHelper::ShouldDiscardTask(
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  // TODO(altimin): Investigate the relationship between thread time and
  // wall time for discarded tasks.
  using State = base::sequence_manager ::TaskQueue::TaskTiming::State;
  return task_timing.state() == State::Finished &&
         task_timing.wall_duration() > kLongTaskDiscardingThreshold;
}

void MetricsHelper::RecordCommonTaskMetrics(
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  bool backgrounded = internal::ProcessState::Get()->is_process_backgrounded;

  if (!task_timing.has_thread_time())
    return;
  thread_task_cpu_duration_reporter_.RecordTask(thread_type_,
                                                task_timing.thread_duration());
  if (backgrounded) {
    background_thread_task_cpu_duration_reporter_.RecordTask(
        thread_type_, task_timing.thread_duration());
  } else {
    foreground_thread_task_cpu_duration_reporter_.RecordTask(
        thread_type_, task_timing.thread_duration());
  }
}

}  // namespace scheduler
}  // namespace blink
