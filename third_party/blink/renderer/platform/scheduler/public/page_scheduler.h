// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_SCHEDULER_H_

#include <memory>
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PLATFORM_EXPORT PageScheduler {
 public:
  class PLATFORM_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // An "ordinary" page is a fully-featured page owned by a web view.
    virtual bool IsOrdinary() const = 0;
    virtual void ReportIntervention(const WTF::String& message) = 0;
    // Returns true if the request has been succcessfully relayed to the
    // compositor.
    virtual bool RequestBeginMainFrameNotExpected(bool new_state) = 0;
    virtual void SetLifecycleState(PageLifecycleState) = 0;
    // Returns true iff the network is idle for the local main frame.
    // Always returns false if the main frame is remote.
    virtual bool LocalMainFrameNetworkIsAlmostIdle() const { return true; }
  };

  virtual ~PageScheduler() = default;

  // The scheduler may throttle tasks associated with background pages.
  virtual void SetPageVisible(bool) = 0;
  // The scheduler transitions app to and from FROZEN state in background.
  virtual void SetPageFrozen(bool) = 0;
  // Tells the scheduler about "keep-alive" state which can be due to:
  // service workers, shared workers, or fetch keep-alive.
  // If true, then the scheduler should not freeze relevant task queues.
  virtual void SetKeepActive(bool) = 0;
  // Whether the main frame of this page is local or not (remote).
  virtual bool IsMainFrameLocal() const = 0;
  virtual void SetIsMainFrameLocal(bool) = 0;
  // Invoked when the local main frame's network becomes almost idle.
  // Never invoked if the main frame is remote.
  virtual void OnLocalMainFrameNetworkAlmostIdle() = 0;

  // Creates a new FrameScheduler. The caller is responsible for deleting
  // it. All tasks executed by the frame scheduler will be attributed to
  // |blame_context|.
  virtual std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      BlameContext*,
      FrameScheduler::FrameType) = 0;

  // Instructs this PageScheduler to use virtual time. When virtual time is
  // enabled the system doesn't actually sleep for the delays between tasks
  // before executing them. Returns the base::TimeTicks that virtual time
  // offsets will be relative to.
  //
  // E.g: A-E are delayed tasks
  //
  // |    A   B C  D           E  (normal)
  // |-----------------------------> time
  //
  // |ABCDE                       (virtual time)
  // |-----------------------------> time
  virtual base::TimeTicks EnableVirtualTime() = 0;

  // Disables virtual time. Note that this is only used for testing, because
  // there's no reason to do this in production.
  virtual void DisableVirtualTimeForTesting() = 0;

  // Returns true if virtual time is currently allowed to advance.
  virtual bool VirtualTimeAllowedToAdvance() const = 0;

  enum class VirtualTimePolicy {
    // In this policy virtual time is allowed to advance. If the blink scheduler
    // runs out of immediate work, the virtual timebase will be incremented so
    // that the next sceduled timer may fire.  NOTE Tasks will be run in time
    // order (as usual).
    kAdvance,

    // In this policy virtual time is not allowed to advance. Delayed tasks
    // posted to task runners owned by any child FrameSchedulers will be
    // paused, unless their scheduled run time is less than or equal to the
    // current virtual time.  Note non-delayed tasks will run as normal.
    kPause,

    // In this policy virtual time is allowed to advance unless there are
    // pending network fetches associated any child FrameScheduler, or a
    // document is being parsed on a background thread. Initially virtual time
    // is not allowed to advance until we have seen at least one load. The aim
    // being to try and make loading (more) deterministic.
    kDeterministicLoading,
  };

  // This is used to set initial Date.now() while in virtual time mode.
  virtual void SetInitialVirtualTime(base::Time time) = 0;

  // This is used for cross origin navigations to account for virtual time
  // advancing in the previous renderer.
  virtual void SetInitialVirtualTimeOffset(base::TimeDelta offset) = 0;

  // Sets the virtual time policy, which is applied imemdiatly to all child
  // FrameSchedulers.
  virtual void SetVirtualTimePolicy(VirtualTimePolicy) = 0;

  // Set the remaining virtual time budget to |budget|. Once the budget runs
  // out, |budget_exhausted_callback| is called. Note that the virtual time
  // policy is not affected when the budget expires.
  virtual void GrantVirtualTimeBudget(
      base::TimeDelta budget,
      base::OnceClosure budget_exhausted_callback) = 0;

  // It's possible for pages to send infinite messages which can arbitrarily
  // block virtual time.  We can prevent this by setting an upper limit on the
  // number of tasks that can run before virtual time is advanced.
  // NB this anti-starvation logic doesn't apply to VirtualTimePolicy::kPause.
  virtual void SetMaxVirtualTimeTaskStarvationCount(
      int max_task_starvation_count) = 0;

  virtual void AudioStateChanged(bool is_audio_playing) = 0;

  virtual bool IsAudioPlaying() const = 0;

  // Returns true if the page should be exempted from aggressive throttling
  // (e.g. due to a page maintaining an active connection).
  virtual bool IsExemptFromBudgetBasedThrottling() const = 0;

  virtual bool OptedOutFromAggressiveThrottlingForTest() const = 0;

  // Returns true if the request has been succcessfully relayed to the
  // compositor.
  virtual bool RequestBeginMainFrameNotExpected(bool new_state) = 0;

  // Returns a WebScopedVirtualTimePauser which can be used to vote for pausing
  // virtual time. Virtual time will be paused if any WebScopedVirtualTimePauser
  // votes to pause it, and only unpaused only if all
  // WebScopedVirtualTimePausers are either destroyed or vote to unpause.  Note
  // the WebScopedVirtualTimePauser returned by this method is initially
  // unpaused.
  virtual WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_SCHEDULER_H_
