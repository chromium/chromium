// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>
#include <set>
#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/blame_context.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/resource_loading_task_runner_handle_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"
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

// Extract a substring from |source| from [start to end), trimming leading
// whitespace.
std::string ExtractAndTrimString(std::string source, size_t start, size_t end) {
  DCHECK(start < source.length());
  DCHECK(end <= source.length());
  DCHECK(start <= end);
  // Trim whitespace
  while (start < end && source[start] == ' ')
    ++start;
  if (start < end)
    return source.substr(start, end - start);
  return "";
}

std::set<std::string> TaskTypesFromFieldTrialParam(const char* param) {
  std::set<std::string> result;
  std::string task_type_list = base::GetFieldTrialParamValueByFeature(
      kThrottleAndFreezeTaskTypes, param);
  if (!task_type_list.length())
    return result;
  // Extract the individual names, separated by ",".
  size_t pos = 0, start = 0;
  while ((pos = task_type_list.find(',', start)) != std::string::npos) {
    std::string task_type = ExtractAndTrimString(task_type_list, start, pos);
    // Not valid to start with "," or have ",," in the list.
    DCHECK(task_type.length());
    result.insert(task_type);
    start = pos + 1;
  }
  // Handle the last or only task type name.
  std::string task_type =
      ExtractAndTrimString(task_type_list, start, task_type_list.length());
  DCHECK(task_type.length());
  result.insert(task_type);

  return result;
}

}  // namespace

FrameSchedulerImpl::ActiveConnectionHandleImpl::ActiveConnectionHandleImpl(
    FrameSchedulerImpl* frame_scheduler)
    : frame_scheduler_(frame_scheduler->GetWeakPtr()) {
  frame_scheduler->DidOpenActiveConnection();
}

FrameSchedulerImpl::ActiveConnectionHandleImpl::~ActiveConnectionHandleImpl() {
  if (frame_scheduler_) {
    static_cast<FrameSchedulerImpl*>(frame_scheduler_.get())
        ->DidCloseActiveConnection();
  }
}

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

std::unique_ptr<FrameSchedulerImpl> FrameSchedulerImpl::Create(
    PageSchedulerImpl* parent_page_scheduler,
    FrameScheduler::Delegate* delegate,
    base::trace_event::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type) {
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler(new FrameSchedulerImpl(
      parent_page_scheduler->GetMainThreadScheduler(), parent_page_scheduler,
      delegate, blame_context, frame_type));
  parent_page_scheduler->RegisterFrameSchedulerImpl(frame_scheduler.get());
  return frame_scheduler;
}

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
                     this,
                     &tracing_controller_,
                     VisibilityStateToString),
      frame_paused_(false,
                    "FrameScheduler.FramePaused",
                    this,
                    &tracing_controller_,
                    PausedStateToString),
      frame_origin_type_(frame_type == FrameType::kMainFrame
                             ? FrameOriginType::kMainFrame
                             : FrameOriginType::kSameOriginFrame,
                         "FrameScheduler.Origin",
                         this,
                         &tracing_controller_,
                         FrameOriginTypeToString),
      subresource_loading_paused_(false,
                                  "FrameScheduler.SubResourceLoadingPaused",
                                  this,
                                  &tracing_controller_,
                                  PausedStateToString),
      url_tracer_("FrameScheduler.URL", this),
      task_queues_throttled_(false,
                             "FrameScheduler.TaskQueuesThrottled",
                             this,
                             &tracing_controller_,
                             YesNoStateToString),
      active_connection_count_(0),
      subresource_loading_pause_count_(0u),
      has_active_connection_(false,
                             "FrameScheduler.HasActiveConnection",
                             this,
                             &tracing_controller_,
                             YesNoStateToString),
      page_frozen_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->IsFrozen() : true,
          "FrameScheduler.PageFrozen",
          this,
          &tracing_controller_,
          FrozenStateToString),
      page_visibility_for_tracing_(
          parent_page_scheduler_ && parent_page_scheduler_->IsPageVisible()
              ? PageVisibilityState::kVisible
              : PageVisibilityState::kHidden,
          "FrameScheduler.PageVisibility",
          this,
          &tracing_controller_,
          PageVisibilityStateToString),
      page_keep_active_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->KeepActive() : false,
          "FrameScheduler.KeepActive",
          this,
          &tracing_controller_,
          KeepActiveStateToString),
      weak_factory_(this) {
  frame_task_queue_controller_.reset(
      new FrameTaskQueueController(main_thread_scheduler_, this, this));
}

FrameSchedulerImpl::FrameSchedulerImpl()
    : FrameSchedulerImpl(nullptr,
                         nullptr,
                         nullptr,
                         nullptr,
                         FrameType::kSubframe) {}

namespace {

void CleanUpQueue(MainThreadTaskQueue* queue) {
  DCHECK(queue);

  queue->DetachFromMainThreadScheduler();
  queue->DetachFromFrameScheduler();
  queue->SetBlameContext(nullptr);
  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
}

}  // namespace

FrameSchedulerImpl::~FrameSchedulerImpl() {
  weak_factory_.InvalidateWeakPtrs();

  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool(
          task_queue_and_voter.first);
    }
    CleanUpQueue(task_queue_and_voter.first);
  }

  if (parent_page_scheduler_) {
    parent_page_scheduler_->Unregister(this);

    if (has_active_connection())
      parent_page_scheduler_->OnConnectionUpdated();
  }
}

void FrameSchedulerImpl::DetachFromPageScheduler() {
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    if (task_queue_and_voter.first->CanBeThrottled()) {
      RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool(
          task_queue_and_voter.first);
    }
  }

  parent_page_scheduler_ = nullptr;
}

void FrameSchedulerImpl::RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool(
    MainThreadTaskQueue* task_queue) {
  DCHECK(task_queue);
  DCHECK(task_queue->CanBeThrottled());

  if (!parent_page_scheduler_)
    return;

  CPUTimeBudgetPool* time_budget_pool =
      parent_page_scheduler_->BackgroundCPUTimeBudgetPool();

  if (!time_budget_pool)
    return;

  // On tests, the scheduler helper might already be shut down and tick is not
  // available.
  base::TimeTicks now;
  if (main_thread_scheduler_->tick_clock())
    now = main_thread_scheduler_->tick_clock()->NowTicks();
  else
    now = base::TimeTicks::Now();
  time_budget_pool->RemoveQueue(now, task_queue);
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

void FrameSchedulerImpl::SetCrossOrigin(bool cross_origin) {
  DCHECK(parent_page_scheduler_);
  if (frame_origin_type_ == FrameOriginType::kMainFrame) {
    DCHECK(!cross_origin);
    return;
  }
  if (cross_origin) {
    frame_origin_type_ = FrameOriginType::kCrossOriginFrame;
  } else {
    frame_origin_type_ = FrameOriginType::kSameOriginFrame;
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

bool FrameSchedulerImpl::IsCrossOrigin() const {
  return frame_origin_type_ == FrameOriginType::kCrossOriginFrame;
}

void FrameSchedulerImpl::TraceUrlChange(const String& url) {
  url_tracer_.TraceString(url);
}

FrameScheduler::FrameType FrameSchedulerImpl::GetFrameType() const {
  return frame_type_;
}

void FrameSchedulerImpl::InitializeTaskTypeQueueTraitsMap(
    FrameTaskTypeToQueueTraitsArray& frame_task_types_to_queue_traits) {
  DCHECK_EQ(frame_task_types_to_queue_traits.size(),
            static_cast<size_t>(TaskType::kCount));
  // Using std set and strings here because field trial parameters are std
  // strings, and we cannot use WTF strings as Blink is not yet initialized.
  std::set<std::string> throttleable_task_type_names;
  std::set<std::string> freezable_task_type_names;
  if (base::FeatureList::IsEnabled(kThrottleAndFreezeTaskTypes)) {
    throttleable_task_type_names =
        TaskTypesFromFieldTrialParam(kThrottleableTaskTypesListParam);
    freezable_task_type_names =
        TaskTypesFromFieldTrialParam(kFreezableTaskTypesListParam);
  }
  for (size_t i = 0; i < static_cast<size_t>(TaskType::kCount); i++) {
    TaskType type = static_cast<TaskType>(i);
    base::Optional<QueueTraits> queue_traits =
        CreateQueueTraitsForTaskType(type);
    if (queue_traits && (throttleable_task_type_names.size() ||
                         freezable_task_type_names.size())) {
      const char* task_type_name = TaskTypeNames::TaskTypeToString(type);
      if (throttleable_task_type_names.erase(task_type_name))
        queue_traits->SetCanBeThrottled(true);
      if (freezable_task_type_names.erase(task_type_name))
        queue_traits->SetCanBeFrozen(true);
    }
    frame_task_types_to_queue_traits[i] = queue_traits;
  }
  // Protect against configuration errors.
  DCHECK(throttleable_task_type_names.empty());
  DCHECK(freezable_task_type_names.empty());
}

// static
base::Optional<QueueTraits> FrameSchedulerImpl::CreateQueueTraitsForTaskType(
    TaskType type) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  switch (type) {
    case TaskType::kJavascriptTimer:
      return ThrottleableTaskQueueTraits();
    case TaskType::kInternalLoading:
    case TaskType::kNetworking:
    case TaskType::kNetworkingWithURLLoaderAnnotation:
    case TaskType::kNetworkingControl:
      // Loading task queues are handled separately.
      return base::nullopt;
    // Throttling following tasks may break existing web pages, so tentatively
    // these are unthrottled.
    // TODO(nhiroki): Throttle them again after we're convinced that it's safe
    // or provide a mechanism that web pages can opt-out it if throttling is not
    // desirable.
    case TaskType::kDatabaseAccess:
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
      // TODO(altimin): Move appropriate tasks to throttleable task queue.
      return DeferrableTaskQueueTraits();
    // PostedMessage can be used for navigation, so we shouldn't defer it
    // when expecting a user gesture.
    case TaskType::kPostedMessage:
    case TaskType::kWorkerAnimation:
    // UserInteraction tasks should be run even when expecting a user gesture.
    case TaskType::kUserInteraction:
    // Media events should not be deferred to ensure that media playback is
    // smooth.
    case TaskType::kMediaElementEvent:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalIndexedDB:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalUserInteraction:
    case TaskType::kInternalIntersectionObserver:
      return PausableTaskQueueTraits();
    case TaskType::kInternalIPC:
    // The TaskType of Inspector tasks needs to be unpausable because they need
    // to run even on a paused page.
    case TaskType::kInternalInspector:
    // The TaskType of worker tasks needs to be unpausable (in addition to
    // unthrottled and undeferred) not to prevent service workers that may
    // control browser navigation on multiple tabs.
    case TaskType::kInternalWorker:
    // Some tasks in the tests need to run when objects are paused e.g. to hook
    // when recovering from debugger JavaScript statetment.
    case TaskType::kInternalTest:
      return UnpausableTaskQueueTraits();
    case TaskType::kInternalTranslation:
      return ForegroundOnlyTaskQueueTraits();
    case TaskType::kDeprecatedNone:
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueDefault:
    case TaskType::kMainThreadTaskQueueInput:
    case TaskType::kMainThreadTaskQueueIdle:
    case TaskType::kMainThreadTaskQueueIPC:
    case TaskType::kMainThreadTaskQueueControl:
    case TaskType::kMainThreadTaskQueueCleanup:
    case TaskType::kCompositorThreadTaskQueueDefault:
    case TaskType::kCompositorThreadTaskQueueInput:
    case TaskType::kWorkerThreadTaskQueueDefault:
    case TaskType::kWorkerThreadTaskQueueV8:
    case TaskType::kWorkerThreadTaskQueueCompositor:
    case TaskType::kExperimentalWebSchedulingUserInteraction:
    case TaskType::kExperimentalWebSchedulingBestEffort:
    case TaskType::kCount:
      // Not a valid frame-level TaskType.
      return base::nullopt;
  }
  // This method is called for all values between 0 and kCount. TaskType,
  // however, has numbering gaps, so even though all enumerated TaskTypes are
  // handled in the switch and return a value, we fall through for some values
  // of |type|.
  return base::nullopt;
}

scoped_refptr<base::SingleThreadTaskRunner> FrameSchedulerImpl::GetTaskRunner(
    TaskType type) {
  scoped_refptr<MainThreadTaskQueue> task_queue = GetTaskQueue(type);
  DCHECK(task_queue);
  return task_queue->CreateTaskRunner(type);
}

scoped_refptr<MainThreadTaskQueue> FrameSchedulerImpl::GetTaskQueue(
    TaskType type) {
  switch (type) {
    case TaskType::kInternalLoading:
    case TaskType::kNetworking:
    case TaskType::kNetworkingWithURLLoaderAnnotation:
      return frame_task_queue_controller_->LoadingTaskQueue();
    case TaskType::kNetworkingControl:
      return frame_task_queue_controller_->LoadingControlTaskQueue();
    case TaskType::kInternalInspector:
      return frame_task_queue_controller_->InspectorTaskQueue();
    case TaskType::kExperimentalWebSchedulingUserInteraction:
      return frame_task_queue_controller_->ExperimentalWebSchedulingTaskQueue(
          FrameTaskQueueController::WebSchedulingTaskQueueType::
              kWebSchedulingUserVisiblePriority);
    case TaskType::kExperimentalWebSchedulingBestEffort:
      return frame_task_queue_controller_->ExperimentalWebSchedulingTaskQueue(
          FrameTaskQueueController::WebSchedulingTaskQueueType::
              kWebSchedulingBestEffortPriority);
    default:
      // Non-loading task queue.
      DCHECK_LT(static_cast<size_t>(type),
                main_thread_scheduler_->scheduling_settings()
                    .frame_task_types_to_queue_traits.size());
      base::Optional<QueueTraits> queue_traits =
          main_thread_scheduler_->scheduling_settings()
              .frame_task_types_to_queue_traits[static_cast<size_t>(type)];
      // We don't have a QueueTraits mapping for |task_type| if it is not a
      // frame-level task type.
      DCHECK(queue_traits);
      return frame_task_queue_controller_->NonLoadingTaskQueue(
          queue_traits.value());
  }
}

std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
FrameSchedulerImpl::CreateResourceLoadingTaskRunnerHandle() {
  return CreateResourceLoadingTaskRunnerHandleImpl();
}

std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl>
FrameSchedulerImpl::CreateResourceLoadingTaskRunnerHandleImpl() {
  if (main_thread_scheduler_->scheduling_settings()
          .use_resource_fetch_priority ||
      (parent_page_scheduler_->IsLoading() &&
       main_thread_scheduler_->scheduling_settings()
           .use_resource_priorities_only_during_loading)) {
    scoped_refptr<MainThreadTaskQueue> task_queue =
        frame_task_queue_controller_->NewResourceLoadingTaskQueue();
    resource_loading_task_queue_priorities_.insert(
        task_queue, task_queue->GetQueuePriority());
    return ResourceLoadingTaskRunnerHandleImpl::WrapTaskRunner(task_queue);
  }

  return ResourceLoadingTaskRunnerHandleImpl::WrapTaskRunner(
      frame_task_queue_controller_->LoadingTaskQueue());
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

blink::PageScheduler* FrameSchedulerImpl::GetPageScheduler() const {
  return parent_page_scheduler_;
}

void FrameSchedulerImpl::DidStartProvisionalLoad(bool is_main_frame) {
  main_thread_scheduler_->DidStartProvisionalLoad(is_main_frame);
}

void FrameSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    bool is_reload,
    bool is_main_frame) {
  main_thread_scheduler_->DidCommitProvisionalLoad(is_web_history_inert_commit,
                                                   is_reload, is_main_frame);
}

WebScopedVirtualTimePauser FrameSchedulerImpl::CreateWebScopedVirtualTimePauser(
    const WTF::String& name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(main_thread_scheduler_, duration, name);
}

void FrameSchedulerImpl::DidOpenActiveConnection() {
  ++active_connection_count_;
  has_active_connection_ = static_cast<bool>(active_connection_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnConnectionUpdated();
}

void FrameSchedulerImpl::DidCloseActiveConnection() {
  DCHECK_GT(active_connection_count_, 0);
  --active_connection_count_;
  has_active_connection_ = static_cast<bool>(active_connection_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnConnectionUpdated();
}

void FrameSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("frame_visible", frame_visible_);
  state->SetBoolean("page_visible", parent_page_scheduler_->IsPageVisible());
  state->SetBoolean("cross_origin", IsCrossOrigin());
  state->SetString("frame_type",
                   frame_type_ == FrameScheduler::FrameType::kMainFrame
                       ? "MainFrame"
                       : "Subframe");
  state->SetBoolean(
      "disable_background_timer_throttling",
      !RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled());

  state->BeginDictionary("frame_task_queue_controller");
  frame_task_queue_controller_->AsValueInto(state);
  state->EndDictionary();

  if (blame_context_) {
    state->BeginDictionary("blame_context");
    state->SetString(
        "id_ref",
        PointerToString(reinterpret_cast<void*>(blame_context_->id())));
    state->SetString("scope", blame_context_->scope());
    state->EndDictionary();
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

bool FrameSchedulerImpl::IsAudioPlaying() const {
  return parent_page_scheduler_ ? parent_page_scheduler_->IsAudioPlaying()
                                : false;
}

void FrameSchedulerImpl::SetPaused(bool frame_paused) {
  DCHECK(parent_page_scheduler_);
  if (frame_paused_ == frame_paused)
    return;

  frame_paused_ = frame_paused;
  UpdatePolicy();
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
  voter->SetQueueEnabled(!queue_disabled);
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
      parent_page_scheduler_->HasActiveConnection()) {
    return SchedulingLifecycleState::kNotThrottled;
  }
  if (parent_page_scheduler_->IsThrottled())
    return SchedulingLifecycleState::kThrottled;
  if (!parent_page_scheduler_->IsPageVisible())
    return SchedulingLifecycleState::kHidden;
  return SchedulingLifecycleState::kNotThrottled;
}

void FrameSchedulerImpl::OnFirstMeaningfulPaint() {
  main_thread_scheduler_->OnFirstMeaningfulPaint();
}

std::unique_ptr<FrameScheduler::ActiveConnectionHandle>
FrameSchedulerImpl::OnActiveConnectionCreated() {
  return std::make_unique<FrameSchedulerImpl::ActiveConnectionHandleImpl>(this);
}

bool FrameSchedulerImpl::ShouldThrottleTaskQueues() const {
  if (!RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled())
    return false;
  if (parent_page_scheduler_ && parent_page_scheduler_->IsAudioPlaying())
    return false;
  if (!parent_page_scheduler_->IsPageVisible())
    return true;
  return RuntimeEnabledFeatures::TimerThrottlingForHiddenFramesEnabled() &&
         !frame_visible_ && IsCrossOrigin();
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
  return has_active_connection();
}

TaskQueue::QueuePriority FrameSchedulerImpl::ComputePriority(
    MainThreadTaskQueue* task_queue) const {
  DCHECK(task_queue);

  FrameScheduler* frame_scheduler = task_queue->GetFrameScheduler();

  // Checks the task queue is associated with this frame scheduler.
  DCHECK_EQ(frame_scheduler, this);

  auto queue_priority_pair = resource_loading_task_queue_priorities_.find(
      base::WrapRefCounted(task_queue));
  if (queue_priority_pair != resource_loading_task_queue_priorities_.end()) {
    return queue_priority_pair->value;
  }

  base::Optional<TaskQueue::QueuePriority> fixed_priority =
      task_queue->FixedPriority();

  if (fixed_priority)
    return fixed_priority.value();

  // A hidden page with no audio.
  if (parent_page_scheduler_->IsBackgrounded()) {
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_background_page)
      return TaskQueue::QueuePriority::kLowPriority;

    if (main_thread_scheduler_->scheduling_settings()
            .best_effort_background_page)
      return TaskQueue::QueuePriority::kBestEffortPriority;
  }

  // If the page is loading or if the priority experiments should take place at
  // all times.
  if (parent_page_scheduler_->IsLoading() ||
      !main_thread_scheduler_->scheduling_settings()
           .use_frame_priorities_only_during_loading) {
    // Low priority feature enabled for hidden frame.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_hidden_frame &&
        !IsFrameVisible())
      return TaskQueue::QueuePriority::kLowPriority;

    bool is_subframe = GetFrameType() == FrameScheduler::FrameType::kSubframe;
    bool is_throttleable_task_queue =
        task_queue->queue_type() ==
        MainThreadTaskQueue::QueueType::kFrameThrottleable;

    // Low priority feature enabled for sub-frame.
    if (main_thread_scheduler_->scheduling_settings().low_priority_subframe &&
        is_subframe)
      return TaskQueue::QueuePriority::kLowPriority;

    // Low priority feature enabled for sub-frame throttleable task queues.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_subframe_throttleable &&
        is_subframe && is_throttleable_task_queue)
      return TaskQueue::QueuePriority::kLowPriority;

    // Low priority feature enabled for throttleable task queues.
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_throttleable &&
        is_throttleable_task_queue)
      return TaskQueue::QueuePriority::kLowPriority;
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
  if (IsCrossOrigin()) {
    if (main_thread_scheduler_->scheduling_settings()
            .low_priority_cross_origin ||
        (main_thread_scheduler_->scheduling_settings()
             .low_priority_cross_origin_only_during_loading &&
         parent_page_scheduler_->IsLoading())) {
      return TaskQueue::QueuePriority::kLowPriority;
    }
  }

  if (task_queue->queue_type() ==
      MainThreadTaskQueue::QueueType::kWebSchedulingUserInteraction) {
    return TaskQueue::QueuePriority::kNormalPriority;
  }

  if (task_queue->queue_type() ==
      MainThreadTaskQueue::QueueType::kWebSchedulingBestEffort) {
    return TaskQueue::QueuePriority::kLowPriority;
  }

  return task_queue->queue_type() ==
                 MainThreadTaskQueue::QueueType::kFrameLoadingControl
             ? TaskQueue::QueuePriority::kHighPriority
             : TaskQueue::QueuePriority::kNormalPriority;
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
    CPUTimeBudgetPool* time_budget_pool =
        parent_page_scheduler_->BackgroundCPUTimeBudgetPool();
    if (time_budget_pool) {
      time_budget_pool->AddQueue(
          main_thread_scheduler_->tick_clock()->NowTicks(), task_queue);
    }
    if (task_queues_throttled_) {
      UpdateTaskQueueThrottling(task_queue, true);
    }
  }
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::ThrottleableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeThrottled(true)
      .SetCanBeFrozen(true)
      .SetCanBeDeferred(true)
      .SetCanBePaused(true);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::DeferrableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeDeferred(true)
      .SetCanBeFrozen(base::FeatureList::IsEnabled(
          blink::features::kStopNonTimersInBackground))
      .SetCanBePaused(true);
}

// static
MainThreadTaskQueue::QueueTraits FrameSchedulerImpl::PausableTaskQueueTraits() {
  return QueueTraits()
      .SetCanBeFrozen(base::FeatureList::IsEnabled(
          blink::features::kStopNonTimersInBackground))
      .SetCanBePaused(true);
}

// static
MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::UnpausableTaskQueueTraits() {
  return QueueTraits();
}

MainThreadTaskQueue::QueueTraits
FrameSchedulerImpl::ForegroundOnlyTaskQueueTraits() {
  return ThrottleableTaskQueueTraits().SetCanRunInBackground(false);
}

}  // namespace scheduler
}  // namespace blink
