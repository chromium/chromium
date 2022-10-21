// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/debug/stack_trace.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/wake_up_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/use_case.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {
namespace scheduler {

namespace {

using blink::FrameScheduler;

constexpr double kDefaultBackgroundBudgetAsCPUFraction = .01;
constexpr double kDefaultMaxBackgroundBudgetLevelInSeconds = 3;
constexpr double kDefaultInitialBackgroundBudgetInSeconds = 1;
constexpr double kDefaultMaxBackgroundThrottlingDelayInSeconds = 0;

// Delay for fully throttling the page after backgrounding.
constexpr base::TimeDelta kThrottlingDelayAfterBackgrounding =
    base::Seconds(10);

// The amount of time to wait before suspending shared timers, and loading
// etc. after the renderer has been backgrounded. This is used only if
// background suspension is enabled.
constexpr base::TimeDelta kDefaultDelayForBackgroundTabFreezing =
    base::Minutes(5);

// Duration of a throttled wake up.
constexpr base::TimeDelta kThrottledWakeUpDuration = base::Milliseconds(3);

// The duration for which intensive throttling should be inhibited for
// same-origin frames when the page title or favicon is updated.
constexpr base::TimeDelta
    kTimeToInhibitIntensiveThrottlingOnTitleOrFaviconUpdate = base::Seconds(3);

constexpr base::TimeDelta kDefaultDelayForTrackingIPCsPostedToCachedFrames =
    base::Seconds(15);

// Values coming from the field trial config are interpreted as follows:
//   -1 is "not set". Scheduler should use a reasonable default.
//   0 corresponds to absl::nullopt.
//   Other values are left without changes.

struct BackgroundThrottlingSettings {
  double budget_recovery_rate;
  absl::optional<base::TimeDelta> max_budget_level;
  absl::optional<base::TimeDelta> max_throttling_delay;
  absl::optional<base::TimeDelta> initial_budget;
};

double GetDoubleParameterFromMap(const base::FieldTrialParams& settings,
                                 const std::string& setting_name,
                                 double default_value) {
  const auto& find_it = settings.find(setting_name);
  if (find_it == settings.end())
    return default_value;
  double parsed_value;
  if (!base::StringToDouble(find_it->second, &parsed_value))
    return default_value;
  if (parsed_value == -1)
    return default_value;
  return parsed_value;
}

absl::optional<base::TimeDelta> DoubleToOptionalTime(double value) {
  if (value == 0)
    return absl::nullopt;
  return base::Seconds(value);
}

BackgroundThrottlingSettings GetBackgroundThrottlingSettings() {
  base::FieldTrialParams background_throttling_settings;
  base::GetFieldTrialParams("ExpensiveBackgroundTimerThrottling",
                            &background_throttling_settings);

  BackgroundThrottlingSettings settings;

  settings.budget_recovery_rate =
      GetDoubleParameterFromMap(background_throttling_settings, "cpu_budget",
                                kDefaultBackgroundBudgetAsCPUFraction);

  settings.max_budget_level = DoubleToOptionalTime(
      GetDoubleParameterFromMap(background_throttling_settings, "max_budget",
                                kDefaultMaxBackgroundBudgetLevelInSeconds));

  settings.max_throttling_delay = DoubleToOptionalTime(
      GetDoubleParameterFromMap(background_throttling_settings, "max_delay",
                                kDefaultMaxBackgroundThrottlingDelayInSeconds));

  settings.initial_budget = DoubleToOptionalTime(GetDoubleParameterFromMap(
      background_throttling_settings, "initial_budget",
      kDefaultInitialBackgroundBudgetInSeconds));

  return settings;
}

base::TimeDelta GetDelayForBackgroundTabFreezing() {
  static const base::FeatureParam<int> kDelayForBackgroundTabFreezingMillis{
      &features::kStopInBackground, "DelayForBackgroundTabFreezingMills",
      static_cast<int>(kDefaultDelayForBackgroundTabFreezing.InMilliseconds())};
  return base::Milliseconds(kDelayForBackgroundTabFreezingMillis.Get());
}

base::TimeDelta GetTimeToDelayIPCTrackingWhileStoredInBackForwardCache() {
  if (base::FeatureList::IsEnabled(
          features::kLogUnexpectedIPCPostedToBackForwardCachedDocuments)) {
    static const base::FeatureParam<int>
        kDelayForLoggingUnexpectedIPCPostedToBckForwardCacheMillis{
            &features::kLogUnexpectedIPCPostedToBackForwardCachedDocuments,
            "delay_before_tracking_ms",
            static_cast<int>(kDefaultDelayForTrackingIPCsPostedToCachedFrames
                                 .InMilliseconds())};
    return base::Milliseconds(
        kDelayForLoggingUnexpectedIPCPostedToBckForwardCacheMillis.Get());
  }
  return kDefaultDelayForTrackingIPCsPostedToCachedFrames;
}

}  // namespace

constexpr base::TimeDelta PageSchedulerImpl::kDefaultThrottledWakeUpInterval;
constexpr base::TimeDelta PageSchedulerImpl::kIntensiveThrottledWakeUpInterval;

PageSchedulerImpl::PageSchedulerImpl(
    PageScheduler::Delegate* delegate,
    AgentGroupSchedulerImpl& agent_group_scheduler)
    : main_thread_scheduler_(static_cast<MainThreadSchedulerImpl*>(
          &agent_group_scheduler.GetMainThreadScheduler())),
      agent_group_scheduler_(agent_group_scheduler),
      page_visibility_(kDefaultPageVisibility),
      page_visibility_changed_time_(main_thread_scheduler_->NowTicks()),
      audio_state_(AudioState::kSilent),
      is_frozen_(false),
      opted_out_from_aggressive_throttling_(false),
      nested_runloop_(false),
      is_main_frame_local_(false),
      is_cpu_time_throttled_(false),
      are_wake_ups_intensively_throttled_(false),
      had_recent_title_or_favicon_update_(false),
      delegate_(delegate),
      delay_for_background_tab_freezing_(GetDelayForBackgroundTabFreezing()),
      throttle_foreground_timers_(
          base::FeatureList::IsEnabled(features::kThrottleForegroundTimers)),
      foreground_timers_throttled_wake_up_interval_(
          GetForegroundTimersThrottledWakeUpInterval()) {
  current_lifecycle_state_ =
      (kDefaultPageVisibility == PageVisibilityState::kVisible
           ? PageLifecycleState::kActive
           : PageLifecycleState::kHiddenBackgrounded);
  do_throttle_cpu_time_callback_.Reset(base::BindRepeating(
      &PageSchedulerImpl::DoThrottleCPUTime, base::Unretained(this)));
  do_intensively_throttle_wake_ups_callback_.Reset(
      base::BindRepeating(&PageSchedulerImpl::DoIntensivelyThrottleWakeUps,
                          base::Unretained(this)));
  reset_had_recent_title_or_favicon_update_.Reset(base::BindRepeating(
      &PageSchedulerImpl::ResetHadRecentTitleOrFaviconUpdate,
      base::Unretained(this)));
  on_audio_silent_closure_.Reset(base::BindRepeating(
      &PageSchedulerImpl::OnAudioSilent, base::Unretained(this)));
  do_freeze_page_callback_.Reset(base::BindRepeating(
      &PageSchedulerImpl::DoFreezePage, base::Unretained(this)));
}

PageSchedulerImpl::~PageSchedulerImpl() {
  // TODO(alexclarke): Find out why we can't rely on the web view outliving the
  // frame.
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->DetachFromPageScheduler();
  }
  main_thread_scheduler_->RemovePageScheduler(this);
}

// static
// kRecentAudioDelay is defined in the header for use in unit tests and requires
// storage for linking to succeed with some compiler toolchains.
constexpr base::TimeDelta PageSchedulerImpl::kRecentAudioDelay;

void PageSchedulerImpl::SetPageVisible(bool page_visible) {
  PageVisibilityState page_visibility = page_visible
                                            ? PageVisibilityState::kVisible
                                            : PageVisibilityState::kHidden;

  if (page_visibility_ == page_visibility)
    return;
  page_visibility_ = page_visibility;
  page_visibility_changed_time_ = main_thread_scheduler_->NowTicks();

  switch (page_visibility_) {
    case PageVisibilityState::kVisible:
      SetPageLifecycleState(PageLifecycleState::kActive);
      break;
    case PageVisibilityState::kHidden:
      SetPageLifecycleState(IsBackgrounded()
                                ? PageLifecycleState::kHiddenBackgrounded
                                : PageLifecycleState::kHiddenForegrounded);
      break;
  }

  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_)
    frame_scheduler->SetPageVisibilityForTracing(page_visibility_);

  MoveTaskQueuesToCorrectWakeUpBudgetPoolAndUpdate();
  UpdatePolicyOnVisibilityChange(NotificationPolicy::kDoNotNotifyFrames);

  NotifyFrames();
}

void PageSchedulerImpl::SetPageFrozen(bool frozen) {
  // Only transitions from HIDDEN to FROZEN are allowed for pages (see
  // https://github.com/WICG/page-lifecycle).
  // This is the page freezing path we expose via WebView, which is how
  // embedders freeze pages. Visibility is also controlled by the embedder,
  // through [WebView|WebViewFrameWidget]::SetVisibilityState(). The following
  // happens if the embedder attempts to freeze a page that it set to visible.
  // We check for this illegal state transition later on this code path in page
  // scheduler and frame scheduler when computing the new lifecycle state, but
  // it is desirable to reject the page freeze to prevent the scheduler from
  // being put in a bad state. See https://crbug.com/873214 for context of how
  // this can happen on the browser side.
  if (frozen && IsPageVisible()) {
    DCHECK(false);
    return;
  }
  SetPageFrozenImpl(frozen, NotificationPolicy::kNotifyFrames);
}

void PageSchedulerImpl::SetPageFrozenImpl(
    bool frozen,
    PageSchedulerImpl::NotificationPolicy notification_policy) {
  // Only pages owned by web views can be frozen.
  DCHECK(IsOrdinary());

  do_freeze_page_callback_.Cancel();
  if (is_frozen_ == frozen)
    return;
  is_frozen_ = frozen;
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->SetPageFrozenForTracing(frozen);
    frame_scheduler->SetShouldReportPostedTasksWhenDisabled(frozen);
  }
  if (notification_policy ==
      PageSchedulerImpl::NotificationPolicy::kNotifyFrames)
    NotifyFrames();
  if (frozen) {
    SetPageLifecycleState(PageLifecycleState::kFrozen);
    main_thread_scheduler_->OnPageFrozen();
  } else {
    // The new state may have already been set if unfreezing through the
    // renderer, but that's okay - duplicate state changes won't be recorded.
    if (IsPageVisible()) {
      SetPageLifecycleState(PageLifecycleState::kActive);
    } else if (IsBackgrounded()) {
      SetPageLifecycleState(PageLifecycleState::kHiddenBackgrounded);
    } else {
      SetPageLifecycleState(PageLifecycleState::kHiddenForegrounded);
    }
    // Since the page is no longer frozen, detach the handler that watches for
    // IPCs posted to frozen pages (or cancel setting up the handler).
    set_ipc_posted_handler_task_.Cancel();
    has_ipc_detection_enabled_ = false;
    main_thread_scheduler_->UpdateIpcTracking();
    for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
      frame_scheduler->DetachOnIPCTaskPostedWhileInBackForwardCacheHandler();
    }

    main_thread_scheduler_->OnPageResumed();
  }

  if (delegate_)
    delegate_->OnSetPageFrozen(frozen);
}

void PageSchedulerImpl::SetPageBackForwardCached(
    bool is_in_back_forward_cache) {
  is_stored_in_back_forward_cache_ = is_in_back_forward_cache;

  if (!is_stored_in_back_forward_cache_) {
    TRACE_EVENT_INSTANT("navigation",
                        "PageSchedulerImpl::SetPageBackForwardCached_Restore");
    stored_in_back_forward_cache_timestamp_ = base::TimeTicks();
  } else {
    TRACE_EVENT_INSTANT("navigation",
                        "PageSchedulerImpl::SetPageBackForwardCached_Store");
    stored_in_back_forward_cache_timestamp_ =
        main_thread_scheduler_->NowTicks();

    // Incorporate a delay of 15 seconds to allow for caching operations to
    // complete before tasks are logged.
    set_ipc_posted_handler_task_ = PostDelayedCancellableTask(
        *main_thread_scheduler_->ControlTaskRunner(), FROM_HERE,
        base::BindRepeating(&PageSchedulerImpl::SetUpIPCTaskDetection,
                            GetWeakPtr()),
        GetTimeToDelayIPCTrackingWhileStoredInBackForwardCache());
  }
}

void PageSchedulerImpl::SetUpIPCTaskDetection() {
  DCHECK(is_stored_in_back_forward_cache_);
  has_ipc_detection_enabled_ = true;
  main_thread_scheduler_->UpdateIpcTracking();
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->SetOnIPCTaskPostedWhileInBackForwardCacheHandler();
  }
}

bool PageSchedulerImpl::IsMainFrameLocal() const {
  return is_main_frame_local_;
}

bool PageSchedulerImpl::IsLoading() const {
  return IsWaitingForMainFrameContentfulPaint() ||
         IsWaitingForMainFrameMeaningfulPaint();
}

bool PageSchedulerImpl::IsOrdinary() const {
  if (!delegate_)
    return true;
  return delegate_->IsOrdinary();
}

void PageSchedulerImpl::SetIsMainFrameLocal(bool is_local) {
  is_main_frame_local_ = is_local;
}

void PageSchedulerImpl::RegisterFrameSchedulerImpl(
    FrameSchedulerImpl* frame_scheduler) {
  base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());

  MaybeInitializeWakeUpBudgetPools(&lazy_now);
  MaybeInitializeBackgroundCPUTimeBudgetPool(&lazy_now);

  frame_schedulers_.insert(frame_scheduler);
  frame_scheduler->UpdatePolicy();
}

std::unique_ptr<blink::FrameScheduler> PageSchedulerImpl::CreateFrameScheduler(
    FrameScheduler::Delegate* delegate,
    bool is_in_embedded_frame_tree,
    FrameScheduler::FrameType frame_type) {
  auto frame_scheduler = std::make_unique<FrameSchedulerImpl>(
      this, delegate, is_in_embedded_frame_tree, frame_type);
  RegisterFrameSchedulerImpl(frame_scheduler.get());
  return frame_scheduler;
}

void PageSchedulerImpl::Unregister(FrameSchedulerImpl* frame_scheduler) {
  DCHECK(frame_schedulers_.find(frame_scheduler) != frame_schedulers_.end());
  frame_schedulers_.erase(frame_scheduler);
}

void PageSchedulerImpl::ReportIntervention(const String& message) {
  delegate_->ReportIntervention(message);
}

void PageSchedulerImpl::AudioStateChanged(bool is_audio_playing) {
  if (is_audio_playing) {
    audio_state_ = AudioState::kAudible;
    on_audio_silent_closure_.Cancel();
    if (!IsPageVisible()) {
      SetPageLifecycleState(PageLifecycleState::kHiddenForegrounded);
    }
    // Pages with audio playing should not be frozen.
    SetPageFrozenImpl(false, NotificationPolicy::kDoNotNotifyFrames);
    NotifyFrames();
    main_thread_scheduler_->OnAudioStateChanged();
    MoveTaskQueuesToCorrectWakeUpBudgetPoolAndUpdate();
  } else {
    if (audio_state_ != AudioState::kAudible)
      return;
    on_audio_silent_closure_.Cancel();

    audio_state_ = AudioState::kRecentlyAudible;
    main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, on_audio_silent_closure_.GetCallback(), kRecentAudioDelay);
    // No need to call NotifyFrames or
    // MainThreadScheduler::OnAudioStateChanged here, as for outside world
    // kAudible and kRecentlyAudible are the same thing.
  }
}

void PageSchedulerImpl::OnAudioSilent() {
  DCHECK_EQ(audio_state_, AudioState::kRecentlyAudible);
  audio_state_ = AudioState::kSilent;
  NotifyFrames();
  main_thread_scheduler_->OnAudioStateChanged();
  if (IsBackgrounded()) {
    SetPageLifecycleState(PageLifecycleState::kHiddenBackgrounded);
    MoveTaskQueuesToCorrectWakeUpBudgetPoolAndUpdate();
  }
  if (ShouldFreezePage()) {
    main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, do_freeze_page_callback_.GetCallback(),
        delay_for_background_tab_freezing_);
  }
}

bool PageSchedulerImpl::IsExemptFromBudgetBasedThrottling() const {
  return opted_out_from_aggressive_throttling_;
}

bool PageSchedulerImpl::OptedOutFromAggressiveThrottlingForTest() const {
  return OptedOutFromAggressiveThrottling();
}

bool PageSchedulerImpl::OptedOutFromAggressiveThrottling() const {
  return opted_out_from_aggressive_throttling_;
}

bool PageSchedulerImpl::RequestBeginMainFrameNotExpected(bool new_state) {
  if (!delegate_)
    return false;
  return delegate_->RequestBeginMainFrameNotExpected(new_state);
}

scoped_refptr<WidgetScheduler> PageSchedulerImpl::CreateWidgetScheduler() {
  return main_thread_scheduler_->CreateWidgetScheduler();
}

bool PageSchedulerImpl::IsAudioPlaying() const {
  return audio_state_ == AudioState::kAudible ||
         audio_state_ == AudioState::kRecentlyAudible;
}

bool PageSchedulerImpl::IsPageVisible() const {
  return page_visibility_ == PageVisibilityState::kVisible;
}

bool PageSchedulerImpl::IsFrozen() const {
  return is_frozen_;
}

bool PageSchedulerImpl::IsCPUTimeThrottled() const {
  return is_cpu_time_throttled_;
}

void PageSchedulerImpl::OnThrottlingStatusUpdated() {
  bool opted_out_from_aggressive_throttling = false;
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    opted_out_from_aggressive_throttling |=
        frame_scheduler->opted_out_from_aggressive_throttling();
  }

  if (opted_out_from_aggressive_throttling_ !=
      opted_out_from_aggressive_throttling) {
    opted_out_from_aggressive_throttling_ =
        opted_out_from_aggressive_throttling;
    base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());
    UpdateCPUTimeBudgetPool(&lazy_now);
    UpdateWakeUpBudgetPools(&lazy_now);
  }
}

void PageSchedulerImpl::OnVirtualTimeEnabled() {
  if (page_visibility_ == PageVisibilityState::kHidden) {
    SetPageLifecycleState(PageLifecycleState::kHiddenForegrounded);
  }
  UpdatePolicyOnVisibilityChange(NotificationPolicy::kNotifyFrames);
}

void PageSchedulerImpl::OnTraceLogEnabled() {
  tracing_controller_.OnTraceLogEnabled();
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->OnTraceLogEnabled();
  }
}

bool PageSchedulerImpl::IsWaitingForMainFrameContentfulPaint() const {
  return base::ranges::any_of(
      frame_schedulers_, [](const FrameSchedulerImpl* fs) {
        return fs->IsWaitingForContentfulPaint() &&
               !fs->IsInEmbeddedFrameTree() &&
               fs->GetFrameType() == FrameScheduler::FrameType::kMainFrame;
      });
}

bool PageSchedulerImpl::IsWaitingForMainFrameMeaningfulPaint() const {
  return base::ranges::any_of(
      frame_schedulers_, [](const FrameSchedulerImpl* fs) {
        return fs->IsWaitingForMeaningfulPaint() &&
               !fs->IsInEmbeddedFrameTree() &&
               fs->GetFrameType() == FrameScheduler::FrameType::kMainFrame;
      });
}

void PageSchedulerImpl::WriteIntoTrace(perfetto::TracedValue context,
                                       base::TimeTicks now) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("page_visible", page_visibility_ == PageVisibilityState::kVisible);
  dict.Add("is_audio_playing", IsAudioPlaying());
  dict.Add("is_frozen", is_frozen_);
  dict.Add("is_page_freezable", IsBackgrounded());

  dict.Add("cpu_time_budget_pool", [&](perfetto::TracedValue context) {
    cpu_time_budget_pool_->WriteIntoTrace(std::move(context), now);
  });
  dict.Add("normal_wake_up_budget_pool", [&](perfetto::TracedValue context) {
    normal_wake_up_budget_pool_->WriteIntoTrace(std::move(context), now);
  });
  dict.Add("same_origin_intensive_wake_up_budget_pool",
           [&](perfetto::TracedValue context) {
             same_origin_intensive_wake_up_budget_pool_->WriteIntoTrace(
                 std::move(context), now);
           });
  dict.Add("cross_origin_intensive_wake_up_budget_pool",
           [&](perfetto::TracedValue context) {
             cross_origin_intensive_wake_up_budget_pool_->WriteIntoTrace(
                 std::move(context), now);
           });

  dict.Add("frame_schedulers", frame_schedulers_);
}

void PageSchedulerImpl::AddQueueToWakeUpBudgetPool(
    MainThreadTaskQueue* task_queue,
    FrameOriginType frame_origin_type,
    bool frame_visible,
    base::LazyNow* lazy_now) {
  DCHECK(!task_queue->GetWakeUpBudgetPool());
  WakeUpBudgetPool* wake_up_budget_pool =
      GetWakeUpBudgetPool(task_queue, frame_origin_type, frame_visible);
  task_queue->AddToBudgetPool(lazy_now->Now(), wake_up_budget_pool);
  task_queue->SetWakeUpBudgetPool(wake_up_budget_pool);
}

void PageSchedulerImpl::RemoveQueueFromWakeUpBudgetPool(
    MainThreadTaskQueue* task_queue,
    base::LazyNow* lazy_now) {
  if (!task_queue->GetWakeUpBudgetPool())
    return;
  task_queue->RemoveFromBudgetPool(lazy_now->Now(),
                                   task_queue->GetWakeUpBudgetPool());
  task_queue->SetWakeUpBudgetPool(nullptr);
}

WakeUpBudgetPool* PageSchedulerImpl::GetWakeUpBudgetPool(
    MainThreadTaskQueue* task_queue,
    FrameOriginType frame_origin_type,
    bool frame_visible) {
  const bool can_be_intensively_throttled =
      task_queue->CanBeIntensivelyThrottled();
  const bool is_same_origin =
      frame_origin_type == FrameOriginType::kMainFrame ||
      frame_origin_type == FrameOriginType::kSameOriginToMainFrame;

  if (IsBackgrounded()) {
    if (can_be_intensively_throttled) {
      if (is_same_origin)
        return same_origin_intensive_wake_up_budget_pool_.get();
      else
        return cross_origin_intensive_wake_up_budget_pool_.get();
    }
    return normal_wake_up_budget_pool_.get();
  }

  if (!is_same_origin && !frame_visible)
    return cross_origin_hidden_normal_wake_up_budget_pool_.get();

  return normal_wake_up_budget_pool_.get();
}

CPUTimeBudgetPool* PageSchedulerImpl::background_cpu_time_budget_pool() {
  return cpu_time_budget_pool_.get();
}

void PageSchedulerImpl::MaybeInitializeBackgroundCPUTimeBudgetPool(
    base::LazyNow* lazy_now) {
  if (cpu_time_budget_pool_)
    return;

  cpu_time_budget_pool_ = std::make_unique<CPUTimeBudgetPool>(
      "background", &tracing_controller_, lazy_now->Now());

  BackgroundThrottlingSettings settings = GetBackgroundThrottlingSettings();

  cpu_time_budget_pool_->SetMaxBudgetLevel(lazy_now->Now(),
                                           settings.max_budget_level);
  cpu_time_budget_pool_->SetMaxThrottlingDelay(lazy_now->Now(),
                                               settings.max_throttling_delay);

  cpu_time_budget_pool_->SetTimeBudgetRecoveryRate(
      lazy_now->Now(), settings.budget_recovery_rate);

  if (settings.initial_budget) {
    cpu_time_budget_pool_->GrantAdditionalBudget(
        lazy_now->Now(), settings.initial_budget.value());
  }

  UpdateCPUTimeBudgetPool(lazy_now);
}

void PageSchedulerImpl::MaybeInitializeWakeUpBudgetPools(
    base::LazyNow* lazy_now) {
  if (HasWakeUpBudgetPools())
    return;

  normal_wake_up_budget_pool_ =
      std::make_unique<WakeUpBudgetPool>("Page - Normal Wake Up Throttling");
  cross_origin_hidden_normal_wake_up_budget_pool_ =
      std::make_unique<WakeUpBudgetPool>(
          "Page - Normal Wake Up Throttling - Hidden & Crosss-Origin to Main "
          "Frame");
  same_origin_intensive_wake_up_budget_pool_ =
      std::make_unique<WakeUpBudgetPool>(
          "Page - Intensive Wake Up Throttling - Same-Origin as Main Frame");
  cross_origin_intensive_wake_up_budget_pool_ =
      std::make_unique<WakeUpBudgetPool>(
          "Page - Intensive Wake Up Throttling - Cross-Origin to Main Frame");

  // The Wake Up Duration and Unaligned Wake Ups Allowance are constant and set
  // here. The Wake Up Interval is set in UpdateWakeUpBudgetPools(), based on
  // current state.
  for (WakeUpBudgetPool* pool : AllWakeUpBudgetPools())
    pool->SetWakeUpDuration(kThrottledWakeUpDuration);

  same_origin_intensive_wake_up_budget_pool_
      ->AllowLowerAlignmentIfNoRecentWakeUp(kDefaultThrottledWakeUpInterval);

  UpdateWakeUpBudgetPools(lazy_now);
}

void PageSchedulerImpl::UpdatePolicyOnVisibilityChange(
    NotificationPolicy notification_policy) {
  base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());

  if (IsPageVisible()) {
    is_cpu_time_throttled_ = false;
    do_throttle_cpu_time_callback_.Cancel();
    UpdateCPUTimeBudgetPool(&lazy_now);

    are_wake_ups_intensively_throttled_ = false;
    do_intensively_throttle_wake_ups_callback_.Cancel();
  } else {
    if (cpu_time_budget_pool_) {
      main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
          FROM_HERE, do_throttle_cpu_time_callback_.GetCallback(),
          kThrottlingDelayAfterBackgrounding);
    }
    main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, do_intensively_throttle_wake_ups_callback_.GetCallback(),
        GetIntensiveWakeUpThrottlingGracePeriod(IsLoading()));
  }

  if (ShouldFreezePage()) {
    main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, do_freeze_page_callback_.GetCallback(),
        delay_for_background_tab_freezing_);
  } else {
    SetPageFrozenImpl(false, NotificationPolicy::kDoNotNotifyFrames);
  }

  if (notification_policy == NotificationPolicy::kNotifyFrames)
    NotifyFrames();
}

void PageSchedulerImpl::DoThrottleCPUTime() {
  do_throttle_cpu_time_callback_.Cancel();
  is_cpu_time_throttled_ = true;

  base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());
  UpdateCPUTimeBudgetPool(&lazy_now);
  NotifyFrames();
}

void PageSchedulerImpl::DoIntensivelyThrottleWakeUps() {
  do_intensively_throttle_wake_ups_callback_.Cancel();
  are_wake_ups_intensively_throttled_ = true;

  base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());
  UpdateWakeUpBudgetPools(&lazy_now);
  NotifyFrames();
}

void PageSchedulerImpl::UpdateCPUTimeBudgetPool(base::LazyNow* lazy_now) {
  if (!cpu_time_budget_pool_)
    return;

  if (is_cpu_time_throttled_ && !opted_out_from_aggressive_throttling_) {
    cpu_time_budget_pool_->EnableThrottling(lazy_now);
  } else {
    cpu_time_budget_pool_->DisableThrottling(lazy_now);
  }
}

void PageSchedulerImpl::OnTitleOrFaviconUpdated() {
  if (!HasWakeUpBudgetPools())
    return;
  if (are_wake_ups_intensively_throttled_ &&
      !opted_out_from_aggressive_throttling_) {
    // When the title of favicon is updated, intensive throttling is inhibited
    // for same-origin frames. This enables alternating effects meant to grab
    // the user's attention. Cross-origin frames are not affected, since they
    // shouldn't be able to observe that the page title or favicon was updated.
    had_recent_title_or_favicon_update_ = true;
    base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());
    UpdateWakeUpBudgetPools(&lazy_now);
    // Re-enable intensive throttling from a delayed task.
    reset_had_recent_title_or_favicon_update_.Cancel();
    main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, reset_had_recent_title_or_favicon_update_.GetCallback(),
        kTimeToInhibitIntensiveThrottlingOnTitleOrFaviconUpdate);
  }
}

void PageSchedulerImpl::ResetHadRecentTitleOrFaviconUpdate() {
  had_recent_title_or_favicon_update_ = false;

  base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());
  UpdateWakeUpBudgetPools(&lazy_now);

  NotifyFrames();
}

base::TimeDelta PageSchedulerImpl::GetIntensiveWakeUpThrottlingInterval(
    bool is_same_origin) const {
  // Title and favicon changes only affect the same_origin wake up budget pool.
  if (is_same_origin && had_recent_title_or_favicon_update_)
    return kDefaultThrottledWakeUpInterval;

  if (are_wake_ups_intensively_throttled_ &&
      !opted_out_from_aggressive_throttling_)
    return kIntensiveThrottledWakeUpInterval;
  else
    return kDefaultThrottledWakeUpInterval;
}

void PageSchedulerImpl::UpdateWakeUpBudgetPools(base::LazyNow* lazy_now) {
  if (!same_origin_intensive_wake_up_budget_pool_)
    return;

  normal_wake_up_budget_pool_->SetWakeUpInterval(
      lazy_now->Now(), IsBackgrounded()
                           ? kDefaultThrottledWakeUpInterval
                           : foreground_timers_throttled_wake_up_interval_);
  cross_origin_hidden_normal_wake_up_budget_pool_->SetWakeUpInterval(
      lazy_now->Now(), kDefaultThrottledWakeUpInterval);
  same_origin_intensive_wake_up_budget_pool_->SetWakeUpInterval(
      lazy_now->Now(), GetIntensiveWakeUpThrottlingInterval(true));
  cross_origin_intensive_wake_up_budget_pool_->SetWakeUpInterval(
      lazy_now->Now(), GetIntensiveWakeUpThrottlingInterval(false));
}

void PageSchedulerImpl::NotifyFrames() {
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->UpdatePolicy();
  }
}

size_t PageSchedulerImpl::FrameCount() const {
  return frame_schedulers_.size();
}

MainThreadSchedulerImpl* PageSchedulerImpl::GetMainThreadScheduler() const {
  return main_thread_scheduler_;
}

AgentGroupSchedulerImpl& PageSchedulerImpl::GetAgentGroupScheduler() {
  return *agent_group_scheduler_;
}

VirtualTimeController* PageSchedulerImpl::GetVirtualTimeController() {
  return main_thread_scheduler_;
}

bool PageSchedulerImpl::IsBackgrounded() const {
  // When virtual time is enabled, a freezing request would have its timeout
  // expire immediately when a page is backgrounded, which is undesirable in
  // headless mode. To prevent that, a page is never considerer backgrounded
  // when virtual time is enabled.
  return !IsPageVisible() && !IsAudioPlaying() &&
         !main_thread_scheduler_->IsVirtualTimeEnabled();
}

bool PageSchedulerImpl::ShouldFreezePage() const {
  if (!base::FeatureList::IsEnabled(blink::features::kStopInBackground))
    return false;
  return IsBackgrounded();
}

void PageSchedulerImpl::DoFreezePage() {
  DCHECK(ShouldFreezePage());

  SetPageFrozenImpl(true, NotificationPolicy::kNotifyFrames);
}

PageLifecycleState PageSchedulerImpl::GetPageLifecycleState() const {
  return current_lifecycle_state_;
}

void PageSchedulerImpl::SetPageLifecycleState(PageLifecycleState new_state) {
  if (new_state == current_lifecycle_state_)
    return;
  current_lifecycle_state_ = new_state;
}

FrameSchedulerImpl* PageSchedulerImpl::SelectFrameForUkmAttribution() {
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    if (frame_scheduler->GetUkmRecorder())
      return frame_scheduler;
  }
  return nullptr;
}

bool PageSchedulerImpl::HasWakeUpBudgetPools() const {
  // All WakeUpBudgetPools should be initialized together.
  DCHECK_EQ(!!normal_wake_up_budget_pool_,
            !!cross_origin_hidden_normal_wake_up_budget_pool_);
  DCHECK_EQ(!!normal_wake_up_budget_pool_,
            !!same_origin_intensive_wake_up_budget_pool_);
  DCHECK_EQ(!!normal_wake_up_budget_pool_,
            !!cross_origin_intensive_wake_up_budget_pool_);

  return !!normal_wake_up_budget_pool_;
}

void PageSchedulerImpl::MoveTaskQueuesToCorrectWakeUpBudgetPoolAndUpdate() {
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_)
    frame_scheduler->MoveTaskQueuesToCorrectWakeUpBudgetPool();

  // Update the WakeUpBudgetPools' interval everytime task queues change their
  // attached WakeUpBudgetPools
  base::LazyNow lazy_now(main_thread_scheduler_->GetTickClock());
  UpdateWakeUpBudgetPools(&lazy_now);
}

std::array<WakeUpBudgetPool*, PageSchedulerImpl::kNumWakeUpBudgetPools>
PageSchedulerImpl::AllWakeUpBudgetPools() {
  return {normal_wake_up_budget_pool_.get(),
          cross_origin_hidden_normal_wake_up_budget_pool_.get(),
          same_origin_intensive_wake_up_budget_pool_.get(),
          cross_origin_intensive_wake_up_budget_pool_.get()};
}

}  // namespace scheduler
}  // namespace blink
