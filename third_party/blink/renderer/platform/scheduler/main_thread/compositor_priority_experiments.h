// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_COMPOSITOR_PRIORITY_EXPERIMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_COMPOSITOR_PRIORITY_EXPERIMENTS_H_

#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/cancelable_closure_holder.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace scheduler {

using TaskQueue = base::sequence_manager::TaskQueue;
using QueuePriority = base::sequence_manager::TaskQueue::QueuePriority;

class CPUTimeBudgetPool;
class MainThreadSchedulerImpl;
class MainThreadTaskQueue;

class PLATFORM_EXPORT CompositorPriorityExperiments {
  DISALLOW_NEW();

 public:
  explicit CompositorPriorityExperiments(MainThreadSchedulerImpl* scheduler);
  ~CompositorPriorityExperiments();

  enum class Experiment {
    kNone,
    kVeryHighPriorityForCompositingAlways,
    kVeryHighPriorityForCompositingWhenFast,
    kVeryHighPriorityForCompositingAlternating,
    kVeryHighPriorityForCompositingAfterDelay,
    kVeryHighPriorityForCompositingBudget
  };

  bool IsExperimentActive() const;

  QueuePriority GetCompositorPriority() const;

  void OnTaskCompleted(MainThreadTaskQueue* queue,
                       QueuePriority current_priority,
                       TaskQueue::TaskTiming* task_timing);

  QueuePriority GetAlternatingPriority() const {
    return alternating_compositor_priority_;
  }

  void OnWillBeginMainFrame();

  void OnMainThreadSchedulerInitialized();
  void OnMainThreadSchedulerShutdown();

  void OnBudgetExhausted();
  void OnBudgetReplenished();

 private:
  class CompositorBudgetPoolController : public BudgetPoolController {
   public:
    explicit CompositorBudgetPoolController(
        CompositorPriorityExperiments* experiment,
        MainThreadSchedulerImpl* scheduler,
        MainThreadTaskQueue* compositor_queue,
        TraceableVariableController* tracing_controller,
        base::TimeDelta min_budget,
        double budget_recovery_rate);
    ~CompositorBudgetPoolController() override;

    void UpdateQueueSchedulingLifecycleState(base::TimeTicks now,
                                             TaskQueue* queue) override;

    void UpdateCompositorBudgetState(base::TimeTicks now);

    void OnTaskCompleted(MainThreadTaskQueue* queue,
                         TaskQueue::TaskTiming* task_timing,
                         bool have_seen_stop_signal);

    // Unimplemented methods.
    void AddQueueToBudgetPool(TaskQueue* queue,
                              BudgetPool* budget_pool) override {}
    void RemoveQueueFromBudgetPool(TaskQueue* queue,
                                   BudgetPool* budget_pool) override {}
    void UnregisterBudgetPool(BudgetPool* budget_pool) override {}
    bool IsThrottled(TaskQueue* queue) const override { return false; }

   private:
    CompositorPriorityExperiments* experiment_;
    std::unique_ptr<CPUTimeBudgetPool> compositor_budget_pool_;
    bool is_exhausted_ = false;
  };

  static Experiment GetExperimentFromFeatureList();

  enum class StopSignalType { kAnyCompositorTask, kBeginMainFrameTask };

  MainThreadSchedulerImpl* scheduler_;  // Not owned.

  const Experiment experiment_;

  QueuePriority alternating_compositor_priority_ =
      QueuePriority::kVeryHighPriority;

  QueuePriority delay_compositor_priority_ = QueuePriority::kNormalPriority;
  base::TimeTicks last_compositor_task_time_;
  base::TimeDelta prioritize_compositing_after_delay_length_;

  QueuePriority budget_compositor_priority_ = QueuePriority::kVeryHighPriority;
  std::unique_ptr<CompositorBudgetPoolController> budget_pool_controller_;

  const StopSignalType stop_signal_;
  bool will_begin_main_frame_ = false;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_COMPOSITOR_PRIORITY_EXPERIMENTS_H_
