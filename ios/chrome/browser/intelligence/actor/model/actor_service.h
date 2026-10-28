// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_

#import <map>
#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <vector>

#import "base/callback_list.h"
#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/web_state_id.h"

@class PageContextWrapper;
@class PostedObserverList<ObserverType>;
@protocol ActorTaskInterventionDelegate;
@protocol ActorTaskLifecycleObserver;
@protocol ActorTaskUpdatesObserver;
class ProfileIOS;

namespace web {
class WebState;
}

namespace actor {

class ActorEnterprisePolicyCheckerIOS;
class ActorTask;
class ActorTaskBackgroundWorker;
class ActorToolRequest;
class ActorToolFactory;
class AggregatedJournal;
struct TaskSourceInfo;

// Service responsible for handling Actor requests. The normal flow is to
// `CreateTask` and reuse this ID for an entire Actor task (journey).
// `PerformActions` can be called multiple times on the same task sequentially.
// Must be created and used on a single sequence. Observers passed to
// `Add*Observer()` must not be nil, `Remove*Observer()` ignores nil.
//
// Ordering model. Callers observe a single timeline, and may call any method
// from anywhere on the service's sequence, including from observer and
// delegate callbacks:
// - Commands (`PerformActions`, `RequestTabObservation`, `InterruptTask`,
//   `StopTask`, `StopAllTasks`) are enqueued and return immediately. They run
//   in call order, each as its own task, so task logic never runs on the
//   caller's stack.
// - Task setup (`CreateTask`, `AddControlledWebState`) applies immediately, so
//   that the task is usable as soon as the call returns.
// - Replies and notifications are posted, in the order of the effects that
//   produced them. Replies run at most once, and pending commands, replies,
//   and notifications are dropped on `Shutdown()`. Callers holding state keyed
//   on a pending reply must release it in their own teardown, which always
//   precedes `Shutdown()`.
// - Registrations (observers and the intervention delegate) apply immediately:
//   an observer receives a notification if and only if it was posted after
//   the observer is added and is delivered before it is removed.
// - Getters return the state applied so far: commands still queued are not
//   reflected yet, and notifications not delivered yet may be behind. Prefer
//   replies and notifications over polling getters.
// - A stopped task leaves the active set immediately and is destroyed from a
//   posted task, since it may still be on the call stack.
class ActorService : public KeyedService {
 public:
  explicit ActorService(ProfileIOS* profile);
  ~ActorService() override;

  // KeyedService:
  void Shutdown() override;

  // Creates a new task. `source_info` identifies the client creating the task.
  ActorTaskId CreateTask(const std::string& title,
                         const TaskSourceInfo& source_info,
                         bool allow_incognito_web_states);

  // Enqueues actions for an active task with a task update string (a short
  // blurb which tells the user what the Actor is currently doing in plain
  // language).
  //
  // Actions are validated and executed sequentially. If any action fails
  // validation or execution, the sequence is aborted immediately, and
  // subsequent actions will not be run.
  //
  // `callback` runs exactly once, asynchronously, unless the service is shut
  // down first. If the task is not active or stops before the reply runs, it
  // runs with a single `kTaskWentAway` result. If the task is still executing a
  // previous request, it runs with a single `kExecutionEngineExistingAction`
  // result and the request's target WebStates are not added to the task.
  void PerformActions(ActorTaskId task_id,
                      std::vector<optimization_guide::proto::Action> actions,
                      std::string task_update,
                      PerformActionsCallback callback);

  // Enqueues a "tab observation" request (nomenclature aligned with
  // `chrome/browser/actor`). Tab is equivalent to WebState and "observation" is
  // equivalent to a PageContext extraction. Not an "observing" pattern. In
  // practice, this is equivalent to requesting a rich actionable mode
  // extraction on `PageContextWrapper`, with a completion callback. `callback`
  // runs at most once, always asynchronously, and never after `Shutdown()`. If
  // the task is not active or `web_state` is gone when the command runs, the
  // callback runs with `PageContextWrapperError::kGenericError`.
  // TODO(crbug.com/568338837): Do not add new callers; move tab observation
  // internal to `ActorTask`.
  void RequestTabObservation(ActorTaskId task_id,
                             web::WebState* web_state,
                             TabObservationCallback callback);

  // Pauses a task, cancelling in-progress actions and returning control of the
  // WebState to the user.
  void PauseTask(ActorTaskId task_id, bool from_actor);

  // Sets the intervention delegate for UI interaction on the given task.
  void SetTaskInterventionDelegate(ActorTaskId task_id,
                                   id<ActorTaskInterventionDelegate> delegate);

  // Enqueues an interrupt of a task to wait for user input, suspending ongoing
  // actions without cancelling them. Accepts an optional message to display to
  // the user.
  void InterruptTask(ActorTaskId task_id,
                     ActorTaskInterruptReason reason,
                     std::string_view message = "");

  // Enqueues a stop of a task. The task remains active, e.g. for getters,
  // until the command runs. A stop still queued at `Shutdown()` never runs: the
  // task is then destroyed without `ActorTask::Stop()` nor a stop notification.
  void StopTask(ActorTaskId task_id, ActorTaskStoppedReason reason);

  // TODO(crbug.com/517583120): Remove when the temporary actuation prototype is
  // cleaned up.
  // Enqueues a stop of the tasks that are active when this is called. Tasks
  // created afterwards are not stopped.
  void StopAllTasks();

  // Returns the aggregated journal for this service.
  AggregatedJournal* GetJournal() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return journal_.get();
  }

  // Registers `observer` for updates of all tasks, including future ones.
  void AddTaskUpdatesObserver(id<ActorTaskUpdatesObserver> observer);

  // Removes a registered observer for task updates.
  void RemoveTaskUpdatesObserver(id<ActorTaskUpdatesObserver> observer);

  // Registers `observer` for updates of the task identified by `task_id` only.
  // Returns false if there is no active task with `task_id`. A stop enqueued
  // before this call is still delivered to `observer`.
  bool AddTaskUpdatesObserver(ActorTaskId task_id,
                              id<ActorTaskUpdatesObserver> observer);

  // Removes `observer` from the task identified by `task_id`.
  void RemoveTaskUpdatesObserver(ActorTaskId task_id,
                                 id<ActorTaskUpdatesObserver> observer);

  // Registers the `observer` for task lifecycle events.
  // Events are posted, never delivered synchronously, and `observer` receives
  // those posted after this call until it is removed or the service shuts down.
  void AddTaskLifecycleObserver(id<ActorTaskLifecycleObserver> observer);

  // Removes a registered task lifecycle observer. Pending events are not
  // delivered to `observer`.
  void RemoveTaskLifecycleObserver(id<ActorTaskLifecycleObserver> observer);

  // Returns the execution state of the most recently created active task, or
  // `std::nullopt` if there are no active tasks.
  std::optional<ActorTaskState> GetActiveTaskState() const;

  // Returns the WebState associated with the given ActorTask by its ID, or
  // nullptr if not found or is not in the set of the task's controlled
  // WebStates.
  // TODO(crbug.com/568338837): Remove once tab observation moves into
  // `ActorTask`.
  web::WebState* GetWebStateForID(web::WebStateID web_state_id,
                                  ActorTaskId task_id);

  // Adds a WebState to the set of controlled WebStates for the given task.
  void AddControlledWebState(ActorTaskId task_id, web::WebState* web_state);

  // Returns a weak pointer to the background worker of each active task.
  std::vector<base::WeakPtr<ActorTaskBackgroundWorker>>
  GetWeakTaskBackgroundWorkers();

  // Returns a weak pointer to this service instance, which is null after
  // `Shutdown()`.
  base::WeakPtr<ActorService> GetWeakPtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_this_;
  }

 private:
  friend class ActorServiceTest;

  // The profile associated with this service instance. Owned by the
  // ProfileManager, which is guaranteed to outlive this KeyedService.
  raw_ptr<ProfileIOS> profile_;

  // Actor tool factory.
  std::unique_ptr<ActorToolFactory> tool_factory_;

  // Journal used for logging task tools, state transitions, and results.
  std::unique_ptr<AggregatedJournal> journal_;

  // Enterprise policy checker for this profile.
  std::unique_ptr<ActorEnterprisePolicyCheckerIOS> policy_checker_;

  // Subscription for enterprise policy changes to the actuation capability.
  base::CallbackListSubscription can_act_on_web_subscription_;

  // Map of active tasks, keyed by their task ID.
  std::map<ActorTaskId, std::unique_ptr<ActorTask>> active_tasks_;

  // Stopped tasks awaiting asynchronous destruction.
  std::map<ActorTaskId, std::unique_ptr<ActorTask>> pending_delete_tasks_;

  // Observers for task updates.
  std::vector<__weak id<ActorTaskUpdatesObserver>> task_observers_;

  // Observers notified of task start and stop, held weakly.
  __strong PostedObserverList<id<ActorTaskLifecycleObserver>>*
      lifecycle_observers_;

  // Map of pending PageContext extractions ("observations"). Used to keep the
  // wrapper alive while the extraction is in progress.
  // TODO(crbug.com/568338837): Move tab observation into `ActorTask` so it is
  // covered by `pending_act_`.
  std::map<web::WebStateID, PageContextWrapper*> pending_observations_;

  // Posts `method` with `args` as a command. Commands run in call order, each
  // as its own task, so that task logic never runs on a caller's stack. Bound
  // to `weak_this_`, so commands still queued at `Shutdown()`, or enqueued
  // after it, never run.
  template <typename Method, typename... Args>
  void EnqueueCommand(Method method, Args&&... args);

  // Posts `reply`, a caller's callback bound to its result, so that callers
  // are never re-entered from within the service. Bound to `weak_this_`, so
  // replies still pending at `Shutdown()`, or posted after it, never run.
  void PostReply(base::OnceClosure reply);

  // Runs a reply posted by `PostReply`.
  void RunReply(base::OnceClosure reply);

  // Command implementations of the public methods of the same name.
  void DoPerformActions(ActorTaskId task_id,
                        std::vector<optimization_guide::proto::Action> actions,
                        std::string task_update,
                        PerformActionsCallback callback);
  void DoRequestTabObservation(ActorTaskId task_id,
                               base::WeakPtr<web::WebState> weak_web_state,
                               TabObservationCallback callback);
  void DoInterruptTask(ActorTaskId task_id,
                       ActorTaskInterruptReason reason,
                       std::string message);
  void DoStopTask(ActorTaskId task_id, ActorTaskStoppedReason reason);

  // Starts a PageContext extraction of `web_state` for the task `task_id`.
  // `callback` may run synchronously, and is destroyed without running if the
  // service shuts down first.
  void StartTabObservation(ActorTaskId task_id,
                           web::WebState* web_state,
                           TabObservationCallback callback);

  // Posts `callback` with `response` as a reply.
  void PostTabObservationReply(TabObservationCallback callback,
                               PageContextWrapperCallbackResponse response);

  // Returns whether `Shutdown()` ran.
  bool IsShutDown() const { return !weak_this_; }

  // Returns the active task identified by `task_id`, or nullptr.
  ActorTask* FindActiveTask(ActorTaskId task_id);

  // Posts the lifecycle stop of `task` to lifecycle observers.
  void NotifyTaskStopped(const ActorTask& task);

  // Callback for when PageContext extraction completes.
  void OnPageContextExtractionComplete(
      web::WebStateID web_state_id,
      const GURL& last_committed_url,
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry>
          pending_journal_entry,
      TabObservationCallback callback,
      PageContextWrapperCallbackResponse response);

  // Removes the stopped task from `active_tasks_`, notifies lifecycle
  // observers, and schedules its destruction.
  void OnTaskStopped(ActorTaskId task_id);

  // Destroys the stopped task for `task_id`.
  void FinishStoppedTask(ActorTaskId task_id);

  // Completes `PerformActions` once post-action tab observations finish.
  void OnActObservationsComplete(
      ActorTaskId task_id,
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry> observation_entry,
      PerformActionsCallback callback,
      PerformActionsResult result,
      std::vector<std::unique_ptr<TabObservationResponse>> contexts);

  // Callback for when a task finishes executing its actions.
  void OnActCompleted(ActorTaskId task_id,
                      PerformActionsCallback callback,
                      std::vector<ActionResult> results);

  // Resolves target WebStates from ActorToolRequests and registers them in the
  // task's controlled WebStates.
  void AddControlledWebStates(
      ActorTask* task,
      const std::vector<std::unique_ptr<ActorToolRequest>>& actions);

  // Stops all active tasks when enterprise policy disables web actuation.
  void OnCanActOnWebChanged(bool can_act_on_web);

  // Returns the WebState associated with the given `web_state_id`, or nullptr
  // if it cannot be found.
  web::WebState* GetWebState(web::WebStateID web_state_id,
                             bool allows_incognito);

  // Ensures the service is only used on the sequence it was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Generator for unique task IDs.
  ActorTaskId::Generator next_task_id_;

  // Copied by every bind of this service's deferred work (commands, replies
  // and completions) and by `GetWeakPtr()`. Invalidated in `Shutdown()`, and
  // copies of an invalidated `WeakPtr` stay invalid, so no work bound before or
  // after `Shutdown()` runs afterwards. Do not call
  // `weak_ptr_factory_.GetWeakPtr()`; copy `weak_this_` instead.
  base::WeakPtr<ActorService> weak_this_;

  // Doomed in `Shutdown()`, after which `weak_ptr_factory_.GetWeakPtr()`
  // DCHECKs, hence `weak_this_`.
  base::WeakPtrFactory<ActorService> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_SERVICE_H_
