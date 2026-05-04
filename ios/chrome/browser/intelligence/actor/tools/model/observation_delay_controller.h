// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_

#import <ostream>
#import <string_view>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/web_state_observer.h"

namespace web {
class WebFrame;
class WebState;
class WebStateObserver;
class NavigationContext;
}  // namespace web

namespace actor {

class AggregatedJournal;

// Observes a page during tool-use and determines when the page has settled
// after an action and is ready for an observation.
//
// Mirrored from the desktop equivalent at:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/tools/observation_delay_controller.h;l=39;drc=5948d13accdd1c0a814b950a7b4b12b84fca004a
class ObservationDelayController : public web::WebStateObserver {
 public:
  enum class Result {
    kOk,
    // This is returned if the primary main frame starts a new navigation
    // while we are waiting.
    kPageNavigated,
  };

  using ReadyCallback = base::OnceCallback<void(Result)>;

  enum class State {
    kInitial,
    kWaitForPageStability,
    kWaitForLoadCompletion,
    kPageNavigated,
    kDidTimeout,
    kDone
  };

  ObservationDelayController(ActorTaskId task_id, AggregatedJournal* journal);
  ~ObservationDelayController() override;

  // Waits for page stability on the given `web_frame`, returning the result via
  // `callback`.
  void Wait(base::WeakPtr<web::WebState> web_state,
            base::WeakPtr<web::WebFrame> web_frame,
            ReadyCallback callback);

  // Returns the history of states this controller has gone through.
  const std::vector<State>& StateHistoryForTesting() const {
    return state_history_;
  }

  // For testing only: allows tests to be notified of state changes.
  using StateChangeTestingCallback = base::RepeatingCallback<void(State)>;
  void SetStateChangeCallbackForTesting(StateChangeTestingCallback callback) {
    state_change_testing_callback_ = std::move(callback);
  }

 private:
  friend class ObservationDelayControllerTest;

  // web::WebStateObserver
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidStopLoading(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

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

  base::WeakPtr<web::WebState> web_state_;
  ActorTaskId task_id_;
  base::WeakPtr<AggregatedJournal> journal_;
  ReadyCallback ready_callback_;
  State state_ = State::kInitial;
  std::vector<State> state_history_ = {state_};
  Result result_ = Result::kOk;

  StateChangeTestingCallback state_change_testing_callback_;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<ObservationDelayController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_
