// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_

#import <ostream>
#import <string_view>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

namespace web {
class WebFrame;
}  // namespace web

namespace actor {

class AggregatedJournal;

// Observes a page during tool-use and determines when the page has settled
// after an action and is ready for an observation.
//
// Mirrored from the desktop equivalent at:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/tools/observation_delay_controller.h;l=39;drc=5948d13accdd1c0a814b950a7b4b12b84fca004a
class ObservationDelayController {
 public:
  enum class Result {
    kOk,
    // This is returned if the primary main frame starts a new navigation
    // while we are waiting.
    kPageNavigated,
  };

  using ReadyCallback = base::OnceCallback<void(Result)>;

  enum class State { kInitial, kWaitForPageStability, kDidTimeout, kDone };

  ObservationDelayController(ActorTaskId task_id, AggregatedJournal* journal);
  ~ObservationDelayController();

  // Waits for page stability on the given `web_frame`, returning the result via
  // `callback`.
  void Wait(base::WeakPtr<web::WebFrame> web_frame, ReadyCallback callback);

 private:
  // Transitions the state machine from `state_` to `state`.
  //
  // Callers should only use this from an async context (e.g. in a callback).
  void MoveToState(State state);

  // A helper to post a task that calls `MoveToState`.
  //
  // Callers should use this from synchronous contexts so that all state
  // transitions happen asynchronously on a sequence.
  base::OnceClosure PostMoveToStateClosure(
      State new_state,
      base::TimeDelta delay = base::TimeDelta());

  // CHECKs that the transition from `old_state` to `new_state` is valid.
  void CheckStateTransition(State old_state, State new_state);
  // These are needed to support CheckStateTransition.
  friend std::ostream& operator<<(
      std::ostream& o,
      const ObservationDelayController::State& state);
  static std::string_view StateToString(State state);

  ActorTaskId task_id_;
  base::WeakPtr<AggregatedJournal> journal_;
  ReadyCallback ready_callback_;
  State state_ = State::kInitial;
  Result result_ = Result::kOk;

  base::WeakPtrFactory<ObservationDelayController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_
