// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/common/task_annotator.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_renderer_process_type.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/scheduler/common/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/widget_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_renderer_scheduler_state.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"
#include "v8/include/v8.h"

namespace base {
class LazyNow;
}

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;
using base::sequence_manager::TaskTimeObserver;
using base::sequence_manager::TimeDomain;

namespace {
const int kShortIdlePeriodDurationSampleCount = 10;
const double kShortIdlePeriodDurationPercentile = 50;
// Amount of idle time left in a frame (as a ratio of the vsync interval) above
// which main thread compositing can be considered fast.
const double kFastCompositingIdleTimeThreshold = .2;
const int64_t kSecondsPerMinute = 60;

constexpr int kDefaultPrioritizeCompositingAfterDelayMs = 100;

v8::RAILMode RAILModeToV8RAILMode(RAILMode rail_mode) {
  switch (rail_mode) {
    case RAILMode::kResponse:
      return v8::RAILMode::PERFORMANCE_RESPONSE;
    case RAILMode::kAnimation:
      return v8::RAILMode::PERFORMANCE_ANIMATION;
    case RAILMode::kIdle:
      return v8::RAILMode::PERFORMANCE_IDLE;
    case RAILMode::kLoad:
      return v8::RAILMode::PERFORMANCE_LOAD;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void AddRAILModeToProto(perfetto::protos::pbzero::TrackEvent* event,
                        RAILMode mode) {
  perfetto::protos::pbzero::ChromeRAILMode proto_mode;
  switch (mode) {
    case RAILMode::kResponse:
      proto_mode = perfetto::protos::pbzero::ChromeRAILMode::RAIL_MODE_RESPONSE;
      break;
    case RAILMode::kAnimation:
      proto_mode =
          perfetto::protos::pbzero::ChromeRAILMode::RAIL_MODE_ANIMATION;
      break;
    case RAILMode::kIdle:
      proto_mode = perfetto::protos::pbzero::ChromeRAILMode::RAIL_MODE_IDLE;
      break;
    case RAILMode::kLoad:
      proto_mode = perfetto::protos::pbzero::ChromeRAILMode::RAIL_MODE_LOAD;
      break;
    default:
      proto_mode = perfetto::protos::pbzero::ChromeRAILMode::RAIL_MODE_NONE;
      break;
  }
  event->set_chrome_renderer_scheduler_state()->set_rail_mode(proto_mode);
}

void AddBackgroundedToProto(perfetto::protos::pbzero::TrackEvent* event,
                            bool is_backgrounded) {
  event->set_chrome_renderer_scheduler_state()->set_is_backgrounded(
      is_backgrounded);
}

void AddHiddenToProto(perfetto::protos::pbzero::TrackEvent* event,
                      bool is_hidden) {
  event->set_chrome_renderer_scheduler_state()->set_is_hidden(is_hidden);
}

const char* AudioPlayingStateToString(bool is_audio_playing) {
  if (is_audio_playing) {
    return "playing";
  } else {
    return "silent";
  }
}

const char* RendererProcessTypeToString(WebRendererProcessType process_type) {
  switch (process_type) {
    case WebRendererProcessType::kRenderer:
      return "normal";
    case WebRendererProcessType::kExtensionRenderer:
      return "extension";
  }
  NOTREACHED_IN_MIGRATION();
  return "";  // MSVC needs that.
}

const char* OptionalTaskDescriptionToString(
    std::optional<MainThreadSchedulerImpl::TaskDescriptionForTracing> desc) {
  if (!desc)
    return nullptr;
  if (desc->task_type != TaskType::kDeprecatedNone)
    return TaskTypeNames::TaskTypeToString(desc->task_type);
  if (!desc->queue_type)
    return "detached_tq";
  return perfetto::protos::pbzero::SequenceManagerTask::QueueName_Name(
      MainThreadTaskQueue::NameForQueueType(desc->queue_type.value()));
}

const char* OptionalTaskPriorityToString(std::optional<TaskPriority> priority) {
  if (!priority)
    return nullptr;
  return TaskPriorityToString(*priority);
}

bool IsBlockingEvent(const blink::WebInputEvent& web_input_event) {
  blink::WebInputEvent::Type type = web_input_event.GetType();
  DCHECK(type == blink::WebInputEvent::Type::kTouchStart ||
         type == blink::WebInputEvent::Type::kMouseWheel);

  if (type == blink::WebInputEvent::Type::kTouchStart) {
    const WebTouchEvent& touch_event =
        static_cast<const WebTouchEvent&>(web_input_event);
    return touch_event.dispatch_type ==
           blink::WebInputEvent::DispatchType::kBlocking;
  }

  const WebMouseWheelEvent& mouse_event =
      static_cast<const WebMouseWheelEvent&>(web_input_event);
  return mouse_event.dispatch_type ==
         blink::WebInputEvent::DispatchType::kBlocking;
}

const char* InputEventStateToString(
    WidgetScheduler::InputEventState input_event_state) {
  switch (input_event_state) {
    case WidgetScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR:
      return "event_consumed_by_compositor";
    case WidgetScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD:
      return "event_forwarded_to_main_thread";
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

const char* RenderingPrioritizationStateToString(
    MainThreadSchedulerImpl::RenderingPrioritizationState state) {
  using RenderingPrioritizationState =
      MainThreadSchedulerImpl::RenderingPrioritizationState;
  switch (state) {
    case RenderingPrioritizationState::kNone:
      return "none";
    case RenderingPrioritizationState::kRenderingStarved:
      return "rendering_starved";
    case RenderingPrioritizationState::kRenderingStarvedByRenderBlocking:
      return "rendering_starved_by_render_blocking";
    case RenderingPrioritizationState::kWaitingForInputResponse:
      return "waiting_for_input_response";
  }
}

}  // namespace

MainThreadSchedulerImpl::MainThreadSchedulerImpl(
    std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager)
    : MainThreadSchedulerImpl(sequence_manager.get()) {
  owned_sequence_manager_ = std::move(sequence_manager);
}

MainThreadSchedulerImpl::MainThreadSchedulerImpl(
    base::sequence_manager::SequenceManager* sequence_manager)
    : sequence_manager_(sequence_manager),
      helper_(sequence_manager_, this),
      idle_helper_queue_(helper_.NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(
              MainThreadTaskQueue::QueueType::kIdle)
              .SetPrioritisationType(MainThreadTaskQueue::QueueTraits::
                                         PrioritisationType::kBestEffort)
              .SetCanBeDeferredForRendering(base::FeatureList::IsEnabled(
                  features::kDeferRendererTasksAfterInput)))),
      idle_queue_voter_(
          base::FeatureList::IsEnabled(features::kDeferRendererTasksAfterInput)
              ? idle_helper_queue_->CreateQueueEnabledVoter()
              : nullptr),
      idle_helper_(&helper_,
                   this,
                   "MainThreadSchedulerIdlePeriod",
                   base::TimeDelta(),
                   idle_helper_queue_->GetTaskQueue()),
      render_widget_scheduler_signals_(this),
      find_in_page_budget_pool_controller_(
          new FindInPageBudgetPoolController(this)),
      control_task_queue_(helper_.ControlMainThreadTaskQueue()),
      back_forward_cache_ipc_tracking_task_queue_(helper_.NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(
              MainThreadTaskQueue::QueueType::kIPCTrackingForCachedPages)
              .SetShouldNotifyObservers(false))),
      memory_purge_task_queue_(helper_.NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(
              MainThreadTaskQueue::QueueType::kIdle)
              .SetPrioritisationType(MainThreadTaskQueue::QueueTraits::
                                         PrioritisationType::kBestEffort))),
      memory_purge_manager_(memory_purge_task_queue_->CreateTaskRunner(
          TaskType::kMainThreadTaskQueueMemoryPurge)),
      delayed_update_policy_runner_(
          base::BindRepeating(&MainThreadSchedulerImpl::UpdatePolicy,
                              base::Unretained(this)),
          helper_.ControlMainThreadTaskQueue()->CreateTaskRunner(
              TaskType::kMainThreadTaskQueueControl)),
      main_thread_only_(this, helper_.GetClock(), helper_.NowTicks()),
      any_thread_(this),
      policy_may_need_update_(&any_thread_lock_) {
  helper_.AttachToCurrentThread();

  // Compositor task queue and default task queue should be managed by
  // WebThreadScheduler. Control task queue should not.
  task_runners_.emplace(helper_.DefaultMainThreadTaskQueue(), nullptr);

  back_forward_cache_ipc_tracking_task_runner_ =
      back_forward_cache_ipc_tracking_task_queue_->CreateTaskRunner(
          TaskType::kMainThreadTaskQueueIPCTracking);

  v8_task_queue_ = NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
      MainThreadTaskQueue::QueueType::kV8));
  v8_user_visible_task_queue_ = NewTaskQueue(
      MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kV8UserVisible)
          .SetPrioritisationType(
              MainThreadTaskQueue::QueueTraits::PrioritisationType::kLow)
          .SetCanBeDeferredForRendering(base::FeatureList::IsEnabled(
              features::kDeferRendererTasksAfterInput)));
  v8_best_effort_task_queue_ = NewTaskQueue(
      MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kV8BestEffort)
          .SetPrioritisationType(
              MainThreadTaskQueue::QueueTraits::PrioritisationType::kBestEffort)
          .SetCanBeDeferredForRendering(base::FeatureList::IsEnabled(
              features::kDeferRendererTasksAfterInput)));
  non_waking_task_queue_ =
      NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                       MainThreadTaskQueue::QueueType::kNonWaking)
                       .SetNonWaking(true));

  v8_task_runner_ =
      v8_task_queue_->CreateTaskRunner(TaskType::kMainThreadTaskQueueV8);
  v8_user_visible_task_runner_ = v8_user_visible_task_queue_->CreateTaskRunner(
      TaskType::kMainThreadTaskQueueV8UserVisible);
  v8_best_effort_task_runner_ = v8_best_effort_task_queue_->CreateTaskRunner(
      TaskType::kMainThreadTaskQueueV8BestEffort);
  control_task_runner_ = helper_.ControlMainThreadTaskQueue()->CreateTaskRunner(
      TaskType::kMainThreadTaskQueueControl);
  non_waking_task_runner_ = non_waking_task_queue_->CreateTaskRunner(
      TaskType::kMainThreadTaskQueueNonWaking);

  // TaskQueueThrottler requires some task runners, then initialize
  // TaskQueueThrottler after task queues/runners are initialized.
  update_policy_closure_ = base::BindRepeating(
      &MainThreadSchedulerImpl::UpdatePolicy, weak_factory_.GetWeakPtr());
  end_renderer_hidden_idle_period_closure_.Reset(base::BindRepeating(
      &MainThreadSchedulerImpl::EndIdlePeriod, weak_factory_.GetWeakPtr()));

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "MainThreadScheduler",
      this);

  helper_.SetObserver(this);

  // Register a tracing state observer unless we're running in a test without a
  // task runner. Note that it's safe to remove a non-existent observer.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::TraceLog::GetInstance()->AddAsyncEnabledStateObserver(
        weak_factory_.GetWeakPtr());
  }

  internal::ProcessState::Get()->is_process_backgrounded =
      main_thread_only().renderer_backgrounded.get();

  main_thread_only().current_policy.find_in_page_priority =
      find_in_page_budget_pool_controller_->CurrentTaskPriority();

  // Explicitly set the priority of this queue since it is not managed by
  // the main thread scheduler.
  memory_purge_task_queue_->SetQueuePriority(
      ComputePriority(memory_purge_task_queue_.get()));
}

MainThreadSchedulerImpl::~MainThreadSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "MainThreadScheduler",
      this);
  // Ensure the renderer scheduler was shut down explicitly, because otherwise
  // we could end up having stale pointers to the Blink heap which has been
  // terminated by this point.
  CHECK(was_shutdown_);

  // These should be cleared during shutdown.
  CHECK(task_runners_.empty());
  CHECK(main_thread_only().detached_task_queues.empty());
  CHECK(!virtual_time_control_task_queue_);

  base::trace_event::TraceLog::GetInstance()->RemoveAsyncEnabledStateObserver(
      this);
}

// static
WebThreadScheduler& WebThreadScheduler::MainThreadScheduler() {
  auto* main_thread = Thread::MainThread();
  // Enforce that this is not called before the main thread is initialized.
  CHECK(main_thread && main_thread->Scheduler() &&
        main_thread->Scheduler()->ToMainThreadScheduler());
  auto* scheduler = main_thread->Scheduler()
                        ->ToMainThreadScheduler()
                        ->ToWebMainThreadScheduler();
  // `scheduler` can be null if it isn't a MainThreadSchedulerImpl, which can
  // happen in tests. Tests should use a real main thread scheduler if a
  // `WebThreadScheduler` is needed.
  CHECK(scheduler);
  return *scheduler;
}

MainThreadSchedulerImpl::MainThreadOnly::MainThreadOnly(
    MainThreadSchedulerImpl* main_thread_scheduler_impl,
    const base::TickClock* time_source,
    base::TimeTicks now)
    : idle_time_estimator(time_source,
                          kShortIdlePeriodDurationSampleCount,
                          kShortIdlePeriodDurationPercentile),
      current_use_case(UseCase::kNone,
                       "Scheduler.UseCase",
                       &main_thread_scheduler_impl->tracing_controller_,
                       UseCaseToString),
      renderer_pause_count(0,
                           "Scheduler.PauseCount",
                           &main_thread_scheduler_impl->tracing_controller_),
      rail_mode_for_tracing(current_policy.rail_mode,
                            "Scheduler.RAILMode",
                            &main_thread_scheduler_impl->tracing_controller_,
                            &AddRAILModeToProto),
      renderer_hidden(false,
                      "RendererVisibility",
                      &main_thread_scheduler_impl->tracing_controller_,
                      &AddHiddenToProto),
      renderer_backgrounded(kLaunchingProcessIsBackgrounded,
                            "RendererPriority",
                            &main_thread_scheduler_impl->tracing_controller_,
                            &AddBackgroundedToProto),
      blocking_input_expected_soon(
          false,
          "Scheduler.BlockingInputExpectedSoon",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      in_idle_period_for_testing(
          false,
          "Scheduler.InIdlePeriod",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      is_audio_playing(false,
                       "RendererAudioState",
                       &main_thread_scheduler_impl->tracing_controller_,
                       AudioPlayingStateToString),
      compositor_will_send_main_frame_not_expected(
          false,
          "Scheduler.CompositorWillSendMainFrameNotExpected",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      has_navigated(false,
                    "Scheduler.HasNavigated",
                    &main_thread_scheduler_impl->tracing_controller_,
                    YesNoStateToString),
      pause_timers_for_webview(false,
                               "Scheduler.PauseTimersForWebview",
                               &main_thread_scheduler_impl->tracing_controller_,
                               YesNoStateToString),
      background_status_changed_at(now),
      metrics_helper(
          main_thread_scheduler_impl,
          main_thread_scheduler_impl->helper_.HasCPUTimingForEachTask(),
          now,
          renderer_backgrounded.get()),
      process_type(WebRendererProcessType::kRenderer,
                   "RendererProcessType",
                   &main_thread_scheduler_impl->tracing_controller_,
                   RendererProcessTypeToString),
      task_description_for_tracing(
          std::nullopt,
          "Scheduler.MainThreadTask",
          &main_thread_scheduler_impl->tracing_controller_,
          OptionalTaskDescriptionToString),
      task_priority_for_tracing(
          std::nullopt,
          "Scheduler.TaskPriority",
          &main_thread_scheduler_impl->tracing_controller_,
          OptionalTaskPriorityToString),
      main_thread_compositing_is_fast(false),
      compositor_priority(TaskPriority::kNormalPriority,
                          "Scheduler.CompositorPriority",
                          &main_thread_scheduler_impl->tracing_controller_,
                          TaskPriorityToString),
      main_frame_prioritization_state(
          RenderingPrioritizationState::kNone,
          "RenderingPrioritizationState",
          &main_thread_scheduler_impl->tracing_controller_,
          RenderingPrioritizationStateToString),
      last_frame_time(now),
      agent_group_schedulers(
          MakeGarbageCollected<
              HeapHashSet<WeakMember<AgentGroupSchedulerImpl>>>()) {}

MainThreadSchedulerImpl::MainThreadOnly::~MainThreadOnly() = default;

MainThreadSchedulerImpl::AnyThread::AnyThread(
    MainThreadSchedulerImpl* main_thread_scheduler_impl)
    : awaiting_touch_start_response(
          false,
          "Scheduler.AwaitingTouchstartResponse",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      awaiting_discrete_input_response(
          false,
          "Scheduler.AwaitingDiscreteInputResponse",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      in_idle_period(false,
                     "Scheduler.InIdlePeriod",
                     &main_thread_scheduler_impl->tracing_controller_,
                     YesNoStateToString),
      begin_main_frame_on_critical_path(
          false,
          "Scheduler.BeginMainFrameOnCriticalPath",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      last_gesture_was_compositor_driven(
          false,
          "Scheduler.LastGestureWasCompositorDriven",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      default_gesture_prevented(
          true,
          "Scheduler.DefaultGesturePrevented",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      have_seen_a_blocking_gesture(
          false,
          "Scheduler.HaveSeenBlockingGesture",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      waiting_for_any_main_frame_contentful_paint(
          false,
          "Scheduler.WaitingForMainFrameContentfulPaint",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      waiting_for_any_main_frame_meaningful_paint(
          false,
          "Scheduler.WaitingForMeaningfulPaint",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      is_any_main_frame_loading(
          false,
          "Scheduler.IsAnyMainFrameLoading",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString),
      have_seen_input_since_navigation(
          false,
          "Scheduler.HaveSeenInputSinceNavigation",
          &main_thread_scheduler_impl->tracing_controller_,
          YesNoStateToString) {}

MainThreadSchedulerImpl::SchedulingSettings::SchedulingSettings() {
  mbi_override_task_runner_handle =
      base::FeatureList::IsEnabled(kMbiOverrideTaskRunnerHandle);

  compositor_gesture_rendering_starvation_threshold =
      GetThreadedScrollRenderingStarvationThreshold();

  if (base::FeatureList::IsEnabled(features::kDeferRendererTasksAfterInput)) {
    discrete_input_task_deferral_policy =
        features::kTaskDeferralPolicyParam.Get();
  }

  prioritize_compositing_after_delay_pre_fcp =
      base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
          kPrioritizeCompositingAfterDelayTrials, "PreFCP",
          kDefaultPrioritizeCompositingAfterDelayMs));
  prioritize_compositing_after_delay_post_fcp =
      base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
          kPrioritizeCompositingAfterDelayTrials, "PostFCP",
          kDefaultPrioritizeCompositingAfterDelayMs));
}

MainThreadSchedulerImpl::AnyThread::~AnyThread() = default;

MainThreadSchedulerImpl::CompositorThreadOnly::CompositorThreadOnly()
    : last_input_type(blink::WebInputEvent::Type::kUndefined) {}

MainThreadSchedulerImpl::CompositorThreadOnly::~CompositorThreadOnly() =
    default;

MainThreadSchedulerImpl::RendererPauseHandleImpl::RendererPauseHandleImpl(
    MainThreadSchedulerImpl* scheduler)
    : scheduler_(scheduler) {
  scheduler_->PauseRendererImpl();
}

MainThreadSchedulerImpl::RendererPauseHandleImpl::~RendererPauseHandleImpl() {
  scheduler_->ResumeRendererImpl();
}

void MainThreadSchedulerImpl::ShutdownAllQueues() {
  while (!task_runners_.empty()) {
    scoped_refptr<MainThreadTaskQueue> queue = task_runners_.begin()->first;
    queue->ShutdownTaskQueue();
  }
  while (!main_thread_only().detached_task_queues.empty()) {
    scoped_refptr<MainThreadTaskQueue> queue =
        *main_thread_only().detached_task_queues.begin();
    queue->ShutdownTaskQueue();
  }
  if (virtual_time_control_task_queue_) {
    virtual_time_control_task_queue_->ShutdownTaskQueue();
    virtual_time_control_task_queue_ = nullptr;
  }
}

bool MainThreadSchedulerImpl::
    IsAnyOrdinaryMainFrameWaitingForFirstMeaningfulPaint() const {
  for (const PageSchedulerImpl* ps : main_thread_only().page_schedulers) {
    if (ps->IsOrdinary() && ps->IsWaitingForMainFrameMeaningfulPaint())
      return true;
  }
  return false;
}

bool MainThreadSchedulerImpl::IsAnyOrdinaryMainFrameLoading() const {
  for (const PageSchedulerImpl* ps : main_thread_only().page_schedulers) {
    if (ps->IsOrdinary() && ps->IsMainFrameLoading()) {
      return true;
    }
  }
  return false;
}

bool MainThreadSchedulerImpl::
    IsAnyOrdinaryMainFrameWaitingForFirstContentfulPaint() const {
  for (const PageSchedulerImpl* ps : main_thread_only().page_schedulers) {
    if (ps->IsOrdinary() && ps->IsWaitingForMainFrameContentfulPaint())
      return true;
  }
  return false;
}

void MainThreadSchedulerImpl::Shutdown() {
  if (was_shutdown_)
    return;
  base::TimeTicks now = NowTicks();
  main_thread_only().metrics_helper.OnRendererShutdown(now);
  // This needs to be after metrics helper, to prevent it being confused by
  // potential virtual time domain shutdown!
  ThreadSchedulerBase::Shutdown();

  ShutdownAllQueues();

  // Shut down |helper_| first, so that the ForceUpdatePolicy() call
  // from |idle_helper_| early-outs and doesn't do anything.
  helper_.Shutdown();
  idle_helper_.Shutdown();
  sequence_manager_ = nullptr;
  owned_sequence_manager_.reset();
  main_thread_only().rail_mode_observers.Clear();
  was_shutdown_ = true;
}

std::unique_ptr<MainThread> MainThreadSchedulerImpl::CreateMainThread() {
  return std::make_unique<MainThreadImpl>(this);
}

scoped_refptr<WidgetScheduler>
MainThreadSchedulerImpl::CreateWidgetScheduler() {
  return base::MakeRefCounted<WidgetSchedulerImpl>(
      this, &render_widget_scheduler_signals_);
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerImpl::ControlTaskRunner() {
  return control_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerImpl::DefaultTaskRunner() {
  return helper_.DefaultTaskRunner();
}

scoped_refptr<SingleThreadIdleTaskRunner>
MainThreadSchedulerImpl::IdleTaskRunner() {
  return idle_helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerImpl::DeprecatedDefaultTaskRunner() {
  return helper_.DeprecatedDefaultTaskRunner();
}

scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerImpl::V8TaskQueue() {
  helper_.CheckOnValidThread();
  return v8_task_queue_;
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerImpl::CleanupTaskRunner() {
  return DefaultTaskRunner();
}

scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerImpl::ControlTaskQueue() {
  return helper_.ControlMainThreadTaskQueue();
}

scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerImpl::DefaultTaskQueue() {
  return helper_.DefaultMainThreadTaskQueue();
}

scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerImpl::NewTaskQueue(
    const MainThreadTaskQueue::QueueCreationParams& params) {
  helper_.CheckOnValidThread();
  scoped_refptr<MainThreadTaskQueue> task_queue(helper_.NewTaskQueue(params));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter;
  if (params.queue_traits.can_be_deferred ||
      params.queue_traits.can_be_deferred_for_rendering ||
      params.queue_traits.can_be_paused || params.queue_traits.can_be_frozen) {
    voter = task_queue->CreateQueueEnabledVoter();
  }

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kCompositor) {
    DCHECK(!voter);
    voter = task_queue->CreateQueueEnabledVoter();
    main_thread_only().idle_time_estimator.AddCompositorTaskQueue(task_queue);
  }

  auto insert_result = task_runners_.emplace(task_queue, std::move(voter));

  UpdateTaskQueueState(task_queue.get(), insert_result.first->second.get(),
                       Policy(), main_thread_only().current_policy,
                       /*should_update_priority=*/true);

  // If this is a timer queue, and virtual time is enabled and paused, it should
  // be suspended by adding a fence to prevent immediate tasks from running when
  // they're not supposed to.
  if (!VirtualTimeAllowedToAdvance() &&
      !task_queue->CanRunWhenVirtualTimePaused()) {
    task_queue->GetTaskQueue()->InsertFence(
        TaskQueue::InsertFencePosition::kNow);
  }

  return task_queue;
}

bool MainThreadSchedulerImpl::IsIpcTrackingEnabledForAllPages() {
  for (auto* scheduler : main_thread_only().page_schedulers) {
    if (!(scheduler->IsInBackForwardCache() &&
          scheduler->has_ipc_detection_enabled())) {
      return false;
    }
  }
  return true;
}

void MainThreadSchedulerImpl::UpdateIpcTracking() {
  bool should_track = IsIpcTrackingEnabledForAllPages();
  if (should_track == has_ipc_callback_set_)
    return;

  has_ipc_callback_set_ = should_track;
  if (has_ipc_callback_set_) {
    SetOnIPCTaskPostedWhileInBackForwardCacheIfNeeded();
  } else {
    DetachOnIPCTaskPostedWhileInBackForwardCacheHandler();
  }
}

void MainThreadSchedulerImpl::
    SetOnIPCTaskPostedWhileInBackForwardCacheIfNeeded() {
  has_ipc_callback_set_ = true;
  helper_.DefaultMainThreadTaskQueue()->SetOnIPCTaskPosted(base::BindRepeating(
      [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
         base::WeakPtr<MainThreadSchedulerImpl> main_thread_scheduler,
         const base::sequence_manager::Task& task) {
        // Only log IPC tasks. IPC tasks are only logged currently as IPC
        // hash can be mapped back to a function name, and IPC tasks may
        // potentially post sensitive information.
        if (!task.ipc_hash && !task.ipc_interface_name) {
          return;
        }
        base::ScopedDeferTaskPosting::PostOrDefer(
            task_runner, FROM_HERE,
            base::BindOnce(&MainThreadSchedulerImpl::
                               OnIPCTaskPostedWhileInAllPagesBackForwardCache,
                           main_thread_scheduler, task.ipc_hash,
                           task.ipc_interface_name),
            base::TimeDelta());
      },
      back_forward_cache_ipc_tracking_task_runner_, GetWeakPtr()));
}

void MainThreadSchedulerImpl::OnIPCTaskPostedWhileInAllPagesBackForwardCache(
    uint32_t ipc_hash,
    const char* ipc_interface_name) {
  // As this is a multi-threaded environment, we need to check that all page
  // schedulers are in the cache before logging. There may be instances where
  // the scheduler has been unfrozen prior to the IPC tracking handler being
  // reset.
  if (!IsIpcTrackingEnabledForAllPages()) {
    return;
  }

  // IPC tasks may have an IPC interface name in addition to, or instead of an
  // IPC hash. IPC hash is known from the mojo Accept method. When IPC hash is
  // 0, then the IPC hash must be calculated form the IPC interface name
  // instead.
  if (!ipc_hash) {
    // base::HashMetricName produces a uint64; however, the MD5 hash calculation
    // for an IPC interface name is always calculated as uint32; the IPC hash on
    // a task is also a uint32. The calculation here is meant to mimic the
    // calculation used in base::MD5Hash32Constexpr.
    ipc_hash = static_cast<uint32_t>(
        base::TaskAnnotator::ScopedSetIpcHash::MD5HashMetricName(
            ipc_interface_name));
  }

  base::UmaHistogramSparse(
      "BackForwardCache.Experimental.UnexpectedIPCMessagePostedToCachedFrame."
      "MethodHash",
      static_cast<int32_t>(ipc_hash));
}

void MainThreadSchedulerImpl::
    DetachOnIPCTaskPostedWhileInBackForwardCacheHandler() {
  has_ipc_callback_set_ = false;
  helper_.DefaultMainThreadTaskQueue()
      ->DetachOnIPCTaskPostedWhileInBackForwardCache();
}

void MainThreadSchedulerImpl::ShutdownEmptyDetachedTaskQueues() {
  if (main_thread_only().detached_task_queues.empty()) {
    return;
  }
  WTF::Vector<scoped_refptr<MainThreadTaskQueue>> queues_to_delete;
  for (auto& queue : main_thread_only().detached_task_queues) {
    if (queue->IsEmpty()) {
      queues_to_delete.push_back(queue);
    }
  }
  for (auto& queue : queues_to_delete) {
    queue->ShutdownTaskQueue();
    // The task queue is removed in `OnShutdownTaskQueue()`.
    CHECK(!main_thread_only().detached_task_queues.Contains(queue));
  }
}

// TODO(sreejakshetty): Cleanup NewLoadingTaskQueue.
scoped_refptr<MainThreadTaskQueue> MainThreadSchedulerImpl::NewLoadingTaskQueue(
    MainThreadTaskQueue::QueueType queue_type,
    FrameSchedulerImpl* frame_scheduler) {
  DCHECK(queue_type == MainThreadTaskQueue::QueueType::kFrameLoading ||
         queue_type == MainThreadTaskQueue::QueueType::kFrameLoadingControl);
  return NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(queue_type)
                          .SetCanBePaused(true)
                          .SetCanBeFrozen(true)
                          .SetCanBeDeferred(true)
                          .SetFrameScheduler(frame_scheduler));
}

scoped_refptr<MainThreadTaskQueue>
MainThreadSchedulerImpl::NewThrottleableTaskQueueForTest(
    FrameSchedulerImpl* frame_scheduler) {
  return NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                          MainThreadTaskQueue::QueueType::kFrameThrottleable)
                          .SetCanBePaused(true)
                          .SetCanBeFrozen(true)
                          .SetCanBeDeferred(true)
                          .SetCanBeThrottled(true)
                          .SetFrameScheduler(frame_scheduler)
                          .SetCanRunWhenVirtualTimePaused(false));
}

void MainThreadSchedulerImpl::OnShutdownTaskQueue(
    const scoped_refptr<MainThreadTaskQueue>& task_queue) {
  if (was_shutdown_) {
    return;
  }
  task_queue.get()->DetachOnIPCTaskPostedWhileInBackForwardCache();
  task_runners_.erase(task_queue.get());
  main_thread_only().detached_task_queues.erase(task_queue.get());
}

void MainThreadSchedulerImpl::OnDetachTaskQueue(
    MainThreadTaskQueue& task_queue) {
  if (was_shutdown_) {
    return;
  }
  // `UpdatePolicy()` is not set up to handle detached frame scheduler queues.
  // TODO(crbug.com/1143007): consider keeping FrameScheduler alive until all
  // tasks have finished running.
  task_runners_.erase(&task_queue);

  // Don't immediately shut down the task queue even if it's empty. Tasks can
  // still be queued before this task ends, which some parts of blink depend on.
  main_thread_only().detached_task_queues.insert(
      base::WrapRefCounted(&task_queue));
}

void MainThreadSchedulerImpl::AddTaskObserver(
    base::TaskObserver* task_observer) {
  helper_.AddTaskObserver(task_observer);
}

void MainThreadSchedulerImpl::RemoveTaskObserver(
    base::TaskObserver* task_observer) {
  helper_.RemoveTaskObserver(task_observer);
}

void MainThreadSchedulerImpl::WillBeginFrame(const viz::BeginFrameArgs& args) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::WillBeginFrame", "args",
               args.AsValue());
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  EndIdlePeriod();
  main_thread_only().estimated_next_frame_begin =
      args.frame_time + args.interval;
  main_thread_only().compositor_frame_interval = args.interval;
  {
    base::AutoLock lock(any_thread_lock_);
    any_thread().begin_main_frame_on_critical_path = args.on_critical_path;
  }
  main_thread_only().is_current_task_main_frame = true;
}

void MainThreadSchedulerImpl::DidCommitFrameToCompositor() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::DidCommitFrameToCompositor");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.NowTicks());
  if (now < main_thread_only().estimated_next_frame_begin) {
    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    idle_helper_.StartIdlePeriod(
        IdleHelper::IdlePeriodState::kInShortIdlePeriod, now,
        main_thread_only().estimated_next_frame_begin);
  }

  main_thread_only().idle_time_estimator.DidCommitFrameToCompositor();
}

void MainThreadSchedulerImpl::BeginFrameNotExpectedSoon() {
  // TODO(crbug/1068426): Should this call |UpdatePolicy|?
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::BeginFrameNotExpectedSoon");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  idle_helper_.EnableLongIdlePeriod();
  {
    base::AutoLock lock(any_thread_lock_);
    any_thread().begin_main_frame_on_critical_path = false;
  }
}

void MainThreadSchedulerImpl::BeginMainFrameNotExpectedUntil(
    base::TimeTicks time) {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.NowTicks());
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::BeginMainFrameNotExpectedUntil",
               "time_remaining", (time - now).InMillisecondsF());

  if (now < time) {
    // End any previous idle period.
    EndIdlePeriod();

    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    idle_helper_.StartIdlePeriod(
        IdleHelper::IdlePeriodState::kInShortIdlePeriod, now, time);
  }
}

void MainThreadSchedulerImpl::SetAllRenderWidgetsHidden(bool hidden) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::SetAllRenderWidgetsHidden", "hidden",
               hidden);

  helper_.CheckOnValidThread();

  if (helper_.IsShutdown() ||
      main_thread_only().renderer_hidden.get() == hidden) {
    return;
  }

  end_renderer_hidden_idle_period_closure_.Cancel();

  if (hidden) {
    idle_helper_.EnableLongIdlePeriod();

    // Ensure that we stop running idle tasks after a few seconds of being
    // hidden.
    base::TimeDelta end_idle_when_hidden_delay =
        base::Milliseconds(kEndIdleWhenHiddenDelayMillis);
    control_task_queue_->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
        FROM_HERE, end_renderer_hidden_idle_period_closure_.GetCallback(),
        end_idle_when_hidden_delay);
    main_thread_only().renderer_hidden = true;
  } else {
    main_thread_only().renderer_hidden = false;
    EndIdlePeriod();
  }

  // TODO(alexclarke): Should we update policy here?
  CreateTraceEventObjectSnapshot();
}

void MainThreadSchedulerImpl::SetRendererHidden(bool hidden) {
  if (hidden) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "MainThreadSchedulerImpl::OnRendererHidden");
    main_thread_only().renderer_hidden_metadata.emplace(
        "MainThreadSchedulerImpl.RendererHidden", /* is_hidden */ 1,
        base::SampleMetadataScope::kProcess);
  } else {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "MainThreadSchedulerImpl::OnRendererVisible");
    main_thread_only().renderer_hidden_metadata.reset();
  }
  helper_.CheckOnValidThread();
  main_thread_only().renderer_hidden = hidden;
}

void MainThreadSchedulerImpl::SetRendererBackgrounded(bool backgrounded) {
  helper_.CheckOnValidThread();

  if (helper_.IsShutdown() ||
      main_thread_only().renderer_backgrounded.get() == backgrounded)
    return;
  if (backgrounded) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "MainThreadSchedulerImpl::OnRendererBackgrounded");
  } else {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "MainThreadSchedulerImpl::OnRendererForegrounded");
  }

  main_thread_only().renderer_backgrounded = backgrounded;
  internal::ProcessState::Get()->is_process_backgrounded = backgrounded;

  main_thread_only().background_status_changed_at = NowTicks();

  UpdatePolicy();

  base::TimeTicks now = NowTicks();
  if (backgrounded) {
    main_thread_only().metrics_helper.OnRendererBackgrounded(now);
  } else {
    main_thread_only().metrics_helper.OnRendererForegrounded(now);
  }

  ParkableStringManager::Instance().SetRendererBackgrounded(backgrounded);
  memory_purge_manager_.SetRendererBackgrounded(backgrounded);
}

void MainThreadSchedulerImpl::SetRendererBackgroundedForTesting(
    bool backgrounded) {
  SetRendererBackgrounded(backgrounded);
}

#if BUILDFLAG(IS_ANDROID)
void MainThreadSchedulerImpl::PauseTimersForAndroidWebView() {
  main_thread_only().pause_timers_for_webview = true;
  UpdatePolicy();
}

void MainThreadSchedulerImpl::ResumeTimersForAndroidWebView() {
  main_thread_only().pause_timers_for_webview = false;
  UpdatePolicy();
}
#endif

void MainThreadSchedulerImpl::OnAudioStateChanged() {
  bool is_audio_playing = false;
  for (PageSchedulerImpl* page_scheduler : main_thread_only().page_schedulers) {
    is_audio_playing = is_audio_playing || page_scheduler->IsAudioPlaying();
  }

  if (is_audio_playing == main_thread_only().is_audio_playing)
    return;

  main_thread_only().is_audio_playing = is_audio_playing;
}

std::unique_ptr<MainThreadScheduler::RendererPauseHandle>
MainThreadSchedulerImpl::PauseScheduler() {
  return std::make_unique<RendererPauseHandleImpl>(this);
}

void MainThreadSchedulerImpl::PauseRendererImpl() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  ++main_thread_only().renderer_pause_count;
  UpdatePolicy();
}

void MainThreadSchedulerImpl::ResumeRendererImpl() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;
  --main_thread_only().renderer_pause_count;
  DCHECK_GE(main_thread_only().renderer_pause_count.value(), 0);
  UpdatePolicy();
}

void MainThreadSchedulerImpl::EndIdlePeriod() {
  if (main_thread_only().in_idle_period_for_testing)
    return;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::EndIdlePeriod");
  helper_.CheckOnValidThread();
  idle_helper_.EndIdlePeriod();
}

void MainThreadSchedulerImpl::EndIdlePeriodForTesting(
    base::TimeTicks time_remaining) {
  main_thread_only().in_idle_period_for_testing = false;
  EndIdlePeriod();
}

bool MainThreadSchedulerImpl::PolicyNeedsUpdateForTesting() {
  return policy_may_need_update_.IsSet();
}

void MainThreadSchedulerImpl::SetHaveSeenABlockingGestureForTesting(
    bool status) {
  base::AutoLock lock(any_thread_lock_);
  any_thread().have_seen_a_blocking_gesture = status;
}

void MainThreadSchedulerImpl::PerformMicrotaskCheckpoint() {
  TRACE_EVENT("toplevel", "BlinkScheduler_PerformMicrotaskCheckpoint");

  // This will fallback to execute the microtask checkpoint for the
  // default EventLoop for the isolate.
  if (isolate())
    EventLoop::PerformIsolateGlobalMicrotasksCheckpoint(isolate());

  // Perform a microtask checkpoint for each AgentSchedulingGroup. This
  // really should only be the ones that are not frozen but AgentSchedulingGroup
  // does not have that concept yet.
  // TODO(dtapuska): Move this to EndAgentGroupSchedulerScope so that we only
  // run the microtask checkpoint for a given AgentGroupScheduler.
  //
  // This code is performance sensitive so we do not wish to allocate
  // memory, use an inline vector of 10. 10 is an appropriate size as typically
  // we only see a few AgentGroupSchedulers (this will change in the future).
  // We use an inline HeapVector here because cloning to a HeapHashSet was
  // causing floating garbage even with ClearCollectionScope. See
  // crbug.com/1376394.
  HeapVector<Member<AgentGroupSchedulerImpl>, 10> schedulers;
  for (AgentGroupSchedulerImpl* scheduler :
       *main_thread_only().agent_group_schedulers) {
    schedulers.push_back(scheduler);
  }
  for (AgentGroupSchedulerImpl* agent_group_scheduler : schedulers) {
    DCHECK(main_thread_only().agent_group_schedulers->Contains(
        agent_group_scheduler));
    agent_group_scheduler->PerformMicrotaskCheckpoint();
  }
}

// static
bool MainThreadSchedulerImpl::ShouldPrioritizeInputEvent(
    const blink::WebInputEvent& web_input_event) {
  // We regard MouseMove events with the left mouse button down as a signal
  // that the user is doing something requiring a smooth frame rate.
  if ((web_input_event.GetType() == blink::WebInputEvent::Type::kMouseDown ||
       web_input_event.GetType() == blink::WebInputEvent::Type::kMouseMove) &&
      (web_input_event.GetModifiers() &
       blink::WebInputEvent::kLeftButtonDown)) {
    return true;
  }
  // Ignore all other mouse events because they probably don't signal user
  // interaction needing a smooth framerate. NOTE isMouseEventType returns false
  // for mouse wheel events, hence we regard them as user input.
  // Ignore keyboard events because it doesn't really make sense to enter
  // compositor priority for them.
  if (blink::WebInputEvent::IsMouseEventType(web_input_event.GetType()) ||
      blink::WebInputEvent::IsKeyboardEventType(web_input_event.GetType())) {
    return false;
  }
  return true;
}

void MainThreadSchedulerImpl::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    WidgetScheduler::InputEventState event_state) {
  TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
      "MainThreadSchedulerImpl::DidHandleInputEventOnCompositorThread");
  if (!ShouldPrioritizeInputEvent(web_input_event))
    return;

  UpdateForInputEventOnCompositorThread(web_input_event, event_state);
}

void MainThreadSchedulerImpl::UpdateForInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    WidgetScheduler::InputEventState input_event_state) {
  base::AutoLock lock(any_thread_lock_);
  base::TimeTicks now = helper_.NowTicks();

  blink::WebInputEvent::Type type = web_input_event.GetType();

  // TODO(alexclarke): Move WebInputEventTraits where we can access it from here
  // and record the name rather than the integer representation.
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::UpdateForInputEventOnCompositorThread",
               "type", static_cast<int>(type), "input_event_state",
               InputEventStateToString(input_event_state));

  base::TimeDelta unused_policy_duration;
  UseCase previous_use_case =
      ComputeCurrentUseCase(now, &unused_policy_duration);
  bool was_awaiting_touch_start_response =
      any_thread().awaiting_touch_start_response;

  any_thread().user_model.DidStartProcessingInputEvent(type, now);
  any_thread().have_seen_input_since_navigation = true;

  if (input_event_state ==
      WidgetScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR)
    any_thread().user_model.DidFinishProcessingInputEvent(now);

  switch (type) {
    case blink::WebInputEvent::Type::kTouchStart:
      any_thread().awaiting_touch_start_response = true;
      // This is just a fail-safe to reset the state of
      // |last_gesture_was_compositor_driven| to the default. We don't know
      // yet where the gesture will run.
      any_thread().last_gesture_was_compositor_driven = false;
      // Assume the default gesture is prevented until we see evidence
      // otherwise.
      any_thread().default_gesture_prevented = true;

      if (IsBlockingEvent(web_input_event))
        any_thread().have_seen_a_blocking_gesture = true;
      break;
    case blink::WebInputEvent::Type::kTouchMove:
      // Observation of consecutive touchmoves is a strong signal that the
      // page is consuming the touch sequence, in which case touchstart
      // response prioritization is no longer necessary. Otherwise, the
      // initial touchmove should preserve the touchstart response pending
      // state.
      if (any_thread().awaiting_touch_start_response &&
          GetCompositorThreadOnly().last_input_type ==
              blink::WebInputEvent::Type::kTouchMove) {
        any_thread().awaiting_touch_start_response = false;
      }
      break;

    case blink::WebInputEvent::Type::kGesturePinchUpdate:
    case blink::WebInputEvent::Type::kGestureScrollUpdate:
      // If we see events for an established gesture, we can lock it to the
      // appropriate thread as the gesture can no longer be cancelled.
      any_thread().last_gesture_was_compositor_driven =
          input_event_state ==
          WidgetScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      any_thread().awaiting_touch_start_response = false;
      any_thread().default_gesture_prevented = false;
      break;

    case blink::WebInputEvent::Type::kGestureFlingCancel:
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kGestureShowPress:
    case blink::WebInputEvent::Type::kGestureScrollEnd:
      // With no observable effect, these meta events do not indicate a
      // meaningful touchstart response and should not impact task priority.
      break;

    case blink::WebInputEvent::Type::kMouseDown:
      // Reset tracking state at the start of a new mouse drag gesture.
      any_thread().last_gesture_was_compositor_driven = false;
      any_thread().default_gesture_prevented = true;
      break;

    case blink::WebInputEvent::Type::kMouseMove:
      // Consider mouse movement with the left button held down (see
      // ShouldPrioritizeInputEvent) similarly to a touch gesture.
      any_thread().last_gesture_was_compositor_driven =
          input_event_state ==
          WidgetScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      any_thread().awaiting_touch_start_response = false;
      break;

    case blink::WebInputEvent::Type::kMouseWheel:
      any_thread().last_gesture_was_compositor_driven =
          input_event_state ==
          WidgetScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      any_thread().awaiting_touch_start_response = false;
      // If the event was sent to the main thread, assume the default gesture is
      // prevented until we see evidence otherwise.
      any_thread().default_gesture_prevented =
          !any_thread().last_gesture_was_compositor_driven;
      if (IsBlockingEvent(web_input_event))
        any_thread().have_seen_a_blocking_gesture = true;
      break;
    case blink::WebInputEvent::Type::kUndefined:
      break;

    default:
      any_thread().awaiting_touch_start_response = false;
      break;
  }

  // Avoid unnecessary policy updates if the use case did not change.
  UseCase use_case = ComputeCurrentUseCase(now, &unused_policy_duration);

  if (use_case != previous_use_case ||
      was_awaiting_touch_start_response !=
          any_thread().awaiting_touch_start_response) {
    EnsureUrgentPolicyUpdatePostedOnMainThread(FROM_HERE);
  }
  GetCompositorThreadOnly().last_input_type = type;
}

void MainThreadSchedulerImpl::WillPostInputEventToMainThread(
    WebInputEvent::Type web_input_event_type,
    const WebInputEventAttribution& web_input_event_attribution) {
  base::AutoLock lock(any_thread_lock_);
  any_thread().pending_input_monitor.OnEnqueue(web_input_event_type,
                                               web_input_event_attribution);
}

void MainThreadSchedulerImpl::WillHandleInputEventOnMainThread(
    WebInputEvent::Type web_input_event_type,
    const WebInputEventAttribution& web_input_event_attribution) {
  helper_.CheckOnValidThread();

  base::AutoLock lock(any_thread_lock_);
  any_thread().pending_input_monitor.OnDequeue(web_input_event_type,
                                               web_input_event_attribution);
}

void MainThreadSchedulerImpl::DidHandleInputEventOnMainThread(
    const WebInputEvent& web_input_event,
    WebInputEventResult result,
    bool frame_requested) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::DidHandleInputEventOnMainThread");
  helper_.CheckOnValidThread();
  if (ShouldPrioritizeInputEvent(web_input_event)) {
    base::AutoLock lock(any_thread_lock_);
    any_thread().user_model.DidFinishProcessingInputEvent(helper_.NowTicks());

    // If we were waiting for a touchstart response and the main thread has
    // prevented the default gesture, consider the gesture established. This
    // ensures single-event gestures such as button presses are promptly
    // detected.
    if (any_thread().awaiting_touch_start_response &&
        result == WebInputEventResult::kHandledApplication) {
      any_thread().awaiting_touch_start_response = false;
      any_thread().default_gesture_prevented = true;
      UpdatePolicyLocked(UpdateType::kMayEarlyOutIfPolicyUnchanged);
    }
  }

  bool is_discrete =
      base::FeatureList::IsEnabled(
          features::kBlinkSchedulerDiscreteInputMatchesResponsivenessMetrics)
          ? WebInputEvent::IsWebInteractionEvent(web_input_event.GetType())
          : !PendingUserInput::IsContinuousEventType(web_input_event.GetType());
  if (is_discrete) {
    main_thread_only().is_current_task_discrete_input = true;
    main_thread_only().is_frame_requested_after_discrete_input =
        frame_requested;
  }
}

bool MainThreadSchedulerImpl::ShouldYieldForHighPriorityWork() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // We only yield if there's a urgent task to be run now, or we are expecting
  // one soon (touch start).
  // Note: even though the control queue has the highest priority we don't yield
  // for it since these tasks are not user-provided work and they are only
  // intended to run before the next task, not interrupt the tasks.
  switch (main_thread_only().current_use_case) {
    case UseCase::kCompositorGesture:
    case UseCase::kNone:
      return main_thread_only().blocking_input_expected_soon;

    case UseCase::kMainThreadGesture:
    case UseCase::kMainThreadCustomInputHandling:
    case UseCase::kSynchronizedGesture:
      for (const auto& pair : task_runners_) {
        if (pair.first->GetPrioritisationType() ==
                MainThreadTaskQueue::QueueTraits::PrioritisationType::
                    kCompositor &&
            pair.first->HasTaskToRunImmediatelyOrReadyDelayedTask())
          return true;
      }
      return main_thread_only().blocking_input_expected_soon;

    case UseCase::kTouchstart:
      return true;

    case UseCase::kEarlyLoading:
    case UseCase::kLoading:
    case UseCase::kDiscreteInputResponse:
      return false;
  }
}

base::TimeTicks MainThreadSchedulerImpl::CurrentIdleTaskDeadlineForTesting()
    const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

void MainThreadSchedulerImpl::StartIdlePeriodForTesting() {
  main_thread_only().in_idle_period_for_testing = true;
  IdleTaskRunner()->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&MainThreadSchedulerImpl::EndIdlePeriodForTesting,
                     weak_factory_.GetWeakPtr()));
  idle_helper_.EnableLongIdlePeriod();
}

void MainThreadSchedulerImpl::MaybeUpdatePolicy() {
  helper_.CheckOnValidThread();
  if (policy_may_need_update_.IsSet()) {
    UpdatePolicy();
  }
}

void MainThreadSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
    const base::Location& from_here) {
  // TODO(scheduler-dev): Check that this method isn't called from the main
  // thread.
  any_thread_lock_.AssertAcquired();
  if (!policy_may_need_update_.IsSet()) {
    policy_may_need_update_.SetWhileLocked(true);
    control_task_queue_->GetTaskRunnerWithDefaultTaskType()->PostTask(
        from_here, update_policy_closure_);
  }
}

void MainThreadSchedulerImpl::UpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::kMayEarlyOutIfPolicyUnchanged);
}

void MainThreadSchedulerImpl::ForceUpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::kForceUpdate);
}

void MainThreadSchedulerImpl::UpdatePolicyLocked(UpdateType update_type) {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now = helper_.NowTicks();
  policy_may_need_update_.SetWhileLocked(false);

  base::TimeDelta expected_use_case_duration;
  main_thread_only().current_use_case =
      ComputeCurrentUseCase(now, &expected_use_case_duration);

  base::TimeDelta gesture_expected_flag_valid_for_duration;

  main_thread_only().blocking_input_expected_soon = false;
  if (any_thread().have_seen_a_blocking_gesture) {
    main_thread_only().blocking_input_expected_soon =
        any_thread().user_model.IsGestureExpectedSoon(
            now, &gesture_expected_flag_valid_for_duration);
  }

  // The |new_policy_duration| is the minimum of |expected_use_case_duration|
  // and |gesture_expected_flag_valid_for_duration| unless one is zero in
  // which case we choose the other.
  base::TimeDelta new_policy_duration = expected_use_case_duration;
  if (new_policy_duration.is_zero() ||
      (gesture_expected_flag_valid_for_duration.is_positive() &&
       new_policy_duration > gesture_expected_flag_valid_for_duration)) {
    new_policy_duration = gesture_expected_flag_valid_for_duration;
  }

  if (new_policy_duration.is_positive()) {
    main_thread_only().current_policy_expiration_time =
        now + new_policy_duration;
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, new_policy_duration,
                                              now);
  } else {
    main_thread_only().current_policy_expiration_time = base::TimeTicks();
  }

  // Avoid prioritizing main thread compositing (e.g., rAF) if it is extremely
  // slow, because that can cause starvation in other task sources.
  main_thread_only().main_thread_compositing_is_fast =
      main_thread_only().idle_time_estimator.GetExpectedIdleDuration(
          main_thread_only().compositor_frame_interval) >
      main_thread_only().compositor_frame_interval *
          kFastCompositingIdleTimeThreshold;

  Policy new_policy;
  new_policy.use_case = main_thread_only().current_use_case;
  new_policy.rail_mode = ComputeCurrentRAILMode(new_policy.use_case);

  if (main_thread_only().renderer_pause_count != 0) {
    new_policy.should_pause_task_queues = true;
  }

  if (main_thread_only().pause_timers_for_webview) {
    new_policy.should_pause_task_queues_for_android_webview = true;
  }

  new_policy.find_in_page_priority =
      find_in_page_budget_pool_controller_->CurrentTaskPriority();

  new_policy.should_prioritize_ipc_tasks =
      num_pending_urgent_ipc_messages_.load(std::memory_order_relaxed) > 0;

  new_policy.should_freeze_compositor_task_queue = AllPagesFrozen();

  // Tracing is done before the early out check, because it's quite possible we
  // will otherwise miss this information in traces.
  CreateTraceEventObjectSnapshotLocked();

  // Update the compositor priority before the early out check because the
  // priority computation relies on state outside of the policy
  // (main_thread_compositing_is_fast) that may have been updated here.
  UpdateCompositorTaskQueuePriority();

  // TODO(alexclarke): Can we get rid of force update now?
  // talp: Can't get rid of this, as per-agent scheduling happens on top of the
  //  policy, based on agent states.
  if (update_type == UpdateType::kMayEarlyOutIfPolicyUnchanged &&
      new_policy == main_thread_only().current_policy) {
    return;
  }

  main_thread_only().rail_mode_for_tracing = new_policy.rail_mode;
  if (new_policy.rail_mode != main_thread_only().current_policy.rail_mode) {
    if (isolate()) {
      isolate()->SetRAILMode(RAILModeToV8RAILMode(new_policy.rail_mode));
    }
    for (auto& observer : main_thread_only().rail_mode_observers) {
      observer.OnRAILModeChanged(new_policy.rail_mode);
    }
  }

  Policy old_policy = main_thread_only().current_policy;
  main_thread_only().current_policy = new_policy;

  UpdateStateForAllTaskQueues(old_policy);
}

RAILMode MainThreadSchedulerImpl::ComputeCurrentRAILMode(
    UseCase use_case) const {
  // TODO(skyostil): Add an idle state for foreground tabs too.
  if (main_thread_only().renderer_hidden.get()) {
    return RAILMode::kIdle;
  }

  switch (use_case) {
    case UseCase::kTouchstart:
      return RAILMode::kResponse;

    case UseCase::kDiscreteInputResponse:
      // TODO(crbug.com/350540984): This really should be `RAILMode::kResponse`,
      // but switching out of the loading mode affects GC and causes some
      // benchmark regressions. For now, don't change the `RAILMode` for this
      // experimental `UseCase`.
      return main_thread_only().current_policy.rail_mode;

    case UseCase::kCompositorGesture:
    case UseCase::kSynchronizedGesture:
    case UseCase::kMainThreadGesture:
      if (main_thread_only().blocking_input_expected_soon) {
        return RAILMode::kResponse;
      }
      break;

    case UseCase::kNone:
      // It's only safe to block tasks if we are expecting a compositor
      // driven gesture.
      if (main_thread_only().blocking_input_expected_soon &&
          any_thread().last_gesture_was_compositor_driven) {
        return RAILMode::kResponse;
      }
      break;

    case UseCase::kMainThreadCustomInputHandling:
      break;

    case UseCase::kEarlyLoading:
    case UseCase::kLoading:
      // TODO(skyostil): Experiment with throttling rendering frame rate.
      return RAILMode::kLoad;
  }

  return RAILMode::kAnimation;
}

void MainThreadSchedulerImpl::UpdateStateForAllTaskQueues(
    std::optional<Policy> previous_policy) {
  helper_.CheckOnValidThread();

  const Policy& current_policy = main_thread_only().current_policy;
  const Policy& old_policy =
      previous_policy.value_or(main_thread_only().current_policy);

  bool should_update_priorities =
      !previous_policy.has_value() ||
      ShouldUpdateTaskQueuePriorities(previous_policy.value());
  for (const auto& pair : task_runners_) {
    UpdateTaskQueueState(pair.first.get(), pair.second.get(), old_policy,
                         current_policy, should_update_priorities);
  }

  if (base::FeatureList::IsEnabled(features::kDeferRendererTasksAfterInput)) {
    // TODO(crbug.com/350540984): The `idle_helper_queue_` is not tracked in
    // `task_runners_`, but should be added if this feature ships.
    UpdateTaskQueueState(idle_helper_queue_.get(), idle_queue_voter_.get(),
                         old_policy, current_policy,
                         /*should_update_priority=*/false);
  }
}

void MainThreadSchedulerImpl::UpdateTaskQueueState(
    MainThreadTaskQueue* task_queue,
    TaskQueue::QueueEnabledVoter* task_queue_enabled_voter,
    const Policy& old_policy,
    const Policy& new_policy,
    bool should_update_priority) const {
  if (should_update_priority)
    task_queue->SetQueuePriority(ComputePriority(task_queue));

  if (task_queue_enabled_voter) {
    task_queue_enabled_voter->SetVoteToEnable(
        new_policy.IsQueueEnabled(task_queue, scheduling_settings()));
  }

  // Make sure if there's no voter that the task queue is enabled.
  DCHECK(task_queue_enabled_voter ||
         old_policy.IsQueueEnabled(task_queue, scheduling_settings()));

  if (task_queue->GetPrioritisationType() ==
      MainThreadTaskQueue::QueueTraits::PrioritisationType::kCompositor) {
    task_queue_enabled_voter->SetVoteToEnable(
        !new_policy.should_freeze_compositor_task_queue);
  }
}

UseCase MainThreadSchedulerImpl::ComputeCurrentUseCase(
    base::TimeTicks now,
    base::TimeDelta* expected_use_case_duration) const {
  any_thread_lock_.AssertAcquired();

  // Above all else we want to be responsive to user input.
  *expected_use_case_duration = base::TimeDelta();
  base::TimeDelta time_left_in_continuous_gesture =
      any_thread().user_model.TimeLeftInContinuousUserGesture(now);
  base::TimeDelta time_left_in_discrete_gesture =
      any_thread().user_model.TimeLeftUntilDiscreteInputResponseDeadline(now);

  // A touchstart event can turn into either an actual gesture (scroll) or a
  // discrete input event (click/tap). The policies for these are similar in
  // that both prioritize the compositor task queue and both defer tasks, but
  // the deferral details are a bit different. For now, the existing behavior
  // takes precedent.
  //
  // TODO(crbug.com/350540984): Try to align the different deferral policies
  // after experimenting with discrete input-based deferral.
  if (time_left_in_continuous_gesture.is_positive() &&
      any_thread().awaiting_touch_start_response) {
    // The gesture hasn't been fully established; arrange for compositor tasks
    // to be run at the highest priority, and for tasks to be deferred as to not
    // block gesture establishment.
    *expected_use_case_duration = time_left_in_continuous_gesture;
    return UseCase::kTouchstart;
  }

  if (time_left_in_discrete_gesture.is_positive() &&
      any_thread().awaiting_discrete_input_response) {
    CHECK(
        base::FeatureList::IsEnabled(features::kDeferRendererTasksAfterInput));
    *expected_use_case_duration = time_left_in_discrete_gesture;
    return UseCase::kDiscreteInputResponse;
  }

  if (time_left_in_continuous_gesture.is_positive()) {
    *expected_use_case_duration = time_left_in_continuous_gesture;
    // A gesture has been established. Based on how the gesture is handled we
    // need to choose between one of four use cases:
    // 1. kCompositorGesture where the gesture is processed only on the
    //    compositor thread.
    // 2. MAIN_THREAD_GESTURE where the gesture is processed only on the main
    //    thread.
    // 3. MAIN_THREAD_CUSTOM_INPUT_HANDLING where the main thread processes a
    //    stream of input events and has prevented a default gesture from being
    //    started.
    // 4. SYNCHRONIZED_GESTURE where the gesture is processed on both threads.
    if (any_thread().last_gesture_was_compositor_driven) {
      if (any_thread().begin_main_frame_on_critical_path) {
        return UseCase::kSynchronizedGesture;
      } else {
        return UseCase::kCompositorGesture;
      }
    }
    if (any_thread().default_gesture_prevented) {
      return UseCase::kMainThreadCustomInputHandling;
    } else {
      return UseCase::kMainThreadGesture;
    }
  }

  // Occasionally the meaningful paint fails to be detected, so as a fallback we
  // treat the presence of input as an indirect signal that there is meaningful
  // content on the page.
  if (!any_thread().have_seen_input_since_navigation) {
    if (any_thread().waiting_for_any_main_frame_contentful_paint)
      return UseCase::kEarlyLoading;

    if (base::FeatureList::IsEnabled(
            features::kLoadingPhaseBufferTimeAfterFirstMeaningfulPaint)) {
      if (any_thread().waiting_for_any_main_frame_meaningful_paint) {
        return UseCase::kLoading;
      }
    } else {
      if (any_thread().is_any_main_frame_loading) {
        return UseCase::kLoading;
      }
    }
  }
  return UseCase::kNone;
}

bool MainThreadSchedulerImpl::CanEnterLongIdlePeriod(
    base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_.CheckOnValidThread();

  MaybeUpdatePolicy();
  if (main_thread_only().current_use_case == UseCase::kTouchstart) {
    // Don't start a long idle task in touch start priority, try again when
    // the policy is scheduled to end.
    *next_long_idle_period_delay_out =
        std::max(base::TimeDelta(),
                 main_thread_only().current_policy_expiration_time - now);
    return false;
  }
  return true;
}

MainThreadSchedulerHelper*
MainThreadSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

IdleTimeEstimator* MainThreadSchedulerImpl::GetIdleTimeEstimatorForTesting() {
  return &main_thread_only().idle_time_estimator;
}

base::SequencedTaskRunner* MainThreadSchedulerImpl::GetVirtualTimeTaskRunner() {
  return virtual_time_control_task_queue_->GetTaskRunnerWithDefaultTaskType()
      .get();
}

void MainThreadSchedulerImpl::OnVirtualTimeEnabled() {
  DCHECK(!virtual_time_control_task_queue_);
  virtual_time_control_task_queue_ =
      helper_.NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kControl));
  virtual_time_control_task_queue_->SetQueuePriority(
      TaskPriority::kControlPriority);

  ForceUpdatePolicy();

  for (auto* page_scheduler : main_thread_only().page_schedulers) {
    page_scheduler->OnVirtualTimeEnabled();
  }
}

void MainThreadSchedulerImpl::OnVirtualTimeDisabled() {
  virtual_time_control_task_queue_->ShutdownTaskQueue();
  virtual_time_control_task_queue_ = nullptr;

  ForceUpdatePolicy();

  // Reset the MetricsHelper because it gets confused by time going backwards.
  base::TimeTicks now = NowTicks();
  main_thread_only().metrics_helper.ResetForTest(now);
}

void MainThreadSchedulerImpl::OnVirtualTimePaused() {
  for (const auto& pair : task_runners_) {
    if (pair.first->CanRunWhenVirtualTimePaused())
      continue;
    DCHECK(!pair.first->IsThrottled());
    pair.first->GetTaskQueue()->InsertFence(
        TaskQueue::InsertFencePosition::kNow);
  }
}

void MainThreadSchedulerImpl::OnVirtualTimeResumed() {
  for (const auto& pair : task_runners_) {
    if (pair.first->CanRunWhenVirtualTimePaused())
      continue;
    DCHECK(!pair.first->IsThrottled());
    DCHECK(pair.first->GetTaskQueue()->HasActiveFence());
    pair.first->GetTaskQueue()->RemoveFence();
  }
}

void MainThreadSchedulerImpl::CreateTraceEventObjectSnapshot() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug"),
      "MainThreadScheduler", this, [&](perfetto::TracedValue context) {
        base::AutoLock lock(any_thread_lock_);
        WriteIntoTraceLocked(std::move(context), helper_.NowTicks());
      });
}

void MainThreadSchedulerImpl::CreateTraceEventObjectSnapshotLocked() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug"),
      "MainThreadScheduler", this, [&](perfetto::TracedValue context) {
        WriteIntoTraceLocked(std::move(context), helper_.NowTicks());
      });
}

void MainThreadSchedulerImpl::WriteIntoTraceLocked(
    perfetto::TracedValue context,
    base::TimeTicks optional_now) const {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();

  auto dict = std::move(context).WriteDictionary();

  if (optional_now.is_null())
    optional_now = helper_.NowTicks();
  dict.Add("current_use_case",
           UseCaseToString(main_thread_only().current_use_case));
  dict.Add("compositor_will_send_main_frame_not_expected",
           main_thread_only().compositor_will_send_main_frame_not_expected);
  dict.Add("blocking_input_expected_soon",
           main_thread_only().blocking_input_expected_soon);
  dict.Add("idle_period_state", IdleHelper::IdlePeriodStateToString(
                                    idle_helper_.SchedulerIdlePeriodState()));
  dict.Add("renderer_hidden", main_thread_only().renderer_hidden.get());
  dict.Add("waiting_for_any_main_frame_contentful_paint",
           any_thread().waiting_for_any_main_frame_contentful_paint);
  dict.Add("waiting_for_any_main_frame_meaningful_paint",
           any_thread().waiting_for_any_main_frame_meaningful_paint);
  dict.Add("is_any_main_frame_loading", any_thread().is_any_main_frame_loading);
  dict.Add("have_seen_input_since_navigation",
           any_thread().have_seen_input_since_navigation);
  dict.Add("renderer_backgrounded",
           main_thread_only().renderer_backgrounded.get());
  dict.Add("now", (optional_now - base::TimeTicks()).InMillisecondsF());
  dict.Add("last_idle_period_end_time",
           (any_thread().last_idle_period_end_time - base::TimeTicks())
               .InMillisecondsF());
  dict.Add("awaiting_touch_start_response",
           any_thread().awaiting_touch_start_response);
  dict.Add("begin_main_frame_on_critical_path",
           any_thread().begin_main_frame_on_critical_path);
  dict.Add("last_gesture_was_compositor_driven",
           any_thread().last_gesture_was_compositor_driven);
  dict.Add("default_gesture_prevented", any_thread().default_gesture_prevented);
  dict.Add("is_audio_playing", main_thread_only().is_audio_playing);
  dict.Add("page_schedulers", [&](perfetto::TracedValue context) {
    auto array = std::move(context).WriteArray();
    for (const auto* page_scheduler : main_thread_only().page_schedulers) {
      page_scheduler->WriteIntoTrace(array.AppendItem(), optional_now);
    }
  });

  dict.Add("policy", main_thread_only().current_policy);

  // TODO(skyostil): Can we somehow trace how accurate these estimates were?
  dict.Add("compositor_frame_interval",
           main_thread_only().compositor_frame_interval.InMillisecondsF());
  dict.Add("estimated_next_frame_begin",
           (main_thread_only().estimated_next_frame_begin - base::TimeTicks())
               .InMillisecondsF());
  dict.Add("in_idle_period", any_thread().in_idle_period);

  dict.Add("user_model", any_thread().user_model);
  dict.Add("render_widget_scheduler_signals", render_widget_scheduler_signals_);
  WriteVirtualTimeInfoIntoTrace(dict);
}

bool MainThreadSchedulerImpl::Policy::IsQueueEnabled(
    MainThreadTaskQueue* task_queue,
    const SchedulingSettings& settings) const {
  if (should_pause_task_queues && task_queue->CanBePaused()) {
    return false;
  }

  if (should_pause_task_queues_for_android_webview &&
      task_queue->CanBePausedForAndroidWebview()) {
    return false;
  }

  if (use_case == UseCase::kTouchstart && task_queue->CanBeDeferred()) {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kDeferRendererTasksAfterInput)) {
    if (use_case == UseCase::kDiscreteInputResponse &&
        task_queue->CanBeDeferredForRendering()) {
      std::optional<WebSchedulingPriority> priority =
          task_queue->GetWebSchedulingPriority();
      if (!priority) {
        return false;
      }
      // Web scheduling task priority is dynamic, and the deferrability of
      // background and user-blocking scheduler tasks depends on the specific
      // policy.
      CHECK(settings.discrete_input_task_deferral_policy);
      switch (*settings.discrete_input_task_deferral_policy) {
        case features::TaskDeferralPolicy::kMinimalTypes:
          if (*priority == WebSchedulingPriority::kBackgroundPriority) {
            return false;
          }
          break;
        case features::TaskDeferralPolicy::kNonUserBlockingDeferrableTypes:
        case features::TaskDeferralPolicy::kNonUserBlockingTypes:
          if (*priority != WebSchedulingPriority::kUserBlockingPriority) {
            return false;
          }
          break;
        case features::TaskDeferralPolicy::kAllDeferrableTypes:
        case features::TaskDeferralPolicy::kAllTypes:
          return false;
      }
    }
  }

  return true;
}

void MainThreadSchedulerImpl::Policy::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("rail_mode", RAILModeToString(rail_mode));
  dict.Add("use_case", UseCaseToString(use_case));
  dict.Add("should_pause_task_queues", should_pause_task_queues);
  dict.Add("should_pause_task_queues_for_android_webview",
           should_pause_task_queues_for_android_webview);
  dict.Add("should_freeze_compositor_task_queue",
           should_freeze_compositor_task_queue);
  dict.Add("should_prioritize_ipc_tasks", should_prioritize_ipc_tasks);
}

void MainThreadSchedulerImpl::OnIdlePeriodStarted() {
  base::AutoLock lock(any_thread_lock_);
  any_thread().in_idle_period = true;
  UpdatePolicyLocked(UpdateType::kMayEarlyOutIfPolicyUnchanged);
}

void MainThreadSchedulerImpl::OnIdlePeriodEnded() {
  base::AutoLock lock(any_thread_lock_);
  any_thread().last_idle_period_end_time = helper_.NowTicks();
  any_thread().in_idle_period = false;
  UpdatePolicyLocked(UpdateType::kMayEarlyOutIfPolicyUnchanged);
}

void MainThreadSchedulerImpl::OnPendingTasksChanged(bool has_tasks) {
  if (has_tasks ==
      main_thread_only().compositor_will_send_main_frame_not_expected.get())
    return;

  // Dispatch RequestBeginMainFrameNotExpectedSoon notifications asynchronously.
  // This is needed because idle task can be posted (and OnPendingTasksChanged
  // called) at any moment, including in the middle of allocating an object,
  // when state is not consistent. Posting a task to dispatch notifications
  // minimizes the amount of code that runs and sees an inconsistent state .
  control_task_queue_->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MainThreadSchedulerImpl::DispatchRequestBeginMainFrameNotExpected,
          weak_factory_.GetWeakPtr(), has_tasks));
}

void MainThreadSchedulerImpl::DispatchRequestBeginMainFrameNotExpected(
    bool has_tasks) {
  if (has_tasks ==
      main_thread_only().compositor_will_send_main_frame_not_expected.get())
    return;

  TRACE_EVENT1(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
      "MainThreadSchedulerImpl::DispatchRequestBeginMainFrameNotExpected",
      "has_tasks", has_tasks);
  bool success = false;
  for (PageSchedulerImpl* page_scheduler : main_thread_only().page_schedulers) {
    success |= page_scheduler->RequestBeginMainFrameNotExpected(has_tasks);
  }
  main_thread_only().compositor_will_send_main_frame_not_expected =
      success && has_tasks;
}

void MainThreadSchedulerImpl::DidStartProvisionalLoad(
    bool is_outermost_main_frame) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::DidStartProvisionalLoad");
  if (is_outermost_main_frame) {
    base::AutoLock lock(any_thread_lock_);
    ResetForNavigationLocked();
  }
}

void MainThreadSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    bool is_reload,
    bool is_outermost_main_frame) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::DidCommitProvisionalLoad");
  main_thread_only().has_navigated = true;

  // If this either isn't a history inert commit or it's a reload then we must
  // reset the task cost estimators.
  if (is_outermost_main_frame && (!is_web_history_inert_commit || is_reload)) {
    RAILMode old_rail_mode;
    RAILMode new_rail_mode;
    {
      base::AutoLock lock(any_thread_lock_);
      old_rail_mode = main_thread_only().current_policy.rail_mode;
      ResetForNavigationLocked();
      new_rail_mode = main_thread_only().current_policy.rail_mode;
    }
    if (old_rail_mode == new_rail_mode && isolate())
      isolate()->UpdateLoadStartTime();
  }
}

void MainThreadSchedulerImpl::OnMainFramePaint() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::OnMainFramePaint");
  base::AutoLock lock(any_thread_lock_);

  // The state of a non-ordinary page (e.g. SVG image) shouldn't affect the
  // scheduler's global UseCase.
  any_thread().waiting_for_any_main_frame_contentful_paint =
      IsAnyOrdinaryMainFrameWaitingForFirstContentfulPaint();
  any_thread().waiting_for_any_main_frame_meaningful_paint =
      IsAnyOrdinaryMainFrameWaitingForFirstMeaningfulPaint();
  any_thread().is_any_main_frame_loading = IsAnyOrdinaryMainFrameLoading();

  UpdatePolicyLocked(UpdateType::kMayEarlyOutIfPolicyUnchanged);
}

void MainThreadSchedulerImpl::ResetForNavigationLocked() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "MainThreadSchedulerImpl::ResetForNavigationLocked");
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  any_thread().user_model.Reset(helper_.NowTicks());
  any_thread().have_seen_a_blocking_gesture = false;
  any_thread().waiting_for_any_main_frame_contentful_paint =
      IsAnyOrdinaryMainFrameWaitingForFirstContentfulPaint();
  any_thread().waiting_for_any_main_frame_meaningful_paint =
      IsAnyOrdinaryMainFrameWaitingForFirstMeaningfulPaint();
  any_thread().is_any_main_frame_loading = IsAnyOrdinaryMainFrameLoading();
  any_thread().have_seen_input_since_navigation = false;
  main_thread_only().idle_time_estimator.Clear();
  UpdatePolicyLocked(UpdateType::kMayEarlyOutIfPolicyUnchanged);
}

void MainThreadSchedulerImpl::AddRAILModeObserver(RAILModeObserver* observer) {
  main_thread_only().rail_mode_observers.AddObserver(observer);
  observer->OnRAILModeChanged(main_thread_only().current_policy.rail_mode);
}

void MainThreadSchedulerImpl::RemoveRAILModeObserver(
    RAILModeObserver const* observer) {
  main_thread_only().rail_mode_observers.RemoveObserver(observer);
}

void MainThreadSchedulerImpl::ForEachMainThreadIsolate(
    base::RepeatingCallback<void(v8::Isolate* isolate)> callback) {
  // TODO(dtapuska): For each AgentGroupScheduler's isolate invoke the callback.
  if (v8::Isolate* isolate = Isolate()) {
    callback.Run(isolate);
  }
}

void MainThreadSchedulerImpl::SetRendererProcessType(
    WebRendererProcessType type) {
  main_thread_only().process_type = type;
}

Vector<WebInputEventAttribution>
MainThreadSchedulerImpl::GetPendingUserInputInfo(
    bool include_continuous) const {
  base::AutoLock lock(any_thread_lock_);
  return any_thread().pending_input_monitor.Info(include_continuous);
}

blink::MainThreadScheduler* MainThreadSchedulerImpl::ToMainThreadScheduler() {
  return this;
}

void MainThreadSchedulerImpl::RunIdleTask(Thread::IdleTask task,
                                          base::TimeTicks deadline) {
  std::move(task).Run(deadline);
}

void MainThreadSchedulerImpl::PostIdleTask(const base::Location& location,
                                           Thread::IdleTask task) {
  IdleTaskRunner()->PostIdleTask(
      location,
      base::BindOnce(&MainThreadSchedulerImpl::RunIdleTask, std::move(task)));
}

void MainThreadSchedulerImpl::PostDelayedIdleTask(
    const base::Location& location,
    base::TimeDelta delay,
    Thread::IdleTask task) {
  IdleTaskRunner()->PostDelayedIdleTask(
      location, delay,
      base::BindOnce(&MainThreadSchedulerImpl::RunIdleTask, std::move(task)));
}

void MainThreadSchedulerImpl::PostNonNestableIdleTask(
    const base::Location& location,
    Thread::IdleTask task) {
  IdleTaskRunner()->PostNonNestableIdleTask(
      location,
      base::BindOnce(&MainThreadSchedulerImpl::RunIdleTask, std::move(task)));
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerImpl::V8TaskRunner() {
  return v8_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerImpl::V8UserVisibleTaskRunner() {
  return v8_user_visible_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerImpl::V8BestEffortTaskRunner() {
  return v8_best_effort_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadSchedulerImpl::NonWakingTaskRunner() {
  return non_waking_task_runner_;
}

AgentGroupScheduler* MainThreadSchedulerImpl::CreateAgentGroupScheduler() {
  auto* agent_group_scheduler =
      MakeGarbageCollected<AgentGroupSchedulerImpl>(*this);
  AddAgentGroupScheduler(agent_group_scheduler);
  return agent_group_scheduler;
}

std::unique_ptr<WebAgentGroupScheduler>
MainThreadSchedulerImpl::CreateWebAgentGroupScheduler() {
  return std::make_unique<WebAgentGroupScheduler>(CreateAgentGroupScheduler());
}

void MainThreadSchedulerImpl::RemoveAgentGroupScheduler(
    AgentGroupSchedulerImpl* agent_group_scheduler) {
  DCHECK(main_thread_only().agent_group_schedulers);
  DCHECK(main_thread_only().agent_group_schedulers->Contains(
      agent_group_scheduler));
  main_thread_only().agent_group_schedulers->erase(agent_group_scheduler);
}

AgentGroupScheduler* MainThreadSchedulerImpl::GetCurrentAgentGroupScheduler() {
  helper_.CheckOnValidThread();
  return current_agent_group_scheduler_;
}

void MainThreadSchedulerImpl::SetV8Isolate(v8::Isolate* isolate) {
  ThreadSchedulerBase::SetV8Isolate(isolate);
}

v8::Isolate* MainThreadSchedulerImpl::Isolate() {
  return isolate();
}

base::TimeTicks MainThreadSchedulerImpl::MonotonicallyIncreasingVirtualTime() {
  return GetTickClock()->NowTicks();
}

void MainThreadSchedulerImpl::BeginAgentGroupSchedulerScope(
    AgentGroupScheduler* next_agent_group_scheduler) {
  scoped_refptr<base::SingleThreadTaskRunner> next_task_runner;
  const char* trace_event_scope_name;
  void* trace_event_scope_id;

  if (next_agent_group_scheduler) {
    // If the |next_agent_group_scheduler| is not null, it means that a
    // per-AgentSchedulingGroup task is about to start. In this case, a
    // per-AgentGroupScheduler scope starts.
    next_task_runner = next_agent_group_scheduler->DefaultTaskRunner(),
    trace_event_scope_name = "scheduler.agent_scope";
    trace_event_scope_id = next_agent_group_scheduler;
  } else {
    // If the |next_agent_group_scheduler| is null, it means that a
    // per-thread task is about to start. In this case, a per-thread scope
    // starts.
    next_task_runner = helper_.DefaultTaskRunner();
    trace_event_scope_name = "scheduler.thread_scope";
    trace_event_scope_id = this;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), trace_event_scope_name,
      trace_event_scope_id, "agent_group_scheduler",
      static_cast<void*>(next_agent_group_scheduler));

  AgentGroupScheduler* previous_agent_group_scheduler =
      current_agent_group_scheduler_;
  current_agent_group_scheduler_ = next_agent_group_scheduler;

  scoped_refptr<base::SingleThreadTaskRunner> previous_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle>
      single_thread_task_runner_current_handle_override;
  if (scheduling_settings().mbi_override_task_runner_handle &&
      next_task_runner != previous_task_runner) {
    // per-thread and per-AgentSchedulingGroup task runner allows nested
    // runloop. `MainThreadSchedulerImpl` guarantees that
    // `SingleThreadTaskRunner::GetCurrentDefault()` and
    // `SequencedTaskRunner::GetCurrentDefault()` return a proper task runner
    // even when a nested runloop is used. Because
    // `MainThreadSchedulerImpl::OnTaskStarted()` always overrides
    // STTR/STR::GetCurrentDefault() properly. So there is no concern about
    // returning an unexpected task runner from STTR/STR::GetCurrentDefault() in
    // this specific case.
    single_thread_task_runner_current_handle_override =
        std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            new base::SingleThreadTaskRunner::CurrentDefaultHandle(
                next_task_runner, base::SingleThreadTaskRunner::
                                      CurrentDefaultHandle::MayAlreadyExist{}));
  }

  main_thread_only().agent_group_scheduler_scope_stack.emplace_back(
      AgentGroupSchedulerScope{
          std::move(single_thread_task_runner_current_handle_override),
          previous_agent_group_scheduler, next_agent_group_scheduler,
          std::move(previous_task_runner), std::move(next_task_runner),
          trace_event_scope_name, trace_event_scope_id});
}

void MainThreadSchedulerImpl::EndAgentGroupSchedulerScope() {
  AgentGroupSchedulerScope& agent_group_scheduler_scope =
      main_thread_only().agent_group_scheduler_scope_stack.back();

  if (scheduling_settings().mbi_override_task_runner_handle) {
    DCHECK_EQ(base::SingleThreadTaskRunner::GetCurrentDefault(),
              agent_group_scheduler_scope.current_task_runner);
    DCHECK_EQ(base::SequencedTaskRunner::GetCurrentDefault(),
              agent_group_scheduler_scope.current_task_runner);
  }
  agent_group_scheduler_scope
      .single_thread_task_runner_current_handle_override = nullptr;
  DCHECK_EQ(base::SingleThreadTaskRunner::GetCurrentDefault(),
            agent_group_scheduler_scope.previous_task_runner);
  DCHECK_EQ(base::SequencedTaskRunner::GetCurrentDefault(),
            agent_group_scheduler_scope.previous_task_runner);

  current_agent_group_scheduler_ =
      agent_group_scheduler_scope.previous_agent_group_scheduler;

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
      agent_group_scheduler_scope.trace_event_scope_name,
      agent_group_scheduler_scope.trace_event_scope_id.get(),
      "agent_group_scheduler",
      static_cast<void*>(
          agent_group_scheduler_scope.current_agent_group_scheduler));

  main_thread_only().agent_group_scheduler_scope_stack.pop_back();
}

WebThreadScheduler* MainThreadSchedulerImpl::ToWebMainThreadScheduler() {
  return this;
}

const base::TickClock* MainThreadSchedulerImpl::GetTickClock() const {
  return helper_.GetClock();
}

base::TimeTicks MainThreadSchedulerImpl::NowTicks() const {
  return GetTickClock()->NowTicks();
}

void MainThreadSchedulerImpl::AddAgentGroupScheduler(
    AgentGroupSchedulerImpl* agent_group_scheduler) {
  bool is_new_entry = main_thread_only()
                          .agent_group_schedulers->insert(agent_group_scheduler)
                          .is_new_entry;
  DCHECK(is_new_entry);
}

void MainThreadSchedulerImpl::AddPageScheduler(
    PageSchedulerImpl* page_scheduler) {
  main_thread_only().page_schedulers.insert(page_scheduler);
  DetachOnIPCTaskPostedWhileInBackForwardCacheHandler();
  if (page_scheduler->IsOrdinary()) {
    // MemoryPurgeManager::OnPageCreated() assumes that the page isn't frozen.
    // Its logic must be modified if this assumption is broken in the future.
    CHECK(!page_scheduler->IsFrozen());
    memory_purge_manager_.OnPageCreated();
  }

  base::AutoLock lock(any_thread_lock_);
  any_thread().waiting_for_any_main_frame_contentful_paint =
      IsAnyOrdinaryMainFrameWaitingForFirstContentfulPaint();
  any_thread().waiting_for_any_main_frame_meaningful_paint =
      IsAnyOrdinaryMainFrameWaitingForFirstMeaningfulPaint();
  any_thread().is_any_main_frame_loading = IsAnyOrdinaryMainFrameLoading();
  UpdatePolicyLocked(UpdateType::kMayEarlyOutIfPolicyUnchanged);
}

void MainThreadSchedulerImpl::RemovePageScheduler(
    PageSchedulerImpl* page_scheduler) {
  DCHECK(base::Contains(main_thread_only().page_schedulers, page_scheduler));
  main_thread_only().page_schedulers.erase(page_scheduler);
  if (page_scheduler->IsOrdinary()) {
    memory_purge_manager_.OnPageDestroyed(
        /* frozen=*/page_scheduler->IsFrozen());
  }

  if (IsIpcTrackingEnabledForAllPages()) {
    SetOnIPCTaskPostedWhileInBackForwardCacheIfNeeded();
  }

  if (main_thread_only().is_audio_playing && page_scheduler->IsAudioPlaying()) {
    // This page may have been the only one playing audio.
    OnAudioStateChanged();
  }

  base::AutoLock lock(any_thread_lock_);
  any_thread().waiting_for_any_main_frame_contentful_paint =
      IsAnyOrdinaryMainFrameWaitingForFirstContentfulPaint();
  any_thread().waiting_for_any_main_frame_meaningful_paint =
      IsAnyOrdinaryMainFrameWaitingForFirstMeaningfulPaint();
  any_thread().is_any_main_frame_loading = IsAnyOrdinaryMainFrameLoading();
  UpdatePolicyLocked(UpdateType::kMayEarlyOutIfPolicyUnchanged);
}

void MainThreadSchedulerImpl::OnPageFrozen(
    base::MemoryReductionTaskContext called_from) {
  memory_purge_manager_.OnPageFrozen(called_from);
  UpdatePolicy();
}

void MainThreadSchedulerImpl::OnPageResumed() {
  memory_purge_manager_.OnPageResumed();
  UpdatePolicy();
}

void MainThreadSchedulerImpl::OnTaskStarted(
    MainThreadTaskQueue* queue,
    const base::sequence_manager::Task& task,
    const TaskQueue::TaskTiming& task_timing) {
  if (scheduling_settings().mbi_override_task_runner_handle) {
    BeginAgentGroupSchedulerScope(queue ? queue->GetAgentGroupScheduler()
                                        : nullptr);
  }

  main_thread_only().running_queues.push(queue);
  if (helper_.IsInNestedRunloop())
    return;

  main_thread_only().current_task_start_time = task_timing.start_time();
  main_thread_only().task_description_for_tracing = TaskDescriptionForTracing{
      static_cast<TaskType>(task.task_type),
      queue ? std::optional<MainThreadTaskQueue::QueueType>(queue->queue_type())
            : std::nullopt};

  main_thread_only().task_priority_for_tracing =
      queue ? std::optional<TaskPriority>(queue->GetQueuePriority())
            : std::nullopt;
}

void MainThreadSchedulerImpl::OnTaskCompleted(
    base::WeakPtr<MainThreadTaskQueue> queue,
    const base::sequence_manager::Task& task,
    TaskQueue::TaskTiming* task_timing,
    base::LazyNow* lazy_now) {
  TRACE_EVENT("renderer.scheduler", "BlinkScheduler_OnTaskCompleted");

  // Microtasks may detach the task queue and invalidate |queue|.
  PerformMicrotaskCheckpoint();

  task_timing->RecordTaskEnd(lazy_now);

  DCHECK_LE(task_timing->start_time(), task_timing->end_time());
  DCHECK(!main_thread_only().running_queues.empty());
  DCHECK(!queue ||
         main_thread_only().running_queues.top().get() == queue.get());
  if (task_timing->has_wall_time() && queue && queue->GetFrameScheduler())
    queue->GetFrameScheduler()->AddTaskTime(task_timing->wall_duration());
  main_thread_only().running_queues.pop();

  // The overriding TaskRunnerHandle scope ends here.
  if (scheduling_settings().mbi_override_task_runner_handle)
    EndAgentGroupSchedulerScope();

  if (helper_.IsInNestedRunloop())
    return;

  DispatchOnTaskCompletionCallbacks();

  if (queue) {
    queue->OnTaskRunTimeReported(task_timing);

    if (FrameSchedulerImpl* frame_scheduler = queue->GetFrameScheduler()) {
      frame_scheduler->OnTaskCompleted(task_timing);
    }
  }

  // TODO(altimin): Per-page metrics should also be considered.
  main_thread_only().metrics_helper.RecordTaskMetrics(queue.get(), task,
                                                      *task_timing);
  main_thread_only().task_description_for_tracing = std::nullopt;

  // Unset the state of |task_priority_for_tracing|.
  main_thread_only().task_priority_for_tracing = std::nullopt;

  RecordTaskUkm(queue.get(), task, *task_timing);

  MaybeUpdatePolicyOnTaskCompleted(queue.get(), *task_timing);

  find_in_page_budget_pool_controller_->OnTaskCompleted(queue.get(),
                                                        task_timing);
  ShutdownEmptyDetachedTaskQueues();
}

void MainThreadSchedulerImpl::RecordTaskUkm(
    MainThreadTaskQueue* queue,
    const base::sequence_manager::Task& task,
    const TaskQueue::TaskTiming& task_timing) {
  if (!helper_.ShouldRecordTaskUkm(task_timing.has_thread_time()))
    return;

  for (PageSchedulerImpl* page_scheduler : main_thread_only().page_schedulers) {
    auto status = RecordTaskUkmImpl(
        queue, task, task_timing,
        page_scheduler->SelectFrameForUkmAttribution(), false);
    UMA_HISTOGRAM_ENUMERATION(
        "Scheduler.Experimental.Renderer.UkmRecordingStatus", status,
        UkmRecordingStatus::kCount);
  }
}

UkmRecordingStatus MainThreadSchedulerImpl::RecordTaskUkmImpl(
    MainThreadTaskQueue* queue,
    const base::sequence_manager::Task& task,
    const TaskQueue::TaskTiming& task_timing,
    FrameSchedulerImpl* frame_scheduler,
    bool precise_attribution) {
  // Skip tasks which have deleted the frame or the page scheduler.
  if (!frame_scheduler)
    return UkmRecordingStatus::kErrorMissingFrame;
  if (!frame_scheduler->GetPageScheduler())
    return UkmRecordingStatus::kErrorDetachedFrame;

  ukm::UkmRecorder* ukm_recorder = frame_scheduler->GetUkmRecorder();
  // OOPIFs are not supported.
  if (!ukm_recorder)
    return UkmRecordingStatus::kErrorMissingUkmRecorder;

  ukm::builders::RendererSchedulerTask builder(
      frame_scheduler->GetUkmSourceId());

  builder.SetVersion(kUkmMetricVersion);
  builder.SetPageSchedulers(main_thread_only().page_schedulers.size());

  builder.SetRendererBackgrounded(
      main_thread_only().renderer_backgrounded.get());
  builder.SetRendererHidden(main_thread_only().renderer_hidden.get());
  builder.SetRendererAudible(main_thread_only().is_audio_playing);
  builder.SetUseCase(
      static_cast<int>(main_thread_only().current_use_case.get()));
  builder.SetTaskType(task.task_type);
  builder.SetQueueType(static_cast<int>(
      queue ? queue->queue_type() : MainThreadTaskQueue::QueueType::kDetached));
  builder.SetFrameStatus(static_cast<int>(
      GetFrameStatus(queue ? queue->GetFrameScheduler() : nullptr)));
  builder.SetTaskDuration(task_timing.wall_duration().InMicroseconds());
  builder.SetIsOOPIF(!frame_scheduler->GetPageScheduler()->IsMainFrameLocal());

  if (main_thread_only().renderer_backgrounded.get()) {
    base::TimeDelta time_since_backgrounded =
        (task_timing.end_time() -
         main_thread_only().background_status_changed_at);

    // Trade off for privacy: Round to seconds for times below 10 minutes and
    // minutes afterwards.
    int64_t seconds_since_backgrounded = 0;
    if (time_since_backgrounded < base::Minutes(10)) {
      seconds_since_backgrounded = time_since_backgrounded.InSeconds();
    } else {
      seconds_since_backgrounded =
          time_since_backgrounded.InMinutes() * kSecondsPerMinute;
    }

    builder.SetSecondsSinceBackgrounded(seconds_since_backgrounded);
  }

  if (task_timing.has_thread_time()) {
    builder.SetTaskCPUDuration(task_timing.thread_duration().InMicroseconds());
  }

  builder.Record(ukm_recorder);

  return UkmRecordingStatus::kSuccess;
}

TaskPriority MainThreadSchedulerImpl::ComputePriority(
    MainThreadTaskQueue* task_queue) const {
  DCHECK(task_queue);

  // If |task_queue| is associated to a frame, then the frame scheduler computes
  // the priority.
  FrameSchedulerImpl* frame_scheduler = task_queue->GetFrameScheduler();

  if (frame_scheduler) {
    return frame_scheduler->ComputePriority(task_queue);
  }

  if (task_queue->queue_type() == MainThreadTaskQueue::QueueType::kDefault) {
    return main_thread_only().current_policy.should_prioritize_ipc_tasks
               ? TaskPriority::kVeryHighPriority
               : TaskPriority::kNormalPriority;
  }

  switch (task_queue->GetPrioritisationType()) {
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::kCompositor:
      return main_thread_only().compositor_priority;
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::kInput:
      return TaskPriority::kHighestPriority;
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::kBestEffort:
      return TaskPriority::kBestEffortPriority;
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::kRegular:
      return TaskPriority::kNormalPriority;
    case MainThreadTaskQueue::QueueTraits::PrioritisationType::kLow:
      return TaskPriority::kLowPriority;
    default:
      NOTREACHED_IN_MIGRATION();
      return TaskPriority::kNormalPriority;
  }
}

void MainThreadSchedulerImpl::AddTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  helper_.AddTaskTimeObserver(task_time_observer);
}

void MainThreadSchedulerImpl::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  helper_.RemoveTaskTimeObserver(task_time_observer);
}

std::unique_ptr<CPUTimeBudgetPool>
MainThreadSchedulerImpl::CreateCPUTimeBudgetPoolForTesting(const char* name) {
  return std::make_unique<CPUTimeBudgetPool>(name, &tracing_controller_,
                                             NowTicks());
}

void MainThreadSchedulerImpl::OnTraceLogEnabled() {
  CreateTraceEventObjectSnapshot();
  tracing_controller_.OnTraceLogEnabled();
  for (PageSchedulerImpl* page_scheduler : main_thread_only().page_schedulers) {
    page_scheduler->OnTraceLogEnabled();
  }
}

void MainThreadSchedulerImpl::OnTraceLogDisabled() {}

base::WeakPtr<MainThreadSchedulerImpl> MainThreadSchedulerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool MainThreadSchedulerImpl::IsAudioPlaying() const {
  return main_thread_only().is_audio_playing;
}

bool MainThreadSchedulerImpl::ShouldUpdateTaskQueuePriorities(
    Policy old_policy) const {
  return old_policy.use_case != main_thread_only().current_policy.use_case ||
         old_policy.find_in_page_priority !=
             main_thread_only().current_policy.find_in_page_priority ||
         old_policy.should_prioritize_ipc_tasks !=
             main_thread_only().current_policy.should_prioritize_ipc_tasks;
}

UseCase MainThreadSchedulerImpl::current_use_case() const {
  return main_thread_only().current_use_case;
}

const MainThreadSchedulerImpl::SchedulingSettings&
MainThreadSchedulerImpl::scheduling_settings() const {
  return scheduling_settings_;
}

TaskPriority MainThreadSchedulerImpl::ComputeCompositorPriority() const {
  std::optional<TaskPriority> targeted_main_frame_priority =
      ComputeCompositorPriorityForMainFrame();
  std::optional<TaskPriority> use_case_priority =
      ComputeCompositorPriorityFromUseCase();
  if (!targeted_main_frame_priority && !use_case_priority) {
    return TaskPriority::kNormalPriority;
  } else if (!use_case_priority) {
    return *targeted_main_frame_priority;
  } else if (!targeted_main_frame_priority) {
    return *use_case_priority;
  }

  // Both are set, so some reconciliation is needed.
  CHECK(targeted_main_frame_priority && use_case_priority);
  // If either votes for the highest priority, use that to simplify the
  // remaining case.
  if (*targeted_main_frame_priority == TaskPriority::kHighestPriority ||
      *use_case_priority == TaskPriority::kHighestPriority) {
    return TaskPriority::kHighestPriority;
  }
  // Otherwise, this must be a combination of UseCase::kCompositorGesture and
  // rendering starvation since all other states set the priority to highest.
  CHECK(current_use_case() == UseCase::kCompositorGesture &&
        (main_thread_only().main_frame_prioritization_state ==
             RenderingPrioritizationState::kRenderingStarved ||
         main_thread_only().main_frame_prioritization_state ==
             RenderingPrioritizationState::kRenderingStarvedByRenderBlocking));

  // The default behavior for compositor gestures like compositor-driven
  // scrolling is to deprioritize compositor TQ tasks (low priority) and not
  // apply delay-based anti-starvation. This can lead to degraded user
  // experience due to increased checkerboarding or scrolling blank content.
  // When `features::kThreadedScrollPreventRenderingStarvation` is enabled, we
  // use a configurable value to control the delay-based anti-starvation to
  // mitigate these issues.
  //
  // Note: for other use cases, the computed priority is higher, so they are
  // not prone to rendering starvation in the same way.
  if (!base::FeatureList::IsEnabled(
          features::kThreadedScrollPreventRenderingStarvation)) {
    return *use_case_priority;
  } else {
    CHECK_LE(*targeted_main_frame_priority, *use_case_priority);
    return *targeted_main_frame_priority;
  }
}

void MainThreadSchedulerImpl::UpdateCompositorTaskQueuePriority() {
  TaskPriority old_compositor_priority = main_thread_only().compositor_priority;
  main_thread_only().compositor_priority = ComputeCompositorPriority();

  if (old_compositor_priority == main_thread_only().compositor_priority)
    return;

  for (const auto& pair : task_runners_) {
    if (pair.first->GetPrioritisationType() !=
        MainThreadTaskQueue::QueueTraits::PrioritisationType::kCompositor)
      continue;
    pair.first->SetQueuePriority(ComputePriority(pair.first.get()));
  }
}

void MainThreadSchedulerImpl::MaybeUpdatePolicyOnTaskCompleted(
    MainThreadTaskQueue* queue,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  bool needs_policy_update = false;

  bool should_prioritize_ipc_tasks =
      num_pending_urgent_ipc_messages_.load(std::memory_order_relaxed) > 0;
  if (should_prioritize_ipc_tasks !=
      main_thread_only().current_policy.should_prioritize_ipc_tasks) {
    needs_policy_update = true;
  }

  if (base::FeatureList::IsEnabled(features::kDeferRendererTasksAfterInput) &&
      queue) {
    base::AutoLock lock(any_thread_lock_);
    // In web tests using non-threaded compositing, BeginMainFrame is scheduled
    // (eagarly) via a per-frame kInternalTest task runner, which is ignored
    // here.
    // TODO(crbug.com/350540984): Consider using the appropriate compositor task
    // queue for tests that use non-threaded compositing.
    if (main_thread_only().is_current_task_main_frame &&
        queue->queue_type() == MainThreadTaskQueue::QueueType::kCompositor) {
      if (any_thread().awaiting_discrete_input_response) {
        any_thread().awaiting_discrete_input_response = false;
        any_thread().user_model.DidProcessDiscreteInputResponse();
        needs_policy_update = true;
      }
    } else if (queue->queue_type() == MainThreadTaskQueue::QueueType::kInput &&
               main_thread_only().is_frame_requested_after_discrete_input) {
      CHECK(main_thread_only().is_current_task_discrete_input);
      any_thread().awaiting_discrete_input_response = true;
      any_thread().user_model.DidProcessDiscreteInputEvent(
          task_timing.end_time());
      needs_policy_update = true;
    }
  }

  RenderingPrioritizationState old_state =
      main_thread_only().main_frame_prioritization_state;
  UpdateRenderingPrioritizationStateOnTaskCompleted(queue, task_timing);

  main_thread_only().is_current_task_discrete_input = false;
  main_thread_only().is_frame_requested_after_discrete_input = false;
  main_thread_only().is_current_task_main_frame = false;

  if (needs_policy_update) {
    UpdatePolicy();
  } else if (old_state != main_thread_only().main_frame_prioritization_state) {
    UpdateCompositorTaskQueuePriority();
  }
}

void MainThreadSchedulerImpl::UpdateRenderingPrioritizationStateOnTaskCompleted(
    MainThreadTaskQueue* queue,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (queue &&
      queue->GetQueuePriority() == TaskPriority::kExtremelyHighPriority) {
    main_thread_only().rendering_blocking_duration_since_last_frame +=
        task_timing.wall_duration();
  }

  // With `features::kThreadedScrollPreventRenderingStarvation` enabled, no
  // rendering anti-starvation policy should kick in until the configurable
  // threshold is reached when in `UseCase::kCompositorGesture`.
  base::TimeDelta render_blocking_starvation_threshold =
      base::FeatureList::IsEnabled(
          features::kThreadedScrollPreventRenderingStarvation) &&
              current_use_case() == UseCase::kCompositorGesture &&
              kRenderBlockingStarvationThreshold <
                  scheduling_settings_
                      .compositor_gesture_rendering_starvation_threshold
          ? scheduling_settings_
                .compositor_gesture_rendering_starvation_threshold
          : kRenderBlockingStarvationThreshold;

  // A main frame task resets the rendering prioritization state. Otherwise if
  // the scheduler is waiting for a frame because of discrete input, the state
  // will only change once a main frame happens. Otherwise, compute the state in
  // descending priority order.
  if (queue &&
      queue->queue_type() == MainThreadTaskQueue::QueueType::kCompositor &&
      main_thread_only().is_current_task_main_frame) {
    main_thread_only().last_frame_time = task_timing.end_time();
    main_thread_only().rendering_blocking_duration_since_last_frame =
        base::TimeDelta();
    main_thread_only().main_frame_prioritization_state =
        RenderingPrioritizationState::kNone;
  } else if (main_thread_only().main_frame_prioritization_state !=
             RenderingPrioritizationState::kWaitingForInputResponse) {
    if (queue &&
        queue->queue_type() == MainThreadTaskQueue::QueueType::kInput &&
        main_thread_only().is_current_task_discrete_input) {
      // Assume this input will result in a frame, which we want to show ASAP.
      main_thread_only().main_frame_prioritization_state =
          RenderingPrioritizationState::kWaitingForInputResponse;
    } else if (main_thread_only()
                   .rendering_blocking_duration_since_last_frame >=
               render_blocking_starvation_threshold) {
      main_thread_only().main_frame_prioritization_state =
          RenderingPrioritizationState::kRenderingStarvedByRenderBlocking;
    } else {
      base::TimeDelta threshold;
      switch (current_use_case()) {
        case UseCase::kCompositorGesture:
          threshold = scheduling_settings_
                          .compositor_gesture_rendering_starvation_threshold;
          break;
        case UseCase::kEarlyLoading:
          threshold =
              scheduling_settings_.prioritize_compositing_after_delay_pre_fcp;
          break;
        default:
          threshold =
              scheduling_settings_.prioritize_compositing_after_delay_post_fcp;
          break;
      }
      if (task_timing.end_time() - main_thread_only().last_frame_time >=
          threshold) {
        main_thread_only().main_frame_prioritization_state =
            RenderingPrioritizationState::kRenderingStarved;
      }
    }
  }
}

std::optional<TaskPriority>
MainThreadSchedulerImpl::ComputeCompositorPriorityFromUseCase() const {
  switch (current_use_case()) {
    case UseCase::kCompositorGesture:
      if (main_thread_only().blocking_input_expected_soon)
        return TaskPriority::kHighestPriority;
      // What we really want to do is priorize loading tasks, but that doesn't
      // seem to be safe. Instead we do that by proxy by deprioritizing
      // compositor tasks. This should be safe since we've already gone to the
      // pain of fixing ordering issues with them.
      //
      // During periods of main-thread contention, e.g. scrolling while loading
      // new content, rendering can be indefinitely starved, leading user
      // experience issues like scrolling blank/stale content and
      // checkerboarding. We adjust the compositor TQ priority and enable
      // delay-based rendering anti-starvation when the
      // `kThreadedScrollPreventRenderingStarvation` experiment is enabled to
      // mitigate these issues.
      return TaskPriority::kLowPriority;

    case UseCase::kSynchronizedGesture:
    case UseCase::kMainThreadCustomInputHandling:
      // In main thread input handling use case we don't have perfect knowledge
      // about which things we should be prioritizing, so we don't attempt to
      // block expensive tasks because we don't know whether they were integral
      // to the page's functionality or not.
      if (main_thread_only().main_thread_compositing_is_fast)
        return TaskPriority::kHighestPriority;
      return std::nullopt;

    case UseCase::kMainThreadGesture:
    case UseCase::kTouchstart:
    case UseCase::kDiscreteInputResponse:
      // A main thread gesture is for example a scroll gesture which is handled
      // by the main thread. Since we know the established gesture type, we can
      // be a little more aggressive about prioritizing compositing and input
      // handling over other tasks.
      return TaskPriority::kHighestPriority;

    case UseCase::kNone:
    case UseCase::kEarlyLoading:
    case UseCase::kLoading:
      return std::nullopt;
  }
}

std::optional<TaskPriority>
MainThreadSchedulerImpl::ComputeCompositorPriorityForMainFrame() const {
  switch (main_thread_only().main_frame_prioritization_state) {
    case RenderingPrioritizationState::kNone:
      return std::nullopt;
    case RenderingPrioritizationState::kRenderingStarved:
      // Set higher than most tasks, but lower than render blocking tasks and
      // input.
      return TaskPriority::kVeryHighPriority;
    case RenderingPrioritizationState::kRenderingStarvedByRenderBlocking:
      // Set to rendering blocking to prevent starvation by render blocking
      // tasks, but don't block input.
      return TaskPriority::kExtremelyHighPriority;
    case RenderingPrioritizationState::kWaitingForInputResponse:
      // Return the highest priority here otherwise consecutive heavy inputs
      // (e.g. typing) will starve rendering.
      return TaskPriority::kHighestPriority;
  }
  NOTREACHED();
}

bool MainThreadSchedulerImpl::AllPagesFrozen() const {
  if (main_thread_only().page_schedulers.empty())
    return false;
  for (const auto* scheduler : main_thread_only().page_schedulers) {
    if (!scheduler->IsFrozen())
      return false;
  }
  return true;
}

// static
const char* MainThreadSchedulerImpl::RAILModeToString(RAILMode rail_mode) {
  switch (rail_mode) {
    case RAILMode::kResponse:
      return "response";
    case RAILMode::kAnimation:
      return "animation";
    case RAILMode::kIdle:
      return "idle";
    case RAILMode::kLoad:
      return "load";
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

// static
const char* MainThreadSchedulerImpl::TimeDomainTypeToString(
    TimeDomainType domain_type) {
  switch (domain_type) {
    case TimeDomainType::kReal:
      return "real";
    case TimeDomainType::kVirtual:
      return "virtual";
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

WTF::Vector<base::OnceClosure>&
MainThreadSchedulerImpl::GetOnTaskCompletionCallbacks() {
  return main_thread_only().on_task_completion_callbacks;
}

void MainThreadSchedulerImpl::ExecuteAfterCurrentTaskForTesting(
    base::OnceClosure on_completion_task,
    ExecuteAfterCurrentTaskRestricted) {
  ThreadSchedulerBase::ExecuteAfterCurrentTask(std::move(on_completion_task));
}

void MainThreadSchedulerImpl::OnUrgentMessageReceived() {
  std::atomic_fetch_add_explicit(&num_pending_urgent_ipc_messages_, 1u,
                                 std::memory_order_relaxed);
}

void MainThreadSchedulerImpl::OnUrgentMessageProcessed() {
  uint64_t prev_urgent_message_count = std::atomic_fetch_sub_explicit(
      &num_pending_urgent_ipc_messages_, 1u, std::memory_order_relaxed);
  CHECK_GT(prev_urgent_message_count, 0u);
}

void MainThreadSchedulerImpl::OnWebSchedulingTaskQueuePriorityChanged(
    MainThreadTaskQueue* queue) {
  if (!base::FeatureList::IsEnabled(features::kDeferRendererTasksAfterInput)) {
    return;
  }
  CHECK(scheduling_settings().discrete_input_task_deferral_policy);
  features::TaskDeferralPolicy policy =
      *scheduling_settings().discrete_input_task_deferral_policy;
  if (policy == features::TaskDeferralPolicy::kNonUserBlockingDeferrableTypes ||
      policy == features::TaskDeferralPolicy::kNonUserBlockingTypes ||
      policy == features::TaskDeferralPolicy::kMinimalTypes) {
    CHECK(queue);
    auto iter = task_runners_.find(queue);
    CHECK(iter != task_runners_.end());
    TaskQueue::QueueEnabledVoter* voter = iter->second.get();
    CHECK(voter);
    voter->SetVoteToEnable(main_thread_only().current_policy.IsQueueEnabled(
        queue, scheduling_settings()));
  }
}

}  // namespace scheduler
}  // namespace blink
