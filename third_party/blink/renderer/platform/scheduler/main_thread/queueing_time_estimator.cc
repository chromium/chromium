// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/queueing_time_estimator.h"

#include <algorithm>

namespace blink {
namespace scheduler {

namespace {

// On Windows, when a computer sleeps, we may end up getting extremely long
// tasks or idling. We'll ignore tasks longer than |kInvalidPeriodThreshold|.
constexpr base::TimeDelta kInvalidPeriodThreshold =
    base::TimeDelta::FromSecondsD(30);

// This method computes the expected queueing time of a randomly distributed
// task R within a window containing a single task T. Let T' be the time range
// for which T overlaps the window. We first compute the probability that R will
// start within T'. We then compute the expected queueing duration if R does
// start within this range. Since the start time of R is uniformly distributed
// within the window, this is equal to the average of the queueing times if R
// started at the beginning or end of T'. The expected queueing time of T is the
// probability that R will start within T', multiplied by the expected queueing
// duration if R does fall in this range.
base::TimeDelta ExpectedQueueingTimeFromTask(base::TimeTicks task_start,
                                             base::TimeTicks task_end,
                                             base::TimeTicks step_start,
                                             base::TimeTicks step_end) {
  DCHECK_LE(task_start, task_end);
  DCHECK_LE(task_start, step_end);
  DCHECK_LT(step_start, step_end);
  // Because we skip steps when disabled, we may have gone into the future, and
  // in that case we ignore this task completely.
  if (task_end < step_start)
    return base::TimeDelta();

  base::TimeTicks task_in_step_start_time = std::max(task_start, step_start);
  base::TimeTicks task_in_step_end_time = std::min(task_end, step_end);
  DCHECK_LE(task_in_step_end_time, task_in_step_end_time);

  double probability_of_this_task =
      (task_in_step_end_time - task_in_step_start_time).InMicrosecondsF() /
      (step_end - step_start).InMicrosecondsF();

  base::TimeDelta expected_queueing_duration_within_task =
      ((task_end - task_in_step_start_time) +
       (task_end - task_in_step_end_time)) /
      2;

  return probability_of_this_task * expected_queueing_duration_within_task;
}

}  // namespace

QueueingTimeEstimator::QueueingTimeEstimator(Client* client,
                                             base::TimeDelta window_duration,
                                             int steps_per_window,
                                             bool start_disabled)
    : client_(client),
      window_step_width_(window_duration / steps_per_window),
      disabled_(start_disabled),
      calculator_(steps_per_window) {
  DCHECK_GE(steps_per_window, 1);
}

void QueueingTimeEstimator::OnExecutionStarted(base::TimeTicks now) {
  DCHECK(!busy_);
  AdvanceTime(now);
  busy_ = true;
  busy_period_start_time_ = now;
}

void QueueingTimeEstimator::OnExecutionStopped(base::TimeTicks now) {
  DCHECK(busy_);
  AdvanceTime(now);
  busy_ = false;
  busy_period_start_time_ = base::TimeTicks();
}

void QueueingTimeEstimator::OnRecordingStateChanged(
    bool disabled,
    base::TimeTicks transition_time) {
  DCHECK_NE(disabled, disabled_);
  if (!busy_)
    AdvanceTime(transition_time);
  disabled_ = disabled;
}

void QueueingTimeEstimator::AdvanceTime(base::TimeTicks current_time) {
  if (step_start_time_.is_null()) {
    // Ignore any time before the first task.
    if (!busy_)
      return;

    step_start_time_ = busy_period_start_time_;
  }
  base::TimeTicks reference_time =
      busy_ ? busy_period_start_time_ : step_start_time_;
  if (disabled_ || current_time - reference_time > kInvalidPeriodThreshold) {
    // Skip steps when we're disabled, when a task took too long, or when we
    // remained idle for too long. May cause |step_start_time_| to go slightly
    // into the future.
    // TODO(npm): crbug.com/776013. Base skipping long tasks/idling on a signal
    // that we've been suspended.
    step_start_time_ =
        current_time.SnappedToNextTick(step_start_time_, window_step_width_);
    calculator_.ResetStep();
    return;
  }
  while (TimePastStepEnd(current_time)) {
    if (busy_) {
      // Include the current task in this window.
      calculator_.AddQueueingTime(ExpectedQueueingTimeFromTask(
          busy_period_start_time_, current_time, step_start_time_,
          step_start_time_ + window_step_width_));
    }
    calculator_.EndStep(client_);
    step_start_time_ += window_step_width_;
  }
  if (busy_) {
    calculator_.AddQueueingTime(ExpectedQueueingTimeFromTask(
        busy_period_start_time_, current_time, step_start_time_,
        step_start_time_ + window_step_width_));
  }
}

bool QueueingTimeEstimator::TimePastStepEnd(base::TimeTicks time) {
  return time >= step_start_time_ + window_step_width_;
}

QueueingTimeEstimator::Calculator::Calculator(int steps_per_window)
    : steps_per_window_(steps_per_window), sliding_window_(steps_per_window) {}

void QueueingTimeEstimator::Calculator::AddQueueingTime(
    base::TimeDelta queueing_time) {
  step_expected_queueing_time_ += queueing_time;
}

void QueueingTimeEstimator::Calculator::EndStep(Client* client) {
  sliding_window_.Add(step_expected_queueing_time_);

  DCHECK(client);
  // MainThreadSchedulerImpl reports the queueing time once per disjoint window.
  //          |stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|
  // Report:  |-------window EQT------|
  // Discard:         |-------window EQT------|
  // Discard:                 |-------window EQT------|
  // Report:                          |-------window EQT------|
  client->OnQueueingTimeForWindowEstimated(sliding_window_.GetAverage(),
                                           sliding_window_.IndexIsZero());
  ResetStep();
}

void QueueingTimeEstimator::Calculator::ResetStep() {
  step_expected_queueing_time_ = base::TimeDelta();
}

QueueingTimeEstimator::RunningAverage::RunningAverage(int size)
    : index_(0), circular_buffer_(size), running_sum_() {}

int QueueingTimeEstimator::RunningAverage::GetStepsPerWindow() const {
  return static_cast<int>(circular_buffer_.size());
}

void QueueingTimeEstimator::RunningAverage::Add(base::TimeDelta bin_value) {
  running_sum_ -= circular_buffer_[index_];
  circular_buffer_[index_] = bin_value;
  running_sum_ += bin_value;
  index_ = (index_ + 1) % circular_buffer_.size();
}

base::TimeDelta QueueingTimeEstimator::RunningAverage::GetAverage() const {
  return running_sum_ / circular_buffer_.size();
}

bool QueueingTimeEstimator::RunningAverage::IndexIsZero() const {
  return index_ == 0;
}

}  // namespace scheduler
}  // namespace blink
