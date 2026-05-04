// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_controller.h"

#import <ostream>
#import <string>
#import <utility>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/notreached.h"
#import "base/state_transitions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace actor {

ObservationDelayController::ObservationDelayController(
    ActorTaskId task_id,
    AggregatedJournal* journal)
    : task_id_(task_id) {
  if (journal) {
    journal_ = journal->GetWeakPtr();
  }
}

ObservationDelayController::~ObservationDelayController() {
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void ObservationDelayController::Wait(base::WeakPtr<web::WebState> web_state,
                                      base::WeakPtr<web::WebFrame> web_frame,
                                      ReadyCallback callback) {
  if (journal_) {
    journal_->Log(GURL(), task_id_, "ObservationDelay: Wait", {});
  }
  if (web_state) {
    web_state_ = web_state;
    web_state_observation_.Observe(web_state.get());
  }
  ready_callback_ = std::move(callback);
  // Schedule a kDidTimeout state transition to happen later.
  PostMoveToStateClosure(State::kDidTimeout, GetActorObservationDelayTimeout())
      .Run();
  // Immediately transition to start waiting for page stability.
  PostMoveToStateClosure(State::kWaitForPageStability).Run();
}

void ObservationDelayController::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  CHECK_EQ(web_state_.get(), web_state);
  // This is only called for main frame navigations so we don't check it here.
  if (!navigation_context || navigation_context->IsSameDocument()) {
    return;
  }
  if (state_ == State::kInitial) {
    return;
  }
  // TODO(crbug.com/498991756) - Count how many navigations occur and keep
  // observing if its beyond a given threshold, see https://crrev.com/c/7239787.
  MoveToState(State::kPageNavigated);
}

void ObservationDelayController::DidStopLoading(web::WebState* web_state) {
  CHECK_EQ(web_state_.get(), web_state);
  if (state_ != State::kWaitForLoadCompletion) {
    return;
  }
  // TODO(crbug.com/498991756) - Wait for the visual state to be settled.
  MoveToState(State::kDone);
}

void ObservationDelayController::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state_.get(), web_state);
  web_state_observation_.Reset();
  web_state_ = nullptr;
  MoveToState(State::kDone);
}

void ObservationDelayController::MoveToState(State state) {
  if (state_ == State::kDone) {
    return;
  }
  CheckStateTransition(state_, state);
  if (journal_) {
    journal_->Log(GURL(), task_id_, "ObservationDelay: State Change",
                  {{"old_state", std::string(StateToString(state_))},
                   {"new_state", std::string(StateToString(state))}});
  }

  state_ = state;
  state_history_.push_back(state_);
  if (state_change_testing_callback_) {
    state_change_testing_callback_.Run(state_);
  }
  switch (state_) {
    case State::kInitial:
      NOTREACHED();
    case State::kWaitForPageStability:
      // TODO(crbug.com/498991756): We'll introduce and use a
      // PageStabilityJavaScriptFeature in a followup. For now, go to kDone.
      PostMoveToStateClosure(State::kWaitForLoadCompletion).Run();
      break;
    case State::kWaitForLoadCompletion:
      if (web_state_ && web_state_->IsLoading()) {
        // The state transition will happen in DidStopLoading().
        break;
      }
      PostMoveToStateClosure(State::kDone).Run();
      break;
    case State::kPageNavigated:
      result_ = Result::kPageNavigated;
      MoveToState(State::kDone);
      break;
    case State::kDidTimeout:
      MoveToState(State::kDone);
      break;
    case State::kDone:
      // The state machine is never entered until Wait is called so a callback
      // must be provided.
      CHECK(ready_callback_);
      web_state_observation_.Reset();
      web_state_ = nullptr;
      // Post a task to run the callback so that it's not tied to the lifetime
      // of this class.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(ready_callback_), result_));
      break;
  }
}

base::OnceClosure ObservationDelayController::PostMoveToStateClosure(
    State new_state,
    base::TimeDelta delay) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::OnceClosure task, base::TimeDelta delay) {
        task_runner->PostDelayedTask(FROM_HERE, std::move(task), delay);
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&ObservationDelayController::MoveToState,
                     weak_ptr_factory_.GetWeakPtr(), new_state),
      delay);
}

void ObservationDelayController::CheckStateTransition(State old_state,
                                                      State new_state) {
  // Note: the transitions listed as "async transitions" below can occur at any
  // point in the state machine and should be tolerated even when unexpected.
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial,
              {State::kWaitForPageStability,
               /* async transitions */
               State::kDidTimeout,
               State::kPageNavigated}},
          {State::kWaitForPageStability,
              {State::kWaitForLoadCompletion,
               State::kDone,
               /* async transitions */
               State::kDidTimeout,
               State::kPageNavigated}},
          {State::kWaitForLoadCompletion,
              {State::kDone,
               /* async transitions */
               State::kDidTimeout,
               State::kPageNavigated}},
          {State::kPageNavigated,
              {State::kDone,
               /* async transitions */
               State::kDidTimeout}},
          {State::kDidTimeout,
              {State::kDone,
               /* async transitions */
               State::kPageNavigated}}
          // clang-format on
      }));
  CHECK((transitions)->IsTransitionValid((old_state), (new_state)),
        base::NotFatalUntil::M160)
      << "Invalid transition: " << old_state << " -> " << new_state;
}

std::ostream& operator<<(std::ostream& o,
                         const ObservationDelayController::State& state) {
  return o << ObservationDelayController::StateToString(state);
}

std::string_view ObservationDelayController::StateToString(State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kWaitForPageStability:
      return "WaitForPageStability";
    case State::kWaitForLoadCompletion:
      return "WaitForLoadCompletion";
    case State::kPageNavigated:
      return "PageNavigated";
    case State::kDidTimeout:
      return "DidTimeout";
    case State::kDone:
      return "Done";
  }
}

}  // namespace actor
