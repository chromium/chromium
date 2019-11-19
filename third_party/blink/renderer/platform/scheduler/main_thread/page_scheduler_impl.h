// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PAGE_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PAGE_SCHEDULER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace base {
namespace trace_event {
class BlameContext;
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {
namespace page_scheduler_impl_unittest {
class PageSchedulerImplTest;
class PageSchedulerImplPageTransitionTest;
class
    PageSchedulerImplPageTransitionTest_PageLifecycleStateTransitionMetric_Test;
}  // namespace page_scheduler_impl_unittest

class CPUTimeBudgetPool;
class FrameSchedulerImpl;
class MainThreadSchedulerImpl;

class PLATFORM_EXPORT PageSchedulerImpl : public PageScheduler {
 public:
  PageSchedulerImpl(PageScheduler::Delegate*, MainThreadSchedulerImpl*);

  ~PageSchedulerImpl() override;

  // PageScheduler implementation:
  void SetPageVisible(bool page_visible) override;
  void SetPageFrozen(bool) override;
  void SetKeepActive(bool) override;
  bool IsMainFrameLocal() const override;
  void SetIsMainFrameLocal(bool is_local) override;
  void OnLocalMainFrameNetworkAlmostIdle() override;

  std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      BlameContext*,
      FrameScheduler::FrameType) override;
  base::TimeTicks EnableVirtualTime() override;
  void DisableVirtualTimeForTesting() override;
  bool VirtualTimeAllowedToAdvance() const override;
  void SetVirtualTimePolicy(VirtualTimePolicy) override;
  void SetInitialVirtualTime(base::Time time) override;
  void SetInitialVirtualTimeOffset(base::TimeDelta offset) override;
  void GrantVirtualTimeBudget(
      base::TimeDelta budget,
      base::OnceClosure budget_exhausted_callback) override;
  void SetMaxVirtualTimeTaskStarvationCount(
      int max_task_starvation_count) override;
  void AudioStateChanged(bool is_audio_playing) override;
  bool IsAudioPlaying() const override;
  bool IsExemptFromBudgetBasedThrottling() const override;
  bool OptedOutFromAggressiveThrottlingForTest() const override;
  bool RequestBeginMainFrameNotExpected(bool new_state) override;
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const WTF::String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override;

  // Virtual for testing.
  virtual void ReportIntervention(const String& message);

  bool IsPageVisible() const;
  bool IsFrozen() const;
  // PageSchedulerImpl::OptedOutFromAggressiveThrottling can be used in non-test
  // code, while PageScheduler::OptedOutFromAggressiveThrottlingForTest can't.
  bool OptedOutFromAggressiveThrottling() const;
  // Note that the frame can throttle queues even when the page is not throttled
  // (e.g. for offscreen frames or recently backgrounded pages).
  bool IsThrottled() const;
  bool KeepActive() const;

  bool IsLoading() const;

  // An "ordinary" PageScheduler is responsible for is a fully-featured page
  // owned by a web view.
  bool IsOrdinary() const;

  void RegisterFrameSchedulerImpl(FrameSchedulerImpl* frame_scheduler);

  MainThreadSchedulerImpl* GetMainThreadScheduler() const;

  void Unregister(FrameSchedulerImpl*);
  void OnNavigation();

  void OnAggressiveThrottlingStatusUpdated();

  void OnTraceLogEnabled();

  // Return a number of child web frame schedulers for this PageScheduler.
  size_t FrameCount() const;

  PageLifecycleState GetPageLifecycleState() const;

  // Generally UKMs are asssociated with the main frame of a page, but the
  // implementation allows to request a recorder from any local frame with
  // the same result (e.g. for OOPIF support), therefore we need to select
  // any frame here.
  // Note that selecting main frame doesn't work for OOPIFs where the main
  // frame it not a local one.
  FrameSchedulerImpl* SelectFrameForUkmAttribution();

  void AsValueInto(base::trace_event::TracedValue* state) const;

  base::WeakPtr<PageSchedulerImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class FrameSchedulerImpl;
  friend class page_scheduler_impl_unittest::PageSchedulerImplTest;
  friend class page_scheduler_impl_unittest::
      PageSchedulerImplPageTransitionTest;
  friend class page_scheduler_impl_unittest::
      PageSchedulerImplPageTransitionTest_PageLifecycleStateTransitionMetric_Test;

  enum class AudioState {
    kSilent,
    kAudible,
    kRecentlyAudible,
  };

  enum class NotificationPolicy { kNotifyFrames, kDoNotNotifyFrames };

  // This enum is used for a histogram and should not be renumbered.
  // It tracks permissible page state transitions between PageLifecycleStates.
  // We allow all transitions except for visible to frozen and self transitions.
  enum class PageLifecycleStateTransition {
    kActiveToHiddenForegrounded = 0,
    kActiveToHiddenBackgrounded = 1,
    kHiddenForegroundedToActive = 2,
    kHiddenForegroundedToHiddenBackgrounded = 3,
    kHiddenForegroundedToFrozen = 4,
    kHiddenBackgroundedToActive = 5,
    kHiddenBackgroundedToHiddenForegrounded = 6,
    kHiddenBackgroundedToFrozen = 7,
    kFrozenToActive = 8,
    kFrozenToHiddenForegrounded = 9,
    kFrozenToHiddenBackgrounded = 10,
    kMaxValue = kFrozenToHiddenBackgrounded,
  };

  class PageLifecycleStateTracker {
    USING_FAST_MALLOC(PageLifecycleStateTracker);

   public:
    explicit PageLifecycleStateTracker(PageSchedulerImpl*, PageLifecycleState);
    ~PageLifecycleStateTracker() = default;

    void SetPageLifecycleState(PageLifecycleState);
    PageLifecycleState GetPageLifecycleState() const;

   private:
    static base::Optional<PageLifecycleStateTransition>
    ComputePageLifecycleStateTransition(PageLifecycleState old_state,
                                        PageLifecycleState new_state);

    static void RecordPageLifecycleStateTransition(
        PageLifecycleStateTransition);

    PageSchedulerImpl* page_scheduler_impl_;
    PageLifecycleState current_state_;

    DISALLOW_COPY_AND_ASSIGN(PageLifecycleStateTracker);
  };

  // We do not throttle anything while audio is played and shortly after that.
  static constexpr base::TimeDelta kRecentAudioDelay =
      base::TimeDelta::FromSeconds(5);

  static const char kHistogramPageLifecycleStateTransition[];

  // Support not issuing a notification to frames when we disable freezing as
  // a part of foregrounding the page.
  void SetPageFrozenImpl(bool frozen, NotificationPolicy notification_policy);

  CPUTimeBudgetPool* BackgroundCPUTimeBudgetPool();
  void MaybeInitializeBackgroundCPUTimeBudgetPool();

  void OnThrottlingReported(base::TimeDelta throttling_duration);

  // Depending on page visibility, either turns throttling off, or schedules a
  // call to enable it after a grace period.
  void UpdateBackgroundSchedulingLifecycleState(
      NotificationPolicy notification_policy);

  // As a part of UpdateBackgroundSchedulingLifecycleState set correct
  // background_time_budget_pool_ state depending on page visibility and
  // number of active connections.
  void UpdateBackgroundBudgetPoolSchedulingLifecycleState();

  // Callback for marking page is silent after a delay since last audible
  // signal.
  void OnAudioSilent();

  // Callback for enabling throttling in background after specified delay.
  // TODO(altimin): Trigger throttling depending on the loading state
  // of the page.
  void DoThrottlePage();

  // Notify frames that the page scheduler state has been updated.
  void NotifyFrames();

  void EnableThrottling();

  // Returns true if the page is backgrounded, false otherwise. A page is
  // considered backgrounded if it is both not visible and not playing audio.
  bool IsBackgrounded() const;

  // Returns true if the page should be frozen after delay, which happens if
  // IsBackgrounded() and freezing is enabled.
  bool ShouldFreezePage() const;

  // Callback for freezing the page. Freezing must be enabled and the page must
  // be freezable.
  void DoFreezePage();

  TraceableVariableController tracing_controller_;
  HashSet<FrameSchedulerImpl*> frame_schedulers_;
  MainThreadSchedulerImpl* main_thread_scheduler_;

  PageVisibilityState page_visibility_;
  base::TimeTicks page_visibility_changed_time_;
  AudioState audio_state_;
  bool is_frozen_;
  bool reported_background_throttling_since_navigation_;
  bool opted_out_from_aggressive_throttling_;
  bool nested_runloop_;
  bool is_main_frame_local_;
  bool is_throttled_;
  bool keep_active_;
  CPUTimeBudgetPool* background_time_budget_pool_;  // Not owned.
  PageScheduler::Delegate* delegate_;               // Not owned.
  CancelableClosureHolder do_throttle_page_callback_;
  CancelableClosureHolder on_audio_silent_closure_;
  CancelableClosureHolder do_freeze_page_callback_;
  const base::TimeDelta delay_for_background_tab_freezing_;

  // Whether a background page can be frozen before
  // |delay_for_background_tab_freezing_| if network is idle.
  const bool freeze_on_network_idle_enabled_;

  // Delay after which a background page can be frozen if network is idle.
  const base::TimeDelta delay_for_background_and_network_idle_tab_freezing_;

  std::unique_ptr<PageLifecycleStateTracker> page_lifecycle_state_tracker_;
  base::WeakPtrFactory<PageSchedulerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PageSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PAGE_SCHEDULER_IMPL_H_
