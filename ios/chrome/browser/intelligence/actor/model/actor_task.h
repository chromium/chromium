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

@protocol ActorTaskUIDelegate;

namespace web {
class WebState;
}

class ActorTool;

namespace actor {

class ActorEngine;

// A class representing a task managed by `ActorService`. A task should live for
// a whole Actor journey and be passed multiple sets of tools to execute
// sequentially.
class ActorTask {
 public:
  // Represents the high-level orchestration state of the task.
  enum class State {
    // Task is initialized but has not started executing tools.
    kInit = 0,
    // Task is actively executing through its engine.
    kActing = 1,
    // Task is waiting for AI provider to reflect on next actions to execute.
    kReflecting = 2,
    // Task execution was paused by the actor.
    kPausedByActor = 3,
    // Task execution was paused by the user.
    kPausedByUser = 4,
    // Task execution was cancelled or aborted.
    kCancelled = 5,
    // Task successfully completed.
    kFinished = 6,
    // Task is currently waiting for input or confirmation from the user.
    kWaitingOnUser = 7,
    // Task execution encountered a terminal failure.
    kFailed = 8
  };

  ActorTask(ActorTaskId task_id,
            const std::string& title,
            id<ActorTaskUIDelegate> delegate);
  ~ActorTask();

  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;

  // Accessors. TODO(crbug.com/496164697): Remove when they are used internally
  // in ActorTask, this is to fix compilation.
  ActorTaskId task_id() const { return task_id_; }
  const std::string& title() const { return title_; }
  id<ActorTaskUIDelegate> delegate() const { return delegate_; }

  // Returns the current execution state of the task.
  State GetState() const;

  // Begins executing the given sequence of tools on the underlying execution
  // engine with a string update blurb in plain language about what the actor is
  // doing.
  void ExecuteTools(std::vector<std::unique_ptr<ActorTool>> tools,
                    const std::string& task_update);

  // Stops the task and cancels any pending tools.
  void Stop(ActorTaskStoppedReason stop_reason);

  // Pauses execution (either initiated by the actor or the user). Subsequent
  // `ExecuteTools()` calls are invalid while paused.
  void Pause(bool from_actor);

  // Resumes task execution from a paused state.
  void Resume();

  // Returns whether this task's underlying engine is actively controlling
  // or observing the given WebState.
  bool IsControllingWebState(web::WebState* web_state) const;

 private:
  friend class ActorTaskTest;

  // The task state.
  State state_ = State::kInit;

  // The task's ID.
  const ActorTaskId task_id_;

  // The task's title.
  const std::string title_;

  // The task's UI delegate.
  __weak id<ActorTaskUIDelegate> delegate_;

  // The execution engine for this task.
  std::unique_ptr<ActorEngine> engine_;

  // Set of web states actively controlled (observed and/or being actuated on)
  // by this task.
  std::vector<base::WeakPtr<web::WebState>> controlled_web_states_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
