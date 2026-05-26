// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_MONITOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_MONITOR_H_

#import <vector>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/timer/timer.h"
#import "base/types/expected.h"
#import "components/page_content_annotations/core/page_stability_state.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace web {
class WebFrame;
}  // namespace web

namespace actor {

// Monitors page stability signals to determine when a page has settled.
//
// Mostly mirrored from the class used by the Desktop actor:
// https://source.chromium.org/chromium/chromium/src/+/main:components/page_content_annotations/content/renderer/page_stability_monitor.h;l=32;drc=924b5bcf53a392fa97f0d517c0d8df67d2cffb1f
//
// TODO(crbug.com/498991756): introduce a PageStabilityMonitorDelegate and use
// it to pass in constants and inform callers of page changes.
class PageStabilityMonitor {
 public:
  using PageStabilityState = ::page_content_annotations::PageStabilityState;
  using State = PageStabilityState;
  using NotifyWhenStableCallback = base::OnceCallback<void()>;

  PageStabilityMonitor(base::WeakPtr<web::WebFrame> target_frame);
  ~PageStabilityMonitor();

  // Invokes the given callback when the page is deemed stable enough for an
  // observation to take place or when the document is no longer active.
  //
  // `observation_delay` is the amount of time to wait before starting to wait
  // for page stability.
  void NotifyWhenStable(base::TimeDelta observation_delay,
                        NotifyWhenStableCallback callback);

  const std::vector<State>& StateHistoryForTesting() const {
    return state_history_;
  }

 private:
  friend class PageStabilityMonitorTest;

  // Synchronously moves to the given state.
  void MoveToState(PageStabilityState new_state);

  // Returns a closure that synchronously moves to the given state. This avoids
  // the extra scheduling hop of `PostMoveToStateClosure`, which is useful if
  // the closure is already being scheduled to run in a separate task.
  base::OnceClosure MoveToStateClosure(PageStabilityState new_state);

  // Helper that provides a closure that invokes MoveToState with the given
  // State on the default task queue for the sequence that created this object.
  base::OnceClosure PostMoveToStateClosure(
      PageStabilityState new_state,
      base::TimeDelta delay = base::TimeDelta());

  void CheckStateTransition(PageStabilityState old_state,
                            PageStabilityState new_state);

  void OnStabilityResult(ToolExecutionResult result);
  void OnWebFrameGoingAway();
  void OnTimeout();
  void StopMonitoring();
  void Teardown();

  base::WeakPtr<web::WebFrame> target_frame_;
  PageStabilityState state_ = PageStabilityState::kInitial;

  NotifyWhenStableCallback is_stable_callback_;

  // Amount of time to delay before monitoring begins.
  base::TimeDelta monitoring_start_delay_;

  // The time at which monitoring begins.
  base::TimeTicks start_monitoring_time_;

  // Amount of time to delay before invoking the callback.
  base::TimeDelta callback_invoke_delay_;

  // Tracks the result of checking the page for stability to propagate any
  // error codes.
  ToolExecutionResult final_result_ = ToolExecutionResult::Ok();

  bool monitoring_complete_ = false;

  std::vector<State> state_history_ = {State::kInitial};

  base::WeakPtrFactory<PageStabilityMonitor> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_MONITOR_H_
