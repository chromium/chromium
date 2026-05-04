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

ObservationDelayController::~ObservationDelayController() = default;

void ObservationDelayController::Wait(base::WeakPtr<web::WebFrame> web_frame,
                                      ReadyCallback callback) {
  if (journal_) {
    journal_->Log(GURL(), task_id_, "ObservationDelay: Wait", {});
  }
  ready_callback_ = std::move(callback);
  // Schedule a kDidTimeout state transition to happen later.
  PostMoveToStateClosure(State::kDidTimeout, GetActorObservationDelayTimeout())
      .Run();
  // Immediately transition to start waiting for page stability.
  PostMoveToStateClosure(State::kWaitForPageStability).Run();
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
  switch (state_) {
    case State::kInitial:
      NOTREACHED();
    case State::kWaitForPageStability:
      // TODO(crbug.com/498991756): We'll introduce and use a
      // PageStabilityJavaScriptFeature in a followup. For now, go to kDone.
      PostMoveToStateClosure(State::kDone).Run();
      break;
    case State::kDidTimeout:
      MoveToState(State::kDone);
      break;
    case State::kDone:
      // The state machine is never entered until Wait is called so a callback
      // must be provided.
      CHECK(ready_callback_);
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
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial,
              {State::kWaitForPageStability}},
          {State::kWaitForPageStability,
              {State::kDidTimeout, State::kDone}},
          {State::kDidTimeout,
              {State::kDone}}
          // clang-format on
      }));
  CHECK_STATE_TRANSITION(transitions, old_state, new_state);
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
    case State::kDidTimeout:
      return "DidTimeout";
    case State::kDone:
      return "Done";
  }
}

}  // namespace actor
