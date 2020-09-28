// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/common/task_annotator.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/time/time.h"
#include "base/trace_event/blame_context.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/find_in_page_budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/resource_loading_task_runner_handle_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/web_scheduling_task_queue_impl.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"

namespace blink {

namespace scheduler {

using base::sequence_manager::TaskQueue;
using QueueTraits = MainThreadTaskQueue::QueueTraits;

namespace {

const char* VisibilityStateToString(bool is_visible) {
  if (is_visible) {
    return "visible";
  } else {
    return "hidden";
  }
}

const char* PausedStateToString(bool is_paused) {
  if (is_paused) {
    return "paused";
  } else {
    return "running";
  }
}

const char* FrozenStateToString(bool is_frozen) {
  if (is_frozen) {
    return "frozen";
  } else {
    return "running";
  }
}

const char* KeepActiveStateToString(bool keep_active) {
  if (keep_active) {
    return "keep_active";
  } else {
    return "no_keep_active";
  }
}

// Used to update the priority of task_queue. Note that this function is
// used for queues associated with a frame.
void UpdatePriority(MainThreadTaskQueue* task_queue) {
  if (!task_queue)
    return;

  FrameSchedulerImpl* frame_scheduler = task_queue->GetFrameScheduler();
  DCHECK(frame_scheduler);
  task_queue->SetQueuePriority(frame_scheduler->ComputePriority(task_queue));
}

}  // namespace

FrameSchedulerImpl::PauseSubresourceLoadingHandleImpl::
    PauseSubresourceLoadingHandleImpl(
        base::WeakPtr<FrameSchedulerImpl> frame_scheduler)
    : frame_scheduler_(std::move(frame_scheduler)) {
  DCHECK(frame_scheduler_);
  frame_scheduler_->AddPauseSubresourceLoadingHandle();
}

FrameSchedulerImpl::PauseSubresourceLoadingHandleImpl::
    ~PauseSubresourceLoadingHandleImpl() {
  if (frame_scheduler_)
    frame_scheduler_->RemovePauseSubresourceLoadingHandle();
}

FrameSchedulerImpl::FrameSchedulerImpl(
    PageSchedulerImpl* parent_page_scheduler,
    FrameScheduler::Delegate* delegate,
    base::trace_event::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type)
    : FrameSchedulerImpl(parent_page_scheduler->GetMainThreadScheduler(),
                         parent_page_scheduler,
                         delegate,
                         blame_context,
                         frame_type) {}

FrameSchedulerImpl::FrameSchedulerImpl(
    MainThreadSchedulerImpl* main_thread_scheduler,
    PageSchedulerImpl* parent_page_scheduler,
    FrameScheduler::Delegate* delegate,
    base::trace_event::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type)
    : frame_type_(frame_type),
      is_ad_frame_(false),
      main_thread_scheduler_(main_thread_scheduler),
      parent_page_scheduler_(parent_page_scheduler),
      delegate_(delegate),
      blame_context_(blame_context),
      throttling_state_(SchedulingLifecycleState::kNotThrottled),
      frame_visible_(true,
                     "FrameScheduler.FrameVisible",
                     &tracing_controller_,
                     VisibilityStateToString),
      frame_paused_(false,
                    "FrameScheduler.FramePaused",
                    &tracing_controller_,
                    PausedStateToString),
      frame_origin_type_(frame_type == FrameType::kMainFrame
                             ? FrameOriginType::kMainFrame
                             : FrameOriginType::kSameOriginToMainFrame,
                         "FrameScheduler.Origin",
                         &tracing_controller_,
                         FrameOriginTypeToString),
      subresource_loading_paused_(false,
                                  "FrameScheduler.SubResourceLoadingPaused",
                                  &tracing_controller_,
                                  PausedStateToString),
      url_tracer_("FrameScheduler.URL"),
      task_queues_throttled_(false,
                             "FrameScheduler.TaskQueuesThrottled",
                             &tracing_controller_,
                             YesNoStateToString),
      preempted_for_cooperative_scheduling_(
          false,
          "FrameScheduler.PreemptedForCooperativeScheduling",
          &tracing_controller_,
          YesNoStateToString),
      all_throttling_opt_out_count_(0),
      aggressive_throttling_opt_out_count_(0),
      opted_out_from_all_throttling_(false,
                                     "FrameScheduler.AllThrottlingDisabled",
                                     &tracing_controller_,
                                     YesNoStateToString),
      opted_out_from_aggressive_throttling_(
          false,
          "FrameScheduler.AggressiveThrottlingDisabled",
          &tracing_controller_,
          YesNoStateToString),
      subresource_loading_pause_count_(0u),
      opted_out_from_back_forward_cache_(
          false,
          "FrameScheduler.OptedOutFromBackForwardCache",
          &tracing_controller_,
          YesNoStateToString),
      page_frozen_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->IsFrozen() : true,
          "FrameScheduler.PageFrozen",
          &tracing_controller_,
          FrozenStateToString),
      page_visibility_for_tracing_(
          parent_page_scheduler_ && parent_page_scheduler_->IsPageVisible()
              ? PageVisibilityState::kVisible
              : PageVisibilityState::kHidden,
          "FrameScheduler.PageVisibility",
          &tracing_controller_,
          PageVisibilityStateToString),
      page_keep_active_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->KeepActive() : false,
          "FrameScheduler.KeepActive",
          &tracing_controller_,
          KeepActiveStateToString),
      waiting_for_contentful_paint_(true,
                                    "FrameScheduler.WaitingForContentfulPaint",
                                    &tracing_controller_,
                                    YesNoStateToString),
      waiting_for_meaningful_paint_(true,
                                    "FrameScheduler.WaitingForMeaningfulPaint",
                                    &tracing_controller_,
                                    YesNoStateToString) {
  frame_task_queue_controller_.reset(
      new FrameTaskQueueController(main_thread_scheduler_, this, this));
}

FrameSchedulerImpl::FrameSchedulerImpl()
    : FrameSchedulerImpl(/*main_thread_scheduler=*/nullptr,
                         /*parent_page_scheduler=*/nullptr,
                         /*delegate=*/nullptr,
                         /*blame_context=*/nullptr,
                         FrameType::kSubframe) {}

namespace {

void CleanUpQueue(MainThreadTaskQueue* queue) {
  DCHECK(queue);

  queue->DetachFromMainThreadScheduler();
  DCHECK(!queue->GetFrameScheduler());
  queue->SetBlameContext(nullptr);
}

}  // namespace

FrameSchedulerImpl::~FrameSchedulerImpl() {
  weak_factory_.InvalidateWeakPtrs();

  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      RemoveThrottleableQueueFromBudgetPools(task_queue_and_voter.first);
    }
    CleanUpQueue(task_queue_and_voter.first);
  }

  if (parent_page_scheduler_) {
    parent_page_scheduler_->Unregister(this);

    if (opted_out_from_all_throttling() ||
        opted_out_from_aggressive_throttling()) {
      parent_page_scheduler_->OnThrottlingStatusUpdated();
    }
  }
}

void FrameSchedulerImpl::DetachFromPageScheduler() {
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      RemoveThrottleableQueueFromBudgetPools(task_queue_and_voter.first);
    }
  }

  parent_page_scheduler_ = nullptr;
}

void FrameSchedulerImpl::RemoveThrottleableQueueFromBudgetPools(
    MainThreadTaskQueue* task_queue) {
  DCHECK(task_queue);
  DCHECK(task_queue->CanBeThrottled());

  if (!parent_page_scheduler_)
    return;

  CPUTimeBudgetPool* cpu_time_budget_pool =
      parent_page_scheduler_->background_cpu_time_budget_pool();

  // On tests, the scheduler helper might already be shut down and tick is not
  // available.
  base::sequence_manager::LazyNow lazy_now =
      main_thread_scheduler_->tick_clock()
          ? base::sequence_manager::LazyNow(
                main_thread_scheduler_->tick_clock())
          : base::sequence_manager::LazyNow(base::TimeTicks::Now());

  if (cpu_time_budget_pool)
    cpu_time_budget_pool->RemoveQueue(lazy_now.Now(), task_queue);

  parent_page_scheduler_->RemoveQueueFromWakeUpBudgetPool(
      task_queue, frame_origin_type_, &lazy_now);
}

void FrameSchedulerImpl::SetFrameVisible(bool frame_visible) {
  DCHECK(parent_page_scheduler_);
  if (frame_visible_ == frame_visible)
    return;
  UMA_HISTOGRAM_BOOLEAN("RendererScheduler.IPC.FrameVisibility", frame_visible);
  frame_visible_ = frame_visible;
  UpdatePolicy();
}

bool FrameSchedulerImpl::IsFrameVisible() const {
  return frame_visible_;
}

void FrameSchedulerImpl::SetCrossOriginToMainFrame(bool cross_origin) {
  DCHECK(parent_page_scheduler_);
  if (frame_origin_type_ == FrameOriginType::kMainFrame) {
    DCHECK(!cross_origin);
    return;
  }

  base::sequence_manager::LazyNow lazy_now(
      main_thread_scheduler_->tick_clock());

  // Remove throttleable TaskQueues from their current WakeUpBudgetPool.
  //
  // The WakeUpBudgetPool is selected based on origin. TaskQueues are reinserted
  // in the appropriate WakeUpBudgetPool at the end of this method, after the
  // |frame_origin_type_| is updated.
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      parent_page_scheduler_->RemoveQueueFromWakeUpBudgetPool(
          task_queue_and_voter.first, frame_origin_type_, &lazy_now);
    }
  }

  // Update the FrameOriginType.
  if (cross_origin) {
    frame_origin_type_ = FrameOriginType::kCrossOriginToMainFrame;
  } else {
    frame_origin_type_ = FrameOriginType::kSameOriginToMainFrame;
  }

  // Add throttleable TaskQueues to WakeUpBudgetPool that corresponds to the
  // updated |frame_origin_type_|.
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      parent_page_scheduler_->AddQueueToWakeUpBudgetPool(
          task_queue_and_voter.first, frame_origin_type_, &lazy_now);
    }
  }

  UpdatePolicy();
}

void FrameSchedulerImpl::SetIsAdFrame() {
  is_ad_frame_ = true;
  UpdatePolicy();
}

bool FrameSchedulerImpl::IsAdFrame() const {
  return is_ad_frame_;
}

bool FrameSchedulerImpl::IsCrossOriginToMainFrame() const {
  return frame_origin_type_ == FrameOriginType::kCrossOriginToMainFrame;
}

void FrameSchedulerImpl::TraceUrlChange(const String& url) {
  url_tracer_.TraceString(url);
}

void FrameSchedulerImpl::AddTaskTime(base::TimeDelta time) {
  // The duration of task time under which AddTaskTime buffers rather than
  // sending the task time update to the delegate.
  constexpr base::TimeDelta kTaskDurationSendThreshold =
      base::TimeDelta::FromMilliseconds(100);
  if (!delegate_)
    return;
  task_time_ += time;
  if (task_time_ >= kTaskDurationSendThreshold) {
    delegate_->UpdateTaskTime(task_time_);
    task_time_ = base::TimeDelta();
  }
}

FrameScheduler::FrameType FrameSchedulerImpl::GetFrameType() const {
  return frame_type_;
}

// static
QueueTraits FrameSchedulerImpl::CreateQueueTraitsForTaskType(TaskType type) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  // TODO(sreejakshetty): Clean up the PrioritisationType QueueTrait and
  // QueueType for kInternalContinueScriptLoading and kInternalContentCapture.
  switch (type) {
    case TaskType::kInternalContentCapture:
      return ThrottleableTaskQueueTraits().SetPrioritisationType(
          QueueTraits::PrioritisationType::kBestEffort);
    case TaskType::kJavascriptTimerDelayedLowNesting:
      return ThrottleableTaskQueueTraits()
          .SetPrioritisationType(
              QueueTraits::PrioritisationType::kJavaScriptTimer)
          .SetCanBeIntensivelyThrottled(
              IsIntensiveWakeUpThrottlingEnabled() &&
              CanIntensivelyThrottleLowNestingLevel());
    case TaskType::kJavascriptTimerDelayedHighNesting:
      return ThrottleableTaskQueueTraits()
          .SetPrioritisationType(
              QueueTraits::PrioritisationType::kJavaScriptTimer)
          .SetCanBeIntensivelyThrottled(IsIntensiveWakeUpThrottlingEnabled());
    case TaskType::kJavascriptTimerImmediate: {
      return DeferrableTaskQueueTraits()
          .SetPrioritisationType(
              QueueTraits::PrioritisationType::kJavaScriptTimer)
          .SetCanBeThrottled(!base::FeatureList::IsEnabled(
              features::kOptOutZeroTimeoutTimersFromThrottling));
    }
    case TaskType::kInternalLoading:
    case TaskType::kNetworking:
    case TaskType::kNetworkingWithURLLoaderAnnotation:
      return LoadingTaskQueueTraits();
    case TaskType::kNetworkingControl:
      return LoadingControlTaskQueueTraits();
    // Throttling following tasks may break existing web pages, so tentatively
    // these are unthrottled.
    // TODO(nhiroki): Throttle them again after we're convinced that it's safe
    // or provide a mechanism that web pages can opt-out it if throttling is not
    // desirable.
    case TaskType::kDOMManipulation:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kRemoteEvent:
    case TaskType::kWebSocket:
    case TaskType::kMicrotask:
    case TaskType::kUnshippedPortMessage:
    case TaskType::kFileReading:
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kIdleTask:
    case TaskType::kInternalDefault:
    case TaskType::kMiscPlatformAPI:
    case TaskType::kFontLoading:
    case TaskType::kApplicationLifeCycle:
    case TaskType::kBackgroundFetch:
    case TaskType::kPermission:
      // TODO(altimin): Move appropriate tasks to throttleable task queue.
      return DeferrableTaskQueueTraits();
    // PostedMessage can be used for navigation, so we shouldn't defer it
    // when expecting a user gesture.
    case TaskType::kPostedMessage:
    case TaskType::kServiceWorkerClientMessage:
    case TaskType::kWorkerAnimation:
    // UserInteraction tasks should be run even when expecting a user gesture.
    case TaskType::kUserInteraction:
    // Media events should not be deferred to ensure that media playback is
    // smooth.
    case TaskType::kMediaElementEvent:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalUserInteraction:
    case TaskType::kInternalIntersectionObserver:
      return PausableTaskQueueTraits();
    case TaskType::kInternalFindInPage:
      return FindInPageTaskQueueTraits();
    case TaskType::kInternalHighPriorityLocalFrame:
      return QueueTraits().SetPrioritisationType(
          QueueTraits::PrioritisationType::kHighPriorityLocalFrame);
    case TaskType::kInternalContinueScriptLoading:
      return PausableTaskQueueTraits().SetPrioritisationType(
          QueueTraits::PrioritisationType::kInternalScriptContinuation);
    case TaskType::kDatabaseAccess:
      if (base::FeatureList::IsEnabled(kHighPriorityDatabaseTaskType)) {
        return PausableTaskQueueTraits().SetPrioritisationType(
            QueueTraits::PrioritisationType::kExperimentalDatabase);
      } else {
        return PausableTaskQueueTraits();
      }
    case TaskType::kInternalNavigationAssociated:
      return FreezableTaskQueueTraits();
    // Some tasks in the tests need to run when objects are paused e.g. to hook
    // when recovering from debugger JavaScript statetment.
    case TaskType::kInternalTest:
    // kWebLocks can be frozen if for entire page, but not for individual
    // frames. See https://crrev.com/c/1687716
    case TaskType::kWebLocks:
    case TaskType::kInternalFrameLifecycleControl:
      return UnpausableTaskQueueTraits();
    case TaskType::kInternalTranslation:
      return ForegroundOnlyTaskQueueTraits();
    // The TaskType of Inspector tasks need to be unpausable and should not use
    // virtual time because they need to run on a paused page or when virtual
    // time is paused.
    case TaskType::kInternalInspector:
    // Navigation IPCs do not run using virtual time to avoid hanging.
    case TaskType::kInternalNavigationAssociatedUnfreezable:
      return DoesNotUseVirtualTimeTaskQueueTraits();
    case TaskType::kDeprecatedNone:
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueDefault:
    case TaskType::kMainThreadTaskQueueInput:
    case TaskType::kMainThreadTaskQueueIdle:
    case TaskType::kMainThreadTaskQueueControl:
    case TaskType::kMainThreadTaskQueueMemoryPurge:
    case TaskType::kCompositorThreadTaskQueueDefault:
    case TaskType::kCompositorThreadTaskQueueInput:
    case TaskType::kWorkerThreadTaskQueueDefault:
    case TaskType::kWorkerThreadTaskQueueV8:
    case TaskType::kWorkerThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueNonWaking:
    // The web scheduling API task types are used by WebSchedulingTaskQueues.
    // The associated TaskRunner should be obtained by creating a
    // WebSchedulingTaskQueue with CreateWebSchedulingTaskQueue().
    case TaskType::kExperimentalWebScheduling:
    case TaskType::kCount:
      // Not a valid frame-level TaskType.
      NOTREACHED();
      return QueueTraits();
  }
  // This method is called for all values between 0 and kCount. TaskType,
  // however, has numbering gaps, so even though all enumerated TaskTypes are
  // handled in the switch and return a value, we fall through for some values
  // of |type|.
  NOTREACHED();
  return QueueTraits();
}

scoped_refptr<base::SingleThreadTaskRunner> FrameSchedulerImpl::GetTaskRunner(
    TaskType type) {
  scoped_refptr<MainThreadTaskQueue> task_queue = GetTaskQueue(type);
  DCHECK(task_queue);
  return task_queue->CreateTaskRunner(type);
}

scoped_refptr<MainThreadTaskQueue> FrameSchedulerImpl::GetTaskQueue(
    TaskType type) {
  QueueTraits queue_traits = CreateQueueTraitsForTaskType(type);
  return frame_task_queue_controller_->GetTaskQueue(queue_traits);
}

std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
FrameSchedulerImpl::CreateResourceLoadingTaskRunnerHandle() {
  return CreateResourceLoadingTaskRunnerHandleImpl();
}

std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl>
FrameSchedulerImpl::CreateResourceLoadingTaskRunnerHandleImpl() {
  if (main_thread_scheduler_->scheduling_settings()
          .use_resource_fetch_priority ||
      (parent_page_scheduler_ && parent_page_scheduler_->IsLoading() &&
       main_thread_scheduler_->scheduling_settings()
           .use_resource_priorities_only_during_loading)) {
    scoped_refptr<MainThreadTaskQueue> task_queue =
        frame_task_queue_controller_->NewResourceLoadingTaskQueue();
    resource_loading_task_queue_priorities_.insert(
        task_queue, task_queue->GetQueuePriority());
    return ResourceLoadingTaskRunnerHandleImpl::WrapTaskRunner(task_queue);
  }

  return ResourceLoadingTaskRunnerHandleImpl::WrapTaskRunner(
      GetTaskQueue(TaskType::kNetworkingWithURLLoaderAnnotation));
}

void FrameSchedulerImpl::DidChangeResourceLoadingPriority(
    scoped_refptr<MainThreadTaskQueue> task_queue,
    net::RequestPriority priority) {
  // This check is done since in some cases (when kUseResourceFetchPriority
  // feature isn't enabled) we use the loading task queue for resource loading
  // and the priority of this queue shouldn't be affected by resource
  // priorities.
  auto queue_priority_pair =
      resource_loading_task_queue_priorities_.find(task_queue);
  if (queue_priority_pair != resource_loading_task_queue_priorities_.end()) {
    task_queue->SetNetRequestPriority(priority);
    queue_priority_pair->value = main_thread_scheduler_->scheduling_settings()
                                     .net_to_blink_priority[priority];
    auto* voter =
        frame_task_queue_controller_->GetQueueEnabledVoter(task_queue);
    UpdateQueuePolicy(task_queue.get(), voter);
  }
}

void FrameSchedulerImpl::OnShutdownResourceLoadingTaskQueue(
    scoped_refptr<MainThreadTaskQueue> task_queue) {
  // This check is done since in some cases (when kUseResourceFetchPriority
  // feature isn't enabled) we use the loading task queue for resource loading,
  // and the lifetime of this queue isn't bound to one resource.
  auto iter = resource_loading_task_queue_priorities_.find(task_queue);
  if (iter != resource_loading_task_queue_priorities_.end()) {
    resource_loading_task_queue_priorities_.erase(iter);
    bool removed = frame_task_queue_controller_->RemoveResourceLoadingTaskQueue(
        task_queue);
    DCHECK(removed);
    CleanUpQueue(task_queue.get());
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
FrameSchedulerImpl::ControlTaskRunner() {
  DCHECK(parent_page_scheduler_);
  return main_thread_scheduler_->ControlTaskRunner();
}

AgentGroupSchedulerImpl* FrameSchedulerImpl::GetAgentGroupScheduler() {
  return parent_page_scheduler_
             ? &parent_page_scheduler_->GetAgentGroupScheduler()
             : nullptr;
}

blink::PageScheduler* FrameSchedulerImpl::GetPageScheduler() const {
  return parent_page_scheduler_;
}

void FrameSchedulerImpl::DidStartProvisionalLoad(bool is_main_frame) {
  main_thread_scheduler_->DidStartProvisionalLoad(is_main_frame);
}

void FrameSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    NavigationType navigation_type) {
  bool is_main_frame = GetFrameType() == FrameType::kMainFrame;
  bool is_same_document = navigation_type == NavigationType::kSameDocument;

  if (!is_same_document) {
    waiting_for_contentful_paint_ = true;
    waiting_for_meaningful_paint_ = true;
  }
  if (is_main_frame && !is_same_document) {
    task_time_ = base::TimeDelta();
    // Ignore result here, based on the assumption that
    // MTSI::DidCommitProvisionalLoad will trigger an update policy.
    ignore_result(main_thread_scheduler_->agent_scheduling_strategy()
                      .OnDocumentChangedInMainFrame(*this));
  }

  main_thread_scheduler_->DidCommitProvisionalLoad(
      is_web_history_inert_commit, navigation_type == NavigationType::kReload,
      is_main_frame);
  if (!is_same_document)
    ResetForNavigation();
}

WebScopedVirtualTimePauser FrameSchedulerImpl::CreateWebScopedVirtualTimePauser(
    const WTF::String& name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(main_thread_scheduler_, duration, name);
}

void FrameSchedulerImpl::ResetForNavigation() {
  document_bound_weak_factory_.InvalidateWeakPtrs();

  for (const auto& it : back_forward_cache_opt_out_counts_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", "ActiveSchedulerTrackedFeature",
        TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                       static_cast<int>(it.first)));
  }

  back_forward_cache_opt_out_counts_.clear();
  back_forward_cache_opt_outs_.reset();
  last_uploaded_active_features_ = 0;
}

void FrameSchedulerImpl::OnStartedUsingFeature(
    SchedulingPolicy::Feature feature,
    const SchedulingPolicy& policy) {
  uint64_t old_mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();

  if (policy.disable_all_throttling)
    OnAddedAllThrottlingOptOut();
  if (policy.disable_aggressive_throttling)
    OnAddedAggressiveThrottlingOptOut();
  if (policy.disable_back_forward_cache) {
    OnAddedBackForwardCacheOptOut(feature);
  }

  uint64_t new_mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();

  if (old_mask != new_mask) {
    NotifyDelegateAboutFeaturesAfterCurrentTask();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "renderer.scheduler", "ActiveSchedulerTrackedFeature",
        TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                       static_cast<int>(feature)),
        "feature", FeatureToString(feature));
  }
}

void FrameSchedulerImpl::OnStoppedUsingFeature(
    SchedulingPolicy::Feature feature,
    const SchedulingPolicy& policy) {
  uint64_t old_mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();

  if (policy.disable_all_throttling)
    OnRemovedAllThrottlingOptOut();
  if (policy.disable_aggressive_throttling)
    OnRemovedAggressiveThrottlingOptOut();
  if (policy.disable_back_forward_cache)
    OnRemovedBackForwardCacheOptOut(feature);

  uint64_t new_mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();

  if (old_mask != new_mask) {
    NotifyDelegateAboutFeaturesAfterCurrentTask();
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", "ActiveSchedulerTrackedFeature",
        TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                       static_cast<int>(feature)));
  }
}

void FrameSchedulerImpl::NotifyDelegateAboutFeaturesAfterCurrentTask() {
  if (!delegate_)
    return;
  if (feature_report_scheduled_)
    return;
  feature_report_scheduled_ = true;

  main_thread_scheduler_->ExecuteAfterCurrentTask(
      base::BindOnce(&FrameSchedulerImpl::ReportFeaturesToDelegate,
                     document_bound_weak_factory_.GetWeakPtr()));
}

void FrameSchedulerImpl::ReportFeaturesToDelegate() {
  DCHECK(delegate_);
  feature_report_scheduled_ = false;
  uint64_t mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();
  if (mask == last_uploaded_active_features_)
    return;
  last_uploaded_active_features_ = mask;
  delegate_->UpdateActiveSchedulerTrackedFeatures(mask);
}

base::WeakPtr<FrameScheduler> FrameSchedulerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<const FrameSchedulerImpl> FrameSchedulerImpl::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

void FrameSchedulerImpl::ReportActiveSchedulerTrackedFeatures() {
  if (delegate_)
    ReportFeaturesToDelegate();
}

base::WeakPtr<FrameSchedulerImpl>
FrameSchedulerImpl::GetInvalidatingOnBFCacheRestoreWeakPtr() {
  return invalidating_on_bfcache_restore_weak_factory_.GetWeakPtr();
}

void FrameSchedulerImpl::OnAddedAllThrottlingOptOut() {
  ++all_throttling_opt_out_count_;
  opted_out_from_all_throttling_ =
      static_cast<bool>(all_throttling_opt_out_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnThrottlingStatusUpdated();
}

void FrameSchedulerImpl::OnRemovedAllThrottlingOptOut() {
  DCHECK_GT(all_throttling_opt_out_count_, 0);
  --all_throttling_opt_out_count_;
  opted_out_from_all_throttling_ =
      static_cast<bool>(all_throttling_opt_out_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnThrottlingStatusUpdated();
}

void FrameSchedulerImpl::OnAddedAggressiveThrottlingOptOut() {
  ++aggressive_throttling_opt_out_count_;
  opted_out_from_aggressive_throttling_ =
      static_cast<bool>(aggressive_throttling_opt_out_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnThrottlingStatusUpdated();
}

void FrameSchedulerImpl::OnRemovedAggressiveThrottlingOptOut() {
  DCHECK_GT(aggressive_throttling_opt_out_count_, 0);
  --aggressive_throttling_opt_out_count_;
  opted_out_from_aggressive_throttling_ =
      static_cast<bool>(aggressive_throttling_opt_out_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnThrottlingStatusUpdated();
}

void FrameSchedulerImpl::OnAddedBackForwardCacheOptOut(
    SchedulingPolicy::Feature feature) {
  ++back_forward_cache_opt_out_counts_[feature];
  back_forward_cache_opt_outs_.set(static_cast<size_t>(feature));
  opted_out_from_back_forward_cache_ = true;
}

void FrameSchedulerImpl::OnRemovedBackForwardCacheOptOut(
    SchedulingPolicy::Feature feature) {
  DCHECK_GT(back_forward_cache_opt_out_counts_[feature], 0);
  auto it = back_forward_cache_opt_out_counts_.find(feature);
  if (it->second == 1) {
    back_forward_cache_opt_out_counts_.erase(it);
    back_forward_cache_opt_outs_.reset(static_cast<size_t>(feature));
  } else {
    --it->second;
  }
  opted_out_from_back_forward_cache_ =
      !back_forward_cache_opt_out_counts_.empty();
}

void FrameSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("frame_visible", frame_visible_);
  state->SetBoolean("page_visible", parent_page_scheduler_->IsPageVisible());
  state->SetBoolean("cross_origin_to_main_frame", IsCrossOriginToMainFrame());
  state->SetString("frame_type",
                   frame_type_ == FrameScheduler::FrameType::kMainFrame
                       ? "MainFrame"
                       : "Subframe");
  state->SetBoolean(
      "disable_background_timer_throttling",
      !RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled());

  {
    auto dictionary_scope =
        state->BeginDictionaryScoped("frame_task_queue_controller");
    frame_task_queue_controller_->AsValueInto(state);
  }

  if (blame_context_) {
    auto dictionary_scope = state->BeginDictionaryScoped("blame_context");
    state->SetString(
        "id_ref",
        PointerToString(reinterpret_cast<void*>(blame_context_->id())));
    state->SetString("scope", blame_context_->scope());
  }
}

void FrameSchedulerImpl::SetPageVisibilityForTracing(
    PageVisibilityState page_visibility) {
  page_visibility_for_tracing_ = page_visibility;
}

bool FrameSchedulerImpl::IsPageVisible() const {
  return parent_page_scheduler_ ? parent_page_scheduler_->IsPageVisible()
                                : true;
}

void FrameSchedulerImpl::SetPaused(bool frame_paused) {
  DCHECK(parent_page_scheduler_);
  if (frame_paused_ == frame_paused)
    return;

  frame_paused_ = frame_paused;
  UpdatePolicy();
}

void FrameSchedulerImpl::SetShouldReportPostedTasksWhenDisabled(
    bool should_report) {
  // Forward this to all the task queues associated with this frame.
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    auto* task_queue = task_queue_and_voter.first;
    if (task_queue->CanBeFrozen())
      task_queue->SetShouldReportPostedTasksWhenDisabled(should_report);
  }
}

void FrameSchedulerImpl::SetPageFrozenForTracing(bool frozen) {
  page_frozen_for_tracing_ = frozen;
}

void FrameSchedulerImpl::SetPageKeepActiveForTracing(bool keep_active) {
  page_keep_active_for_tracing_ = keep_active;
}

void FrameSchedulerImpl::UpdatePolicy() {
  bool task_queues_were_throttled = task_queues_throttled_;
  task_queues_throttled_ = ShouldThrottleTaskQueues();

  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    UpdateQueuePolicy(task_queue_and_voter.first, task_queue_and_voter.second);
    if (task_queues_were_throttled != task_queues_throttled_) {
      UpdateTaskQueueThrottling(task_queue_and_voter.first,
                                task_queues_throttled_);
    }
  }

  NotifyLifecycleObservers();
}

void FrameSchedulerImpl::UpdateQueuePolicy(
    MainThreadTaskQueue* queue,
    TaskQueue::QueueEnabledVoter* voter) {
  DCHECK(queue);
  UpdatePriority(queue);
  if (!voter)
    return;
  DCHECK(parent_page_scheduler_);
  bool queue_disabled = false;
  queue_disabled |= frame_paused_ && queue->CanBePaused();
  queue_disabled |= preempted_for_cooperative_scheduling_;
  // Per-frame freezable task queues will be frozen after 5 mins in background
  // on Android, and if the browser freezes the page in the background. They
  // will be resumed when the page is visible.
  bool queue_frozen =
      parent_page_scheduler_->IsFrozen() && queue->CanBeFrozen();
  // Override freezing if keep-active is true.
  if (queue_frozen && !queue->FreezeWhenKeepActive())
    queue_frozen = !parent_page_scheduler_->KeepActive();
  queue_disabled |= queue_frozen;
  // Per-frame freezable queues of tasks which are specified as getting frozen
  // immediately when their frame becomes invisible get frozen. They will be
  // resumed when the frame becomes visible again.
  queue_disabled |= !frame_visible_ && !queue->CanRunInBackground();
  voter->SetVoteToEnable(!queue_disabled);
}

SchedulingLifecycleState FrameSchedulerImpl::CalculateLifecycleState(
    ObserverType type) const {
  // Detached frames are not throttled.
  if (!parent_page_scheduler_)
    return SchedulingLifecycleState::kNotThrottled;

  if (parent_page_scheduler_->IsFrozen() &&
      !parent_page_scheduler_->KeepActive()) {
    DCHECK(!parent_page_scheduler_->IsPageVisible());
    return SchedulingLifecycleState::kStopped;
  }
  if (subresource_loading_paused_ && type == ObserverType::kLoader)
    return SchedulingLifecycleState::kStopped;
  if (type == ObserverType::kLoader &&
      parent_page_scheduler_->OptedOutFromAggressiveThrottling()) {
    return SchedulingLifecycleState::kNotThrottled;
  }
  // Note: The scheduling lifecycle state ignores wake up rate throttling.
  if (parent_page_scheduler_->IsCPUTimeThrottled())
    return SchedulingLifecycleState::kThrottled;
  if (!parent_page_scheduler_->IsPageVisible())
    return SchedulingLifecycleState::kHidden;
  return SchedulingLifecycleState::kNotThrottled;
}

void FrameSchedulerImpl::OnFirstContentfulPaint() {
  waiting_for_contentful_paint_ = false;
  if (GetFrameType() == FrameScheduler::FrameType::kMainFrame)
    main_thread_scheduler_->OnMainFramePaint();
}

void FrameSchedulerImpl::OnFirstMeaningfulPaint() {
  waiting_for_meaningful_paint_ = false;

  if (GetFrameType() != FrameScheduler::FrameType::kMainFrame)
    return;

  main_thread_scheduler_->OnMainFramePaint();

  if (main_thread_scheduler_->agent_scheduling_strategy()
          .OnMainFrameFirstMeaningfulPaint(*this) ==
      AgentSchedulingStrategy::ShouldUpdatePolicy::kYes) {
    main_thread_scheduler_->OnAgentStrategyUpdated();
  }
}

void FrameSchedulerImpl::OnLoad() {
  if (GetFrameType() == FrameScheduler::FrameType::kMainFrame) {
    // TODO(talp): Once MTSI::UpdatePolicyLocked is refactored, this can notify
    // the agent strategy directly and, if necessary, trigger the queue priority
    // update.
    main_thread_scheduler_->OnMainFrameLoad(*this);
  }
}

bool FrameSchedulerImpl::IsWaitingForContentfulPaint() const {
  return waiting_for_contentful_paint_;
}

bool FrameSchedulerImpl::IsWaitingForMeaningfulPaint() const {
  return waiting_for_meaningful_paint_;
}

bool FrameSchedulerImpl::IsOrdinary() const {
  if (!parent_page_scheduler_)
    return true;
  return parent_page_scheduler_->IsOrdinary();
}

bool FrameSchedulerImpl::ShouldThrottleTaskQueues() const {
  // TODO(crbug.com/1078387): Convert the CHECK to a DCHECK once enough time has
  // passed to confirm that it is correct. (November 2020).
  CHECK(parent_page_scheduler_);

  if (!RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled())
    return false;
  if (parent_page_scheduler_->IsAudioPlaying())
    return false;
  if (parent_page_scheduler_->OptedOutFromAllThrottling())
    return false;
  if (!parent_page_scheduler_->IsPageVisible())
    return true;
  return RuntimeEnabledFeatures::TimerThrottlingForHiddenFramesEnabled() &&
         !frame_visible_ && IsCrossOriginToMainFrame();
}

void FrameSchedulerImpl::UpdateTaskQueueThrottling(
    MainThreadTaskQueue* task_queue,
    bool should_throttle) {
  if (!task_queue->CanBeThrottled())
    return;
  if (should_throttle) {
    main_thread_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
        task_queue);
  } else {
    main_thread_scheduler_->task_queue_throttler()->DecreaseThrottleRefCount(
        task_queue);
  }
}

bool FrameSchedulerImpl::IsExemptFromBudgetBasedThrottling() const {
  return opted_out_from_aggressive_throttling();
}

TaskQueue::QueuePriority FrameSchedulerImpl::ComputePriority(
    MainThreadTaskQueue* task_queue) const {
  DCHECK(task_queue);

  FrameScheduler* frame_scheduler = task_queue->GetFrameScheduler();

  // Checks the task queue is associated with this frame scheduler.
  DCHECK_EQ(frame_scheduler, this);

  auto queue_priority_pair = resource_loading_task_queue_priorities_.find(
      base::WrapRefCounted(task_queue));
  if (queue_priority_pair != resource_loading_task_queue_priorities_.end())
    return queue_priority_pair->value;

  // TODO(kdillon): Ordering here is relative to the experiments below. Cleanup
  // unused experiment logic so that this switch can be merged with the
  // prioritisation type decisions below.
  switch (task_queue->GetPrioritisationType()) {
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::
        kInternalScriptContinuation:
      return TaskQueue::QueuePriority::kVeryHighPriority;
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::kBestEffort:
      return TaskQueue::QueuePriority::kBestEffortPriority;
    default:
      break;
  }

  // TODO(shaseley): This should use lower priorities if the frame is
  // deprioritized. Change this once we refactor and add frame policy/priorities
  // and add a range of new priorities less than low.
  if (task_queue->web_scheduling_priority()) {
    switch (task_queue->web_scheduling_priority().value()) {
      case WebSchedulingPriority::kUserBlockingPriority:
        return TaskQueue::QueuePriority::kHighPriority;
      case WebSchedulingPriority::kUserVisiblePriority:
        return TaskQueue::QueuePriority::kNormalPriority;
      case WebSchedulingPriority::kBackgroundPriority:
        return TaskQueue::QueuePriority::kLowPriority;
    }
  }

  if (!parent_page_scheduler_) {
    // Frame might be detached during its shutdown. Return a default priority
    // in that case.
    return TaskQueue::QueuePriority::kNormalPriority;
  }

  // A hidden page with no audio.
  if (parent_page_scheduler_->IsBackgrounded()) {
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_background_page) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    if (main_thread_scheduler_->scheduling_settings()
            .best_effort_background_page) {
      return TaskQueue::QueuePriority::kBestEffortPriority;
    }
  }

  // If the page is loading or if the priority experiments should take place at
  // all times.
  if (parent_page_scheduler_->IsLoading() ||
      !main_thread_scheduler_->scheduling_settings()
           .use_frame_priorities_only_during_loading) {
    // Low priority feature enabled for hidden frame.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_hidden_frame &&
        !IsFrameVisible()) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    bool is_subframe = GetFrameType() == FrameScheduler::FrameType::kSubframe;
    bool is_throttleable_task_queue =
        task_queue->queue_type() ==
        MainThreadTaskQueue::QueueType::kFrameThrottleable;

    // Low priority feature enabled for sub-frame.
    if (main_thread_scheduler_->scheduling_settings().low_priority_subframe &&
        is_subframe) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    // Low priority feature enabled for sub-frame throttleable task queues.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_subframe_throttleable &&
        is_subframe && is_throttleable_task_queue) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    // Low priority feature enabled for throttleable task queues.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_throttleable &&
        is_throttleable_task_queue) {
      return TaskQueue::QueuePriority::kLowPriority;
    }
  }

  // Ad frame experiment.
  if (IsAdFrame() && (parent_page_scheduler_->IsLoading() ||
                      !main_thread_scheduler_->scheduling_settings()
                           .use_adframe_priorities_only_during_loading)) {
    if (main_thread_scheduler_->scheduling_settings().low_priority_ad_frame) {
      return TaskQueue::QueuePriority::kLowPriority;
    }

    if (main_thread_scheduler_->scheduling_settings().best_effort_ad_frame) {
      return TaskQueue::QueuePriority::kBestEffortPriority;
    }
  }

  // Frame origin type experiment.
  if (IsCrossOriginToMainFrame()) {
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_cross_origin ||
        (main_thread_scheduler_->scheduling_settings()
             .low_priority_cross_origin_only_during_loading &&
         parent_page_scheduler_->IsLoading())) {
      return TaskQueue::QueuePriority::kLowPriority;
    }
  }

  // Consult per-agent scheduling strategy to see if it wants to affect queue
  // priority. Done here to avoid interfering with other policy decisions.
  base::Optional<TaskQueue::QueuePriority> per_agent_priority =
      main_thread_scheduler_->agent_scheduling_strategy().QueuePriority(
          *task_queue);
  if (per_agent_priority.has_value())
    return per_agent_priority.value();

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kLoadingControl) {
    return main_thread_scheduler_->should_prioritize_loading_with_compositing()
               ? TaskQueue::QueuePriority::kVeryHighPriority
               : TaskQueue::QueuePriority::kHighPriority;
  }

  if (task_queue->GetPrioritisationType() ==
          MainThreadTaskQueue::QueueTraits::PrioritisationType::kLoading &&
      main_thread_scheduler_->should_prioritize_loading_with_compositing()) {
    return main_thread_scheduler_->compositor_priority();
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kFindInPage) {
    return main_thread_scheduler_->find_in_page_priority();
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::
          kHighPriorityLocalFrame) {
    return TaskQueue::QueuePriority::kHighestPriority;
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::
          kExperimentalDatabase) {
    // TODO(shaseley): This decision should probably be based on Agent
    // visibility. Consider changing this before shipping anything.
    return parent_page_scheduler_->IsPageVisible()
               ? TaskQueue::QueuePriority::kHighPriority
               : TaskQueue::QueuePriority::kNormalPriority;
  }

  return TaskQueue::QueuePriority::kNormalPriority;
}

std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
FrameSchedulerImpl::GetPauseSubresourceLoadingHandle() {
  return std::make_unique<PauseSubresourceLoadingHandleImpl>(
      weak_factory_.GetWeakPtr());
}

void FrameSchedulerImpl::AddPauseSubresourceLoadingHandle() {
  ++subresource_loading_pause_count_;
  if (subresource_loading_pause_count_ != 1) {
    DCHECK(subresource_loading_paused_);
    return;
  }

  DCHECK(!subresource_loading_paused_);
  subresource_loading_paused_ = true;
  UpdatePolicy();
}

void FrameSchedulerImpl::RemovePauseSubresourceLoadingHandle() {
  DCHECK_LT(0u, subresource_loading_pause_count_);
  --subresource_loading_pause_count_;
  DCHECK(subresource_loading_paused_);
  if (subresource_loading_pause_count_ == 0) {
    subresource_loading_paused_ = false;
    UpdatePolicy();
  }
}

ukm::UkmRecorder* FrameSchedulerImpl::GetUkmRecorder() {
  if (!delegate_)
    return nullptr;
  return delegate_->GetUkmRecorder();
}

ukm::SourceId FrameSchedulerImpl::GetUkmSourceId() {
  if (!delegate_)
    return ukm::kInvalidSourceId;
  return delegate_->GetUkmSourceId();
}

void FrameSchedulerImpl::OnTaskQueueCreated(
    MainThreadTaskQueue* task_queue,
    base::sequence_manager::TaskQueue::QueueEnabledVoter* voter) {
  DCHECK(parent_page_scheduler_);

  task_queue->SetBlameContext(blame_context_);
  UpdateQueuePolicy(task_queue, voter);

  if (task_queue->CanBeThrottled()) {
    base::sequence_manager::LazyNow lazy_now(
        main_thread_scheduler_->tick_clock());

    CPUTimeBudgetPool* cpu_time_budget_pool =
        parent_page_scheduler_->background_cpu_time_budget_pool();
    if (cpu_time_budget_pool) {
      cpu_time_budget_pool->AddQueue(lazy_now.Now(), task_queue);
    }

    parent_page_scheduler_->AddQueueToWakeUpBudgetPool(
        task_queue, frame_origin_type_, &lazy_now);

    if (task_queues_throttled_) {
      UpdateTaskQueueThrottling(task_queue, true);
    }
  }
}

void FrameSchedulerImpl::SetOnIPCTaskPostedWhileInBackForwardCacheHandler() {
  DCHECK(parent_page_scheduler_->IsStoredInBackForwardCache());
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    task_queue_and_voter.first->SetOnIPCTaskPosted(base::BindRepeating(
        [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
           base::WeakPtr<FrameSchedulerImpl> frame_scheduler,
           const base::sequence_manager::Task& task) {
          // Only log IPC tasks. IPC tasks are only logged currently as IPC
          // hash can be mapped back to a function name, and IPC tasks may
          // potentially post sensitive information.
          if (!task.ipc_hash && !task.ipc_interface_name) {
            return;
          }
          base::ScopedDeferTaskPosting::PostOrDefer(
              task_runner, FROM_HERE,
              base::BindOnce(
                  &FrameSchedulerImpl::OnIPCTaskPostedWhileInBackForwardCache,
                  frame_scheduler, task.ipc_hash, task.ipc_interface_name),
              base::TimeDelta());
        },
        main_thread_scheduler_->DefaultTaskRunner(),
        GetInvalidatingOnBFCacheRestoreWeakPtr()));
  }
}

void FrameSchedulerImpl::DetachOnIPCTaskPostedWhileInBackForwardCacheHandler() {
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    task_queue_and_voter.first->DetachOnIPCTaskPostedWhileInBackForwardCache();
  }

  invalidating_on_bfcache_restore_weak_factory_.InvalidateWeakPtrs();
}

void FrameSchedulerImpl::OnIPCTaskPostedWhileInBackForwardCache(
    uint32_t ipc_hash,
    const char* ipc_interface_name) {
  // IPC tasks may have an IPC interface name in addition to, or instead of an
  // IPC hash. IPC hash is known from the mojo Accept method. When IPC hash is
  // 0, then the IPC hash must be calculated from the IPC interface name
  // instead.
  if (!ipc_hash) {
    // base::HashMetricName produces a uint64; however, the MD5 hash calculation
    // for an IPC interface name is always calculated as uint32; the IPC hash on
    // a task is also a uint32. The calculation here is meant to mimic the
    // calculation used in base::MD5Hash32Constexpr.
    ipc_hash = base::TaskAnnotator::ScopedSetIpcHash::MD5HashMetricName(
        ipc_interface_name);
  }

  DCHECK(parent_page_scheduler_->IsStoredInBackForwardCache());
  base::UmaHistogramSparse(
      "BackForwardCache.Experimental.UnexpectedIPCMessagePostedToCachedFrame."
      "MethodHash",
      static_cast<int32_t>(ipc_hash));

  base::TimeDelta duration =
      main_thread_scheduler_->tick_clock()->NowTicks() -
      parent_page_scheduler_->GetStoredInBackForwardCacheTimestamp();
  base::UmaHistogramCustomTimes(
      "BackForwardCache.Experimental.UnexpectedIPCMessagePostedToCachedFrame."
      "TimeUntilIPCReceived",
      duration, base::TimeDelta(), base::TimeDelta::FromMinutes(5), 100);
}

WTF::HashSet<SchedulingPolicy::Feature>
FrameSchedulerImpl::GetActiveFeaturesTrackedForBackForwardCacheMetrics() {
  WTF::HashSet<SchedulingPolicy::Feature> result;
  for (const auto& it : back_forward_cache_opt_out_counts_)
    result.insert(it.first);
  return result;
}

uint64_t
FrameSchedulerImpl::GetActiveFeaturesTrackedForBackForwardCacheMetricsMask()
    const {
  auto result = back_forward_cache_opt_outs_.to_ullong();
  static_assert(static_cast<size_t>(SchedulingPolicy::Feature::kMaxValue) <
                    sizeof(result) * 8,
                "Number of the features should allow a bitmask to fit into "
                "64-bit integer");
  return result;
}

base::WeakPtr<FrameOrWorkerScheduler>
FrameSchedulerImpl::GetDocumentBoundWeakPtr() {
  return document_bound_weak_factory_.GetWeakPtr();
}

std::unique_ptr<WebSchedulingTaskQueue>
FrameSchedulerImpl::CreateWebSchedulingTaskQueue(
    WebSchedulingPriority priority) {
  // Use QueueTraits here that are the same as postMessage, which is one current
  // method for scheduling script.
  scoped_refptr<MainThreadTaskQueue> task_queue =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          PausableTaskQueueTraits(), priority);
  return std::make_unique<WebSchedulingTaskQueueImpl>(task_queue->AsWeakPtr());
}

void FrameSchedulerImpl::OnWebSchedulingTaskQueuePriorityChanged(
    MainThreadTaskQueue* queue) {
  UpdateQueuePolicy(queue,
                    frame_task_queue_controller_->GetQueueEnabledVoter(queue));
}

const base::UnguessableToken& FrameSchedulerImpl::GetAgentClusterId() const {
  if (!delegate_)
    return base::UnguessableToken::Null();
  return delegate_->GetAgentClusterId();
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::ThrottleableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeThrottled(true)
      .SetCanBeFrozen(true)
      .SetCanBeDeferred(true)
      .SetCanBePaused(true)
      .SetCanRunWhenVirtualTimePaused(false)
      .SetCanBePausedForAndroidWebview(true);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::DeferrableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeDeferred(true)
      .SetCanBeFrozen(base::FeatureList::IsEnabled(
          blink::features::kStopNonTimersInBackground))
      .SetCanBePaused(true)
      .SetCanRunWhenVirtualTimePaused(false)
      .SetCanBePausedForAndroidWebview(true);
}

// static
MainThreadTaskQueue::QueueTraits FrameSchedulerImpl::PausableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeFrozen(base::FeatureList::IsEnabled(
          blink::features::kStopNonTimersInBackground))
      .SetCanBePaused(true)
      .SetCanRunWhenVirtualTimePaused(false)
      .SetCanBePausedForAndroidWebview(true);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::FreezableTaskQueueTraits() {
  // Should not use VirtualTime because using VirtualTime would make the task
  // execution non-deterministic and produce timeouts failures.
  return QueueTraits().SetCanBeFrozen(true);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::UnpausableTaskQueueTraits() {
  return QueueTraits().SetCanRunWhenVirtualTimePaused(false);
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::ForegroundOnlyTaskQueueTraits() {
  return ThrottleableTaskQueueTraits()
      .SetCanRunInBackground(false)
      .SetCanRunWhenVirtualTimePaused(false);
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::DoesNotUseVirtualTimeTaskQueueTraits() {
  return QueueTraits().SetCanRunWhenVirtualTimePaused(true);
}

void FrameSchedulerImpl::SetPreemptedForCooperativeScheduling(
    Preempted preempted) {
  DCHECK_NE(preempted.value(), preempted_for_cooperative_scheduling_);
  preempted_for_cooperative_scheduling_ = preempted.value();
  UpdatePolicy();
}

MainThreadTaskQueue::QueueTraits FrameSchedulerImpl::LoadingTaskQueueTraits() {
  return QueueTraits()
      .SetCanBePaused(true)
      .SetCanBeFrozen(true)
      .SetCanBeDeferred(true)
      .SetPrioritisationType(QueueTraits::PrioritisationType::kLoading);
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::LoadingControlTaskQueueTraits() {
  return QueueTraits()
      .SetCanBePaused(true)
      .SetCanBeFrozen(true)
      .SetCanBeDeferred(true)
      .SetPrioritisationType(QueueTraits::PrioritisationType::kLoadingControl);
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::FindInPageTaskQueueTraits() {
  return PausableTaskQueueTraits().SetPrioritisationType(
      QueueTraits::PrioritisationType::kFindInPage);
}
}  // namespace scheduler
}  // namespace blink
