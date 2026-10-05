// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

#import <algorithm>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/location.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_web_state_policy_decider.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

namespace {

// Safety timeout duration to wait for pages to finish loading.
constexpr base::TimeDelta kPageLoadTimeout = base::Seconds(7);

// Default button text for confirmation intervention prompts.
// TODO(crbug.com/556739755): Localize default button text string.
NSString* const kDefaultConfirmationButtonText = @"Continue";

// Interval between JavaScript heartbeat pings. Found to be the sweetspot for
// keeping renderer processes alive & accepting IPC messages (otherwise they
// drop their keep-alive assertions after about 1 second of inactivity.)
constexpr base::TimeDelta kHeartbeatInterval = base::Milliseconds(400);

// Minimal zero side effects script executed to generate IPC activity.
constexpr char16_t kHeartbeatScript[] = u";";

// Returns the string representation of the ActorTaskState.
std::string ActorTaskStateToString(ActorTaskState state) {
  switch (state) {
    case ActorTaskState::kInit:
      return "Init";
    case ActorTaskState::kActing:
      return "Acting";
    case ActorTaskState::kReflecting:
      return "Reflecting";
    case ActorTaskState::kPausedByActor:
      return "PausedByActor";
    case ActorTaskState::kPausedByUser:
      return "PausedByUser";
    case ActorTaskState::kCancelled:
      return "Cancelled";
    case ActorTaskState::kFinished:
      return "Finished";
    case ActorTaskState::kWaitingOnUser:
      return "WaitingOnUser";
    case ActorTaskState::kFailed:
      return "Failed";
  }
}

// Returns the control state corresponding to `task_state`.
ActorControlState ControlStateForTaskState(ActorTaskState task_state) {
  switch (task_state) {
    case ActorTaskState::kActing:
    case ActorTaskState::kReflecting:
    case ActorTaskState::kWaitingOnUser:
      return ActorControlState::kActorControlled;
    case ActorTaskState::kInit:
      return ActorControlState::kInactive;
    // TODO(crbug.com/496164697): Add all states and remove the default case.
    default:
      return ActorControlState::kInactive;
  }
}

// Posts `callback` with a single `code` result. Posted so that callers are
// never re-entered from within `ActorTask::Act()`.
void PostActReply(ActCallback callback, mojom::ActionResultCode code) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), MakeActionResults(code)));
}

// Sends a notification to `targets`. The block must not capture the
// `ActorTask`, which may be destroyed before the notification is delivered.
using ObserverNotification =
    void (^)(CRBProtocolObservers<ActorTaskUpdatesObserver>* targets);

// Posts `notification` to the observers registered in `observers` when it
// runs. Observers are never notified synchronously, so they cannot re-enter the
// task (e.g. by stopping it) from within a task method.
void PostToAllObservers(
    CRBProtocolObservers<ActorTaskUpdatesObserver>* observers,
    ObserverNotification notification) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        notification(observers);
      }));
}

// Sends `didRegisterAsObserverForTaskID:` to a single observer.
using RegistrationNotification =
    void (^)(id<ActorTaskUpdatesObserver> observer);

// Returns whether `observer` is registered in `observers`.
bool ContainsObserver(CRBProtocolObservers<ActorTaskUpdatesObserver>* observers,
                      id<ActorTaskUpdatesObserver> observer) {
  __block bool contains = false;
  [observers executeOnObservers:^(id<ActorTaskUpdatesObserver> candidate) {
    contains |= candidate == observer;
  }];
  return contains;
}

// Moves `observer` from `pending_observers` to `observers` and sends it
// `notify_registered`. No-op if `observer` was deallocated or removed from
// `pending_observers` since the registration was posted.
void CompletePendingRegistration(
    CRBProtocolObservers<ActorTaskUpdatesObserver>* observers,
    CRBProtocolObservers<ActorTaskUpdatesObserver>* pending_observers,
    id<ActorTaskUpdatesObserver> observer,
    RegistrationNotification notify_registered) {
  if (!observer || !ContainsObserver(pending_observers, observer)) {
    return;
  }
  [pending_observers removeObserver:observer];
  [observers addObserver:observer];
  // TODO(crbug.com/501043031): Remove the check once `didRegister` is a
  // required protocol method.
  if ([observer
          respondsToSelector:@selector(didRegisterAsObserverForTaskID:taskTitle:
                                       taskUpdate:currentState:webStates:)]) {
    notify_registered(observer);
  }
}

// Adds `observer` to `pending_observers` and posts its move to `observers`,
// followed by `notify_registered`. Removing `observer` from
// `pending_observers` before the registration runs cancels it. Like
// notifications, the registration must not capture the `ActorTask`, which may
// be destroyed before it runs.
void PostObserverRegistration(
    CRBProtocolObservers<ActorTaskUpdatesObserver>* observers,
    CRBProtocolObservers<ActorTaskUpdatesObserver>* pending_observers,
    id<ActorTaskUpdatesObserver> observer,
    RegistrationNotification notify_registered) {
  [pending_observers addObserver:observer];
  // Captured weakly so that a pending registration does not extend the
  // lifetime of `observer`.
  __weak id<ActorTaskUpdatesObserver> weak_observer = observer;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        CompletePendingRegistration(observers, pending_observers, weak_observer,
                                    notify_registered);
      }));
}

}  // namespace

#pragma mark - ActorTask::PendingAct

ActorTask::PendingAct::PendingAct(ActCallback callback)
    : callback(std::move(callback)) {}
ActorTask::PendingAct::PendingAct(PendingAct&&) = default;
ActorTask::PendingAct& ActorTask::PendingAct::operator=(PendingAct&&) = default;
ActorTask::PendingAct::~PendingAct() = default;

#pragma mark - ActorTask

ActorTask::ActorTask(ActorTaskId task_id,
                     const std::string& title,
                     const TaskSourceInfo& source_info,
                     bool allow_incognito_web_states,
                     AggregatedJournal* journal,
                     ActorToolFactory* tool_factory,
                     BrowserList* browser_list)
    : task_id_(task_id),
      browser_list_(browser_list),
      title_(title),
      source_info_(source_info),
      allow_incognito_web_states_(allow_incognito_web_states),
      journal_(journal),
      tool_factory_(tool_factory) {
  CHECK(journal);
  CHECK(tool_factory);
  CHECK(browser_list);
  // TODO(crbug.com/504704411): Allow incognito WebStates.
  CHECK(!allow_incognito_web_states_);
  engine_ = std::make_unique<ActorEngine>(/*execution_updates_delegate=*/this,
                                          /*owner_task=*/this);
  observers_ = static_cast<CRBProtocolObservers<ActorTaskUpdatesObserver>*>(
      [CRBProtocolObservers
          observersWithProtocol:@protocol(ActorTaskUpdatesObserver)]);
  pending_observers_ =
      static_cast<CRBProtocolObservers<ActorTaskUpdatesObserver>*>(
          [CRBProtocolObservers
              observersWithProtocol:@protocol(ActorTaskUpdatesObserver)]);
}

ActorTask::~ActorTask() {
  SetControlStateOnWebStates(ActorControlState::kInactive);
  SetKeepRenderProcessAliveOnControlledWebStates(/*keep_alive=*/false);
  load_timeout_timer_.Stop();

  StopHeartbeatTimer();
  FinalizeBackgroundTask(/*success=*/false);

  observers_ = nil;
}

void ActorTask::AddObserver(id<ActorTaskUpdatesObserver> observer) {
  NSMutableArray<NSNumber*>* web_state_ids = [NSMutableArray array];
  for (const auto& web_state_weak : controlled_web_states_) {
    if (web_state_weak) {
      [web_state_ids
          addObject:@(web_state_weak->GetUniqueIdentifier().identifier())];
    }
  }

  const ActorTaskId task_id = task_id_;
  NSString* title = base::SysUTF8ToNSString(title_);
  NSString* task_update = base::SysUTF8ToNSString(last_task_update_);
  const ActorTaskState state = state_;
  RegistrationNotification notify_registered =
      ^(id<ActorTaskUpdatesObserver> target) {
        [target didRegisterAsObserverForTaskID:task_id
                                     taskTitle:title
                                    taskUpdate:task_update
                                  currentState:state
                                     webStates:web_state_ids];
      };
  // Registration is posted, so that `observer` skips the notifications posted
  // before this call, which the snapshot already reflects, and receives the
  // snapshot ahead of the later ones.
  PostObserverRegistration(observers_, pending_observers_, observer,
                           notify_registered);
}

void ActorTask::RemoveObserver(id<ActorTaskUpdatesObserver> observer) {
  [pending_observers_ removeObserver:observer];
  [observers_ removeObserver:observer];
}

ActorEngine& ActorTask::engine() const {
  CHECK(engine_);
  return *engine_;
}

ActorTaskState ActorTask::GetState() const {
  return state_;
}

void ActorTask::Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
                    const std::string& task_update,
                    ActCallback callback) {
  // TODO(crbug.com/503054406): Reject `Act()` in invalid states (e.g. stopped
  // with `kTaskWentAway`, paused with `kTaskPaused`, `kWaitingOnUser`).
  if (pending_act_) {
    constexpr mojom::ActionResultCode kRejectionCode =
        mojom::ActionResultCode::kExecutionEngineExistingAction;
    LogActRejection(*journal_, task_id_, "ActorTask::Act", kRejectionCode);
    PostActReply(std::move(callback), kRejectionCode);
    return;
  }

  pending_act_.emplace(std::move(callback));
  SetState(ActorTaskState::kActing);
  if (!task_update.empty()) {
    last_task_update_ = task_update;
  }

  UpdateBackgroundTaskSubtitle(task_update);
  StartHeartbeatTimer();

  engine_->Act(std::move(actions),
               base::BindOnce(&ActorTask::OnActCompleted,
                              weak_ptr_factory_.GetWeakPtr()));
}

bool ActorTask::HasPendingAct() const {
  return pending_act_.has_value();
}

void ActorTask::AddControlledWebState(web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  if (!std::ranges::contains(controlled_web_states_, web_state,
                             &base::WeakPtr<web::WebState>::get)) {
    LogJournalEvent(
        *journal_, GURL(), task_id_, "ActorTask::AddControlledWebState",
        {{"web_state_id", base::NumberToString(
                              web_state->GetUniqueIdentifier().identifier())}});
    controlled_web_states_.push_back(web_state->GetWeakPtr());
    if (!IsTerminalState(state_)) {
      web_state->SetKeepRenderProcessAlive(/*keep_alive=*/true);
    }

    // Attach a policy decider to intercept and gate implicit navigations
    // (e.g., link clicks, redirects) against origin policies.
    if (engine_) {
      policy_deciders_[web_state->GetUniqueIdentifier()] =
          std::make_unique<ActorWebStatePolicyDecider>(
              web_state,
              tool_factory_->profile_context_resolver()
                  .GetOriginGatingService(),
              engine_->GetOriginGatingCheckerId(), task_id_,
              base::BindRepeating(&ActorTask::OnNavigationBlocked,
                                  weak_ptr_factory_.GetWeakPtr()));
    }

    if (ActorTabHelper* tab_helper = ActorTabHelper::FromWebState(web_state)) {
      tab_helper->SetControlState(ControlStateForTaskState(state_));
    }

    StartHeartbeatTimer();

    const ActorTaskId task_id = task_id_;
    const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
    PostToAllObservers(
        observers_, ^(CRBProtocolObservers<ActorTaskUpdatesObserver>* targets) {
          [targets actorTaskWithID:task_id didAddWebState:web_state_id];
        });
  }
}

void ActorTask::Stop(ActorTaskStoppedReason stop_reason) {
  if (IsTerminalState(state_)) {
    return;
  }
  // TODO(crbug.com/532978481): Map `stop_reason` to the corresponding terminal
  // `ActorTaskState` (`kFinished`, `kFailed`, `kCancelled`) instead of
  // hardcoding `kCancelled`.
  // `SetState` also transitions the web states to `kInactive` control state.
  SetState(ActorTaskState::kCancelled);
  SetKeepRenderProcessAliveOnControlledWebStates(/*keep_alive=*/false);

  StopHeartbeatTimer();
  const bool success = stop_reason == ActorTaskStoppedReason::kTaskComplete ||
                       stop_reason == ActorTaskStoppedReason::kStoppedByUser;
  FinalizeBackgroundTask(success);

  // TODO(crbug.com/496164697): Implement and test.
  // TODO(crbug.com/565875367): Remove once observers migrate to
  // `ActorTaskLifecycleObserver`.
  const ActorTaskId task_id = task_id_;
  const ActorTaskState final_state = state_;
  PostToAllObservers(
      observers_, ^(CRBProtocolObservers<ActorTaskUpdatesObserver>* targets) {
        [targets actorTaskDidStopWithID:task_id finalState:final_state];
      });
}

void ActorTask::Pause(bool from_actor) {
  // TODO(crbug.com/496164697): Implement and test.
}

void ActorTask::Resume() {
  StartHeartbeatTimer();

  // TODO(crbug.com/496164697): Implement and test.
}

void ActorTask::SetInterventionDelegate(
    id<ActorTaskInterventionDelegate> delegate) {
  intervention_delegate_ = delegate;
}

void ActorTask::Interrupt(ActorTaskInterruptReason interrupt_reason,
                          std::string_view message) {
  if (state_ != ActorTaskState::kInit &&
      state_ != ActorTaskState::kReflecting &&
      state_ != ActorTaskState::kActing) {
    // TODO(crbug.com/548051839): Enforce valid state transitions with a CHECK
    // or log a signal when an unexpected interrupt occurs.
    return;
  }

  engine_->PauseOngoingActions();
  SetState(ActorTaskState::kWaitingOnUser);

  switch (interrupt_reason) {
    case ActorTaskInterruptReason::kWaitingUserConfirmation:
      RequestInterruptConfirmation(message);
      break;
    case ActorTaskInterruptReason::kWaitingUserClarification:
    case ActorTaskInterruptReason::kWaitingUserTakeover:
    case ActorTaskInterruptReason::kWaitingIrrelevantUserInput:
      // TODO(crbug.com/548051839): Support additional interrupt reasons.
      // TODO(crbug.com/532978481): Route task-initiated stops through
      // `ActorService::StopTask`.
      Stop(ActorTaskStoppedReason::kBrowserFailure);
      break;
    case ActorTaskInterruptReason::kUnknownReason:
      // Internal or tool-level interrupts do not request user confirmation.
      break;
  }
}

void ActorTask::RequestInterruptConfirmation(std::string_view message) {
  if (!intervention_delegate_ || message.empty()) {
    // TODO(crbug.com/532978481): Route task-initiated stops through
    // `ActorService::StopTask`.
    Stop(ActorTaskStoppedReason::kBrowserFailure);
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ActorTask::ShowInterruptConfirmation,
                     weak_ptr_factory_.GetWeakPtr(), std::string(message)));
}

void ActorTask::ShowInterruptConfirmation(const std::string& message) {
  // The task may have been stopped or uninterrupted since the request was
  // posted.
  if (state_ != ActorTaskState::kWaitingOnUser) {
    return;
  }
  if (!intervention_delegate_) {
    // TODO(crbug.com/567037054): Stop the task once task-initiated stops are
    // reported to `ActorService`. Until then, the task keeps waiting on the
    // user until it is stopped externally.
    return;
  }
  // Posted so that a delegate completing synchronously never re-enters the
  // task.
  void (^completion)(void) =
      base::CallbackToBlock(base::BindPostTaskToCurrentDefault(
          base::BindOnce(&ActorTask::OnInterruptConfirmationResolved,
                         weak_ptr_factory_.GetWeakPtr())));

  // Must remain the last statement: the delegate may stop, and thereby
  // destroy, the task.
  [intervention_delegate_ actorTask:task_id_
      requestUserInterventionWithTitle:base::SysUTF8ToNSString(message)
                              subtitle:nil
                            buttonText:kDefaultConfirmationButtonText
                     completionHandler:completion];
}

void ActorTask::OnInterruptConfirmationResolved() {
  if (state_ != ActorTaskState::kWaitingOnUser) {
    // TODO(crbug.com/548051839): Enforce valid state transitions once task
    // state transitions and async intervention cancellation are fully audited.
    return;
  }
  Uninterrupt(ActorTaskState::kReflecting);
  const ActorTaskId task_id = task_id_;
  PostToAllObservers(
      observers_, ^(CRBProtocolObservers<ActorTaskUpdatesObserver>* targets) {
        [targets actorTaskDidResolveConfirmationInterruptWithID:task_id];
      });
}

void ActorTask::Uninterrupt(ActorTaskState resumed_state) {
  if (state_ != ActorTaskState::kWaitingOnUser) {
    return;
  }
  SetState(resumed_state);
  engine_->DidUninterruptTask();
}

bool ActorTask::IsControllingWebState(web::WebState* web_state) const {
  if (!web_state) {
    return false;
  }

  for (const base::WeakPtr<web::WebState> controlled_web_state :
       controlled_web_states_) {
    if (controlled_web_state && controlled_web_state->GetUniqueIdentifier() ==
                                    web_state->GetUniqueIdentifier()) {
      return true;
    }
  }
  return false;
}

AggregatedJournal& ActorTask::GetJournal() const {
  CHECK(journal_);
  return *journal_;
}

ActorToolFactory& ActorTask::GetToolFactory() const {
  CHECK(tool_factory_);
  return *tool_factory_;
}

bool ActorTask::IsWindowIdValid(int32_t window_id) {
  return GetBrowserForWindowId(window_id) != nullptr;
}

web::WebState* ActorTask::InsertWebState(
    int32_t window_id,
    const web::NavigationManager::WebLoadParams& load_params,
    bool in_background) {
  Browser* targeted_browser = GetBrowserForWindowId(window_id);
  if (!targeted_browser) {
    return nullptr;
  }
  TabInsertionBrowserAgent* insertion_agent =
      TabInsertionBrowserAgent::FromBrowser(targeted_browser);
  if (!insertion_agent) {
    return nullptr;
  }

  TabInsertion::Params insertion_params;
  insertion_params.in_background = in_background;

  // Position the new tab immediately to the right of the prompting tab
  // (which is the first controlled WebState).
  if (!controlled_web_states_.empty()) {
    web::WebState* prompting_web_state = nullptr;
    for (const auto& weak_web_state : controlled_web_states_) {
      if (weak_web_state) {
        prompting_web_state = weak_web_state.get();
        break;
      }
    }
    if (prompting_web_state) {
      int prompting_index =
          targeted_browser->GetWebStateList()->GetIndexOfWebState(
              prompting_web_state);
      if (prompting_index != WebStateList::kInvalidIndex) {
        insertion_params.index = prompting_index + 1;
      }
    }
  }

  web::WebState* web_state =
      insertion_agent->InsertWebState(load_params, insertion_params);
  if (web_state) {
    AddControlledWebState(web_state);
  }
  return web_state;
}

const std::vector<base::WeakPtr<web::WebState>>&
ActorTask::controlled_web_states() const {
  return controlled_web_states_;
}

bool ActorTask::allow_incognito_web_states() const {
  return allow_incognito_web_states_;
}

void ActorTask::SetBackgroundTaskContext(
    BackgroundContinuedProcessingTaskContext* background_task_context) {
  if (!IsGeminiActorBackgroundingEnabled()) {
    return;
  }
  background_task_context_ = background_task_context;
  UpdateBackgroundTaskSubtitle(last_task_update_);
}

#pragma mark - web::WebStateObserver

void ActorTask::DidStopLoading(web::WebState* web_state) {
  OnWebStateFinishedLoading(web_state);
}

void ActorTask::WebStateDestroyed(web::WebState* web_state) {
  policy_deciders_.erase(web_state->GetUniqueIdentifier());
  OnWebStateFinishedLoading(web_state);
  if (ActorTabHelper* tab_helper = ActorTabHelper::FromWebState(web_state)) {
    tab_helper->SetControlState(ActorControlState::kInactive);
  }
  PruneDestroyedWebStates(web_state);

  if (controlled_web_states_.empty()) {
    StopHeartbeatTimer();
  }
}

#pragma mark - Private

void ActorTask::SetControlStateOnWebStates(ActorControlState control_state) {
  for (const base::WeakPtr<web::WebState>& web_state_weak :
       controlled_web_states_) {
    web::WebState* web_state = web_state_weak.get();
    if (!web_state) {
      continue;
    }
    ActorTabHelper* tab_helper = ActorTabHelper::FromWebState(web_state);
    if (!tab_helper) {
      continue;
    }
    tab_helper->SetControlState(control_state);
  }
}

void ActorTask::SetKeepRenderProcessAliveOnControlledWebStates(
    bool keep_alive) {
  for (const base::WeakPtr<web::WebState>& web_state_weak :
       controlled_web_states_) {
    if (web::WebState* web_state = web_state_weak.get()) {
      web_state->SetKeepRenderProcessAlive(keep_alive);
    }
  }
}

void ActorTask::SetState(ActorTaskState new_state) {
  LogJournalEvent(*journal_, GURL(), task_id_, "ActorTask::SetState",
                  {{"current_state", ActorTaskStateToString(state_)},
                   {"new_state", ActorTaskStateToString(new_state)}});
  ActorTaskState old_state = state_;
  state_ = new_state;

  ActorControlState old_control_state = ControlStateForTaskState(old_state);
  ActorControlState new_control_state = ControlStateForTaskState(new_state);
  if (old_control_state != new_control_state) {
    SetControlStateOnWebStates(new_control_state);
  }

  if (IsTerminalState(new_state)) {
    StopHeartbeatTimer();
  }

  const ActorTaskId task_id = task_id_;
  PostToAllObservers(
      observers_, ^(CRBProtocolObservers<ActorTaskUpdatesObserver>* targets) {
        [targets actorTaskWithID:task_id
                  didChangeState:new_state
                       fromState:old_state];
      });
}

void ActorTask::OnActCompleted(std::vector<ActionResult> results) {
  // `Act()` registers `pending_act_` before starting the engine, and only
  // `FinishAct()` clears it.
  CHECK(pending_act_);
  // TODO(crbug.com/503054406): Check for tool errors.

  if (ObserveLoadingWebStates()) {
    DeferActCompletion(std::move(results));
    return;
  }
  FinishAct(std::move(results));
}

bool ActorTask::ObserveLoadingWebStates() {
  for (const auto& weak_web_state : controlled_web_states_) {
    web::WebState* web_state = weak_web_state.get();
    if (web_state && web_state->IsLoading()) {
      scoped_web_state_observations_.AddObservation(web_state);
    }
  }

  return scoped_web_state_observations_.IsObservingAnySource();
}

void ActorTask::DeferActCompletion(std::vector<ActionResult> results) {
  pending_act_->deferred_results = std::move(results);

  load_timeout_timer_.Start(FROM_HERE, kPageLoadTimeout,
                            base::BindOnce(&ActorTask::OnPageLoadedTimeout,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void ActorTask::FinishAct(std::vector<ActionResult> results) {
  // Detached before replying so that an `Act()` issued from the callback is
  // accepted.
  ActCallback callback = std::move(pending_act_->callback);
  pending_act_.reset();
  // Not bound to the task so that the caller is always answered, even if the
  // task is destroyed before the reply is delivered. Posted before the state
  // notification so that the caller sees the result before observers see the
  // transition.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
  SetState(ActorTaskState::kReflecting);
}

void ActorTask::FinishDeferredAct() {
  // Load observations and the timeout only exist after `DeferActCompletion()`,
  // and are cleared before `FinishAct()` resets `pending_act_`.
  CHECK(pending_act_);
  CHECK(pending_act_->deferred_results);
  FinishAct(std::move(*pending_act_->deferred_results));
}

void ActorTask::OnWebStateFinishedLoading(web::WebState* web_state) {
  if (!scoped_web_state_observations_.IsObservingSource(web_state)) {
    return;
  }
  scoped_web_state_observations_.RemoveObservation(web_state);

  if (scoped_web_state_observations_.IsObservingAnySource()) {
    return;
  }

  // Stop the timeout and finish the deferred `Act()` since no more observed
  // WebStates are still loading.
  load_timeout_timer_.Stop();
  FinishDeferredAct();
}

void ActorTask::OnPageLoadedTimeout() {
  scoped_web_state_observations_.RemoveAllObservations();
  FinishDeferredAct();
}

void ActorTask::OnWillExecuteTool(ToolType tool_type,
                                  web::WebStateID web_state_id) {
  UpdateBackgroundTaskProgress();

  const ActorTaskId task_id = task_id_;
  NSString* task_update = base::SysUTF8ToNSString(last_task_update_);
  PostToAllObservers(
      observers_, ^(CRBProtocolObservers<ActorTaskUpdatesObserver>* targets) {
        [targets actorTaskWithID:task_id
                 willExecuteTool:tool_type
                      taskUpdate:task_update
                      onWebState:web_state_id];
      });
}

Browser* ActorTask::GetBrowserForWindowId(int32_t window_id) const {
  BrowserList::BrowserType browser_type = BrowserList::BrowserType::kRegular;
  if (allow_incognito_web_states_) {
    browser_type = BrowserList::BrowserType::kRegularAndIncognito;
  }
  for (Browser* browser : browser_list_->BrowsersOfType(browser_type)) {
    ActorBrowserAgent* agent = ActorBrowserAgent::FromBrowser(browser);
    if (agent &&
        agent->browser_id() == SessionID::FromSerializedValue(window_id)) {
      return browser;
    }
  }
  return nullptr;
}

void ActorTask::PruneDestroyedWebStates(web::WebState* destroying_web_state) {
  std::erase_if(controlled_web_states_,
                [destroying_web_state](
                    const base::WeakPtr<web::WebState>& weak_web_state) {
                  return !weak_web_state ||
                         weak_web_state.get() == destroying_web_state;
                });
}

void ActorTask::OnNavigationBlocked(mojom::ActionResultCode code) {
  if (engine_) {
    engine_->FailCurrentTool(code);
  }
}

void ActorTask::UpdateBackgroundTaskSubtitle(const std::string& task_update) {
  if (!background_task_context_ || task_update.empty()) {
    return;
  }
  NSString* subtitle = base::SysUTF8ToNSString(task_update);
  if ([background_task_context_.subtitle isEqualToString:subtitle]) {
    return;
  }
  background_task_context_.subtitle = subtitle;
}

void ActorTask::UpdateBackgroundTaskProgress() {
  if (background_task_context_) {
    [background_task_context_ incrementStepProgress];
  }
}

void ActorTask::FinalizeBackgroundTask(bool success) {
  if (!background_task_context_) {
    return;
  }

  if (!background_task_context_.completed) {
    [background_task_context_ setTaskCompletedWithSuccess:success];
  }
  background_task_context_ = nil;
}

void ActorTask::StartHeartbeatTimer() {
  if (!IsGeminiActorBackgroundingEnabled()) {
    return;
  }

  if (IsTerminalState(state_)) {
    return;
  }

  if (heartbeat_timer_.IsRunning()) {
    return;
  }

  PruneDestroyedWebStates();
  if (controlled_web_states_.empty()) {
    return;
  }

  heartbeat_timer_.Start(FROM_HERE, kHeartbeatInterval,
                         base::BindRepeating(&ActorTask::SendHeartbeatPing,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void ActorTask::StopHeartbeatTimer() {
  heartbeat_timer_.Stop();
}

void ActorTask::SendHeartbeatPing() {
  PruneDestroyedWebStates();
  if (controlled_web_states_.empty()) {
    StopHeartbeatTimer();
    return;
  }

  for (const base::WeakPtr<web::WebState>& web_state_weak :
       controlled_web_states_) {
    web::WebState* web_state = web_state_weak.get();
    if (!web_state) {
      continue;
    }

    web::WebFramesManager* frames_manager =
        web_state->GetWebFramesManager(web::ContentWorld::kIsolatedWorld);
    if (!frames_manager) {
      continue;
    }

    web::WebFrame* main_frame = frames_manager->GetMainWebFrame();
    if (!main_frame) {
      continue;
    }

    main_frame->ExecuteJavaScript(
        kHeartbeatScript, base::BindOnce(&ActorTask::OnHeartbeatPingResponse,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         web_state->GetUniqueIdentifier()));
  }
}

void ActorTask::OnHeartbeatPingResponse(web::WebStateID web_state_id,
                                        const base::Value* /*result*/,
                                        NSError* error) {
  if (error) {
    LogJournalEvent(
        *journal_, GURL(), task_id_, "ActorTask::HeartbeatPingFailed",
        {{"web_state_id", base::NumberToString(web_state_id.identifier())},
         {"error_domain", base::SysNSStringToUTF8(error.domain)},
         {"error_code", base::NumberToString(error.code)}});
  }
}

}  // namespace actor
