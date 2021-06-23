// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FIND_IN_PAGE_BUDGET_POOL_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FIND_IN_PAGE_BUDGET_POOL_CONTROLLER_H_

#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

using TaskQueue = base::sequence_manager::TaskQueue;
using QueuePriority = base::sequence_manager::TaskQueue::QueuePriority;

class CPUTimeBudgetPool;
class MainThreadSchedulerImpl;

class PLATFORM_EXPORT FindInPageBudgetPoolController
    : public BudgetPoolController {
 public:
  static constexpr auto kFindInPageBudgetNotExhaustedPriority =
      QueuePriority::kVeryHighPriority;
  static constexpr auto kFindInPageBudgetExhaustedPriority =
      QueuePriority::kNormalPriority;

  explicit FindInPageBudgetPoolController(MainThreadSchedulerImpl* scheduler);
  ~FindInPageBudgetPoolController() override;

  void OnTaskCompleted(MainThreadTaskQueue* queue,
                       TaskQueue::TaskTiming* task_timing);

  QueuePriority CurrentTaskPriority() { return task_priority_; }

  // Unimplemented methods.
  // TODO(crbug.com/1056512): Remove these functions once we factor out the
  // budget calculating logic from BudgetPoolController.
  void UpdateQueueSchedulingLifecycleState(base::TimeTicks now,
                                           TaskQueue* queue) override {}
  void AddQueueToBudgetPool(TaskQueue* queue,
                            BudgetPool* budget_pool) override {}
  void RemoveQueueFromBudgetPool(TaskQueue* queue,
                                 BudgetPool* budget_pool) override {}
  void UnregisterBudgetPool(BudgetPool* budget_pool) override {}
  bool IsThrottled(TaskQueue* queue) const override { return false; }

 private:
  MainThreadSchedulerImpl* scheduler_;  // Not owned.
  std::unique_ptr<CPUTimeBudgetPool> find_in_page_budget_pool_;
  QueuePriority task_priority_;
  const bool best_effort_budget_experiment_enabled_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FIND_IN_PAGE_BUDGET_POOL_CONTROLLER_H_
