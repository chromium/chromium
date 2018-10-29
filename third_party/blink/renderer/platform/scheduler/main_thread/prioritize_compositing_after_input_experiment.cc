// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/prioritize_compositing_after_input_experiment.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

namespace {

// Prioritize compositing after input trial.
constexpr const char kPrioritizeCompositingAfterInputTrial[] =
    "BlinkSchedulerPrioritizeCompositingAfterInput";
constexpr const char kNumberOfCompositingTasksToPrioritizeAfterInputParam[] =
    "number_of_tasks";

constexpr int kDefaultNumberOfTasksToPrioritizeAfterInput = 1;

int GetNumberOfCompositingTasksToPrioritizeAfterInput() {
  if (!base::FeatureList::IsEnabled(kPrioritizeCompositingAfterInput))
    return 0;
  int number_of_tasks;
  if (!base::StringToInt(
          base::GetFieldTrialParamValue(
              kPrioritizeCompositingAfterInputTrial,
              kNumberOfCompositingTasksToPrioritizeAfterInputParam),
          &number_of_tasks)) {
    return kDefaultNumberOfTasksToPrioritizeAfterInput;
  }
  return number_of_tasks;
}

}  // namespace

PrioritizeCompositingAfterInputExperiment::
    PrioritizeCompositingAfterInputExperiment(
        MainThreadSchedulerImpl* scheduler)
    : scheduler_(scheduler),
      increased_compositing_priority_(
          base::FeatureList::IsEnabled(kHighestPriorityForCompositingAfterInput)
              ? base::sequence_manager::TaskQueue::QueuePriority::
                    kHighestPriority
              : base::sequence_manager::TaskQueue::QueuePriority::
                    kHighPriority),
      number_of_tasks_to_prioritize_after_input_(
          GetNumberOfCompositingTasksToPrioritizeAfterInput()),
      trigger_type_(
          base::FeatureList::IsEnabled(
              kUseExplicitSignalForTriggeringCompositingPrioritization)
              ? TriggerType::kExplicitSignal
              : TriggerType::kInferredFromInput),
      stop_signal_type_(base::FeatureList::IsEnabled(
                            kUseWillBeginMainFrameForCompositingPrioritization)
                            ? StopSignalType::kWillBeginMainFrameSignal
                            : StopSignalType::kAllCompositingTasks),
      number_of_tasks_to_prioritize_(0) {}

PrioritizeCompositingAfterInputExperiment::
    ~PrioritizeCompositingAfterInputExperiment() {}

void PrioritizeCompositingAfterInputExperiment::
    SetNumberOfCompositingTasksToPrioritize(int number_of_tasks) {
  number_of_tasks = std::max(number_of_tasks, 0);
  bool did_prioritize_compositing = number_of_tasks_to_prioritize_ > 0;
  number_of_tasks_to_prioritize_ = number_of_tasks;
  bool should_prioritize_compositing = number_of_tasks_to_prioritize_ > 0;
  if (did_prioritize_compositing != should_prioritize_compositing)
    scheduler_->SetShouldPrioritizeCompositing(should_prioritize_compositing);
}

base::sequence_manager::TaskQueue::QueuePriority
PrioritizeCompositingAfterInputExperiment::GetIncreasedCompositingPriority() {
  return increased_compositing_priority_;
}

void PrioritizeCompositingAfterInputExperiment::OnTaskCompleted(
    MainThreadTaskQueue* queue) {
  if (!queue)
    return;
  if (queue->queue_type() == MainThreadTaskQueue::QueueType::kInput &&
      trigger_type_ == TriggerType::kInferredFromInput) {
    SetNumberOfCompositingTasksToPrioritize(
        number_of_tasks_to_prioritize_after_input_);
  } else if (queue->queue_type() ==
                 MainThreadTaskQueue::QueueType::kCompositor &&
             stop_signal_type_ == StopSignalType::kAllCompositingTasks) {
    SetNumberOfCompositingTasksToPrioritize(number_of_tasks_to_prioritize_ - 1);
  }
}

void PrioritizeCompositingAfterInputExperiment::OnWillBeginMainFrame() {
  if (stop_signal_type_ != StopSignalType::kWillBeginMainFrameSignal)
    return;
  SetNumberOfCompositingTasksToPrioritize(number_of_tasks_to_prioritize_ - 1);
}

void PrioritizeCompositingAfterInputExperiment::OnMainFrameRequestedForInput() {
  if (trigger_type_ != TriggerType::kExplicitSignal)
    return;
  SetNumberOfCompositingTasksToPrioritize(
      number_of_tasks_to_prioritize_after_input_);
}

}  // namespace scheduler
}  // namespace blink
