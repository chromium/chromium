// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_THREAD_SCHEDULER_H_

#include "base/single_thread_task_runner.h"
#include "components/scheduling_metrics/task_duration_metric_reporter.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/single_thread_idle_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/compositor_metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_impl.h"

namespace base {
class TaskObserver;
}

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT CompositorThreadScheduler
    : public NonMainThreadSchedulerImpl,
      public SingleThreadIdleTaskRunner::Delegate {
 public:
  explicit CompositorThreadScheduler(
      base::sequence_manager::SequenceManager* sequence_manager);
  CompositorThreadScheduler(const CompositorThreadScheduler&) = delete;
  CompositorThreadScheduler& operator=(const CompositorThreadScheduler&) =
      delete;

  ~CompositorThreadScheduler() override;

  // NonMainThreadSchedulerImpl:
  scoped_refptr<NonMainThreadTaskQueue> DefaultTaskQueue() override;
  void OnTaskCompleted(
      NonMainThreadTaskQueue* worker_task_queue,
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::sequence_manager::LazyNow* lazy_now) override;

  // WebThreadScheduler:
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> NonWakingTaskRunner() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::TaskObserver* task_observer) override;
  void RemoveTaskObserver(base::TaskObserver* task_observer) override;
  void AddRAILModeObserver(RAILModeObserver*) override {}
  void RemoveRAILModeObserver(RAILModeObserver const*) override {}
  void Shutdown() override;

  // ThreadSchedulerImpl:
  scoped_refptr<scheduler::SingleThreadIdleTaskRunner> IdleTaskRunner()
      override;

  // SingleThreadIdleTaskRunner::Delegate:
  void OnIdleTaskPosted() override;
  base::TimeTicks WillProcessIdleTask() override;
  void DidProcessIdleTask() override;
  base::TimeTicks NowTicks() override;

 private:
  CompositorMetricsHelper compositor_metrics_helper_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_THREAD_SCHEDULER_H_
