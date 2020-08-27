// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/compositor_priority_experiments.h"

#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

CompositorPriorityExperiments::CompositorPriorityExperiments(
    MainThreadSchedulerImpl* scheduler)
    : scheduler_(scheduler),
      experiment_(GetExperimentFromFeatureList()),
      last_compositor_task_time_(scheduler_->GetTickClock()->NowTicks()),
      prioritize_compositing_after_delay_length_(
          base::TimeDelta::FromMilliseconds(kCompositingDelayLength.Get())),
      stop_signal_(base::FeatureList::IsEnabled(
                       kPrioritizeCompositingUntilBeginMainFrame)
                       ? StopSignalType::kBeginMainFrameTask
                       : StopSignalType::kAnyCompositorTask) {}
CompositorPriorityExperiments::~CompositorPriorityExperiments() = default;

CompositorPriorityExperiments::Experiment
CompositorPriorityExperiments::GetExperimentFromFeatureList() {
  if (base::FeatureList::IsEnabled(kVeryHighPriorityForCompositingAlways)) {
    return Experiment::kVeryHighPriorityForCompositingAlways;
  } else if (base::FeatureList::IsEnabled(
                 kVeryHighPriorityForCompositingWhenFast)) {
    return Experiment::kVeryHighPriorityForCompositingWhenFast;
  } else if (base::FeatureList::IsEnabled(
                 kVeryHighPriorityForCompositingAlternating)) {
    return Experiment::kVeryHighPriorityForCompositingAlternating;
  } else if (base::FeatureList::IsEnabled(
                 kVeryHighPriorityForCompositingBudget)) {
    return Experiment::kVeryHighPriorityForCompositingBudget;
  } else if (base::FeatureList::IsEnabled(
                 kVeryHighPriorityForCompositingAfterDelay)) {
    return Experiment::kVeryHighPriorityForCompositingAfterDelay;
  } else {
    return Experiment::kNone;
  }
}

bool CompositorPriorityExperiments::IsExperimentActive() const {
  if (experiment_ == Experiment::kNone)
    return false;
  return true;
}

QueuePriority CompositorPriorityExperiments::GetCompositorPriority() const {
  switch (experiment_) {
    case Experiment::kVeryHighPriorityForCompositingAlways:
      return QueuePriority::kVeryHighPriority;
    case Experiment::kVeryHighPriorityForCompositingWhenFast:
      if (scheduler_->main_thread_compositing_is_fast()) {
        return QueuePriority::kVeryHighPriority;
      }
      return QueuePriority::kNormalPriority;
    case Experiment::kVeryHighPriorityForCompositingAlternating:
      return alternating_compositor_priority_;
    case Experiment::kVeryHighPriorityForCompositingAfterDelay:
      return delay_compositor_priority_;
    case Experiment::kVeryHighPriorityForCompositingBudget:
      return budget_compositor_priority_;
    case Experiment::kNone:
      NOTREACHED();
      return QueuePriority::kNormalPriority;
  }
}

void CompositorPriorityExperiments::OnMainThreadSchedulerInitialized() {
  if (experiment_ == Experiment::kVeryHighPriorityForCompositingBudget) {
    budget_pool_controller_.reset(new CompositorBudgetPoolController(
        this, scheduler_, scheduler_->CompositorTaskQueue().get(),
        &scheduler_->tracing_controller_,
        base::TimeDelta::FromMilliseconds(
            kInitialCompositorBudgetInMilliseconds.Get()),
        kCompositorBudgetRecoveryRate.Get()));
  }
}

void CompositorPriorityExperiments::OnMainThreadSchedulerShutdown() {
  budget_pool_controller_.reset();
}

void CompositorPriorityExperiments::OnWillBeginMainFrame() {
  will_begin_main_frame_ = true;
}

void CompositorPriorityExperiments::OnTaskCompleted(
    MainThreadTaskQueue* queue,
    QueuePriority current_compositor_priority,
    MainThreadTaskQueue::TaskTiming* task_timing) {
  if (!queue)
    return;

  bool have_seen_stop_signal = false;
  if (queue->queue_type() == MainThreadTaskQueue::QueueType::kCompositor) {
    if (stop_signal_ == StopSignalType::kAnyCompositorTask) {
      have_seen_stop_signal = true;
    } else if (will_begin_main_frame_) {
      have_seen_stop_signal = true;
      will_begin_main_frame_ = false;
    }
  }

  switch (experiment_) {
    case Experiment::kVeryHighPriorityForCompositingAlways:
      return;
    case Experiment::kVeryHighPriorityForCompositingWhenFast:
      scheduler_->OnCompositorPriorityExperimentUpdateCompositorPriority();
      return;
    case Experiment::kVeryHighPriorityForCompositingAlternating:
      // Deprioritize the compositor if it has just run a task. Prioritize the
      // compositor if another task has run regardless of its priority. This
      // prevents starving the compositor while allowing other work to run
      // in-between.
      if (have_seen_stop_signal && alternating_compositor_priority_ ==
                                       QueuePriority::kVeryHighPriority) {
        alternating_compositor_priority_ = QueuePriority::kNormalPriority;
        scheduler_->OnCompositorPriorityExperimentUpdateCompositorPriority();
      } else if (alternating_compositor_priority_ ==
                 QueuePriority::kNormalPriority) {
        alternating_compositor_priority_ = QueuePriority::kVeryHighPriority;
        scheduler_->OnCompositorPriorityExperimentUpdateCompositorPriority();
      }
      return;
    case Experiment::kVeryHighPriorityForCompositingAfterDelay:
      if (have_seen_stop_signal) {
        delay_compositor_priority_ = QueuePriority::kNormalPriority;
        last_compositor_task_time_ = task_timing->end_time();
      } else {
        if (task_timing->end_time() - last_compositor_task_time_ >=
            prioritize_compositing_after_delay_length_) {
          delay_compositor_priority_ = QueuePriority::kVeryHighPriority;
        }
      }
      if (current_compositor_priority != delay_compositor_priority_)
        scheduler_->OnCompositorPriorityExperimentUpdateCompositorPriority();
      return;
    case Experiment::kVeryHighPriorityForCompositingBudget:
      budget_pool_controller_->OnTaskCompleted(queue, task_timing,
                                               have_seen_stop_signal);
      return;
    case Experiment::kNone:
      return;
  }
}

void CompositorPriorityExperiments::OnBudgetExhausted() {
  // Compositor will still be allowed to run tasks if the budget is exhausted
  // if there is no other higher priority work to be done. If a compositor task
  // is run while the budget is exhausted it will still deplete the budget which
  // will keep it at a normal priority for longer.
  budget_compositor_priority_ = QueuePriority::kNormalPriority;
  scheduler_->OnCompositorPriorityExperimentUpdateCompositorPriority();
}

void CompositorPriorityExperiments::OnBudgetReplenished() {
  budget_compositor_priority_ = QueuePriority::kVeryHighPriority;
  scheduler_->OnCompositorPriorityExperimentUpdateCompositorPriority();
}

CompositorPriorityExperiments::CompositorBudgetPoolController::
    CompositorBudgetPoolController(
        CompositorPriorityExperiments* experiment,
        MainThreadSchedulerImpl* scheduler,
        MainThreadTaskQueue* compositor_queue,
        TraceableVariableController* tracing_controller,
        base::TimeDelta min_budget,
        double budget_recovery_rate)
    : experiment_(experiment) {
  DCHECK_EQ(compositor_queue->queue_type(),
            MainThreadTaskQueue::QueueType::kCompositor);
  base::TimeTicks now = scheduler->GetTickClock()->NowTicks();

  compositor_budget_pool_.reset(new CPUTimeBudgetPool(
      "CompositorBudgetPool", this, tracing_controller, now));
  compositor_budget_pool_->SetMinBudgetLevelToRun(now, min_budget);
  compositor_budget_pool_->SetTimeBudgetRecoveryRate(now, budget_recovery_rate);
  compositor_budget_pool_->AddQueue(now, compositor_queue);
}

CompositorPriorityExperiments::CompositorBudgetPoolController::
    ~CompositorBudgetPoolController() = default;

void CompositorPriorityExperiments::CompositorBudgetPoolController::
    UpdateQueueSchedulingLifecycleState(base::TimeTicks now, TaskQueue* queue) {
  UpdateCompositorBudgetState(now);
}

void CompositorPriorityExperiments::CompositorBudgetPoolController::
    UpdateCompositorBudgetState(base::TimeTicks now) {
  bool is_exhausted =
      !compositor_budget_pool_->CanRunTasksAt(now, false /* is_wake_up */);
  if (is_exhausted_ == is_exhausted)
    return;

  is_exhausted_ = is_exhausted;
  if (is_exhausted_) {
    experiment_->OnBudgetExhausted();
    return;
  }
  experiment_->OnBudgetReplenished();
}

void CompositorPriorityExperiments::CompositorBudgetPoolController::
    OnTaskCompleted(MainThreadTaskQueue* queue,
                    MainThreadTaskQueue::TaskTiming* task_timing,
                    bool have_seen_stop_signal) {
  if (have_seen_stop_signal) {
    compositor_budget_pool_->RecordTaskRunTime(queue, task_timing->start_time(),
                                               task_timing->end_time());
  }
  UpdateCompositorBudgetState(task_timing->end_time());
}

}  // namespace scheduler
}  // namespace blink
