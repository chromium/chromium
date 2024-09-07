// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/common/lazy_now.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/common/task_annotator.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/type.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/find_in_page_budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_web_scheduling_task_queue_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/policy_updater.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {

namespace scheduler {

using base::sequence_manager::TaskQueue;
using QueueTraits = MainThreadTaskQueue::QueueTraits;
using perfetto::protos::pbzero::RendererMainThreadTaskExecution;

namespace {

// When enabled, the main thread's type is reduced from `kDisplayCritical` to
// `kDefault` when WebRTC is in use within the renderer. This is a simple
// workaround meant to be merged to higher channels while we're working on a
// more refined solution. See crbug.com/1513904.
BASE_FEATURE(kRendererMainIsDefaultThreadTypeForWebRTC,
             "RendererMainIsNormalThreadTypeForWebRTC",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char* VisibilityStateToString(bool is_visible) {
  if (is_visible) {
    return "visible";
  } else {
    return "hidden";
  }
}

const char* IsVisibleAreaLargeStateToString(bool is_large) {
  if (is_large) {
    return "large";
  } else {
    return "small";
  }
}

const char* UserActivationStateToString(bool had_user_activation) {
  if (had_user_activation) {
    return "had user activation";
  } else {
    return "no user activation";
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

// Used to update the priority of task_queue. Note that this function is
// used for queues associated with a frame.
void UpdatePriority(MainThreadTaskQueue* task_queue) {
  if (!task_queue)
    return;

  FrameSchedulerImpl* frame_scheduler = task_queue->GetFrameScheduler();
  DCHECK(frame_scheduler);
  task_queue->SetQueuePriority(frame_scheduler->ComputePriority(task_queue));
}

TaskPriority GetLowPriorityAsyncScriptTaskPriority() {
  switch (
      features::kLowPriorityAsyncScriptExecutionLowerTaskPriorityParam.Get()) {
    case features::AsyncScriptPrioritisationType::kHigh:
      return TaskPriority::kHighPriority;
    case features::AsyncScriptPrioritisationType::kLow:
      return TaskPriority::kLowPriority;
    case features::AsyncScriptPrioritisationType::kBestEffort:
      return TaskPriority::kBestEffortPriority;
  }
  NOTREACHED();
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

FrameSchedulerImpl::FrameSchedulerImpl(PageSchedulerImpl* parent_page_scheduler,
                                       FrameScheduler::Delegate* delegate,
                                       bool is_in_embedded_frame_tree,
                                       FrameScheduler::FrameType frame_type)
    : FrameSchedulerImpl(parent_page_scheduler->GetMainThreadScheduler(),
                         parent_page_scheduler,
                         delegate,
                         is_in_embedded_frame_tree,
                         frame_type) {}

FrameSchedulerImpl::FrameSchedulerImpl(
    MainThreadSchedulerImpl* main_thread_scheduler,
    PageSchedulerImpl* parent_page_scheduler,
    FrameScheduler::Delegate* delegate,
    bool is_in_embedded_frame_tree,
    FrameScheduler::FrameType frame_type)
    : frame_type_(frame_type),
      is_in_embedded_frame_tree_(is_in_embedded_frame_tree),
      main_thread_scheduler_(main_thread_scheduler),
      parent_page_scheduler_(parent_page_scheduler),
      delegate_(delegate),
      page_visibility_(
          parent_page_scheduler_ && parent_page_scheduler_->IsPageVisible()
              ? PageVisibilityState::kVisible
              : PageVisibilityState::kHidden,
          "FrameScheduler.PageVisibility",
          &tracing_controller_,
          PageVisibilityStateToString),
      frame_visible_(true,
                     "FrameScheduler.FrameVisible",
                     &tracing_controller_,
                     VisibilityStateToString),
      is_visible_area_large_(true,
                             "FrameScheduler.IsVisibleAreaLarge",
                             &tracing_controller_,
                             IsVisibleAreaLargeStateToString),
      had_user_activation_(false,
                           "FrameScheduler.HadUserActivation",
                           &tracing_controller_,
                           UserActivationStateToString),
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
      throttling_type_(ThrottlingType::kNone,
                       "FrameScheduler.ThrottlingType",
                       &tracing_controller_,
                       ThrottlingTypeToString),
      preempted_for_cooperative_scheduling_(
          false,
          "FrameScheduler.PreemptedForCooperativeScheduling",
          &tracing_controller_,
          YesNoStateToString),
      aggressive_throttling_opt_out_count_(0),
      opted_out_from_aggressive_throttling_(
          false,
          "FrameScheduler.AggressiveThrottlingDisabled",
          &tracing_controller_,
          YesNoStateToString),
      subresource_loading_pause_count_(0u),
      back_forward_cache_disabling_feature_tracker_(&tracing_controller_,
                                                    main_thread_scheduler_),
      low_priority_async_script_task_priority_(
          GetLowPriorityAsyncScriptTaskPriority()),
      page_frozen_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->IsFrozen() : true,
          "FrameScheduler.PageFrozen",
          &tracing_controller_,
          FrozenStateToString),
      waiting_for_contentful_paint_(true,
                                    "FrameScheduler.WaitingForContentfulPaint",
                                    &tracing_controller_,
                                    YesNoStateToString),
      waiting_for_meaningful_paint_(true,
                                    "FrameScheduler.WaitingForMeaningfulPaint",
                                    &tracing_controller_,
                                    YesNoStateToString),
      is_load_event_dispatched_(false,
                                "FrameScheduler.IsLoadEventDispatched",
                                &tracing_controller_,
                                YesNoStateToString) {
  frame_task_queue_controller_ = base::WrapUnique(
      new FrameTaskQueueController(main_thread_scheduler_, this, this));
  back_forward_cache_disabling_feature_tracker_.SetDelegate(delegate_);
}

FrameSchedulerImpl::FrameSchedulerImpl()
    : FrameSchedulerImpl(/*main_thread_scheduler=*/nullptr,
                         /*parent_page_scheduler=*/nullptr,
                         /*delegate=*/nullptr,
                         /*is_in_embedded_frame_tree=*/false,
                         FrameType::kSubframe) {}

FrameSchedulerImpl::~FrameSchedulerImpl() {
  weak_factory_.InvalidateWeakPtrs();

  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      RemoveThrottleableQueueFromBudgetPools(task_queue_and_voter.first);
    }
    auto* queue = task_queue_and_voter.first;
    CHECK(queue);
    queue->DetachTaskQueue();
    CHECK(!queue->GetFrameScheduler());
  }

  if (parent_page_scheduler_) {
    parent_page_scheduler_->Unregister(this);

    if (AreFrameAndPageVisible()) {
      PolicyUpdater policy_updater;
      GetAgentGroupSchedulerImpl().DecrementVisibleFramesForAgent(
          agent_cluster_id_, policy_updater);
    }

    if (opted_out_from_aggressive_throttling())
      parent_page_scheduler_->OnThrottlingStatusUpdated();
  }
}

AgentGroupSchedulerImpl& FrameSchedulerImpl::GetAgentGroupSchedulerImpl() {
  CHECK(parent_page_scheduler_);
  return parent_page_scheduler_->GetAgentGroupScheduler();
}

void FrameSchedulerImpl::OnPageVisibilityChange(
    PageVisibilityState page_visibility,
    PolicyUpdater& policy_updater) {
  CHECK_NE(page_visibility, page_visibility_.get());

  const bool were_frame_and_page_visible = AreFrameAndPageVisible();
  page_visibility_ = page_visibility;
  CHECK_EQ(page_visibility_ == PageVisibilityState::kVisible,
           parent_page_scheduler_->IsPageVisible());
  const bool are_frame_and_page_visible = AreFrameAndPageVisible();

  if (were_frame_and_page_visible != are_frame_and_page_visible) {
    OnFrameAndPageVisibleChanged(policy_updater);
  }
}

void FrameSchedulerImpl::OnPageSchedulerDeletion(
    PolicyUpdater& policy_updater) {
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      RemoveThrottleableQueueFromBudgetPools(task_queue_and_voter.first);
    }
  }

  if (AreFrameAndPageVisible()) {
    GetAgentGroupSchedulerImpl().DecrementVisibleFramesForAgent(
        agent_cluster_id_, policy_updater);
  }

  parent_page_scheduler_ = nullptr;
}

void FrameSchedulerImpl::OnFrameAndPageVisibleChanged(
    PolicyUpdater& policy_updater) {
  if (AreFrameAndPageVisible()) {
    return GetAgentGroupSchedulerImpl().IncrementVisibleFramesForAgent(
        agent_cluster_id_, policy_updater);
  } else {
    return GetAgentGroupSchedulerImpl().DecrementVisibleFramesForAgent(
        agent_cluster_id_, policy_updater);
  }
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
  base::LazyNow lazy_now =
      main_thread_scheduler_->GetTickClock()
          ? base::LazyNow(main_thread_scheduler_->GetTickClock())
          : base::LazyNow(base::TimeTicks::Now());

  if (cpu_time_budget_pool) {
    task_queue->RemoveFromBudgetPool(lazy_now.Now(), cpu_time_budget_pool);
  }

  parent_page_scheduler_->RemoveQueueFromWakeUpBudgetPool(task_queue,
                                                          &lazy_now);
}

void FrameSchedulerImpl::SetFrameVisible(bool frame_visible) {
  if (frame_visible_ == frame_visible) {
    return;
  }

  const bool were_frame_and_page_visible = AreFrameAndPageVisible();
  frame_visible_ = frame_visible;
  const bool are_frame_and_page_visible = AreFrameAndPageVisible();

  PolicyUpdater policy_updater;
  policy_updater.UpdateFramePolicy(this);
  if (were_frame_and_page_visible != are_frame_and_page_visible) {
    OnFrameAndPageVisibleChanged(policy_updater);
  }
}

bool FrameSchedulerImpl::IsFrameVisible() const {
  return frame_visible_;
}

void FrameSchedulerImpl::SetVisibleAreaLarge(bool is_large) {
  DCHECK(parent_page_scheduler_);
  if (is_visible_area_large_ == is_large) {
    return;
  }
  is_visible_area_large_ = is_large;

  if (!IsCrossOriginToNearestMainFrame()) {
    return;
  }

  UpdatePolicy();
}

void FrameSchedulerImpl::SetHadUserActivation(bool had_user_activation) {
  DCHECK(parent_page_scheduler_);
  if (had_user_activation_ == had_user_activation) {
    return;
  }
  had_user_activation_ = had_user_activation;

  if (!IsCrossOriginToNearestMainFrame()) {
    return;
  }

  UpdatePolicy();
}

void FrameSchedulerImpl::SetCrossOriginToNearestMainFrame(bool cross_origin) {
  DCHECK(parent_page_scheduler_);
  if (frame_origin_type_ == FrameOriginType::kMainFrame) {
    DCHECK(!cross_origin);
    return;
  }

  if (cross_origin) {
    frame_origin_type_ = FrameOriginType::kCrossOriginToMainFrame;
  } else {
    frame_origin_type_ = FrameOriginType::kSameOriginToMainFrame;
  }

  UpdatePolicy();
}

void FrameSchedulerImpl::SetIsAdFrame(bool is_ad_frame) {
  is_ad_frame_ = is_ad_frame;
  UpdatePolicy();
}

bool FrameSchedulerImpl::IsAdFrame() const {
  return is_ad_frame_;
}

bool FrameSchedulerImpl::IsInEmbeddedFrameTree() const {
  return is_in_embedded_frame_tree_;
}

bool FrameSchedulerImpl::IsCrossOriginToNearestMainFrame() const {
  return frame_origin_type_ == FrameOriginType::kCrossOriginToMainFrame;
}

void FrameSchedulerImpl::SetAgentClusterId(
    const base::UnguessableToken& agent_cluster_id) {
  PolicyUpdater policy_updater;

  if (AreFrameAndPageVisible()) {
    GetAgentGroupSchedulerImpl().IncrementVisibleFramesForAgent(
        agent_cluster_id, policy_updater);
    GetAgentGroupSchedulerImpl().DecrementVisibleFramesForAgent(
        agent_cluster_id_, policy_updater);
  }

  agent_cluster_id_ = agent_cluster_id;
  policy_updater.UpdateFramePolicy(this);
}

void FrameSchedulerImpl::TraceUrlChange(const String& url) {
  url_tracer_.TraceString(url);
}

void FrameSchedulerImpl::AddTaskTime(base::TimeDelta time) {
  // The duration of task time under which AddTaskTime buffers rather than
  // sending the task time update to the delegate.
  constexpr base::TimeDelta kTaskDurationSendThreshold =
      base::Milliseconds(100);
  if (!delegate_)
    return;
  unreported_task_time_ += time;
  if (unreported_task_time_ >= kTaskDurationSendThreshold) {
    delegate_->UpdateTaskTime(unreported_task_time_);
    unreported_task_time_ = base::TimeDelta();
  }
}

FrameScheduler::FrameType FrameSchedulerImpl::GetFrameType() const {
  return frame_type_;
}

// static
QueueTraits FrameSchedulerImpl::CreateQueueTraitsForTaskType(TaskType type) {
  // TODO(sreejakshetty): Clean up the PrioritisationType QueueTrait and
  // QueueType for kInternalContinueScriptLoading and kInternalContentCapture.
  switch (type) {
    case TaskType::kInternalContentCapture:
      return ThrottleableTaskQueueTraits().SetPrioritisationType(
          QueueTraits::PrioritisationType::kBestEffort);
    case TaskType::kJavascriptTimerDelayedLowNesting:
      return ThrottleableTaskQueueTraits().SetPrioritisationType(
          QueueTraits::PrioritisationType::kJavaScriptTimer);
    case TaskType::kJavascriptTimerDelayedHighNesting:
      return ThrottleableTaskQueueTraits()
          .SetPrioritisationType(
              QueueTraits::PrioritisationType::kJavaScriptTimer)
          .SetCanBeIntensivelyThrottled(IsIntensiveWakeUpThrottlingEnabled());
    case TaskType::kJavascriptTimerImmediate: {
      // Immediate timers are not throttled.
      return DeferrableTaskQueueTraits().SetPrioritisationType(
          QueueTraits::PrioritisationType::kJavaScriptTimer);
    }
    case TaskType::kInternalLoading:
    case TaskType::kNetworking:
      return LoadingTaskQueueTraits();
    case TaskType::kNetworkingUnfreezable:
      return IsInflightNetworkRequestBackForwardCacheSupportEnabled()
                 ? UnfreezableLoadingTaskQueueTraits()
                 : LoadingTaskQueueTraits();
    case TaskType::kNetworkingUnfreezableRenderBlockingLoading: {
      QueueTraits queue_traits =
          IsInflightNetworkRequestBackForwardCacheSupportEnabled()
              ? UnfreezableLoadingTaskQueueTraits()
              : LoadingTaskQueueTraits();
      queue_traits.SetPrioritisationType(
          QueueTraits::PrioritisationType::kRenderBlocking);
      return queue_traits;
    }
    case TaskType::kNetworkingControl:
      return LoadingControlTaskQueueTraits();
    case TaskType::kLowPriorityScriptExecution:
      return LoadingTaskQueueTraits().SetPrioritisationType(
          QueueTraits::PrioritisationType::kAsyncScript);
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
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kWebGPU:
    case TaskType::kIdleTask:
    case TaskType::kInternalDefault:
    case TaskType::kMiscPlatformAPI:
    case TaskType::kFontLoading:
    case TaskType::kApplicationLifeCycle:
    case TaskType::kBackgroundFetch:
    case TaskType::kPermission:
    case TaskType::kWakeLock:
    case TaskType::kStorage:
    case TaskType::kClipboard:
    case TaskType::kMachineLearning:
      // TODO(altimin): Move appropriate tasks to throttleable task queue.
      return DeferrableTaskQueueTraits();
    case TaskType::kFileReading:
      // This is used by Blob operations (BlobURLStore in particular, which is
      // associated to BlobRegistry) and should run with VT paused to prevent
      // deadlocks when reading network requests as Blobs. See crbug.com/1455267
      // for more details.
      return DeferrableTaskQueueTraits().SetCanRunWhenVirtualTimePaused(true);
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
    case TaskType::kInternalNavigationCancellation:
      return FreezableTaskQueueTraits().SetPrioritisationType(
          QueueTraits::PrioritisationType::kInternalNavigationCancellation);
    case TaskType::kInternalInputBlocking:
      return InputBlockingQueueTraits();
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
      return CanRunWhenVirtualTimePausedTaskQueueTraits();
    case TaskType::kInternalPostMessageForwarding:
      // postMessages to remote frames hop through the scheduler so that any
      // IPCs generated in the same task arrive first. These tasks must be
      // pausable in order to maintain this invariant, otherwise they might run
      // in a nested event loop before the task completes, e.g. debugger
      // breakpoints or javascript dialogs.
      //
      // Freezing this task type would prevent transmission of postMessages to
      // remote frames that occurred in unfreezable tasks or from tasks that ran
      // prior to being frozen (e.g. freeze event handler), which is not
      // desirable. The messages are still queued on the receiving side, which
      // is where frozenness should be assessed.
      return PausableTaskQueueTraits()
          .SetCanBeFrozen(false)
          .SetPrioritisationType(
              QueueTraits::PrioritisationType::kPostMessageForwarding);
    case TaskType::kDeprecatedNone:
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueV8UserVisible:
    case TaskType::kMainThreadTaskQueueV8BestEffort:
    case TaskType::kMainThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueDefault:
    case TaskType::kMainThreadTaskQueueInput:
    case TaskType::kMainThreadTaskQueueIdle:
    case TaskType::kMainThreadTaskQueueControl:
    case TaskType::kMainThreadTaskQueueMemoryPurge:
    case TaskType::kMainThreadTaskQueueIPCTracking:
    case TaskType::kCompositorThreadTaskQueueDefault:
    case TaskType::kCompositorThreadTaskQueueInput:
    case TaskType::kWorkerThreadTaskQueueDefault:
    case TaskType::kWorkerThreadTaskQueueV8:
    case TaskType::kWorkerThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueNonWaking:
    // The web scheduling API task types are used by WebSchedulingTaskQueues.
    // The associated TaskRunner should be obtained by creating a
    // WebSchedulingTaskQueue with CreateWebSchedulingTaskQueue().
    case TaskType::kWebSchedulingPostedTask:
      // Not a valid frame-level TaskType.
      NOTREACHED_IN_MIGRATION();
      return QueueTraits();
  }
  // This method is called for all values between 0 and kCount. TaskType,
  // however, has numbering gaps, so even though all enumerated TaskTypes are
  // handled in the switch and return a value, we fall through for some values
  // of |type|.
  NOTREACHED_IN_MIGRATION();
  return QueueTraits();
}

scoped_refptr<base::SingleThreadTaskRunner> FrameSchedulerImpl::GetTaskRunner(
    TaskType type) {
  auto it = task_runners_.find(type);
  if (it == task_runners_.end()) {
    scoped_refptr<MainThreadTaskQueue> task_queue = GetTaskQueue(type);
    DCHECK(task_queue);
    auto res = task_queue->CreateTaskRunner(type);
    task_runners_.insert(type, res);
    return res;
  }
  return it->value;
}

scoped_refptr<MainThreadTaskQueue> FrameSchedulerImpl::GetTaskQueue(
    TaskType type) {
  QueueTraits queue_traits = CreateQueueTraitsForTaskType(type);
  queue_traits = queue_traits.SetCanBeDeferredForRendering(
      ComputeCanBeDeferredForRendering(queue_traits.can_be_deferred, type));
  return frame_task_queue_controller_->GetTaskQueue(queue_traits);
}

scoped_refptr<base::SingleThreadTaskRunner>
FrameSchedulerImpl::ControlTaskRunner() {
  DCHECK(parent_page_scheduler_);
  return main_thread_scheduler_->ControlTaskRunner();
}

AgentGroupScheduler* FrameSchedulerImpl::GetAgentGroupScheduler() {
  return parent_page_scheduler_
             ? &parent_page_scheduler_->GetAgentGroupScheduler()
             : nullptr;
}

blink::PageScheduler* FrameSchedulerImpl::GetPageScheduler() const {
  return parent_page_scheduler_;
}

void FrameSchedulerImpl::DidStartProvisionalLoad() {
  main_thread_scheduler_->DidStartProvisionalLoad(
      frame_type_ == FrameScheduler::FrameType::kMainFrame &&
      !is_in_embedded_frame_tree_);
}

void FrameSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    NavigationType navigation_type,
    DidCommitProvisionalLoadParams params) {
  bool is_outermost_main_frame =
      GetFrameType() == FrameType::kMainFrame && !is_in_embedded_frame_tree_;
  bool is_same_document = navigation_type == NavigationType::kSameDocument;

  if (!is_same_document) {
    waiting_for_contentful_paint_ = true;
    waiting_for_meaningful_paint_ = true;
    is_load_event_dispatched_ = false;
  }

  if (is_outermost_main_frame && !is_same_document) {
    unreported_task_time_ = base::TimeDelta();
  } else {
    unreported_task_time_ = params.previous_document_unreported_task_time;
  }

  main_thread_scheduler_->DidCommitProvisionalLoad(
      is_web_history_inert_commit, navigation_type == NavigationType::kReload,
      is_outermost_main_frame);
  if (!is_same_document)
    ResetForNavigation();
}

WebScopedVirtualTimePauser FrameSchedulerImpl::CreateWebScopedVirtualTimePauser(
    const WTF::String& name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(main_thread_scheduler_, duration, name);
}

scoped_refptr<base::SingleThreadTaskRunner>
FrameSchedulerImpl::CompositorTaskRunner() {
  return parent_page_scheduler_->GetAgentGroupScheduler()
      .CompositorTaskRunner();
}

void FrameSchedulerImpl::ResetForNavigation() {
  document_bound_weak_factory_.InvalidateWeakPtrs();
  back_forward_cache_disabling_feature_tracker_.Reset();
}

bool FrameSchedulerImpl::IsImportant() const {
  // Hidden frame is never important.
  if (!AreFrameAndPageVisible()) {
    return false;
  }

  return is_visible_area_large_ || had_user_activation_;
}

bool FrameSchedulerImpl::AreFrameAndPageVisible() const {
  return frame_visible_ && page_visibility_ == PageVisibilityState::kVisible;
}

void FrameSchedulerImpl::OnStartedUsingNonStickyFeature(
    SchedulingPolicy::Feature feature,
    const SchedulingPolicy& policy,
    std::unique_ptr<SourceLocation> source_location,
    SchedulingAffectingFeatureHandle* handle) {
  if (policy.disable_aggressive_throttling)
    OnAddedAggressiveThrottlingOptOut();
  if (policy.disable_back_forward_cache) {
    back_forward_cache_disabling_feature_tracker_.AddNonStickyFeature(
        feature, std::move(source_location), handle);
  }
  if (policy.disable_align_wake_ups) {
    DisableAlignWakeUpsForProcess();
  }

  if (feature == SchedulingPolicy::Feature::kWebRTC) {
    if (base::FeatureList::IsEnabled(
            kRendererMainIsDefaultThreadTypeForWebRTC) &&
        base::PlatformThread::GetCurrentThreadType() ==
            base::ThreadType::kDisplayCritical) {
      base::PlatformThread::SetCurrentThreadType(base::ThreadType::kDefault);
    }

    if (auto* rc = delegate_->GetDocumentResourceCoordinator()) {
      rc->OnStartedUsingWebRTC();
    }
  }
}

void FrameSchedulerImpl::OnStartedUsingStickyFeature(
    SchedulingPolicy::Feature feature,
    const SchedulingPolicy& policy,
    std::unique_ptr<SourceLocation> source_location) {
  if (policy.disable_aggressive_throttling)
    OnAddedAggressiveThrottlingOptOut();
  if (policy.disable_back_forward_cache) {
    back_forward_cache_disabling_feature_tracker_.AddStickyFeature(
        feature, std::move(source_location));
  }
  if (policy.disable_align_wake_ups) {
    DisableAlignWakeUpsForProcess();
  }
}

void FrameSchedulerImpl::OnStoppedUsingNonStickyFeature(
    SchedulingAffectingFeatureHandle* handle) {
  if (handle->GetPolicy().disable_aggressive_throttling)
    OnRemovedAggressiveThrottlingOptOut();
  if (handle->GetPolicy().disable_back_forward_cache) {
    back_forward_cache_disabling_feature_tracker_.Remove(
        handle->GetFeatureAndJSLocationBlockingBFCache());
  }

  if (handle->GetFeature() == SchedulingPolicy::Feature::kWebRTC) {
    if (auto* rc = delegate_->GetDocumentResourceCoordinator()) {
      rc->OnStoppedUsingWebRTC();
    }
  }
}

base::WeakPtr<FrameScheduler> FrameSchedulerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<const FrameSchedulerImpl> FrameSchedulerImpl::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

void FrameSchedulerImpl::ReportActiveSchedulerTrackedFeatures() {
  back_forward_cache_disabling_feature_tracker_.ReportFeaturesToDelegate();
}

base::WeakPtr<FrameSchedulerImpl>
FrameSchedulerImpl::GetInvalidatingOnBFCacheRestoreWeakPtr() {
  return invalidating_on_bfcache_restore_weak_factory_.GetWeakPtr();
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

void FrameSchedulerImpl::OnTaskCompleted(TaskQueue::TaskTiming* timing) {
  if (delegate_) {
    delegate_->OnTaskCompleted(timing->start_time(), timing->end_time());
  }
}

void FrameSchedulerImpl::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame_visible", frame_visible_);
  dict.Add("page_visible", parent_page_scheduler_->IsPageVisible());
  dict.Add("cross_origin_to_main_frame", IsCrossOriginToNearestMainFrame());
  dict.Add("frame_type", frame_type_ == FrameScheduler::FrameType::kMainFrame
                             ? "MainFrame"
                             : "Subframe");
  dict.Add("is_visible_area_large", is_visible_area_large_);
  dict.Add("had_user_activation", had_user_activation_);
  dict.Add("disable_background_timer_throttling",
           !RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled());

  dict.Add("frame_task_queue_controller", frame_task_queue_controller_);
}

void FrameSchedulerImpl::WriteIntoTrace(
    perfetto::TracedProto<
        perfetto::protos::pbzero::RendererMainThreadTaskExecution> proto)
    const {
  proto->set_frame_visible(frame_visible_);
  proto->set_page_visible(parent_page_scheduler_->IsPageVisible());
  proto->set_frame_type(
      frame_type_ == FrameScheduler::FrameType::kMainFrame
          ? RendererMainThreadTaskExecution::FRAME_TYPE_MAIN_FRAME
      : IsCrossOriginToNearestMainFrame()
          ? RendererMainThreadTaskExecution::FRAME_TYPE_CROSS_ORIGIN_SUBFRAME
          : RendererMainThreadTaskExecution::FRAME_TYPE_SAME_ORIGIN_SUBFRAME);
  proto->set_is_ad_frame(is_ad_frame_);
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
    if (task_queue->CanBeFrozen()) {
      task_queue->SetShouldReportPostedTasksWhenDisabled(should_report);
    }
  }
}

void FrameSchedulerImpl::SetPageFrozenForTracing(bool frozen) {
  page_frozen_for_tracing_ = frozen;
}


void FrameSchedulerImpl::UpdatePolicy() {
  base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());

  ThrottlingType previous_throttling_type = throttling_type_;
  throttling_type_ = ComputeThrottlingType();

  if (throttling_type_ == ThrottlingType::kNone) {
    throttled_task_queue_handles_.clear();
  }

  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    auto* task_queue = task_queue_and_voter.first;
    UpdateQueuePolicy(task_queue, task_queue_and_voter.second);

    if (!task_queue->CanBeThrottled()) {
      continue;
    }

    auto* new_wake_up_budget_pool = parent_page_scheduler_->GetWakeUpBudgetPool(
        task_queue, frame_origin_type_, throttling_type_);
    if (task_queue->GetWakeUpBudgetPool() != new_wake_up_budget_pool) {
      parent_page_scheduler_->RemoveQueueFromWakeUpBudgetPool(task_queue,
                                                              &lazy_now);
      parent_page_scheduler_->AddQueueToWakeUpBudgetPool(
          task_queue, new_wake_up_budget_pool, &lazy_now);
    }

    if (throttling_type_ != ThrottlingType::kNone &&
        previous_throttling_type == ThrottlingType::kNone) {
      MainThreadTaskQueue::ThrottleHandle handle =
          task_queue_and_voter.first->Throttle();
      throttled_task_queue_handles_.push_back(std::move(handle));
    }
  }

  NotifyLifecycleObservers();
}

void FrameSchedulerImpl::UpdateQueuePolicy(
    MainThreadTaskQueue* queue,
    TaskQueue::QueueEnabledVoter* voter) {
  DCHECK(queue);
  UpdatePriority(queue);

  DCHECK(voter);
  DCHECK(parent_page_scheduler_);
  bool queue_disabled = false;
  queue_disabled |= frame_paused_ && queue->CanBePaused();
  queue_disabled |= preempted_for_cooperative_scheduling_;
  // Per-frame freezable task queues will be frozen after 5 mins in background
  // on Android, and if the browser freezes the page in the background. They
  // will be resumed when the page is visible.
  bool queue_frozen =
      parent_page_scheduler_->IsFrozen() && queue->CanBeFrozen();
  queue_disabled |= queue_frozen;
  // Per-frame freezable queues of tasks which are specified as getting frozen
  // immediately when their frame becomes invisible get frozen. They will be
  // resumed when the frame becomes visible again.
  queue_disabled |= !frame_visible_ && !queue->CanRunInBackground();
  if (queue_disabled) {
    TRACE_EVENT_INSTANT("renderer.scheduler",
                        "FrameSchedulerImpl::UpdateQueuePolicy_QueueDisabled");
  } else {
    TRACE_EVENT_INSTANT("renderer.scheduler",
                        "FrameSchedulerImpl::UpdateQueuePolicy_QueueEnabled");
  }
  voter->SetVoteToEnable(!queue_disabled);
}

SchedulingLifecycleState FrameSchedulerImpl::CalculateLifecycleState(
    ObserverType type) const {
  // Detached frames are not throttled.
  if (!parent_page_scheduler_)
    return SchedulingLifecycleState::kNotThrottled;
  if (parent_page_scheduler_->IsFrozen()) {
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

void FrameSchedulerImpl::OnFirstContentfulPaintInMainFrame() {
  waiting_for_contentful_paint_ = false;
  DCHECK_EQ(GetFrameType(), FrameScheduler::FrameType::kMainFrame);
  main_thread_scheduler_->OnMainFramePaint();
}

void FrameSchedulerImpl::OnMainFrameInteractive() {
  if (delegate_) {
    return delegate_->MainFrameInteractive();
  }
}

void FrameSchedulerImpl::OnFirstMeaningfulPaint(base::TimeTicks timestamp) {
  waiting_for_meaningful_paint_ = false;
  first_meaningful_paint_timestamp_ = timestamp;

  if (GetFrameType() != FrameScheduler::FrameType::kMainFrame ||
      is_in_embedded_frame_tree_) {
    return;
  }

  main_thread_scheduler_->OnMainFramePaint();
  if (delegate_) {
    return delegate_->MainFrameFirstMeaningfulPaint();
  }
}

void FrameSchedulerImpl::OnDispatchLoadEvent() {
  is_load_event_dispatched_ = true;
}

bool FrameSchedulerImpl::IsWaitingForContentfulPaint() const {
  return waiting_for_contentful_paint_;
}

bool FrameSchedulerImpl::IsWaitingForMeaningfulPaint() const {
  return waiting_for_meaningful_paint_;
}

bool FrameSchedulerImpl::IsLoading() const {
  if (waiting_for_meaningful_paint_) {
    return true;
  }

  if (is_load_event_dispatched_) {
    return false;
  }

  return base::TimeTicks::Now() - first_meaningful_paint_timestamp_ <=
         GetLoadingPhaseBufferTimeAfterFirstMeaningfulPaint();
}

bool FrameSchedulerImpl::IsOrdinary() const {
  if (!parent_page_scheduler_)
    return true;
  return parent_page_scheduler_->IsOrdinary();
}

ThrottlingType FrameSchedulerImpl::ComputeThrottlingType() {
  DCHECK(parent_page_scheduler_);

  const bool page_can_be_throttled_intensively =
      !parent_page_scheduler_->IsAudioPlaying() &&
      !parent_page_scheduler_->IsPageVisible();

  const bool frame_can_be_throttled_background =
      !AreFrameAndPageVisible() && !parent_page_scheduler_->IsAudioPlaying() &&
      !(parent_page_scheduler_->IsPageVisible() &&
        !IsCrossOriginToNearestMainFrame()) &&
      !(base::FeatureList::IsEnabled(features::kNoThrottlingVisibleAgent) &&
        GetAgentGroupSchedulerImpl().IsAgentVisible(agent_cluster_id_));

  const bool frame_can_be_throttled_foreground =
      IsCrossOriginToNearestMainFrame() && !IsImportant() &&
      base::FeatureList::IsEnabled(features::kThrottleUnimportantFrameTimers);

  if (RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled()) {
    if (frame_can_be_throttled_background) {
      if (page_can_be_throttled_intensively) {
        return ThrottlingType::kBackgroundIntensive;
      }
      return ThrottlingType::kBackground;
    }
  }

  if (frame_can_be_throttled_foreground) {
    return ThrottlingType::kForegroundUnimportant;
  }

  return ThrottlingType::kNone;
}

bool FrameSchedulerImpl::IsExemptFromBudgetBasedThrottling() const {
  return opted_out_from_aggressive_throttling();
}

TaskPriority FrameSchedulerImpl::ComputePriority(
    MainThreadTaskQueue* task_queue) const {
  DCHECK(task_queue);

  FrameScheduler* frame_scheduler = task_queue->GetFrameScheduler();

  // Checks the task queue is associated with this frame scheduler.
  DCHECK_EQ(frame_scheduler, this);

  // TODO(crbug.com/986569): Ordering here is relative to the experiments below.
  // Cleanup unused experiment logic so that this switch can be merged with the
  // prioritisation type decisions below.
  switch (task_queue->GetPrioritisationType()) {
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::
        kInternalScriptContinuation:
      return TaskPriority::kVeryHighPriority;
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::kBestEffort:
      return TaskPriority::kBestEffortPriority;
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::
        kPostMessageForwarding:
      return TaskPriority::kVeryHighPriority;
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::
        kInternalNavigationCancellation:
      return TaskPriority::kVeryHighPriority;
    default:
      break;
  }

  // TODO(shaseley): This should use lower priorities if the frame is
  // deprioritized. Change this once we refactor and add frame policy/priorities
  // and add a range of new priorities less than low.
  if (std::optional<WebSchedulingQueueType> queue_type =
          task_queue->GetWebSchedulingQueueType()) {
    bool is_continuation =
        *queue_type == WebSchedulingQueueType::kContinuationQueue;
    switch (*task_queue->GetWebSchedulingPriority()) {
      case WebSchedulingPriority::kUserBlockingPriority:
        return is_continuation ? TaskPriority::kHighPriorityContinuation
                               : TaskPriority::kHighPriority;
      case WebSchedulingPriority::kUserVisiblePriority:
        return is_continuation ? TaskPriority::kNormalPriorityContinuation
                               : TaskPriority::kNormalPriority;
      case WebSchedulingPriority::kBackgroundPriority:
        return is_continuation ? TaskPriority::kLowPriorityContinuation
                               : TaskPriority::kLowPriority;
    }
  }

  if (!parent_page_scheduler_) {
    // Frame might be detached during its shutdown. Return a default priority
    // in that case.
    return TaskPriority::kNormalPriority;
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kLoadingControl) {
    return TaskPriority::kHighPriority;
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kFindInPage) {
    return main_thread_scheduler_->find_in_page_priority();
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::
          kHighPriorityLocalFrame) {
    return TaskPriority::kHighestPriority;
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kInput) {
    return TaskPriority::kHighestPriority;
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::
          kExperimentalDatabase) {
    // TODO(shaseley): This decision should probably be based on Agent
    // visibility. Consider changing this before shipping anything.
    return parent_page_scheduler_->IsPageVisible()
               ? TaskPriority::kHighPriority
               : TaskPriority::kNormalPriority;
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kRenderBlocking) {
    return parent_page_scheduler_->IsPageVisible()
               ? TaskPriority::kExtremelyHighPriority
               : TaskPriority::kNormalPriority;
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kAsyncScript) {
    return low_priority_async_script_task_priority_;
  }

  return TaskPriority::kNormalPriority;
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

  UpdateQueuePolicy(task_queue, voter);

  if (task_queue->CanBeThrottled()) {
    base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());

    CPUTimeBudgetPool* cpu_time_budget_pool =
        parent_page_scheduler_->background_cpu_time_budget_pool();
    if (cpu_time_budget_pool) {
      task_queue->AddToBudgetPool(lazy_now.Now(), cpu_time_budget_pool);
    }

    if (throttling_type_ != ThrottlingType::kNone) {
      parent_page_scheduler_->AddQueueToWakeUpBudgetPool(
          task_queue,
          parent_page_scheduler_->GetWakeUpBudgetPool(
              task_queue, frame_origin_type_, throttling_type_),
          &lazy_now);

      MainThreadTaskQueue::ThrottleHandle handle = task_queue->Throttle();
      throttled_task_queue_handles_.push_back(std::move(handle));
    }
  }
}

void FrameSchedulerImpl::SetOnIPCTaskPostedWhileInBackForwardCacheHandler() {
  DCHECK(parent_page_scheduler_->IsInBackForwardCache());
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
              base::Seconds(1));
        },
        main_thread_scheduler_->BackForwardCacheIpcTrackingTaskRunner(),
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

  DCHECK(parent_page_scheduler_->IsInBackForwardCache());
  base::UmaHistogramSparse(
      "BackForwardCache.Experimental.UnexpectedIPCMessagePostedToCachedFrame."
      "MethodHash",
      static_cast<int32_t>(ipc_hash));

  base::TimeDelta duration =
      main_thread_scheduler_->NowTicks() -
      parent_page_scheduler_->GetStoredInBackForwardCacheTimestamp();
  base::UmaHistogramCustomTimes(
      "BackForwardCache.Experimental.UnexpectedIPCMessagePostedToCachedFrame."
      "TimeUntilIPCReceived",
      duration, base::TimeDelta(), base::Minutes(5), 100);
}

WTF::HashSet<SchedulingPolicy::Feature>
FrameSchedulerImpl::GetActiveFeaturesTrackedForBackForwardCacheMetrics() {
  return back_forward_cache_disabling_feature_tracker_
      .GetActiveFeaturesTrackedForBackForwardCacheMetrics();
}

base::WeakPtr<FrameOrWorkerScheduler>
FrameSchedulerImpl::GetFrameOrWorkerSchedulerWeakPtr() {
  // We reset feature sets upon frame navigation, so having a document-bound
  // weak pointer ensures that the feature handle associated with previous
  // document can't influence the new one.
  return document_bound_weak_factory_.GetWeakPtr();
}

std::unique_ptr<WebSchedulingTaskQueue>
FrameSchedulerImpl::CreateWebSchedulingTaskQueue(
    WebSchedulingQueueType queue_type,
    WebSchedulingPriority priority) {
  bool can_be_deferred_for_rendering = ComputeCanBeDeferredForRendering(
      /*is_deferrable_for_touchstart=*/true,
      TaskType::kWebSchedulingPostedTask);

  // The QueueTraits for scheduler.postTask() are similar to those of
  // setTimeout() (deferrable queue traits + throttling for delayed tasks), with
  // the following differences:
  //  1. All delayed tasks are intensively throttled (no nesting-level exception
  //     or policy/flag opt-out)
  //  2. There is no separate PrioritisationType (prioritization is based on the
  //     WebSchedulingPriority, which is only set for these task queues)
  scoped_refptr<MainThreadTaskQueue> immediate_task_queue =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          DeferrableTaskQueueTraits().SetCanBeDeferredForRendering(
              can_be_deferred_for_rendering),
          queue_type, priority);
  // Continuation task queues can only be used for immediate tasks since there
  // the yield API doesn't support delayed continuations.
  if (queue_type == WebSchedulingQueueType::kContinuationQueue) {
    return std::make_unique<MainThreadWebSchedulingTaskQueueImpl>(
        immediate_task_queue->AsWeakPtr(), nullptr);
  }
  scoped_refptr<MainThreadTaskQueue> delayed_task_queue =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          DeferrableTaskQueueTraits()
              .SetCanBeThrottled(true)
              .SetCanBeIntensivelyThrottled(true)
              .SetCanBeDeferredForRendering(can_be_deferred_for_rendering),
          queue_type, priority);
  return std::make_unique<MainThreadWebSchedulingTaskQueueImpl>(
      immediate_task_queue->AsWeakPtr(), delayed_task_queue->AsWeakPtr());
}

void FrameSchedulerImpl::OnWebSchedulingTaskQueuePriorityChanged(
    MainThreadTaskQueue* queue) {
  UpdateQueuePolicy(queue,
                    frame_task_queue_controller_->GetQueueEnabledVoter(queue));
  main_thread_scheduler_->OnWebSchedulingTaskQueuePriorityChanged(queue);
}

void FrameSchedulerImpl::OnWebSchedulingTaskQueueDestroyed(
    MainThreadTaskQueue* queue) {
  if (queue->CanBeThrottled()) {
    RemoveThrottleableQueueFromBudgetPools(queue);
  }

  // Don't run web scheduling tasks after detach.
  queue->ShutdownTaskQueue();

  // After this is called, the queue will be destroyed. Do not attempt
  // to use it further.
  frame_task_queue_controller_->RemoveWebSchedulingTaskQueue(queue);
}

const base::UnguessableToken& FrameSchedulerImpl::GetAgentClusterId() const {
  if (!delegate_)
    return base::UnguessableToken::Null();
  return delegate_->GetAgentClusterId();
}

base::TimeDelta FrameSchedulerImpl::UnreportedTaskTime() const {
  return unreported_task_time_;
}

bool FrameSchedulerImpl::ComputeCanBeDeferredForRendering(
    bool is_deferrable_for_touchstart,
    TaskType task_type) const {
  if (!base::FeatureList::IsEnabled(features::kDeferRendererTasksAfterInput)) {
    return false;
  }
  std::optional<features::TaskDeferralPolicy> policy =
      main_thread_scheduler_->scheduling_settings()
          .discrete_input_task_deferral_policy;
  CHECK(policy);
  switch (*policy) {
    case features::TaskDeferralPolicy::kMinimalTypes:
      return task_type == TaskType::kDOMManipulation ||
             task_type == TaskType::kIdleTask ||
             task_type == TaskType::kWebSchedulingPostedTask;
    case features::TaskDeferralPolicy::kNonUserBlockingDeferrableTypes:
    case features::TaskDeferralPolicy::kAllDeferrableTypes:
      // kPosteMessaged is used for scheduling, so unlike touchstart deferral,
      // consider this a deferrable type.
      return is_deferrable_for_touchstart ||
             task_type == TaskType::kPostedMessage;
    case features::TaskDeferralPolicy::kNonUserBlockingTypes:
    case features::TaskDeferralPolicy::kAllTypes:
      return true;
  }
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
      .SetCanBeFrozen(true)
      .SetCanBePaused(true)
      .SetCanRunWhenVirtualTimePaused(false)
      .SetCanBePausedForAndroidWebview(true);
}

// static
MainThreadTaskQueue::QueueTraits FrameSchedulerImpl::PausableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeFrozen(true)
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
FrameSchedulerImpl::CanRunWhenVirtualTimePausedTaskQueueTraits() {
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
FrameSchedulerImpl::UnfreezableLoadingTaskQueueTraits() {
  return LoadingTaskQueueTraits().SetCanBeFrozen(false);
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

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::InputBlockingQueueTraits() {
  return QueueTraits().SetPrioritisationType(
      QueueTraits::PrioritisationType::kInput);
}
}  // namespace scheduler
}  // namespace blink
