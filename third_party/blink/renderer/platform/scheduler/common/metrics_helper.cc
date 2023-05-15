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
    : thread_type_(thread_type) {}

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

}  // namespace scheduler
}  // namespace blink
