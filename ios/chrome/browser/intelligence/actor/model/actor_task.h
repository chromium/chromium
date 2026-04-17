// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_

#import <set>
#import <string>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

namespace web {
class WebState;
}

namespace actor {

class ActorEngine;
class ActorTool;

// A class representing a task managed by `ActorService`. A task should live for
// a whole Actor journey and be passed multiple sets of actions to execute
// sequentially.
class ActorTask {
 public:
  ActorTask(ActorTaskId task_id, const std::string& title);
  ~ActorTask();

  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;

  // Accessors. TODO(crbug.com/496164697): Remove when they are used internally
  // in ActorTask, this is to fix compilation.
  ActorTaskId task_id() const { return task_id_; }
  const std::string& title() const { return title_; }

  // Returns the current execution state of the task.
  ActorTaskState GetState() const;

  // Begins executing the given sequence of actions on the underlying execution
  // engine with a string update blurb in plain language about what the actor is
  // doing.
  void Act(std::vector<std::unique_ptr<ActorTool>> actions,
           const std::string& task_update,
           PerformActionsCallback callback);

  // Stops the task and cancels any pending actions.
  void Stop(ActorTaskStoppedReason stop_reason);

  // Pauses execution (either initiated by the actor or the user). Subsequent
  // `Act()` calls are invalid while paused.
  void Pause(bool from_actor);

  // Resumes task execution from a paused state.
  void Resume();

  // Returns whether this task's underlying engine is actively controlling
  // or observing the given WebState.
  bool IsControllingWebState(web::WebState* web_state) const;

 private:
  friend class ActorTaskTest;

  // Called when tools execution is completed.
  void OnActCompleted(PerformActionsCallback callback,
                      std::vector<ActionResult> results);

  // The task state.
  ActorTaskState state_ = ActorTaskState::kInit;

  // The task's ID.
  const ActorTaskId task_id_;

  // The task's title.
  const std::string title_;

  // The execution engine for this task.
  std::unique_ptr<ActorEngine> engine_;

  // Set of web states actively controlled (observed and/or being actuated on)
  // by this task.
  std::vector<base::WeakPtr<web::WebState>> controlled_web_states_;

  // Weak pointer factory.
  base::WeakPtrFactory<ActorTask> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
