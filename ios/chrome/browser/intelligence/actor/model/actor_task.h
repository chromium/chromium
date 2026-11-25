// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/containers/flat_map.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/timer/timer.h"
#import "base/types/pass_key.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom-forward.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_web_state_policy_decider.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer.h"

@class PostedObserverList<ObserverType>;
@protocol ActorTaskInterventionDelegate;
@protocol ActorTaskUpdatesObserver;

class Browser;
class BrowserList;

namespace web {
class WebState;
}

namespace actor {

class ActorEnterprisePolicyCheckerIOS;
class ActorToolFactory;
class ActorToolRequest;
class AggregatedJournal;
class ProfileContextResolver;

// A class representing a task managed by `ActorService`. A task should live for
// a whole Actor journey and be passed multiple sets of actions to execute
// sequentially.
//
// Re-entrancy: code outside the actor model must never run task logic while a
// task method is on the stack. Therefore:
// - Outbound call-outs (observer notifications and `Act()` replies) are always
//   posted with snapshot arguments.
// - Intervention delegate and navigation confirmation requests are issued
// synchronously. External callers never re-enters the task, as they only reach
// it through `ActorService` enqueued commands.
// - Callbacks handed to code outside the actor model (e.g. intervention
//   delegate and navigation confirmation completions) are wrapped with
//  `BindPostTaskToCurrentDefault`.
// Callbacks handed to the engine or timers stay synchronous.
class ActorTask : public web::WebStateObserver,
                  public ActorEngine::ExecutionUpdatesDelegate,
                  public ActorTaskBackgroundWorker::TaskStateDelegate {
 public:
  // `on_stopped` runs synchronously when the task stops; the owner must not
  // destroy the task from it.
  ActorTask(ActorTaskId task_id,
            const std::string& title,
            const TaskSourceInfo& source_info,
            bool allow_incognito_web_states,
            AggregatedJournal* journal,
            ActorToolFactory* tool_factory,
            const ProfileContextResolver* profile_context_resolver,
            BrowserList* browser_list,
            const ActorEnterprisePolicyCheckerIOS* policy_checker,
            base::OnceClosure on_stopped);
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

  // Returns the enterprise policy checker associated with this task
  const ActorEnterprisePolicyCheckerIOS* policy_checker() const {
    return policy_checker_;
  }

  // Returns the execution engine associated with this task.
  ActorEngine& engine() const;

  // Returns the current execution state of the task.
  ActorTaskState GetState() const;

  // Returns whether the task is in a terminal state.
  bool IsCompleted() const;

  // Begins executing the given sequence of actions on the underlying execution
  // engine with a string update blurb in plain language about what the actor is
  // doing. `callback` is posted, so a reply is delivered even if the task is
  // destroyed after replying:
  // - with the engine results once actions complete and pages finish loading;
  // - with a single `kTaskWentAway` result if the task stops;
  // - with a single `kExecutionEngineExistingAction` result if an operation
  //   (`Act()` or `Interrupt()`) is pending;
  // - with a single `kTaskPaused` result, without executing the actions, if
  //   the user paused the task since the last reply. While the task is paused,
  //   the reply waits until the user resumes it.
  void Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
           const std::string& task_update,
           ActCallback callback);

  // Returns whether an `Act()` or `Interrupt()` is pending.
  bool HasPendingOperation() const;

  // Adds a WebState to the set of controlled WebStates.
  void AddControlledWebState(web::WebState* web_state);

  // Stops the task, cancels any pending actions, and runs `on_stopped`. No-op
  // if the task is already stopped.
  virtual void Stop(ActorTaskStoppedReason stop_reason);

  // Pauses the task for the user, cancelling in-progress actions. The pending
  // operation's reply, if any, waits until `Resume()`. No-op unless the task is
  // in `kInit`, `kActing`, `kReflecting` or `kWaitingOnUser`.
  void Pause();

  // Resumes the task paused by `Pause()`: answers the held operation, if any,
  // with `kTaskPaused` or `kUserPaused` and transitions to `kReflecting`.
  // No-op unless the task is in `kPausedByUser`.
  void Resume();

  // Sets the intervention delegate for UI interaction.
  void SetInterventionDelegate(id<ActorTaskInterventionDelegate> delegate);

  // Interrupts the task as described by `request` and prompts the user.
  // `on_response` is posted at most once with:
  // - the user's answer once the task has resumed (`kUserPaused`, without
  //   prompting, if the user paused the task since the last reply; waits until
  //   resumed while paused),
  // - `kTaskRejected` if the task cannot take the interrupt,
  // - `kTaskWentAway` if it stops first, or
  // - the intervention delegate's error once the task has resumed.
  void Interrupt(const InterruptRequest& request,
                 InterventionResultCallback<InterruptReply> on_response);

  // Interrupts the task while a tool waits on the user, without prompting.
  void InterruptForTool(base::PassKey<ActorEngine> pass_key);

  // Resumes acting once the tool no longer waits on the user.
  void UninterruptForTool(base::PassKey<ActorEngine> pass_key);

  // Returns whether this task's underlying engine is actively controlling
  // or observing the given WebState.
  bool IsControllingWebState(web::WebState* web_state) const;

  // Returns the journal used for logging.
  AggregatedJournal& GetJournal() const;

  // Returns the tool factory associated with this task.
  ActorToolFactory& GetToolFactory() const;

  // Returns the profile context resolver associated with this task.
  const ProfileContextResolver& profile_context_resolver() const;

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

  // Returns the worker of this task's background work. Owned by this task
  // and valid for its lifetime.
  ActorTaskBackgroundWorker* background_worker() const {
    return background_worker_.get();
  }

  // Sets the callback used to query the Origin Critic model for out-of-turn
  // cross-origin navigations.
  void SetNavigationConfirmationCallback(
      NavigationConfirmationCallback callback);

  // Requests confirmation from the Origin Critic model for an out-of-turn
  // navigation to `navigation_origin`.
  void ConfirmNavigationToOrigin(
      const url::Origin& navigation_origin,
      NavigationConfirmationDecisionCallback callback);

  // Prompts the user via `intervention_delegate_` to confirm navigating to
  // `navigation_origin`. Transitions the task from `kActing` to
  // `kWaitingOnUser` while waiting for the user's response.
  void RequestUserConfirmationForNavigation(
      const url::Origin& navigation_origin,
      UserConfirmationDialogDecisionCallback callback);

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

  // Sets the task state and logs the transition. Must not be called once the
  // task is completed.
  void SetState(ActorTaskState new_state);

  // Cancels the pending operation, like `CancelOngoingOperation()`, and answers
  // it with `kTaskWentAway`.
  void CancelOngoingOperationAndReply();

  // Cancels the engine work, page load wait and intervention prompt of the
  // pending operation. Its reply stays pending until `Resume()` or
  // `CancelOngoingOperationAndReply()` answers it.
  void CancelOngoingOperation();

  // Called when the engine finishes executing the pending `Act()` actions.
  void OnActCompleted(std::vector<ActionResult> results);

  // Starts observing controlled WebStates that are loading. Returns true if any
  // observations are active, and false otherwise.
  bool ObserveLoadingWebStates();

  // Holds `results` in `pending_act_` until controlled WebStates finish loading
  // and registers the safety timeout timer.
  void DeferActCompletion(std::vector<ActionResult> results);

  // Starts the safety timeout timer for a deferred `Act()` waiting on page
  // loads.
  void StartPageLoadTimeout();

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

  // ActorTaskBackgroundWorker::TaskStateDelegate.
  ActorTaskState GetTaskState() const override;
  const std::string& GetTaskTitle() const override;
  const std::string& GetLastTaskUpdate() const override;

  // Returns the Browser associated with the given `window_id`.
  Browser* GetBrowserForWindowId(int32_t window_id) const;

  // Prunes destroyed or null WebStates from `controlled_web_states_`, including
  // the matching `destroying_web_state`.
  void PruneDestroyedWebStates(web::WebState* destroying_web_state);

  // Handles an implicit navigation on a controlled WebState being
  // blocked by origin gating policy.
  void OnNavigationBlocked(mojom::ActionResultCode code);

  // Returns whether the task is in a state that can be interrupted.
  bool CanBeInterrupted() const;

  // Suspends ongoing actions and any deferred page-load timer, and transitions
  // to `kWaitingOnUser`.
  void EnterWaitingOnUser();

  // Transitions from `kWaitingOnUser` to `resumed_state` and resumes the
  // engine (and any deferred page-load wait when returning to `kActing`).
  // No-op if the task is not waiting on the user.
  void ExitWaitingOnUser(ActorTaskState resumed_state);

  // Handles `Interrupt()` for a `ConfirmationInterrupt`.
  void InterruptFor(const ConfirmationInterrupt& confirmation,
                    InterventionResultCallback<InterruptReply> on_response);

  // Shows the `confirmation` prompt via the intervention delegate.
  void ShowInterruptConfirmation(const ConfirmationInterrupt& confirmation);

  // Handles the user confirming, or the delegate failing with an error.
  void OnInterruptConfirmationResolved(InterventionDelegateResult<void> result);

  // Handles `Interrupt()` for a `ClarificationInterrupt`.
  void InterruptFor(const ClarificationInterrupt& clarification,
                    InterventionResultCallback<InterruptReply> on_response);

  // Asks the `clarification` question via the intervention delegate.
  void ShowInterruptClarification(const ClarificationInterrupt& clarification);

  // Handles the user's answer, or the delegate failing with an error.
  void OnInterruptClarificationResolved(
      InterventionDelegateResult<NSString*> result);

  // Replies to the pending interrupt with `result` and resumes the task.
  void ResolveInterrupt(InterventionResult<InterruptReply> result);

  // Returns whether the task is in a state that can be paused.
  bool CanBePaused() const;

  // Handles the user's decision on a navigation confirmation prompt, or the
  // delegate failing with an error.
  void OnNavigationUserConfirmationResolved(
      InterventionDelegateResult<bool> result);

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

  // Resolves profile-scoped services and browser state. Owned by the
  // `ActorService`, which is guaranteed to outlive this `ActorTask`.
  raw_ptr<const ProfileContextResolver> profile_context_resolver_;

  // Enterprise policy checker owned by `ActorService`.
  raw_ptr<const ActorEnterprisePolicyCheckerIOS> policy_checker_;

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

  // The pending `Act()` request, if any. At most one of `pending_act_` and
  // `pending_interrupt_` is set, as the caller awaits a single reply however
  // many actions it runs.
  std::optional<PendingAct> pending_act_;

  // Reply of the pending `Interrupt()`, if any.
  InterventionResultCallback<InterruptReply> pending_interrupt_;

  // Whether the user paused the task since the last reply. The next reply then
  // tells the caller the task was paused, as the pages may have changed.
  bool owes_pause_notice_ = false;

  // Run once by `Stop()`.
  base::OnceClosure on_stopped_;

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

  // Handles the background continued processing task and the heartbeat.
  std::unique_ptr<ActorTaskBackgroundWorker> background_worker_;

  // Callback for confirming out-of-turn cross-origin navigations via the
  // Origin Critic model.
  NavigationConfirmationCallback navigation_confirmation_callback_;

  // Held while waiting on the user to confirm a cross-origin navigation.
  UserConfirmationDialogDecisionCallback
      pending_navigation_confirmation_callback_;

  // Weak pointer factory for callbacks scoped to the current operation (action
  // completion, page load, interrupt confirmation). Invalidated when the
  // operation is cancelled by a pause or a stop, so late callbacks of a
  // cancelled operation are dropped.
  base::WeakPtrFactory<ActorTask> operation_weak_ptr_factory_{this};

  // Weak pointer factory for callbacks registered once for the task's lifetime
  // (e.g. navigation gating per controlled WebState). Separate from
  // `operation_weak_ptr_factory_` because they are not re-registered on resume,
  // so they must survive a pause. Invalidated only when the task stops.
  base::WeakPtrFactory<ActorTask> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TASK_H_
