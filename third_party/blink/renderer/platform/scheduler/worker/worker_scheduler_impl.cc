// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_web_scheduling_task_queue_impl.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

std::unique_ptr<WorkerScheduler> WorkerScheduler::CreateWorkerScheduler(
    WorkerThreadScheduler* worker_thread_scheduler,
    WorkerSchedulerProxy* proxy) {
  return std::make_unique<WorkerSchedulerImpl>(worker_thread_scheduler, proxy);
}

WorkerSchedulerImpl::PauseHandleImpl::PauseHandleImpl(
    base::WeakPtr<WorkerSchedulerImpl> scheduler)
    : scheduler_(scheduler) {
  scheduler_->PauseImpl();
}

WorkerSchedulerImpl::PauseHandleImpl::~PauseHandleImpl() {
  if (scheduler_)
    scheduler_->ResumeImpl();
}

WorkerSchedulerImpl::WorkerSchedulerImpl(
    WorkerThreadScheduler* worker_thread_scheduler,
    WorkerSchedulerProxy* proxy)
    : throttleable_task_queue_(worker_thread_scheduler->CreateTaskQueue(
          base::sequence_manager::QueueName::WORKER_THROTTLEABLE_TQ,
          NonMainThreadTaskQueue::QueueCreationParams().SetCanBeThrottled(
              true))),
      pausable_task_queue_(worker_thread_scheduler->CreateTaskQueue(
          base::sequence_manager::QueueName::WORKER_PAUSABLE_TQ)),
      pausable_non_vt_task_queue_(worker_thread_scheduler->CreateTaskQueue(
          base::sequence_manager::QueueName::WORKER_PAUSABLE_TQ)),
      unpausable_task_queue_(worker_thread_scheduler->CreateTaskQueue(
          base::sequence_manager::QueueName::WORKER_UNPAUSABLE_TQ)),
      thread_scheduler_(worker_thread_scheduler),
      back_forward_cache_disabling_feature_tracker_(&tracing_controller_,
                                                    thread_scheduler_) {
  task_runners_.emplace(throttleable_task_queue_,
                        throttleable_task_queue_->CreateQueueEnabledVoter());
  task_runners_.emplace(pausable_task_queue_,
                        pausable_task_queue_->CreateQueueEnabledVoter());
  task_runners_.emplace(pausable_non_vt_task_queue_,
                        pausable_non_vt_task_queue_->CreateQueueEnabledVoter());
  task_runners_.emplace(unpausable_task_queue_, nullptr);

  thread_scheduler_->RegisterWorkerScheduler(this);

  SetUpThrottling();

  // |proxy| can be nullptr in unit tests.
  if (proxy)
    proxy->OnWorkerSchedulerCreated(GetWeakPtr());
}

base::WeakPtr<WorkerSchedulerImpl> WorkerSchedulerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

WorkerSchedulerImpl::~WorkerSchedulerImpl() {
  DCHECK(is_disposed_);
  DCHECK_EQ(0u, paused_count_);
}

WorkerThreadScheduler* WorkerSchedulerImpl::GetWorkerThreadScheduler() const {
  return thread_scheduler_;
}

std::unique_ptr<WorkerSchedulerImpl::PauseHandle> WorkerSchedulerImpl::Pause() {
  thread_scheduler_->GetHelper().CheckOnValidThread();
  if (is_disposed_)
    return nullptr;
  return std::make_unique<PauseHandleImpl>(GetWeakPtr());
}

void WorkerSchedulerImpl::PauseImpl() {
  thread_scheduler_->GetHelper().CheckOnValidThread();
  paused_count_++;
  if (paused_count_ == 1) {
    for (const auto& pair : task_runners_) {
      if (pair.second) {
        pair.second->SetVoteToEnable(false);
      }
    }
  }
}

void WorkerSchedulerImpl::ResumeImpl() {
  thread_scheduler_->GetHelper().CheckOnValidThread();
  paused_count_--;
  if (paused_count_ == 0 && !is_disposed_) {
    for (const auto& pair : task_runners_) {
      if (pair.second) {
        pair.second->SetVoteToEnable(true);
      }
    }
  }
}

void WorkerSchedulerImpl::SetUpThrottling() {
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

SchedulingLifecycleState WorkerSchedulerImpl::CalculateLifecycleState(
    ObserverType) const {
  return thread_scheduler_->lifecycle_state();
}

void WorkerSchedulerImpl::Dispose() {
  thread_scheduler_->UnregisterWorkerScheduler(this);

  for (const auto& pair : task_runners_) {
    pair.first->ShutdownTaskQueue();
  }

  task_runners_.clear();

  is_disposed_ = true;
}

scoped_refptr<base::SingleThreadTaskRunner> WorkerSchedulerImpl::GetTaskRunner(
    TaskType type) const {
  switch (type) {
    case TaskType::kJavascriptTimerImmediate:
    case TaskType::kJavascriptTimerDelayedLowNesting:
    case TaskType::kJavascriptTimerDelayedHighNesting:
    case TaskType::kPostedMessage:
    case TaskType::kWorkerAnimation:
      return throttleable_task_queue_->CreateTaskRunner(type);
    case TaskType::kNetworking:
    case TaskType::kNetworkingControl:
    case TaskType::kWebSocket:
    case TaskType::kInternalLoading:
      return pausable_non_vt_task_queue_->CreateTaskRunner(type);
    case TaskType::kDOMManipulation:
    case TaskType::kUserInteraction:
    case TaskType::kLowPriorityScriptExecution:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kMediaElementEvent:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kMicrotask:
    case TaskType::kRemoteEvent:
    case TaskType::kUnshippedPortMessage:
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
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalUserInteraction:
    case TaskType::kInternalIntersectionObserver:
    case TaskType::kInternalNavigationAssociated:
    case TaskType::kInternalNavigationCancellation:
    case TaskType::kInternalContinueScriptLoading:
    case TaskType::kWakeLock:
    case TaskType::kStorage:
    case TaskType::kClipboard:
    case TaskType::kMachineLearning:
      // UnthrottledTaskRunner is generally discouraged in future.
      // TODO(nhiroki): Identify which tasks can be throttled / suspendable and
      // move them into other task runners. See also comments in
      // Get(LocalFrame). (https://crbug.com/670534)
      return pausable_task_queue_->CreateTaskRunner(type);
    case TaskType::kFileReading:
      return pausable_non_vt_task_queue_->CreateTaskRunner(type);
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
    case TaskType::kNetworkingUnfreezableRenderBlockingLoading:
      return IsInflightNetworkRequestBackForwardCacheSupportEnabled()
                 ? unpausable_task_queue_->CreateTaskRunner(type)
                 : pausable_non_vt_task_queue_->CreateTaskRunner(type);
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueV8UserVisible:
    case TaskType::kMainThreadTaskQueueV8BestEffort:
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void WorkerSchedulerImpl::OnLifecycleStateChanged(
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

void WorkerSchedulerImpl::InitializeOnWorkerThread(Delegate* delegate) {
  DCHECK(delegate);
  back_forward_cache_disabling_feature_tracker_.SetDelegate(delegate);
}

VirtualTimeController* WorkerSchedulerImpl::GetVirtualTimeController() {
  return thread_scheduler_;
}

scoped_refptr<NonMainThreadTaskQueue>
WorkerSchedulerImpl::UnpausableTaskQueue() {
  return unpausable_task_queue_.get();
}

scoped_refptr<NonMainThreadTaskQueue> WorkerSchedulerImpl::PausableTaskQueue() {
  return pausable_task_queue_.get();
}

scoped_refptr<NonMainThreadTaskQueue>
WorkerSchedulerImpl::ThrottleableTaskQueue() {
  return throttleable_task_queue_.get();
}

void WorkerSchedulerImpl::OnStartedUsingNonStickyFeature(
    SchedulingPolicy::Feature feature,
    const SchedulingPolicy& policy,
    std::unique_ptr<SourceLocation> source_location,
    SchedulingAffectingFeatureHandle* handle) {
  if (policy.disable_align_wake_ups) {
    scheduler::DisableAlignWakeUpsForProcess();
  }

  if (!policy.disable_back_forward_cache) {
    return;
  }
  back_forward_cache_disabling_feature_tracker_.AddNonStickyFeature(
      feature, std::move(source_location), handle);
}

void WorkerSchedulerImpl::OnStartedUsingStickyFeature(
    SchedulingPolicy::Feature feature,
    const SchedulingPolicy& policy,
    std::unique_ptr<SourceLocation> source_location) {
  if (policy.disable_align_wake_ups) {
    scheduler::DisableAlignWakeUpsForProcess();
  }

  if (!policy.disable_back_forward_cache) {
    return;
  }
  back_forward_cache_disabling_feature_tracker_.AddStickyFeature(
      feature, std::move(source_location));
}

void WorkerSchedulerImpl::OnStoppedUsingNonStickyFeature(
    SchedulingAffectingFeatureHandle* handle) {
  if (!handle->GetPolicy().disable_back_forward_cache) {
    return;
  }
  back_forward_cache_disabling_feature_tracker_.Remove(
      handle->GetFeatureAndJSLocationBlockingBFCache());
}

base::WeakPtr<FrameOrWorkerScheduler>
WorkerSchedulerImpl::GetFrameOrWorkerSchedulerWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<WebSchedulingTaskQueue>
WorkerSchedulerImpl::CreateWebSchedulingTaskQueue(
    WebSchedulingQueueType queue_type,
    WebSchedulingPriority priority) {
  scoped_refptr<NonMainThreadTaskQueue> task_queue =
      thread_scheduler_->CreateTaskQueue(
          base::sequence_manager::QueueName::WORKER_WEB_SCHEDULING_TQ,
          NonMainThreadTaskQueue::QueueCreationParams()
              .SetWebSchedulingQueueType(queue_type)
              .SetWebSchedulingPriority(priority));
  return std::make_unique<NonMainThreadWebSchedulingTaskQueueImpl>(
      std::move(task_queue));
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerSchedulerImpl::CompositorTaskRunner() {
  return thread_scheduler_->CompositorTaskRunner();
}

WebScopedVirtualTimePauser
WorkerSchedulerImpl::CreateWebScopedVirtualTimePauser(
    const String& name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return thread_scheduler_->CreateWebScopedVirtualTimePauser(name, duration);
}

void WorkerSchedulerImpl::PauseVirtualTime() {
  for (auto& [queue, voter] : task_runners_) {
    // A queue without the voter is treated as unpausable. There's only one
    // at the time of writing, AKA `unpausable_task_queue_`, but we may have
    // more than one eventually as other schedulers do, so just check for voter.
    if (queue == pausable_non_vt_task_queue_.get() || !voter) {
      continue;
    }
    queue->GetTaskQueue()->InsertFence(TaskQueue::InsertFencePosition::kNow);
  }
}

void WorkerSchedulerImpl::UnpauseVirtualTime() {
  for (auto& [queue, voter] : task_runners_) {
    // This needs to match the logic of `PauseVirtualTime()`, see comment there.
    if (queue == pausable_non_vt_task_queue_.get() || !voter) {
      continue;
    }
    DCHECK(queue->GetTaskQueue()->HasActiveFence());
    queue->GetTaskQueue()->RemoveFence();
  }
}

}  // namespace scheduler
}  // namespace blink
