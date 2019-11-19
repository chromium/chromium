// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_SCHEDULER_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "components/scheduling_metrics/task_duration_metric_reporter.h"
#include "third_party/blink/renderer/platform/scheduler/common/idle_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_status.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_metrics_helper.h"

namespace base {
class TaskObserver;
namespace sequence_manager {
class SequenceManager;
}
}  // namespace base

namespace ukm {
class UkmRecorder;
}

namespace blink {
namespace scheduler {

class WorkerScheduler;
class WorkerSchedulerProxy;
class TaskQueueThrottler;
class WakeUpBudgetPool;
class CPUTimeBudgetPool;

class PLATFORM_EXPORT WorkerThreadScheduler : public NonMainThreadSchedulerImpl,
                                              public IdleHelper::Delegate {
 public:
  // |sequence_manager| and |proxy| must remain valid for the entire lifetime of
  // this object.
  WorkerThreadScheduler(
      ThreadType thread_type,
      base::sequence_manager::SequenceManager* sequence_manager,
      WorkerSchedulerProxy* proxy);
  ~WorkerThreadScheduler() override;

  // WebThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::TaskObserver* task_observer) override;
  void RemoveTaskObserver(base::TaskObserver* task_observer) override;
  void AddRAILModeObserver(RAILModeObserver*) override {}
  void RemoveRAILModeObserver(RAILModeObserver const*) override {}
  void Shutdown() override;

  // ThreadSchedulerImpl implementation:
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;

  // NonMainThreadSchedulerImpl implementation:
  scoped_refptr<NonMainThreadTaskQueue> DefaultTaskQueue() override;
  void OnTaskCompleted(
      NonMainThreadTaskQueue* worker_task_queue,
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::sequence_manager::LazyNow* lazy_now) override;

  SchedulerHelper* GetSchedulerHelperForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;

  // Virtual for test.
  virtual void OnLifecycleStateChanged(
      SchedulingLifecycleState lifecycle_state);

  SchedulingLifecycleState lifecycle_state() const { return lifecycle_state_; }

  // Each WorkerScheduler should notify NonMainThreadSchedulerImpl when it is
  // created or destroyed.
  void RegisterWorkerScheduler(WorkerScheduler* worker_scheduler);
  void UnregisterWorkerScheduler(WorkerScheduler* worker_scheduler);

  // Returns the control task queue.  Tasks posted to this queue are executed
  // with the highest priority. Care must be taken to avoid starvation of other
  // task queues.
  scoped_refptr<NonMainThreadTaskQueue> ControlTaskQueue();

  // TaskQueueThrottler might be null if throttling is not enabled or
  // not supported.
  TaskQueueThrottler* task_queue_throttler() const {
    return task_queue_throttler_.get();
  }
  WakeUpBudgetPool* wake_up_budget_pool() const { return wake_up_budget_pool_; }
  CPUTimeBudgetPool* cpu_time_budget_pool() const {
    return cpu_time_budget_pool_;
  }

 protected:
  // NonMainThreadScheduler implementation:
  void InitImpl() override;

  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override {}
  void OnIdlePeriodEnded() override {}
  void OnPendingTasksChanged(bool new_state) override {}

  void CreateTaskQueueThrottler();

  void SetCPUTimeBudgetPoolForTesting(CPUTimeBudgetPool* cpu_time_budget_pool);

  HashSet<WorkerScheduler*>& GetWorkerSchedulersForTesting();

  void SetUkmTaskSamplingRateForTest(double rate);
  void SetUkmRecorderForTest(std::unique_ptr<ukm::UkmRecorder> ukm_recorder);

  virtual void PerformMicrotaskCheckpoint();

 private:
  void MaybeStartLongIdlePeriod();

  void RecordTaskUkm(
      NonMainThreadTaskQueue* worker_task_queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  const ThreadType thread_type_;
  IdleHelper idle_helper_;
  bool initialized_;
  scoped_refptr<NonMainThreadTaskQueue> control_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> v8_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  SchedulingLifecycleState lifecycle_state_;

  WorkerMetricsHelper worker_metrics_helper_;

  // This controller should be initialized before any TraceableVariables
  // because they require one to initialize themselves.
  TraceableVariableController traceable_variable_controller_;

  // Worker schedulers associated with this thread.
  HashSet<WorkerScheduler*> worker_schedulers_;

  std::unique_ptr<TaskQueueThrottler> task_queue_throttler_;
  // Owned by |task_queue_throttler_|.
  WakeUpBudgetPool* wake_up_budget_pool_ = nullptr;
  CPUTimeBudgetPool* cpu_time_budget_pool_ = nullptr;

  // The status of the parent frame when the worker was created.
  const FrameStatus initial_frame_status_;

  const ukm::SourceId ukm_source_id_;
  std::unique_ptr<ukm::UkmRecorder> ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThreadScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_SCHEDULER_H_
