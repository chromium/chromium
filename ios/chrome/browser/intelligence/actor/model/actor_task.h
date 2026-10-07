// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_

#import <optional>
#import <string>
#import <string_view>
#import <vector>

#import "base/containers/flat_map.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/timer/timer.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom-forward.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_web_state_policy_decider.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer.h"

@class BackgroundContinuedProcessingTaskContext;
@class NSError;
@class PostedObserverList<ObserverType>;
@protocol ActorTaskInterventionDelegate;
@protocol ActorTaskUpdatesObserver;

class Browser;
class BrowserList;

namespace base {
class Value;
}  // namespace base

namespace web {
class WebState;
}

namespace actor {

class ActorToolFactory;
class ActorToolRequest;
class AggregatedJournal;
class ActorWebStatePolicyDecider;

// A class representing a task managed by `ActorService`. A task should live for
// a whole Actor journey and be passed multiple sets of actions to execute
// sequentially.
//
// Re-entrancy: code outside the actor model must never run task logic while a
// task method is on the stack. Therefore:
// - Outbound call-outs (observer notifications and `Act()` replies) are always
//   posted with snapshot arguments.
// - Intervention delegate requests are issued from a posted task, so no task
//   method caller is re-entered. The delegate itself is messaged synchronously
//   and may stop, and thereby destroy, the task, so the request must be the
//   last statement of its method.
// - Callbacks handed to code outside the actor model (e.g. intervention
//   delegate completions) are wrapped with `BindPostTaskToCurrentDefault`.
// Callbacks handed to the engine or timers stay synchronous.
class ActorTask : public web::WebStateObserver,
                  public ActorEngine::ExecutionUpdatesDelegate {
 public:
  ActorTask(ActorTaskId task_id,
            const std::string& title,
            const TaskSourceInfo& source_info,
            bool allow_incognito_web_states,
            AggregatedJournal* journal,
            ActorToolFactory* tool_factory,
            BrowserList* browser_list);
  ~ActorTask() override;

  ActorTask(const ActorTask&) = delete;
  ActorTask& operator=(const ActorTask&) = delete;

  // Adds an observer of task state transitions and tool executions. `observer`
  // first receives a posted `didRegisterAsObserverForTaskID:` with the task
  // state at call time, then exactly the notifications posted after this call.
  // No-op if `observer` is already added.
  void AddObserver(id<ActorTaskUpdatesObserver> observer);

  // Removes `observer` and drops its pending notifications, including the
  // registration snapshot.
  void RemoveObserver(id<ActorTaskUpdatesObserver> observer);

  const std::string& title() const { return title_; }

  // Returns information about the client that created this task.
  const TaskSourceInfo& source_info() const { return source_info_; }

  // Returns the unique identifier of the task.
  ActorTaskId task_id() const { return task_id_; }

  // Returns the execution engine associated with this task.
  ActorEngine& engine() const;

  // Returns the current execution state of the task.
  ActorTaskState GetState() const;

  // Begins executing the given sequence of actions on the underlying execution
  // engine with a string update blurb in plain language about what the actor is
  // doing. `callback` is posted, so a reply is delivered even if the task is
  // destroyed after replying:
  // - with the engine results once actions complete and pages finish loading;
  // - with a single `kExecutionEngineExistingAction` result if a previous
  //   `Act()` has not replied yet.
  void Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
           const std::string& task_update,
           ActCallback callback);

  // Returns whether an `Act()` is pending.
  bool HasPendingAct() const;

  // Adds a WebState to the set of controlled WebStates.
  void AddControlledWebState(web::WebState* web_state);

  // Stops the task and cancels any pending actions.
  virtual void Stop(ActorTaskStoppedReason stop_reason);

  // Pauses execution (either initiated by the actor or the user), cancelling
  // in-progress actions. Subsequent `Act()` calls are invalid while paused.
  void Pause(bool from_actor);

  // Resumes task execution from a paused state.
  void Resume();

  // Sets the intervention delegate for UI interaction.
  void SetInterventionDelegate(id<ActorTaskInterventionDelegate> delegate);

  // Interrupts task execution to wait for user input, suspending ongoing
  // actions without cancelling them. Accepts an optional message to display to
  // the user.
  void Interrupt(ActorTaskInterruptReason interrupt_reason,
                 std::string_view message = "");

  // Uninterrupts the task from waiting on user input, resuming execution into
  // the given `resumed_state`.
  void Uninterrupt(ActorTaskState resumed_state);

  // Returns whether this task's underlying engine is actively controlling
  // or observing the given WebState.
  bool IsControllingWebState(web::WebState* web_state) const;

  // Returns the journal used for logging.
  AggregatedJournal& GetJournal() const;

  // Returns the tool factory associated with this task.
  ActorToolFactory& GetToolFactory() const;

  // Returns whether the window identified by `window_id` exists.
  bool IsWindowIdValid(int32_t window_id);

  // Inserts a new WebState with `load_params` in the window identified by
  // `window_id`. Position of the new tab is determined by the task. Returns
  // the inserted WebState, or nullptr if the insertion failed.
  web::WebState* InsertWebState(
      int32_t window_id,
      const web::NavigationManager::WebLoadParams& load_params,
      bool in_background);

  // Returns the set of web states actively controlled by this task.
  const std::vector<base::WeakPtr<web::WebState>>& controlled_web_states()
      const;

  // Returns whether this task allows actuating on incognito WebStates.
  bool allow_incognito_web_states() const;

  // Sets the background continued processing task context and updates its
  // subtitle with the latest cached task update, if any. No-op if
  // `GeminiActorBackgrounding` is disabled.
  void SetBackgroundTaskContext(
      BackgroundContinuedProcessingTaskContext* background_task_context);

  // web::WebStateObserver overrides.
  void DidStopLoading(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class ActorTaskTest;

  // A pending `Act()` request.
  struct PendingAct {
    explicit PendingAct(ActCallback callback);
    PendingAct(PendingAct&&);
    PendingAct& operator=(PendingAct&&);
    ~PendingAct();

    // Callback of the `Act()` request.
    ActCallback callback;

    // Engine results held while the reply waits for controlled WebStates to
    // finish loading. Unset while the engine is still executing actions.
    std::optional<std::vector<ActionResult>> deferred_results;
  };

  // Sets the `ActorControlState` on all controlled `WebState`s based on
  // `control_state`.
  void SetControlStateOnWebStates(ActorControlState control_state);

  // Sets `SetKeepRenderProcessAlive` on all controlled `WebState`s.
  void SetKeepRenderProcessAliveOnControlledWebStates(bool keep_alive);

  // Sets the task state and logs the transition.
  void SetState(ActorTaskState new_state);

  // Called when the engine finishes executing the pending `Act()` actions.
  void OnActCompleted(std::vector<ActionResult> results);

  // Starts observing controlled WebStates that are loading. Returns true if any
  // observations are active, and false otherwise.
  bool ObserveLoadingWebStates();

  // Holds `results` in `pending_act_` until controlled WebStates finish loading
  // and registers the safety timeout timer.
  void DeferActCompletion(std::vector<ActionResult> results);

  // Transitions to `kReflecting` and replies to the pending `Act()` with
  // `results`.
  void FinishAct(std::vector<ActionResult> results);

  // Calls `FinishAct()` with the deferred results. `pending_act_` must hold
  // deferred results.
  void FinishDeferredAct();

  // Handles observation removal when a WebState finishes loading or is
  // destroyed. Also finishes the deferred `Act()` if no more WebStates are
  // loading.
  void OnWebStateFinishedLoading(web::WebState* web_state);

  // Handles the page load timeout.
  void OnPageLoadedTimeout();

  // ActorEngine::ExecutionUpdatesDelegate.
  void OnWillExecuteTool(ToolType tool_type,
                         web::WebStateID web_state_id) override;

  // Returns the Browser associated with the given `window_id`.
  Browser* GetBrowserForWindowId(int32_t window_id) const;

  // Prunes destroyed or null WebStates from `controlled_web_states_`. If
  // `destroying_web_state` is provided, also prunes the matching entry.
  void PruneDestroyedWebStates(web::WebState* destroying_web_state = nullptr);

  // Handles an implicit navigation on a controlled WebState being
  // blocked by origin gating policy.
  void OnNavigationBlocked(mojom::ActionResultCode code);

  // Validates the confirmation request synchronously, so that a rejected
  // request stops the task before `Interrupt()` returns, where `ActorService`
  // can still clean it up. Then posts `ShowInterruptConfirmation()`, so that
  // the delegate never runs while the caller of `Interrupt()` is on the stack.
  void RequestInterruptConfirmation(std::string_view message);

  // Shows the confirmation `message` via the intervention delegate. The task
  // state and the delegate are re-checked in the same posted task as the
  // delegate call, so the user is never prompted for a task that was stopped
  // or uninterrupted since the request was made.
  void ShowInterruptConfirmation(const std::string& message);

  // Handles the user resolving the confirmation.
  void OnInterruptConfirmationResolved();

  // Updates the subtitle of the background continued processing task to match
  // `task_update`. Does nothing if `task_update` is empty or identical to the
  // current value.
  void UpdateBackgroundTaskSubtitle(const std::string& task_update);

  // Advances the background task progress by one discrete step using the
  // context's stepped progress tracker.
  void UpdateBackgroundTaskProgress();

  // Finalizes the background task, reporting whether it succeeded.
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

  // The task state.
  ActorTaskState state_ = ActorTaskState::kInit;

  // The task's ID.
  const ActorTaskId task_id_;

  // The BrowserList associated with this task. ActorTask is owned by
  // ActorService, which depends on BrowserList (as declared in
  // ActorServiceFactory). Consequently, ActorService and this task are
  // destroyed before the BrowserList during profile shutdown.
  raw_ptr<BrowserList> browser_list_ = nullptr;

  // The task's title.
  const std::string title_;

  // Information about the client that created this task.
  const TaskSourceInfo source_info_;

  const bool allow_incognito_web_states_;

  // The execution engine for this task.
  std::unique_ptr<ActorEngine> engine_;

  // The aggregated journal for logging. Owned by the ActorService, which is
  // guaranteed to outlive this ActorTask.
  raw_ptr<AggregatedJournal> journal_;

  // The tool factory used for creating tools under this task. Owned by the
  // ActorService, which is guaranteed to outlive this ActorTask.
  raw_ptr<ActorToolFactory> tool_factory_;

  // Set of web states actively controlled (observed and/or being actuated on)
  // by this task.
  std::vector<base::WeakPtr<web::WebState>> controlled_web_states_;

  // Navigation policy deciders attached to each controlled WebState to gate
  // implicit navigations.
  base::flat_map<web::WebStateID, std::unique_ptr<ActorWebStatePolicyDecider>>
      policy_deciders_;

  // Scoped observation to safely observe events of controlled WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      scoped_web_state_observations_{this};

  // The pending `Act()` request, if any.
  std::optional<PendingAct> pending_act_;

  // Timer to enforce the page load timeout. The timeout exists to limit the
  // amount of time ActorTask can wait for a page to finish loading before
  // executing the Act callback.
  base::OneShotTimer load_timeout_timer_;

  // The latest non-empty task update string.
  std::string last_task_update_;

  // The delegate handling user intervention UI. Weak reference.
  __weak id<ActorTaskInterventionDelegate> intervention_delegate_ = nil;

  // Observers notified of task state changes and tool executions. Posted, so
  // observers cannot re-enter the task mid-transition, and `Act()` callers get
  // their reply before observers see the resulting state change.
  __strong PostedObserverList<id<ActorTaskUpdatesObserver>>* observers_ = nil;

  // Active context for background continued processing, if requested.
  __strong BackgroundContinuedProcessingTaskContext* background_task_context_ =
      nil;

  // Repeating timer for sending JavaScript heartbeat pings.
  base::RepeatingTimer heartbeat_timer_;

  // Weak pointer factory.
  base::WeakPtrFactory<ActorTask> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
