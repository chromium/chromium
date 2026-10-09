// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_BACKGROUND_WORKER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_BACKGROUND_WORKER_H_

#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class BackgroundContinuedProcessingTaskContext;
@class NSError;

namespace base {
class Value;
}  // namespace base

namespace web {
class WebState;
class WebStateID;
}  // namespace web

namespace actor {

class AggregatedJournal;

// Owns the background work of an `ActorTask`: the background continued
// processing task context and the JavaScript heartbeat that keeps controlled
// WebContent processes alive. Owned by the task, which forwards its events.
class ActorTaskBackgroundWorker {
 public:
  // Delegate interface through which the worker reads the owning
  // `ActorTask`'s current state: its lifecycle state, controlled WebStates and
  // latest task update.
  class TaskStateDelegate {
   public:
    virtual ~TaskStateDelegate() = default;

    // Returns the live controlled WebStates.
    virtual std::vector<web::WebState*> GetControlledWebStates() = 0;

    // Returns the task's current state.
    virtual ActorTaskState GetTaskState() const = 0;

    // Returns the task's title, used as the background task title.
    virtual const std::string& GetTaskTitle() const = 0;

    // Returns the latest task update, used as the background task subtitle.
    virtual const std::string& GetLastTaskUpdate() const = 0;
  };

  // `delegate` and `journal` must outlive this object.
  ActorTaskBackgroundWorker(TaskStateDelegate* delegate,
                            ActorTaskId task_id,
                            AggregatedJournal& journal);
  ~ActorTaskBackgroundWorker();

  ActorTaskBackgroundWorker(const ActorTaskBackgroundWorker&) = delete;
  ActorTaskBackgroundWorker& operator=(const ActorTaskBackgroundWorker&) =
      delete;

  // Returns the owning task's ID.
  ActorTaskId task_id() const { return task_id_; }

  // Returns the owning task's title.
  const std::string& title() const;

  // Returns a weak pointer to this worker.
  base::WeakPtr<ActorTaskBackgroundWorker> GetWeakPtr();

  // Returns whether a background task should be requested for the owning task
  // now: backgrounding is enabled, no live context is held and the task's
  // current state should keep a background task alive.
  bool ShouldRequestBackgroundTask() const;

  // Sets the background task context and applies the latest task update as its
  // subtitle. No-op if `GeminiActorBackgrounding` is disabled.
  void SetContext(BackgroundContinuedProcessingTaskContext* context);

  // Called when a WebState is added to the task's controlled WebStates.
  void OnWebStateAdded();

  // Called when a controlled WebState is destroyed.
  void OnWebStateDestroyed();

  // Called when the task's state changes to `new_state`. Acting refreshes the
  // subtitle and starts the heartbeat; terminal states stop it; non-terminal
  // wait or pause states complete the background task with success.
  void OnStateChanged(ActorTaskState new_state);

  // Called when the task is about to execute a tool.
  void OnWillExecuteTool();

  // Called when the task is stopped, reporting whether it succeeded.
  void OnStopped(bool success);

 private:
  friend class ActorTaskBackgroundWorkerTest;

  // Updates the context subtitle to `task_update`. No-op if `task_update` is
  // empty or identical to the current value.
  void UpdateBackgroundTaskSubtitle(const std::string& task_update);

  // Advances the context progress by one discrete step.
  void IncrementBackgroundTaskProgress();

  // Ends the background task: marks the context completed with `success`
  // (no-op if it already completed or expired) and drops the context reference.
  void FinalizeBackgroundTask(bool success);

  // Starts the JavaScript heartbeat ping timer if backgrounding is enabled and
  // the timer is not already running.
  // TODO(crbug.com/561253684): Ensure we only start the heartbeat timer when
  // the app is backgrounded.
  void StartHeartbeatTimer();

  // Stops the JavaScript heartbeat ping timer.
  void StopHeartbeatTimer();

  // Sends a lightweight JavaScript ping to all controlled WebStates to keep
  // their out-of-process WebContent processes alive.
  void SendHeartbeatPing();

  // Handles completion or failure of a JavaScript heartbeat ping for
  // `web_state_id`. Failed pings are logged to the journal.
  void OnHeartbeatPingResponse(web::WebStateID web_state_id,
                               const base::Value* result,
                               NSError* error);

  // Reads the owning task's state.
  const raw_ptr<TaskStateDelegate> delegate_;

  // The owning task's ID, used for journal logging.
  const ActorTaskId task_id_;

  // The journal used for logging heartbeat failures.
  const raw_ref<AggregatedJournal> journal_;

  // Active context for background continued processing, if requested.
  __strong BackgroundContinuedProcessingTaskContext* background_task_context_ =
      nil;

  // Repeating timer for sending JavaScript heartbeat pings.
  base::RepeatingTimer heartbeat_timer_;

  base::WeakPtrFactory<ActorTaskBackgroundWorker> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_BACKGROUND_WORKER_H_
