// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_web_scheduling_task_queue_impl.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

WorkerScheduler::PauseHandle::PauseHandle(
    base::WeakPtr<WorkerScheduler> scheduler)
    : scheduler_(scheduler) {
  scheduler_->PauseImpl();
}

WorkerScheduler::PauseHandle::~PauseHandle() {
  if (scheduler_)
    scheduler_->ResumeImpl();
}

WorkerScheduler::WorkerScheduler(WorkerThreadScheduler* worker_thread_scheduler,
                                 WorkerSchedulerProxy* proxy)
    : throttleable_task_queue_(
          worker_thread_scheduler->CreateTaskQueue("worker_throttleable_tq",
                                                   true)),
      pausable_task_queue_(
          worker_thread_scheduler->CreateTaskQueue("worker_pausable_tq")),
      unpausable_task_queue_(
          worker_thread_scheduler->CreateTaskQueue("worker_unpausable_tq")),
      thread_scheduler_(worker_thread_scheduler) {
  task_runners_.emplace(throttleable_task_queue_,
                        throttleable_task_queue_->CreateQueueEnabledVoter());
  task_runners_.emplace(pausable_task_queue_,
                        pausable_task_queue_->CreateQueueEnabledVoter());
  task_runners_.emplace(unpausable_task_queue_, nullptr);

  thread_scheduler_->RegisterWorkerScheduler(this);

  SetUpThrottling();

  // |proxy| can be nullptr in unit tests.
  if (proxy)
    proxy->OnWorkerSchedulerCreated(GetWeakPtr());
}

base::WeakPtr<WorkerScheduler> WorkerScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

WorkerScheduler::~WorkerScheduler() {
  DCHECK(is_disposed_);
  DCHECK_EQ(0u, paused_count_);
}

std::unique_ptr<WorkerScheduler::PauseHandle> WorkerScheduler::Pause() {
  thread_scheduler_->helper()->CheckOnValidThread();
  if (is_disposed_)
    return nullptr;
  return std::make_unique<PauseHandle>(GetWeakPtr());
}

void WorkerScheduler::PauseImpl() {
  thread_scheduler_->helper()->CheckOnValidThread();
  paused_count_++;
  if (paused_count_ == 1) {
    for (const auto& pair : task_runners_) {
      if (pair.second) {
        pair.second->SetVoteToEnable(false);
      }
    }
  }
}

void WorkerScheduler::ResumeImpl() {
  thread_scheduler_->helper()->CheckOnValidThread();
  paused_count_--;
  if (paused_count_ == 0 && !is_disposed_) {
    for (const auto& pair : task_runners_) {
      if (pair.second) {
        pair.second->SetVoteToEnable(true);
      }
    }
  }
}

void WorkerScheduler::SetUpThrottling() {
  if (!thread_scheduler_->wake_up_budget_pool() &&
      !thread_scheduler_->cpu_time_budget_pool()) {
    return;
  }
  base::TimeTicks now = thread_scheduler_->GetTickClock()->NowTicks();

  WakeUpBudgetPool* wake_up_budget_pool =
      thread_scheduler_->wake_up_budget_pool();
  CPUTimeBudgetPool* cpu_time_budget_pool =
      thread_scheduler_->cpu_time_budget_pool();

  if (wake_up_budget_pool) {
    throttleable_task_queue_->AddToBudgetPool(now, wake_up_budget_pool);
  }
  if (cpu_time_budget_pool) {
    throttleable_task_queue_->AddToBudgetPool(now, cpu_time_budget_pool);
  }
}

SchedulingLifecycleState WorkerScheduler::CalculateLifecycleState(
    ObserverType) const {
  return thread_scheduler_->lifecycle_state();
}

void WorkerScheduler::Dispose() {
  thread_scheduler_->UnregisterWorkerScheduler(this);

  for (const auto& pair : task_runners_) {
    pair.first->ShutdownTaskQueue();
  }

  task_runners_.clear();

  is_disposed_ = true;
}

scoped_refptr<base::SingleThreadTaskRunner> WorkerScheduler::GetTaskRunner(
    TaskType type) const {
  switch (type) {
    case TaskType::kJavascriptTimerImmediate:
    case TaskType::kJavascriptTimerDelayedLowNesting:
    case TaskType::kJavascriptTimerDelayedHighNesting:
    case TaskType::kPostedMessage:
    case TaskType::kWorkerAnimation:
      return throttleable_task_queue_->CreateTaskRunner(type);
    case TaskType::kDOMManipulation:
    case TaskType::kUserInteraction:
    case TaskType::kNetworking:
    case TaskType::kNetworkingWithURLLoaderAnnotation:
    case TaskType::kNetworkingControl:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kMediaElementEvent:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kMicrotask:
    case TaskType::kRemoteEvent:
    case TaskType::kWebSocket:
    case TaskType::kUnshippedPortMessage:
    case TaskType::kFileReading:
    case TaskType::kDatabaseAccess:
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kWebGPU:
    case TaskType::kIdleTask:
    case TaskType::kMiscPlatformAPI:
    case TaskType::kFontLoading:
    case TaskType::kApplicationLifeCycle:
    case TaskType::kBackgroundFetch:
    case TaskType::kPermission:
    case TaskType::kInternalDefault:
    case TaskType::kInternalLoading:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalUserInteraction:
    case TaskType::kInternalIntersectionObserver:
    case TaskType::kInternalNavigationAssociated:
    case TaskType::kInternalContinueScriptLoading:
    case TaskType::kWakeLock:
      // UnthrottledTaskRunner is generally discouraged in future.
      // TODO(nhiroki): Identify which tasks can be throttled / suspendable and
      // move them into other task runners. See also comments in
      // Get(LocalFrame). (https://crbug.com/670534)
      return pausable_task_queue_->CreateTaskRunner(type);
    case TaskType::kDeprecatedNone:
    case TaskType::kInternalInspector:
    case TaskType::kInternalTest:
    case TaskType::kInternalNavigationAssociatedUnfreezable:
      // kWebLocks can be frozen if for entire page, but not for individual
      // frames. See https://crrev.com/c/1687716
    case TaskType::kWebLocks:
      // UnthrottledTaskRunner is generally discouraged in future.
      // TODO(nhiroki): Identify which tasks can be throttled / suspendable and
      // move them into other task runners. See also comments in
      // Get(LocalFrame). (https://crbug.com/670534)
      return unpausable_task_queue_->CreateTaskRunner(type);
    case TaskType::kNetworkingUnfreezable:
      return IsInflightNetworkRequestBackForwardCacheSupportEnabled()
                 ? unpausable_task_queue_->CreateTaskRunner(type)
                 : pausable_task_queue_->CreateTaskRunner(type);
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueDefault:
    case TaskType::kMainThreadTaskQueueInput:
    case TaskType::kMainThreadTaskQueueIdle:
    case TaskType::kMainThreadTaskQueueControl:
    case TaskType::kMainThreadTaskQueueMemoryPurge:
    case TaskType::kMainThreadTaskQueueNonWaking:
    case TaskType::kCompositorThreadTaskQueueDefault:
    case TaskType::kCompositorThreadTaskQueueInput:
    case TaskType::kWorkerThreadTaskQueueDefault:
    case TaskType::kWorkerThreadTaskQueueV8:
    case TaskType::kWorkerThreadTaskQueueCompositor:
    case TaskType::kInternalTranslation:
    case TaskType::kServiceWorkerClientMessage:
    case TaskType::kInternalContentCapture:
    case TaskType::kWebSchedulingPostedTask:
    case TaskType::kInternalFrameLifecycleControl:
    case TaskType::kInternalFindInPage:
    case TaskType::kInternalHighPriorityLocalFrame:
    case TaskType::kInternalInputBlocking:
    case TaskType::kMainThreadTaskQueueIPCTracking:
    case TaskType::kInternalPostMessageForwarding:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

void WorkerScheduler::OnLifecycleStateChanged(
    SchedulingLifecycleState lifecycle_state) {
  if (lifecycle_state_ == lifecycle_state)
    return;
  lifecycle_state_ = lifecycle_state;
  thread_scheduler_->OnLifecycleStateChanged(lifecycle_state);

  if (thread_scheduler_->cpu_time_budget_pool() ||
      thread_scheduler_->wake_up_budget_pool()) {
    if (lifecycle_state_ == SchedulingLifecycleState::kThrottled) {
      throttleable_task_queue_->IncreaseThrottleRefCount();
    } else {
      throttleable_task_queue_->DecreaseThrottleRefCount();
    }
  }
  NotifyLifecycleObservers();
}

scoped_refptr<NonMainThreadTaskQueue> WorkerScheduler::UnpausableTaskQueue() {
  return unpausable_task_queue_.get();
}

scoped_refptr<NonMainThreadTaskQueue> WorkerScheduler::PausableTaskQueue() {
  return pausable_task_queue_.get();
}

scoped_refptr<NonMainThreadTaskQueue> WorkerScheduler::ThrottleableTaskQueue() {
  return throttleable_task_queue_.get();
}

void WorkerScheduler::OnStartedUsingFeature(SchedulingPolicy::Feature feature,
                                            const SchedulingPolicy& policy) {}

void WorkerScheduler::OnStoppedUsingFeature(SchedulingPolicy::Feature feature,
                                            const SchedulingPolicy& policy) {}

std::unique_ptr<WebSchedulingTaskQueue>
WorkerScheduler::CreateWebSchedulingTaskQueue(WebSchedulingPriority priority) {
  scoped_refptr<NonMainThreadTaskQueue> task_queue =
      thread_scheduler_->CreateTaskQueue("worker_web_scheduling_tq");
  task_queue->SetWebSchedulingPriority(priority);
  return std::make_unique<NonMainThreadWebSchedulingTaskQueueImpl>(
      std::move(task_queue));
}

}  // namespace scheduler
}  // namespace blink
