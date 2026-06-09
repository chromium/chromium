// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

#import <algorithm>

#import "base/functional/bind.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/stl_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/journal_details_builder.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/web/public/web_state.h"

namespace actor {

namespace {

// Safety timeout duration to wait for pages to finish loading.
constexpr base::TimeDelta kPageLoadTimeout = base::Seconds(7);

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

// Logs a task state transition to the journal.
void LogTaskStateTransition(AggregatedJournal* journal,
                            ActorTaskId task_id,
                            ActorTaskState old_state,
                            ActorTaskState new_state) {
  CHECK(journal);

  std::vector<mojom::JournalDetailsPtr> details =
      JournalDetailsBuilder()
          .Add("current_state", ActorTaskStateToString(old_state))
          .Add("new_state", ActorTaskStateToString(new_state))
          .Build();

  journal->Log(GURL(), task_id, "ActorTask::SetState", std::move(details));
}

// Logs adding a controlled WebState to the journal.
void LogAddControlledWebState(AggregatedJournal* journal,
                              ActorTaskId task_id,
                              web::WebStateID web_state_id) {
  CHECK(journal);

  std::vector<mojom::JournalDetailsPtr> details =
      JournalDetailsBuilder()
          .Add("web_state_id", base::NumberToString(web_state_id.identifier()))
          .Build();

  journal->Log(GURL::EmptyGURL(), task_id, "ActorTask::AddControlledWebState",
               std::move(details));
}

// Returns true if the task state corresponds to an actuating state.
bool IsActuatingState(ActorTaskState state) {
  switch (state) {
    case ActorTaskState::kActing:
    case ActorTaskState::kReflecting:
      return true;
    case ActorTaskState::kInit:
      return false;
    // TODO(crbug.com/496164697): Add all states and remove the default case.
    default:
      return false;
  }
}

}  // namespace

ActorTask::ActorTask(ActorTaskId task_id,
                     const std::string& title,
                     bool allow_incognito_web_states,
                     AggregatedJournal* journal,
                     ActorToolFactory* tool_factory)
    : task_id_(task_id),
      title_(title),
      allow_incognito_web_states_(allow_incognito_web_states),
      journal_(journal) {
  // TODO(crbug.com/504704411): Allow incognito WebStates.
  CHECK(!allow_incognito_web_states_);

  engine_ =
      std::make_unique<ActorEngine>(task_id_, journal_, this, tool_factory);
  observers_ = static_cast<CRBProtocolObservers<ActorTaskUpdatesObserver>*>(
      [CRBProtocolObservers
          observersWithProtocol:@protocol(ActorTaskUpdatesObserver)]);
}

ActorTask::~ActorTask() {
  SetActuatingOnWebStates(false);
  load_timeout_timer_.Stop();
  observers_ = nil;
}

void ActorTask::AddObserver(id<ActorTaskUpdatesObserver> observer) {
  [observers_ addObserver:observer];

  NSMutableArray<NSNumber*>* web_state_ids = [NSMutableArray array];
  for (const auto& web_state_weak : controlled_web_states_) {
    if (web_state_weak) {
      [web_state_ids
          addObject:@(web_state_weak->GetUniqueIdentifier().identifier())];
    }
  }

  // TODO(crbug.com/501043031): Remove respondsToSelector check when didRegister
  // becomes a required protocol method.
  if ([observer respondsToSelector:@selector
                (didRegisterAsObserverForTaskID:
                                      taskTitle:taskUpdate:currentState
                                               :webStates:)]) {
    [observer didRegisterAsObserverForTaskID:task_id_
                                   taskTitle:base::SysUTF8ToNSString(title_)
                                  taskUpdate:base::SysUTF8ToNSString(
                                                 last_task_update_)
                                currentState:state_
                                   webStates:web_state_ids];
  }
}

void ActorTask::RemoveObserver(id<ActorTaskUpdatesObserver> observer) {
  [observers_ removeObserver:observer];
}

ActorTaskState ActorTask::GetState() const {
  return state_;
}

void ActorTask::Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
                    const std::string& task_update,
                    ActCallback callback) {
  // TODO(crbug.com/503054406): Check for invalid states.
  SetState(ActorTaskState::kActing);
  last_task_update_ = task_update;
  engine_->Act(
      std::move(actions),
      base::BindOnce(&ActorTask::OnActCompleted, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ActorTask::AddControlledWebState(web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  if (!std::ranges::contains(controlled_web_states_, web_state,
                             &base::WeakPtr<web::WebState>::get)) {
    LogAddControlledWebState(journal_, task_id_,
                             web_state->GetUniqueIdentifier());
    controlled_web_states_.push_back(web_state->GetWeakPtr());
    if (ActorTabHelper* tab_helper = ActorTabHelper::FromWebState(web_state)) {
      const bool is_actuating = IsActuatingState(state_);
      tab_helper->SetActuating(is_actuating);
    }
    [observers_ actorTaskWithID:task_id_
                 didAddWebState:web_state->GetUniqueIdentifier()];
  }
}

void ActorTask::OnActCompleted(ActCallback callback,
                               std::vector<ActionResult> results) {
  // TODO(crbug.com/503054406): Check for tool errors.

  if (ObserveLoadingWebStates()) {
    DeferActCompletion(std::move(callback), std::move(results));
    return;
  }

  SetState(ActorTaskState::kReflecting);
  std::move(callback).Run(std::move(results));
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

void ActorTask::DeferActCompletion(ActCallback callback,
                                   std::vector<ActionResult> results) {
  deferred_act_callback_ =
      base::BindOnce(std::move(callback), std::move(results));

  load_timeout_timer_.Start(FROM_HERE, kPageLoadTimeout,
                            base::BindOnce(&ActorTask::OnPageLoadedTimeout,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void ActorTask::DidStopLoading(web::WebState* web_state) {
  OnWebStateFinishedLoading(web_state);
}

void ActorTask::WebStateDestroyed(web::WebState* web_state) {
  OnWebStateFinishedLoading(web_state);
}

void ActorTask::OnWebStateFinishedLoading(web::WebState* web_state) {
  scoped_web_state_observations_.RemoveObservation(web_state);

  if (scoped_web_state_observations_.IsObservingAnySource()) {
    return;
  }

  // Stop the timeout and execute the deferred callback since no more observed
  // WebStates are still loading.
  load_timeout_timer_.Stop();
  SetState(ActorTaskState::kReflecting);
  if (deferred_act_callback_) {
    std::move(deferred_act_callback_).Run();
  }
}

void ActorTask::OnPageLoadedTimeout() {
  scoped_web_state_observations_.RemoveAllObservations();

  SetState(ActorTaskState::kReflecting);
  if (deferred_act_callback_) {
    std::move(deferred_act_callback_).Run();
  }
}

void ActorTask::Stop(ActorTaskStoppedReason stop_reason) {
  [observers_ actorTaskDidStopWithID:task_id_ finalState:state_];
  SetActuatingOnWebStates(false);
  // TODO(crbug.com/496164697): Implement and test.
}

void ActorTask::Pause(bool from_actor) {
  // TODO(crbug.com/496164697): Implement and test.
}

void ActorTask::Resume() {
  // TODO(crbug.com/496164697): Implement and test.
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

const std::vector<base::WeakPtr<web::WebState>>&
ActorTask::controlled_web_states() const {
  return controlled_web_states_;
}

bool ActorTask::allow_incognito_web_states() const {
  return allow_incognito_web_states_;
}

void ActorTask::SetActuatingOnWebStates(bool actuating) {
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
    tab_helper->SetActuating(actuating);
  }
}

void ActorTask::SetState(ActorTaskState new_state) {
  LogTaskStateTransition(journal_, task_id_, state_, new_state);
  ActorTaskState old_state = state_;
  state_ = new_state;

  bool old_is_actuating = IsActuatingState(old_state);
  bool new_is_actuating = IsActuatingState(new_state);
  if (old_is_actuating != new_is_actuating) {
    SetActuatingOnWebStates(new_is_actuating);
  }

  [observers_ actorTaskWithID:task_id_
               didChangeState:new_state
                    fromState:old_state];
}

void ActorTask::OnWillExecuteTool(ToolType tool_type,
                                  web::WebStateID web_state_id) {
  [observers_ actorTaskWithID:task_id_
              willExecuteTool:tool_type
                   taskUpdate:base::SysUTF8ToNSString(last_task_update_)
                   onWebState:web_state_id];
}

}  // namespace actor
