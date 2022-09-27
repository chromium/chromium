// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/idle_time_estimator.h"

#include "base/time/default_tick_clock.h"

namespace blink {
namespace scheduler {

IdleTimeEstimator::IdleTimeEstimator(const base::TickClock* time_source,
                                     int sample_count,
                                     double estimation_percentile)
    : per_frame_compositor_task_runtime_(sample_count),
      time_source_(time_source),
      estimation_percentile_(estimation_percentile),
      nesting_level_(0),
      did_commit_(false) {}

IdleTimeEstimator::~IdleTimeEstimator() = default;

base::TimeDelta IdleTimeEstimator::GetExpectedIdleDuration(
    base::TimeDelta compositor_frame_interval) const {
  base::TimeDelta expected_compositor_task_runtime_ =
      per_frame_compositor_task_runtime_.Percentile(estimation_percentile_);
  return std::max(base::TimeDelta(), compositor_frame_interval -
                                         expected_compositor_task_runtime_);
}

void IdleTimeEstimator::DidCommitFrameToCompositor() {
  // This will run inside of a WillProcessTask / DidProcessTask pair, let
  // DidProcessTask know a frame was comitted.
  if (nesting_level_ == 1)
    did_commit_ = true;
}

void IdleTimeEstimator::Clear() {
  task_start_time_ = base::TimeTicks();
  prev_commit_time_ = base::TimeTicks();
  cumulative_compositor_runtime_ = base::TimeDelta();
  per_frame_compositor_task_runtime_.Clear();
  did_commit_ = false;
}

void IdleTimeEstimator::WillProcessTask(const base::PendingTask& pending_task,
                                        bool was_blocked_or_low_priority) {
  nesting_level_++;
  if (nesting_level_ == 1)
    task_start_time_ = time_source_->NowTicks();
}

void IdleTimeEstimator::DidProcessTask(const base::PendingTask& pending_task) {
  nesting_level_--;
  DCHECK_GE(nesting_level_, 0);
  if (nesting_level_ != 0)
    return;

  cumulative_compositor_runtime_ += time_source_->NowTicks() - task_start_time_;

  if (did_commit_) {
    per_frame_compositor_task_runtime_.InsertSample(
        cumulative_compositor_runtime_);
    cumulative_compositor_runtime_ = base::TimeDelta();
    did_commit_ = false;
  }
}

void IdleTimeEstimator::AddCompositorTaskQueue(
    scoped_refptr<MainThreadTaskQueue> compositor_task_queue) {
  compositor_task_queue->AddTaskObserver(this);
}

void IdleTimeEstimator::RemoveCompositorTaskQueue(
    scoped_refptr<MainThreadTaskQueue> compositor_task_queue) {
  compositor_task_queue->RemoveTaskObserver(this);
}

}  // namespace scheduler
}  // namespace blink
