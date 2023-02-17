// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/find_in_page_budget_pool_controller.h"

#include <memory>

#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

namespace {
// We will accumulate at most 1000ms for find-in-page budget.
constexpr base::TimeDelta kFindInPageMaxBudget = base::Seconds(1);
// At least 25% of the total CPU time will go to find-in-page tasks.
// TODO(rakina): Experiment with this number to figure out the right percentage
// for find-in-page. Currently this is following CompositorPriorityExperiments.
const double kFindInPageBudgetRecoveryRate = 0.25;
}  // namespace

const TaskPriority
    FindInPageBudgetPoolController::kFindInPageBudgetNotExhaustedPriority;
const TaskPriority
    FindInPageBudgetPoolController::kFindInPageBudgetExhaustedPriority;

FindInPageBudgetPoolController::FindInPageBudgetPoolController(
    MainThreadSchedulerImpl* scheduler)
    : scheduler_(scheduler),
      best_effort_budget_experiment_enabled_(
          base::FeatureList::IsEnabled(kBestEffortPriorityForFindInPage)) {
  if (best_effort_budget_experiment_enabled_) {
    task_priority_ = TaskPriority::kBestEffortPriority;
  } else {
    task_priority_ = kFindInPageBudgetNotExhaustedPriority;
  }

  base::TimeTicks now = scheduler_->GetTickClock()->NowTicks();
  find_in_page_budget_pool_ = std::make_unique<CPUTimeBudgetPool>(
      "FindInPageBudgetPool", &scheduler_->tracing_controller_, now);
  find_in_page_budget_pool_->SetMaxBudgetLevel(now, kFindInPageMaxBudget);
  find_in_page_budget_pool_->SetTimeBudgetRecoveryRate(
      now, kFindInPageBudgetRecoveryRate);
}

FindInPageBudgetPoolController::~FindInPageBudgetPoolController() = default;

void FindInPageBudgetPoolController::OnTaskCompleted(
    MainThreadTaskQueue* queue,
    TaskQueue::TaskTiming* task_timing) {
  if (!queue || best_effort_budget_experiment_enabled_)
    return;
  DCHECK(find_in_page_budget_pool_);
  if (queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kFindInPage) {
    find_in_page_budget_pool_->RecordTaskRunTime(task_timing->start_time(),
                                                 task_timing->end_time());
  }

  bool is_exhausted =
      !find_in_page_budget_pool_->CanRunTasksAt(task_timing->end_time());
  TaskPriority task_priority = is_exhausted
                                   ? kFindInPageBudgetExhaustedPriority
                                   : kFindInPageBudgetNotExhaustedPriority;

  if (task_priority != task_priority_) {
    task_priority_ = task_priority;
    // If the priority changed, we need to make sure all find-in-page task
    // queues across all frames get updated. Note that UpdatePolicy will
    // update all task queues for all frames, which is a bit overkill - this
    // should probably be optimized in the future.
    scheduler_->UpdatePolicy();
  }
}

}  // namespace scheduler
}  // namespace blink
