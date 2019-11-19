// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PRIORITIZE_COMPOSITING_AFTER_INPUT_EXPERIMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PRIORITIZE_COMPOSITING_AFTER_INPUT_EXPERIMENT_H_

#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace scheduler {

class MainThreadSchedulerImpl;
class MainThreadTaskQueue;

class PLATFORM_EXPORT PrioritizeCompositingAfterInputExperiment {
  DISALLOW_NEW();

 public:
  explicit PrioritizeCompositingAfterInputExperiment(
      MainThreadSchedulerImpl* scheduler);
  ~PrioritizeCompositingAfterInputExperiment();

  base::sequence_manager::TaskQueue::QueuePriority
  GetIncreasedCompositingPriority();

  void OnTaskCompleted(MainThreadTaskQueue* queue);

  void OnWillBeginMainFrame();

  void OnMainFrameRequestedForInput();

 private:
  enum class TriggerType { kExplicitSignal, kInferredFromInput };

  enum class StopSignalType { kAllCompositingTasks, kWillBeginMainFrameSignal };

  void SetNumberOfCompositingTasksToPrioritize(int number_of_tasks);

  MainThreadSchedulerImpl* scheduler_;  // Not owned.

  const base::sequence_manager::TaskQueue::QueuePriority
      increased_compositing_priority_;
  const int number_of_tasks_to_prioritize_after_input_;
  const TriggerType trigger_type_;
  const StopSignalType stop_signal_type_;

  int number_of_tasks_to_prioritize_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PRIORITIZE_COMPOSITING_AFTER_INPUT_EXPERIMENT_H_
