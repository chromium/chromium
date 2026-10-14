// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_BACKGROUND_WORKER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_BACKGROUND_WORKER_H_

#import <Foundation/Foundation.h>

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/intelligence/actor/metrics/actor_background_task_metrics.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/web_state_observer.h"

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
// WebContent processes alive when they are hidden. Owned by the task, which
// forwards its events.
class ActorTaskBackgroundWorker : public web::WebStateObserver {
 public:
  // Delegate interface through which the worker reads the owning
  // `ActorTask`'s current state: its lifecycle state, title and latest task
  // update.
  class TaskStateDelegate {
   public:
    virtual ~TaskStateDelegate() = default;

    // Returns the task's current state.
    virtual ActorTaskState GetTaskState() const = 0;

    // Returns the task's title.
    virtual const std::string& GetTaskTitle() const = 0;

    // Returns the latest task update.
    virtual const std::string& GetLastTaskUpdate() const = 0;
  };

  // `delegate` and `journal` must outlive this object.
  ActorTaskBackgroundWorker(TaskStateDelegate* delegate,
                            ActorTaskId task_id,
                            AggregatedJournal& journal);
  ~ActorTaskBackgroundWorker() override;

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
  // now (backgrounding is enabled, no live context is held and the task's
  // current state should keep a background task alive) and records the request
  // eligibility and any ineligibility reasons.
  bool ShouldRequestBackgroundTask() const;

  // Sets the background task context and applies the latest task update as its
  // subtitle. No-op if `GeminiActorBackgrounding` is disabled.
  void SetContext(BackgroundContinuedProcessingTaskContext* context);

  // Called when `web_state` is added to the task's controlled WebStates. The
  // worker observes it until it is destroyed.
  void OnWebStateAdded(web::WebState* web_state);

  // Sets whether every scene of the profile is foreground active.
  void SetAllScenesForegroundActive(bool all_scenes_foreground_active);

  // Called when the task's state changes to `new_state`. Re-evaluates the
  // heartbeat; acting refreshes the subtitle; non-terminal wait or pause states
  // complete the background task with success.
  void OnStateChanged(ActorTaskState new_state);

  // Called when the task is about to execute a tool.
  void OnWillExecuteTool();

  // Called when the task is stopped, reporting whether it succeeded.
  void OnStopped(bool success);

  // web::WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class ActorTaskBackgroundWorkerTest;

  // Returns whether a background task should be requested for the owning task
  // now, recording any applicable ineligibility reasons.
  bool GetBackgroundTaskRequestEligibility() const;

  // Updates the context subtitle to `task_update`. No-op if `task_update` is
  // empty or identical to the current value.
  void UpdateBackgroundTaskSubtitle(const std::string& task_update);

  // Advances the context progress by one discrete step.
  void IncrementBackgroundTaskProgress();

  // Finalizes the background task for `reason` and records the end reason &
  // duration. When waiting on the user (e.g., paused or yielded), sets the
  // title describing the wait, keeps the task's own title as subtitle, and
  // leaves the progress untouched to indicate that the task is not done.
  void FinalizeBackgroundTask(ActorBackgroundTaskEndReason reason);

  // Returns whether the WebContent process keep-alive heartbeat should run.
  bool ShouldRunHeartbeat();

  // Starts the JavaScript heartbeat ping timer if the heartbeat should run,
  // and stops it otherwise.
  void UpdateHeartbeatTimer();

  // Sends a lightweight JavaScript ping to all controlled WebStates to keep
  // their out-of-process WebContent processes alive. Stops the timer once the
  // heartbeat should no longer run.
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

  // Whether all scenes of the profile are currently active in the foreground.
  bool all_scenes_foreground_active_ = true;

  // Active context for background continued processing, if requested.
  __strong BackgroundContinuedProcessingTaskContext* background_task_context_ =
      nil;

  // When `background_task_context_` was received.
  base::TimeTicks background_task_started_at_;

  // Repeating timer for sending JavaScript heartbeat pings.
  base::RepeatingTimer heartbeat_timer_;

  // Observations of the controlled WebStates, for visibility changes.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  base::WeakPtrFactory<ActorTaskBackgroundWorker> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_BACKGROUND_WORKER_H_
