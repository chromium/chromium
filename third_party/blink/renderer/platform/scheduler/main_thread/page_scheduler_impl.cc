// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/use_case.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"

namespace blink {
namespace scheduler {

namespace {

constexpr double kDefaultBackgroundBudgetAsCPUFraction = .01;
constexpr double kDefaultMaxBackgroundBudgetLevelInSeconds = 3;
constexpr double kDefaultInitialBackgroundBudgetInSeconds = 1;
constexpr double kDefaultMaxBackgroundThrottlingDelayInSeconds = 0;

// Given that we already align timers to 1Hz, do not report throttling if
// it is under 3s.
constexpr base::TimeDelta kMinimalBackgroundThrottlingDurationToReport =
    base::TimeDelta::FromSeconds(3);

// Delay for fully throttling the page after backgrounding.
constexpr base::TimeDelta kThrottlingDelayAfterBackgrounding =
    base::TimeDelta::FromSeconds(10);

// The amount of time to wait before suspending shared timers, and loading
// etc. after the renderer has been backgrounded. This is used only if
// background suspension is enabled.
constexpr base::TimeDelta kDefaultDelayForBackgroundTabFreezing =
    base::TimeDelta::FromMinutes(5);

// The amount of time to wait before checking network idleness
// after the page has been backgrounded. If network is idle,
// suspend shared timers, and loading etc. This is used only if
// freeze-background-tab-on-network-idle feature is enabled.
// This value should be smaller than kDefaultDelayForBackgroundTabFreezing.
constexpr base::TimeDelta kDefaultDelayForBackgroundAndNetworkIdleTabFreezing =
    base::TimeDelta::FromMinutes(1);

// Values coming from the field trial config are interpreted as follows:
//   -1 is "not set". Scheduler should use a reasonable default.
//   0 corresponds to base::nullopt.
//   Other values are left without changes.

struct BackgroundThrottlingSettings {
  double budget_recovery_rate;
  base::Optional<base::TimeDelta> max_budget_level;
  base::Optional<base::TimeDelta> max_throttling_delay;
  base::Optional<base::TimeDelta> initial_budget;
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

base::Optional<base::TimeDelta> DoubleToOptionalTime(double value) {
  if (value == 0)
    return base::nullopt;
  return base::TimeDelta::FromSecondsD(value);
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
  return base::TimeDelta::FromMilliseconds(
      kDelayForBackgroundTabFreezingMillis.Get());
}

base::TimeDelta GetDelayForBackgroundAndNetworkIdleTabFreezing() {
  static const base::FeatureParam<int>
      kDelayForBackgroundAndNetworkIdleTabFreezingMillis{
          &features::kFreezeBackgroundTabOnNetworkIdle,
          "DelayForBackgroundAndNetworkIdleTabFreezingMills",
          static_cast<int>(kDefaultDelayForBackgroundAndNetworkIdleTabFreezing
                               .InMilliseconds())};
  return base::TimeDelta::FromMilliseconds(
      kDelayForBackgroundAndNetworkIdleTabFreezingMillis.Get());
}

}  // namespace

PageSchedulerImpl::PageSchedulerImpl(
    PageScheduler::Delegate* delegate,
    MainThreadSchedulerImpl* main_thread_scheduler)
    : main_thread_scheduler_(main_thread_scheduler),
      page_visibility_(kDefaultPageVisibility),
      page_visibility_changed_time_(
          main_thread_scheduler->GetTickClock()->NowTicks()),
      audio_state_(AudioState::kSilent),
      is_frozen_(false),
      reported_background_throttling_since_navigation_(false),
      opted_out_from_aggressive_throttling_(false),
      nested_runloop_(false),
      is_main_frame_local_(false),
      is_throttled_(false),
      keep_active_(main_thread_scheduler->SchedulerKeepActive()),
      background_time_budget_pool_(nullptr),
      delegate_(delegate),
      delay_for_background_tab_freezing_(GetDelayForBackgroundTabFreezing()),
      freeze_on_network_idle_enabled_(base::FeatureList::IsEnabled(
          blink::features::kFreezeBackgroundTabOnNetworkIdle)),
      delay_for_background_and_network_idle_tab_freezing_(
          GetDelayForBackgroundAndNetworkIdleTabFreezing()) {
  page_lifecycle_state_tracker_.reset(new PageLifecycleStateTracker(
      this, kDefaultPageVisibility == PageVisibilityState::kVisible
                ? PageLifecycleState::kActive
                : PageLifecycleState::kHiddenBackgrounded));
  main_thread_scheduler->AddPageScheduler(this);
  do_throttle_page_callback_.Reset(base::BindRepeating(
      &PageSchedulerImpl::DoThrottlePage, base::Unretained(this)));
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

  if (background_time_budget_pool_)
    background_time_budget_pool_->Close();
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
  page_visibility_changed_time_ =
      main_thread_scheduler_->GetTickClock()->NowTicks();

  switch (page_visibility_) {
    case PageVisibilityState::kVisible:
      // Visible pages should not be frozen.
      SetPageFrozenImpl(false, NotificationPolicy::kDoNotNotifyFrames);
      page_lifecycle_state_tracker_->SetPageLifecycleState(
          PageLifecycleState::kActive);
      break;
    case PageVisibilityState::kHidden:
      page_lifecycle_state_tracker_->SetPageLifecycleState(
          IsBackgrounded() ? PageLifecycleState::kHiddenBackgrounded
                           : PageLifecycleState::kHiddenForegrounded);
      break;
  }

  if (ShouldFreezePage()) {
    main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, do_freeze_page_callback_.GetCallback(),
        freeze_on_network_idle_enabled_
            ? delay_for_background_and_network_idle_tab_freezing_
            : delay_for_background_tab_freezing_);
  }

  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_)
    frame_scheduler->SetPageVisibilityForTracing(page_visibility_);

  UpdateBackgroundSchedulingLifecycleState(
      NotificationPolicy::kDoNotNotifyFrames);

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
    page_lifecycle_state_tracker_->SetPageLifecycleState(
        PageLifecycleState::kFrozen);
    main_thread_scheduler_->OnPageFrozen();
  } else {
    // The new state may have already been set if unfreezing through the
    // renderer, but that's okay - duplicate state changes won't be recorded.
    if (page_visibility_ == PageVisibilityState::kVisible) {
      page_lifecycle_state_tracker_->SetPageLifecycleState(
          PageLifecycleState::kActive);
    } else if (IsBackgrounded()) {
      page_lifecycle_state_tracker_->SetPageLifecycleState(
          PageLifecycleState::kHiddenBackgrounded);
    } else {
      page_lifecycle_state_tracker_->SetPageLifecycleState(
          PageLifecycleState::kHiddenForegrounded);
    }
    main_thread_scheduler_->OnPageResumed();
  }
}

void PageSchedulerImpl::SetKeepActive(bool keep_active) {
  if (keep_active_ == keep_active)
    return;
  keep_active_ = keep_active;

  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_)
    frame_scheduler->SetPageKeepActiveForTracing(keep_active);

  NotifyFrames();
}

bool PageSchedulerImpl::KeepActive() const {
  return keep_active_;
}

bool PageSchedulerImpl::IsMainFrameLocal() const {
  return is_main_frame_local_;
}

bool PageSchedulerImpl::IsLoading() const {
  return main_thread_scheduler_->current_use_case() == UseCase::kEarlyLoading ||
         main_thread_scheduler_->current_use_case() == UseCase::kLoading;
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
  MaybeInitializeBackgroundCPUTimeBudgetPool();
  frame_schedulers_.insert(frame_scheduler);
  frame_scheduler->UpdatePolicy();
}

std::unique_ptr<blink::FrameScheduler> PageSchedulerImpl::CreateFrameScheduler(
    FrameScheduler::Delegate* delegate,
    blink::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type) {
  return FrameSchedulerImpl::Create(this, delegate, blame_context, frame_type);
}

void PageSchedulerImpl::Unregister(FrameSchedulerImpl* frame_scheduler) {
  DCHECK(frame_schedulers_.find(frame_scheduler) != frame_schedulers_.end());
  frame_schedulers_.erase(frame_scheduler);
}

void PageSchedulerImpl::OnNavigation() {
  reported_background_throttling_since_navigation_ = false;
}

void PageSchedulerImpl::ReportIntervention(const String& message) {
  delegate_->ReportIntervention(message);
}

base::TimeTicks PageSchedulerImpl::EnableVirtualTime() {
  return main_thread_scheduler_->EnableVirtualTime();
}

void PageSchedulerImpl::DisableVirtualTimeForTesting() {
  main_thread_scheduler_->DisableVirtualTimeForTesting();
}

void PageSchedulerImpl::SetVirtualTimePolicy(VirtualTimePolicy policy) {
  main_thread_scheduler_->SetVirtualTimePolicy(policy);
}

void PageSchedulerImpl::SetInitialVirtualTime(base::Time time) {
  main_thread_scheduler_->SetInitialVirtualTime(time);
}

void PageSchedulerImpl::SetInitialVirtualTimeOffset(base::TimeDelta offset) {
  main_thread_scheduler_->SetInitialVirtualTimeOffset(offset);
}

bool PageSchedulerImpl::VirtualTimeAllowedToAdvance() const {
  return main_thread_scheduler_->VirtualTimeAllowedToAdvance();
}

void PageSchedulerImpl::GrantVirtualTimeBudget(
    base::TimeDelta budget,
    base::OnceClosure budget_exhausted_callback) {
  main_thread_scheduler_->VirtualTimeControlTaskRunner()->PostDelayedTask(
      FROM_HERE, std::move(budget_exhausted_callback), budget);
  // This can shift time forwards if there's a pending MaybeAdvanceVirtualTime,
  // so it's important this is called second.
  main_thread_scheduler_->GetVirtualTimeDomain()->SetVirtualTimeFence(
      main_thread_scheduler_->GetVirtualTimeDomain()->Now() + budget);
}

void PageSchedulerImpl::AudioStateChanged(bool is_audio_playing) {
  if (is_audio_playing) {
    audio_state_ = AudioState::kAudible;
    on_audio_silent_closure_.Cancel();
    if (page_visibility_ == PageVisibilityState::kHidden) {
      page_lifecycle_state_tracker_->SetPageLifecycleState(
          PageLifecycleState::kHiddenForegrounded);
    }
    // Pages with audio playing should not be frozen.
    SetPageFrozenImpl(false, NotificationPolicy::kDoNotNotifyFrames);
    NotifyFrames();
    main_thread_scheduler_->OnAudioStateChanged();
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
    page_lifecycle_state_tracker_->SetPageLifecycleState(
        PageLifecycleState::kHiddenBackgrounded);
  }
  if (ShouldFreezePage()) {
    main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, do_freeze_page_callback_.GetCallback(),
        freeze_on_network_idle_enabled_
            ? delay_for_background_and_network_idle_tab_freezing_
            : delay_for_background_tab_freezing_);
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

bool PageSchedulerImpl::IsThrottled() const {
  return is_throttled_;
}

void PageSchedulerImpl::OnAggressiveThrottlingStatusUpdated() {
  bool opted_out_from_aggressive_throttling = false;
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    opted_out_from_aggressive_throttling |=
        frame_scheduler->opted_out_from_aggressive_throttling();
  }

  if (opted_out_from_aggressive_throttling_ !=
      opted_out_from_aggressive_throttling) {
    opted_out_from_aggressive_throttling_ =
        opted_out_from_aggressive_throttling;
    UpdateBackgroundBudgetPoolSchedulingLifecycleState();
  }
}

void PageSchedulerImpl::OnTraceLogEnabled() {
  tracing_controller_.OnTraceLogEnabled();
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->OnTraceLogEnabled();
  }
}

void PageSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("page_visible",
                    page_visibility_ == PageVisibilityState::kVisible);
  state->SetBoolean("is_audio_playing", IsAudioPlaying());
  state->SetBoolean("is_frozen", is_frozen_);
  state->SetBoolean("reported_background_throttling_since_navigation",
                    reported_background_throttling_since_navigation_);
  state->SetBoolean("is_page_freezable", IsBackgrounded());

  state->BeginDictionary("frame_schedulers");
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    state->BeginDictionaryWithCopiedName(PointerToString(frame_scheduler));
    frame_scheduler->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndDictionary();
}

CPUTimeBudgetPool* PageSchedulerImpl::BackgroundCPUTimeBudgetPool() {
  MaybeInitializeBackgroundCPUTimeBudgetPool();
  return background_time_budget_pool_;
}

void PageSchedulerImpl::MaybeInitializeBackgroundCPUTimeBudgetPool() {
  if (background_time_budget_pool_)
    return;

  if (!RuntimeEnabledFeatures::ExpensiveBackgroundTimerThrottlingEnabled())
    return;

  background_time_budget_pool_ =
      main_thread_scheduler_->task_queue_throttler()->CreateCPUTimeBudgetPool(
          "background");
  base::sequence_manager::LazyNow lazy_now(
      main_thread_scheduler_->tick_clock());

  BackgroundThrottlingSettings settings = GetBackgroundThrottlingSettings();

  background_time_budget_pool_->SetMaxBudgetLevel(lazy_now.Now(),
                                                  settings.max_budget_level);
  background_time_budget_pool_->SetMaxThrottlingDelay(
      lazy_now.Now(), settings.max_throttling_delay);

  background_time_budget_pool_->SetTimeBudgetRecoveryRate(
      lazy_now.Now(), settings.budget_recovery_rate);

  if (settings.initial_budget) {
    background_time_budget_pool_->GrantAdditionalBudget(
        lazy_now.Now(), settings.initial_budget.value());
  }

  UpdateBackgroundBudgetPoolSchedulingLifecycleState();
}

void PageSchedulerImpl::OnThrottlingReported(
    base::TimeDelta throttling_duration) {
  if (throttling_duration < kMinimalBackgroundThrottlingDurationToReport)
    return;

  if (reported_background_throttling_since_navigation_)
    return;
  reported_background_throttling_since_navigation_ = true;

  String message = String::Format(
      "Timer tasks have taken too much time while the page was in the "
      "background. "
      "As a result, they have been deferred for %.3f seconds. "
      "See https://www.chromestatus.com/feature/6172836527865856 "
      "for more details",
      throttling_duration.InSecondsF());

  delegate_->ReportIntervention(message);
}

void PageSchedulerImpl::UpdateBackgroundSchedulingLifecycleState(
    NotificationPolicy notification_policy) {
  if (page_visibility_ == PageVisibilityState::kVisible) {
    is_throttled_ = false;
    do_throttle_page_callback_.Cancel();
    UpdateBackgroundBudgetPoolSchedulingLifecycleState();
  } else {
    main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, do_throttle_page_callback_.GetCallback(),
        kThrottlingDelayAfterBackgrounding);
  }
  if (notification_policy == NotificationPolicy::kNotifyFrames)
    NotifyFrames();
}

void PageSchedulerImpl::DoThrottlePage() {
  do_throttle_page_callback_.Cancel();
  is_throttled_ = true;

  UpdateBackgroundBudgetPoolSchedulingLifecycleState();
  NotifyFrames();
}

void PageSchedulerImpl::UpdateBackgroundBudgetPoolSchedulingLifecycleState() {
  if (!background_time_budget_pool_)
    return;

  base::sequence_manager::LazyNow lazy_now(
      main_thread_scheduler_->tick_clock());
  if (is_throttled_ && !opted_out_from_aggressive_throttling_) {
    background_time_budget_pool_->EnableThrottling(&lazy_now);
  } else {
    background_time_budget_pool_->DisableThrottling(&lazy_now);
  }
}

void PageSchedulerImpl::NotifyFrames() {
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->UpdatePolicy();
  }
}

size_t PageSchedulerImpl::FrameCount() const {
  return frame_schedulers_.size();
}

void PageSchedulerImpl::SetMaxVirtualTimeTaskStarvationCount(
    int max_task_starvation_count) {
  main_thread_scheduler_->SetMaxVirtualTimeTaskStarvationCount(
      max_task_starvation_count);
}

MainThreadSchedulerImpl* PageSchedulerImpl::GetMainThreadScheduler() const {
  return main_thread_scheduler_;
}

bool PageSchedulerImpl::IsBackgrounded() const {
  return page_visibility_ == PageVisibilityState::kHidden && !IsAudioPlaying();
}

bool PageSchedulerImpl::ShouldFreezePage() const {
  if (!base::FeatureList::IsEnabled(blink::features::kStopInBackground))
    return false;
  return IsBackgrounded();
}

void PageSchedulerImpl::OnLocalMainFrameNetworkAlmostIdle() {
  if (!freeze_on_network_idle_enabled_)
    return;

  if (!ShouldFreezePage())
    return;

  if (IsFrozen())
    return;

  // If delay_for_background_and_network_idle_tab_freezing_ passes after
  // the page is not visible, we should freeze the page.
  base::TimeDelta passed = main_thread_scheduler_->GetTickClock()->NowTicks() -
                           page_visibility_changed_time_;
  if (passed < delay_for_background_and_network_idle_tab_freezing_)
    return;

  SetPageFrozenImpl(true, NotificationPolicy::kNotifyFrames);
}

void PageSchedulerImpl::DoFreezePage() {
  DCHECK(ShouldFreezePage());

  if (freeze_on_network_idle_enabled_) {
    DCHECK(delegate_);
    base::TimeDelta passed =
        main_thread_scheduler_->GetTickClock()->NowTicks() -
        page_visibility_changed_time_;
    // The page will be frozen if:
    // (1) the main frame is remote, or,
    // (2) the local main frame's network is almost idle, or,
    // (3) delay_for_background_tab passes after the page is not visible.
    if (!delegate_->LocalMainFrameNetworkIsAlmostIdle() &&
        passed < delay_for_background_tab_freezing_) {
      main_thread_scheduler_->ControlTaskRunner()->PostDelayedTask(
          FROM_HERE, do_freeze_page_callback_.GetCallback(),
          delay_for_background_tab_freezing_ - passed);
      return;
    }
  }

  SetPageFrozenImpl(true, NotificationPolicy::kNotifyFrames);
}

PageLifecycleState PageSchedulerImpl::GetPageLifecycleState() const {
  return page_lifecycle_state_tracker_->GetPageLifecycleState();
}

PageSchedulerImpl::PageLifecycleStateTracker::PageLifecycleStateTracker(
    PageSchedulerImpl* page_scheduler_impl,
    PageLifecycleState state)
    : page_scheduler_impl_(page_scheduler_impl),
      current_state_(kDefaultPageLifecycleState) {
  SetPageLifecycleState(state);
}

void PageSchedulerImpl::PageLifecycleStateTracker::SetPageLifecycleState(
    PageLifecycleState new_state) {
  if (new_state == current_state_)
    return;
  base::Optional<PageLifecycleStateTransition> transition =
      ComputePageLifecycleStateTransition(current_state_, new_state);
  if (transition) {
    UMA_HISTOGRAM_ENUMERATION(
        kHistogramPageLifecycleStateTransition,
        static_cast<PageLifecycleStateTransition>(transition.value()));
  }
  if (page_scheduler_impl_->delegate_)
    page_scheduler_impl_->delegate_->SetLifecycleState(new_state);
  current_state_ = new_state;
}

PageLifecycleState
PageSchedulerImpl::PageLifecycleStateTracker::GetPageLifecycleState() const {
  return current_state_;
}

// static
base::Optional<PageSchedulerImpl::PageLifecycleStateTransition>
PageSchedulerImpl::PageLifecycleStateTracker::
    ComputePageLifecycleStateTransition(PageLifecycleState old_state,
                                        PageLifecycleState new_state) {
  switch (old_state) {
    case PageLifecycleState::kUnknown:
      // We don't track the initial transition.
      return base::nullopt;
    case PageLifecycleState::kActive:
      switch (new_state) {
        case PageLifecycleState::kHiddenForegrounded:
          return PageLifecycleStateTransition::kActiveToHiddenForegrounded;
        case PageLifecycleState::kHiddenBackgrounded:
          return PageLifecycleStateTransition::kActiveToHiddenBackgrounded;
        default:
          NOTREACHED();
          return base::nullopt;
      }
    case PageLifecycleState::kHiddenForegrounded:
      switch (new_state) {
        case PageLifecycleState::kActive:
          return PageLifecycleStateTransition::kHiddenForegroundedToActive;
        case PageLifecycleState::kHiddenBackgrounded:
          return PageLifecycleStateTransition::
              kHiddenForegroundedToHiddenBackgrounded;
        case PageLifecycleState::kFrozen:
          return PageLifecycleStateTransition::kHiddenForegroundedToFrozen;
        default:
          NOTREACHED();
          return base::nullopt;
      }
    case PageLifecycleState::kHiddenBackgrounded:
      switch (new_state) {
        case PageLifecycleState::kActive:
          return PageLifecycleStateTransition::kHiddenBackgroundedToActive;
        case PageLifecycleState::kHiddenForegrounded:
          return PageLifecycleStateTransition::
              kHiddenBackgroundedToHiddenForegrounded;
        case PageLifecycleState::kFrozen:
          return PageLifecycleStateTransition::kHiddenBackgroundedToFrozen;
        default:
          NOTREACHED();
          return base::nullopt;
      }
    case PageLifecycleState::kFrozen:
      switch (new_state) {
        case PageLifecycleState::kActive:
          return PageLifecycleStateTransition::kFrozenToActive;
        case PageLifecycleState::kHiddenForegrounded:
          return PageLifecycleStateTransition::kFrozenToHiddenForegrounded;
        case PageLifecycleState::kHiddenBackgrounded:
          return PageLifecycleStateTransition::kFrozenToHiddenBackgrounded;
        default:
          NOTREACHED();
          return base::nullopt;
      }
  }
}

FrameSchedulerImpl* PageSchedulerImpl::SelectFrameForUkmAttribution() {
  for (FrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    if (frame_scheduler->GetUkmRecorder())
      return frame_scheduler;
  }
  return nullptr;
}

WebScopedVirtualTimePauser PageSchedulerImpl::CreateWebScopedVirtualTimePauser(
    const String& name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(main_thread_scheduler_, duration, name);
}

// static
const char PageSchedulerImpl::kHistogramPageLifecycleStateTransition[] =
    "PageScheduler.PageLifecycleStateTransition";

}  // namespace scheduler
}  // namespace blink
