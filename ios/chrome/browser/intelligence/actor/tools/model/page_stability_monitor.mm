// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_monitor.h"

#import <iostream>
#import <memory>
#import <string_view>
#import <utility>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/callback.h"
#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "base/notreached.h"
#import "base/state_transitions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/page_content_annotations/core/page_stability_state.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_java_script_feature.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace actor {

using State = ::page_content_annotations::PageStabilityState;

PageStabilityMonitor::PageStabilityMonitor(
    base::WeakPtr<web::WebFrame> target_frame)
    : target_frame_(target_frame) {}

PageStabilityMonitor::~PageStabilityMonitor() {
  if (state_ == State::kDone) {
    return;
  }

  // If we have a callback, ensure it replies now.
  OnWebFrameGoingAway();
  Teardown();
}

void PageStabilityMonitor::NotifyWhenStable(base::TimeDelta observation_delay,
                                            NotifyWhenStableCallback callback) {
  CHECK_EQ(state_, State::kInitial);
  CHECK(!is_stable_callback_);
  is_stable_callback_ = std::move(callback);

  if (!target_frame_) {
    MoveToState(State::kRenderFrameGoingAway);
    return;
  }

  monitoring_start_delay_ = observation_delay;

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PageStabilityMonitor::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      GetActorPageStabilityTimeout());

  MoveToState(State::kMonitorStartDelay);
}

void PageStabilityMonitor::MoveToState(State new_state) {
  if (state_ == State::kDone) {
    return;
  }

  CheckStateTransition(state_, new_state);

  state_ = new_state;
  state_history_.push_back(state_);
  switch (state_) {
    case State::kInitial: {
      NOTREACHED();
    }
    case State::kMonitorStartDelay: {
      PostMoveToStateClosure(State::kStartMonitoring, monitoring_start_delay_)
          .Run();
      break;
    }
    case State::kStartMonitoring: {
      start_monitoring_time_ = base::TimeTicks::Now();

      if (target_frame_ && target_frame_->GetBrowserState()) {
        PageStabilityJavaScriptFeature::GetInstance()->WaitForStability(
            target_frame_,
            base::BindOnce(&PageStabilityMonitor::OnStabilityResult,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    }
    case State::kTimeout: {
      final_result_ =
          ToolExecutionResult(mojom::ActionResultCode::kToolTimeout);
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kMonitorCompleted: {
      base::TimeDelta min_wait_time = GetActorPageStabilityMinWait();

      base::TimeDelta callback_invoke_delay;
      if (min_wait_time.is_positive()) {
        base::TimeDelta elapsed_time =
            base::TimeTicks::Now() - start_monitoring_time_;
        callback_invoke_delay = min_wait_time - elapsed_time;
      }

      if (callback_invoke_delay.is_positive()) {
        callback_invoke_delay_ = callback_invoke_delay;
        MoveToState(State::kDelayCallback);
      } else {
        MoveToState(State::kInvokeCallback);
      }
      break;
    }
    case State::kDelayCallback: {
      CHECK(callback_invoke_delay_.is_positive());
      PostMoveToStateClosure(State::kInvokeCallback, callback_invoke_delay_)
          .Run();
      break;
    }
    case State::kInvokeCallback: {
      CHECK(is_stable_callback_);

      // TODO(crbug.com/498991756): Log the `final_result_` here.
      std::move(is_stable_callback_).Run();

      MoveToState(State::kDone);
      break;
    }
    case State::kRenderFrameGoingAway: {
      final_result_ =
          ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway);
      StopMonitoring();
      MoveToState(State::kInvokeCallback);
      break;
    }
    case State::kDone: {
      Teardown();
      break;
    }
    // These states are not used for the PageStabilityMonitor on iOS.
    case State::kMojoDisconnected:
    case State::kWaitForNavigation:
      NOTREACHED();
  }
}

void PageStabilityMonitor::StopMonitoring() {
  // TODO(crbug.com/498991756): Stop JS monitoring here.
}

void PageStabilityMonitor::Teardown() {
  weak_ptr_factory_.InvalidateWeakPtrsAndDoom();
}

base::OnceClosure PageStabilityMonitor::MoveToStateClosure(State new_state) {
  return base::BindOnce(&PageStabilityMonitor::MoveToState,
                        weak_ptr_factory_.GetWeakPtr(), new_state);
}

base::OnceClosure PageStabilityMonitor::PostMoveToStateClosure(
    State new_state,
    base::TimeDelta delay) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::OnceClosure task, base::TimeDelta delay) {
        task_runner->PostDelayedTask(FROM_HERE, std::move(task), delay);
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      MoveToStateClosure(new_state), delay);
}

// Currently used only for testing.
//
// TODO(crbug.com/498991756): Call from this kStartMonitoring state when the DOM
// is stable.
void PageStabilityMonitor::OnStabilityResult(ToolExecutionResult result) {
  if (state_ == State::kDone || state_ == State::kInvokeCallback ||
      state_ == State::kTimeout) {
    return;
  }
  final_result_ = result;
  if (!monitoring_complete_) {
    monitoring_complete_ = true;
    MoveToState(State::kMonitorCompleted);
  }
}

void PageStabilityMonitor::OnWebFrameGoingAway() {
  // Don't enter the state machine until NotifyWhenStable is called.
  if (state_ == State::kInitial) {
    return;
  }
  MoveToState(State::kRenderFrameGoingAway);
}

void PageStabilityMonitor::OnTimeout() {
  StopMonitoring();
  MoveToState(State::kTimeout);
}

void PageStabilityMonitor::CheckStateTransition(State old_state,
                                                State new_state) {
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial, {
              State::kMonitorStartDelay,
              State::kRenderFrameGoingAway}},
          {State::kMonitorStartDelay, {
              State::kStartMonitoring,
              State::kTimeout,
              State::kRenderFrameGoingAway}},
          {State::kStartMonitoring, {
              State::kMonitorCompleted,
              State::kTimeout,
              State::kRenderFrameGoingAway}},
          {State::kTimeout, {
              State::kInvokeCallback}},
          {State::kMonitorCompleted, {
              State::kDelayCallback,
              State::kInvokeCallback}},
          {State::kDelayCallback, {
              State::kInvokeCallback,
              State::kTimeout,
              State::kRenderFrameGoingAway}},
          {State::kRenderFrameGoingAway, {
              State::kInvokeCallback}},
          {State::kInvokeCallback, {
              State::kDone}},
          {State::kDone, {}}
          // clang-format on
      }));
  CHECK((transitions)->IsTransitionValid((old_state), (new_state)))
      << "Invalid transition: " << old_state << " -> " << new_state;
}

}  // namespace actor
