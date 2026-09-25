// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

#import <algorithm>

#import "base/functional/bind.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_web_state_policy_decider.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/web/public/web_state.h"

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"  // nogncheck
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#endif

namespace actor {

namespace {

// Safety timeout duration to wait for pages to finish loading.
constexpr base::TimeDelta kPageLoadTimeout = base::Seconds(7);

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
// Interval between JavaScript heartbeat pings. Found to be the sweetspot for
// keeping renderer processes alive & accepting IPC messages (otherwise they
// drop their keep-alive assertions after about 1 second of inactivity.)
constexpr base::TimeDelta kHeartbeatInterval = base::Milliseconds(400);

// Minimal zero side effects script executed to generate IPC activity.
constexpr char16_t kHeartbeatScript[] = u";";
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

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
      return ActorControlState::kActorControlled;
    case ActorTaskState::kInit:
      return ActorControlState::kInactive;
    // TODO(crbug.com/496164697): Add all states and remove the default case.
    default:
      return ActorControlState::kInactive;
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
}

ActorTask::~ActorTask() {
  SetControlStateOnWebStates(ActorControlState::kInactive);
  SetKeepRenderProcessAliveOnControlledWebStates(/*keep_alive=*/false);
  load_timeout_timer_.Stop();

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  StopHeartbeatTimer();
  FinalizeBackgroundTask(/*success=*/false);
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

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
  // TODO(crbug.com/503054406): Check for invalid states.
  SetState(ActorTaskState::kActing);
  if (!task_update.empty()) {
    last_task_update_ = task_update;
  }

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  UpdateBackgroundTaskSubtitle(task_update);
  StartHeartbeatTimer();
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

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

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
    StartHeartbeatTimer();
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

    [observers_ actorTaskWithID:task_id_
                 didAddWebState:web_state->GetUniqueIdentifier()];
  }
}

void ActorTask::Stop(ActorTaskStoppedReason stop_reason) {
  SetKeepRenderProcessAliveOnControlledWebStates(/*keep_alive=*/false);

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  StopHeartbeatTimer();
  const bool success = stop_reason == ActorTaskStoppedReason::kTaskComplete ||
                       stop_reason == ActorTaskStoppedReason::kStoppedByUser;
  FinalizeBackgroundTask(success);
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  // TODO(crbug.com/496164697): Implement and test.
  SetControlStateOnWebStates(ActorControlState::kInactive);
  [observers_ actorTaskDidStopWithID:task_id_ finalState:state_];
}

void ActorTask::Pause(bool from_actor) {
  // TODO(crbug.com/496164697): Implement and test.
}

void ActorTask::Resume() {
#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  StartHeartbeatTimer();
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

  // TODO(crbug.com/496164697): Implement and test.
}

void ActorTask::Interrupt(bool retain_user_control,
                          ActorTaskInterruptReason interrupt_reason) {
  // TODO(crbug.com/548051839): Implement and test.
  if (state_ != ActorTaskState::kReflecting &&
      state_ != ActorTaskState::kActing) {
    return;
  }
  Pause(/*from_actor=*/true);
  SetState(ActorTaskState::kWaitingOnUser);
}

void ActorTask::Uninterrupt(ActorTaskState resumed_state) {
  // TODO(crbug.com/548051839): Implement and test.
  if (state_ != ActorTaskState::kWaitingOnUser) {
    return;
  }
  Resume();
  SetState(resumed_state);
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

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
void ActorTask::SetBackgroundTaskContext(
    BackgroundContinuedProcessingTaskContext* background_task_context) {
  background_task_context_ = background_task_context;
  UpdateBackgroundTaskSubtitle(last_task_update_);
}
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

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

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  if (controlled_web_states_.empty()) {
    StopHeartbeatTimer();
  }
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
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

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  if (IsTerminalState(new_state)) {
    StopHeartbeatTimer();
  }
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

  [observers_ actorTaskWithID:task_id_
               didChangeState:new_state
                    fromState:old_state];
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

void ActorTask::OnWebStateFinishedLoading(web::WebState* web_state) {
  if (!scoped_web_state_observations_.IsObservingSource(web_state)) {
    return;
  }
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

void ActorTask::OnWillExecuteTool(ToolType tool_type,
                                  web::WebStateID web_state_id) {
#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  UpdateBackgroundTaskProgress();
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

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

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
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
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

}  // namespace actor
