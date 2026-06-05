// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_

#import <set>
#import <string>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/web_state_observer.h"

@class CRBProtocolObservers;

namespace web {
class WebState;
}

namespace actor {

class ActorToolFactory;
class ActorToolRequest;
class AggregatedJournal;

// A class representing a task managed by `ActorService`. A task should live for
// a whole Actor journey and be passed multiple sets of actions to execute
// sequentially.
class ActorTask : public web::WebStateObserver,
                  public ActorEngine::ExecutionUpdatesDelegate {
 public:
  ActorTask(ActorTaskId task_id,
            const std::string& title,
            bool allow_incognito_web_states,
            AggregatedJournal* journal,
            ActorToolFactory* tool_factory);
  ~ActorTask() override;

  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;

  // Adds an observer to be notified of task state transitions and tool
  // executions. Registered observers will immediately receive a notification
  // with the current state via `didRegisterAsObserverForTaskID:`.
  void AddObserver(id<ActorTaskUpdatesObserver> observer);

  // Removes a registered observer.
  void RemoveObserver(id<ActorTaskUpdatesObserver> observer);

  // Accessors. TODO(crbug.com/496164697): Remove when they are used internally
  // in ActorTask, this is to fix compilation.
  const std::string& title() const { return title_; }

  // Returns the current execution state of the task.
  ActorTaskState GetState() const;

  // Begins executing the given sequence of actions on the underlying execution
  // engine with a string update blurb in plain language about what the actor is
  // doing.
  void Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
           const std::string& task_update,
           ActCallback callback);

  // Adds a WebState to the set of controlled WebStates.
  void AddControlledWebState(web::WebState* web_state);

  // Stops the task and cancels any pending actions.
  virtual void Stop(ActorTaskStoppedReason stop_reason);

  // Pauses execution (either initiated by the actor or the user). Subsequent
  // `Act()` calls are invalid while paused.
  void Pause(bool from_actor);

  // Resumes task execution from a paused state.
  void Resume();

  // Returns whether this task's underlying engine is actively controlling
  // or observing the given WebState.
  bool IsControllingWebState(web::WebState* web_state) const;

  // Returns the set of web states actively controlled by this task.
  const std::vector<base::WeakPtr<web::WebState>>& controlled_web_states()
      const;

  // Returns whether this task allows actuating on incognito WebStates.
  bool allow_incognito_web_states() const;

  // web::WebStateObserver overrides.
  void DidStopLoading(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class ActorTaskTest;

  // Sets the task state and logs the transition.
  void SetState(ActorTaskState new_state);

  // Called when tools execution is completed.
  void OnActCompleted(ActCallback callback, std::vector<ActionResult> results);

  // Starts observing controlled WebStates that are loading. Returns true if any
  // observations are active, and false otherwise.
  bool ObserveLoadingWebStates();

  // Defers the `Act()` completion callback and registers the safety timeout
  // timer.
  void DeferActCompletion(ActCallback callback,
                          std::vector<ActionResult> results);

  // Handles observation removal when a WebState finishes loading or is
  // destroyed. Also resolves the deferred callback if no more WebStates are
  // loading.
  void OnWebStateFinishedLoading(web::WebState* web_state);

  // Handles the page load timeout.
  void OnPageLoadedTimeout();

  // ActorEngine::ExecutionUpdatesDelegate.
  void OnWillExecuteTool(ToolType tool_type,
                         web::WebStateID web_state_id) override;

  // The task state.
  ActorTaskState state_ = ActorTaskState::kInit;

  // The task's ID.
  const ActorTaskId task_id_;

  // The task's title.
  const std::string title_;

  const bool allow_incognito_web_states_;

  // The execution engine for this task.
  std::unique_ptr<ActorEngine> engine_;

  // The aggregated journal for logging. Owned by the ActorService, which is
  // guaranteed to outlive this ActorTask.
  raw_ptr<AggregatedJournal> journal_;

  // Set of web states actively controlled (observed and/or being actuated on)
  // by this task.
  std::vector<base::WeakPtr<web::WebState>> controlled_web_states_;

  // Scoped observation to safely observe events of controlled WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      scoped_web_state_observations_{this};

  // Deferred Act callback. The `Act()` callback can be deferred if any of the
  // controlled WebStates are loading when Act is done executing actions.
  base::OnceClosure deferred_act_callback_;

  // Timer to enforce the page load timeout. The timeout exists to limit the
  // amount of time ActorTask can wait for a page to finish loading before
  // executing the Act callback.
  base::OneShotTimer load_timeout_timer_;

  // The latest task update string.
  std::string last_task_update_;

  // List of registered observers notified of task state changes and tool
  // executions. `CRBProtocolObservers` itself is held strongly, but the
  // observers inside are held weakly.
  __strong CRBProtocolObservers<ActorTaskUpdatesObserver>* observers_;

  // Weak pointer factory.
  base::WeakPtrFactory<ActorTask> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
