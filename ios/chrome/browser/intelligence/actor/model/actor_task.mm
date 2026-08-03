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
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task_intervention_handler.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
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
                     ActorToolFactory* tool_factory,
                     BrowserList* browser_list)
    : task_id_(task_id),
      browser_list_(browser_list),
      title_(title),
      allow_incognito_web_states_(allow_incognito_web_states),
      journal_(journal),
      tool_factory_(tool_factory) {
  CHECK(browser_list_);
  // TODO(crbug.com/504704411): Allow incognito WebStates.
  CHECK(!allow_incognito_web_states_);
  engine_ = std::make_unique<ActorEngine>(/*execution_updates_delegate=*/this,
                                          /*tool_delegate=*/this);
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
    LogJournalEvent(
        GetJournal(), GURL(), task_id_, "ActorTask::AddControlledWebState",
        {{"web_state_id", base::NumberToString(
                              web_state->GetUniqueIdentifier().identifier())}});
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

ActorTaskId ActorTask::GetTaskId() const {
  return task_id_;
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

AggregatedJournal& ActorTask::GetJournal() const {
  CHECK(journal_);
  return *journal_;
}

ActorToolFactory& ActorTask::GetToolFactory() const {
  CHECK(tool_factory_);
  return *tool_factory_;
}

void ActorTask::InterruptFromTool() {
  if (GetState() != ActorTaskState::kReflecting &&
      GetState() != ActorTaskState::kActing) {
    return;
  }
  Pause(/*by_actor=*/true);
  SetState(ActorTaskState::kWaitingOnUser);
}

void ActorTask::UninterruptFromTool() {
  if (GetState() != ActorTaskState::kWaitingOnUser) {
    return;
  }
  Resume();
  SetState(ActorTaskState::kActing);
}

ActorTaskFormFillingHandler* ActorTask::GetActorTaskFormFillingHandler() {
  if (!form_filling_handler_) {
    intervention_handler_ = [[ActorTaskInterventionHandler alloc] init];
    form_filling_handler_ = ActorTaskFormFillingHandler::Create(
        base::PassKey<ActorTask>(), GetJournal(), task_id_);
    form_filling_handler_->SetInterventionDelegate(base::PassKey<ActorTask>(),
                                                   intervention_handler_);
  }
  return form_filling_handler_.get();
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
  LogJournalEvent(GetJournal(), GURL(), task_id_, "ActorTask::SetState",
                  {{"current_state", ActorTaskStateToString(state_)},
                   {"new_state", ActorTaskStateToString(new_state)}});
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

}  // namespace actor
