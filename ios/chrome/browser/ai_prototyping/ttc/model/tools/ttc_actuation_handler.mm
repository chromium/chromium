// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

#import <map>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler+Testing.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {

using ActuationCallback = base::OnceCallback<void(TTCActuationResponse*)>;

struct ActiveActuation {
  ActuationCallback callback;
  NSString* callID = nil;
};

// Message constants for internal actuation error reporting.
NSString* const kActuationHandlerDisconnectedMessage =
    @"Actuation handler disconnected.";
NSString* const kTaskNotActiveOrStoppedMessage =
    @"Task does not exist or has been stopped.";
NSString* const kFailedToParseActionProtoMessage =
    @"Failed to parse action proto.";
NSString* const kEmptyActionSequenceMessage = @"Empty action sequence.";
NSString* const kConcurrentActionSequenceMessage =
    @"An action sequence is already in progress for this task.";

// Sets `tab_id` on `sub_action` if not already explicitly populated.
template <typename SubAction>
void SetTabIdIfUnset(SubAction* sub_action, int32_t tab_id) {
  if (!sub_action->has_tab_id()) {
    sub_action->set_tab_id(tab_id);
  }
}

// Injects the current tab and window ID into the given action proto if not
// already explicitly specified by the action.
void InjectDataIntoAction(optimization_guide::proto::Action& action,
                          web::WebStateID web_state_id,
                          SessionID window_id) {
  if (web_state_id.valid()) {
    int32_t tab_id = web_state_id.identifier();
    switch (action.action_case()) {
      case optimization_guide::proto::Action::kNavigate:
        SetTabIdIfUnset(action.mutable_navigate(), tab_id);
        break;
      case optimization_guide::proto::Action::kClick:
        SetTabIdIfUnset(action.mutable_click(), tab_id);
        break;
      case optimization_guide::proto::Action::kBack:
        SetTabIdIfUnset(action.mutable_back(), tab_id);
        break;
      case optimization_guide::proto::Action::kForward:
        SetTabIdIfUnset(action.mutable_forward(), tab_id);
        break;
      case optimization_guide::proto::Action::kSelect:
        SetTabIdIfUnset(action.mutable_select(), tab_id);
        break;
      case optimization_guide::proto::Action::kDragAndRelease:
        SetTabIdIfUnset(action.mutable_drag_and_release(), tab_id);
        break;
      case optimization_guide::proto::Action::kType:
        SetTabIdIfUnset(action.mutable_type(), tab_id);
        break;
      case optimization_guide::proto::Action::kWait:
        if (!action.wait().has_observe_tab_id()) {
          action.mutable_wait()->set_observe_tab_id(tab_id);
        }
        break;
      case optimization_guide::proto::Action::kScroll:
        SetTabIdIfUnset(action.mutable_scroll(), tab_id);
        break;
      case optimization_guide::proto::Action::kScrollTo:
        SetTabIdIfUnset(action.mutable_scroll_to(), tab_id);
        break;
      case optimization_guide::proto::Action::kAttemptLogin:
        SetTabIdIfUnset(action.mutable_attempt_login(), tab_id);
        break;
      case optimization_guide::proto::Action::kAttemptFormFilling:
        SetTabIdIfUnset(action.mutable_attempt_form_filling(), tab_id);
        break;
      case optimization_guide::proto::Action::kCloseTab:
        SetTabIdIfUnset(action.mutable_close_tab(), tab_id);
        break;
      case optimization_guide::proto::Action::kActivateTab:
        SetTabIdIfUnset(action.mutable_activate_tab(), tab_id);
        break;
      default:
        break;
    }
  }

  if (window_id.is_valid() &&
      action.action_case() == optimization_guide::proto::Action::kCreateTab &&
      !action.create_tab().has_window_id()) {
    action.mutable_create_tab()->set_window_id(window_id.id());
  }
}

}  // namespace

@interface TTCActuationHandler () <ActorTaskInterventionDelegate>
@end

@implementation TTCActuationHandler {
  SEQUENCE_CHECKER(_sequenceChecker);

  // The Profile associated with the browser session.
  raw_ptr<ProfileIOS> _profile;

  // The WebStateList associated with the active browser.
  raw_ptr<WebStateList> _webStateList;

  // The SessionID of the browser window.
  std::optional<SessionID> _browserID;

  // Testing override for ActorService.
  raw_ptr<actor::ActorService> _actorServiceForTesting;

  // Map from active Actor task IDs to their controlled WebState IDs.
  std::map<actor::ActorTaskId, web::WebStateID> _taskToWebStateIDMap;

  // Active actuation callbacks awaiting completion, keyed by task ID.
  std::map<actor::ActorTaskId, ActiveActuation> _activeCallbacks;
}

#pragma mark - Initializers

- (instancetype)initWithProfile:(ProfileIOS*)profile
                   webStateList:(WebStateList*)webStateList
                      browserID:(SessionID)browserID {
  self = [super init];
  if (self) {
    if (!profile || !webStateList || !browserID.is_valid()) {
      return nil;
    }
    _profile = profile;
    _webStateList = webStateList;
    _browserID = browserID;
  }
  return self;
}

#pragma mark - Teardown

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  _webStateList = nullptr;
  _browserID.reset();

  actor::ActorService* actorService = [self actorService];
  if (actorService) {
    for (const auto& [taskID, _] : _taskToWebStateIDMap) {
      actorService->StopTask(taskID, actor::ActorTaskStoppedReason::kShutdown);
    }
  }

  _taskToWebStateIDMap.clear();
  _profile = nullptr;
  _actorServiceForTesting = nullptr;

  std::map<actor::ActorTaskId, ActiveActuation> callbacks =
      std::exchange(_activeCallbacks, {});
  for (auto& [taskID, actuation] : callbacks) {
    if (actuation.callback) {
      std::move(actuation.callback)
          .Run([[TTCActuationResponse alloc]
              initWithResultCode:actor::mojom::ActionResultCode::
                                     kExecutorDestroyed
                    errorMessage:kActuationHandlerDisconnectedMessage
                          callID:actuation.callID]);
    }
  }
}

#pragma mark - Public Actuation

- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  actor::ActorTaskId taskID = actor::ActorTaskId();
  actor::ActorService* actorService = [self actorService];
  if (!_webStateList || !actorService) {
    return taskID;
  }

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return taskID;
  }

  if (activeWebState->GetBrowserState() &&
      activeWebState->GetBrowserState()->IsOffTheRecord()) {
    return taskID;
  }

  taskID = actorService->CreateTask(
      base::SysNSStringToUTF8(title),
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTtc,
                            /*id=*/std::nullopt),
      /*allow_incognito_web_states=*/false);
  actorService->AddControlledWebState(taskID, activeWebState);
  actorService->SetTaskInterventionDelegate(taskID, self);
  _taskToWebStateIDMap[taskID] = activeWebState->GetUniqueIdentifier();
  return taskID;
}

- (void)dispatchActuationRequest:(TTCActuationRequest*)request
                       forTaskID:(actor::ActorTaskId)taskID
                 completionBlock:
                     (void (^)(TTCActuationResponse* response))completionBlock {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(request);
  CHECK(completionBlock);

  actor::ActorService* actorService = [self actorService];
  auto taskIt = _taskToWebStateIDMap.find(taskID);
  if (!actorService || taskIt == _taskToWebStateIDMap.end()) {
    completionBlock([[TTCActuationResponse alloc]
        initWithResultCode:actor::mojom::ActionResultCode::kTaskWentAway
              errorMessage:kTaskNotActiveOrStoppedMessage
                    callID:request.callID]);
    return;
  }

  if (request.actionProtos.count == 0) {
    completionBlock([[TTCActuationResponse alloc]
        initWithResultCode:actor::mojom::ActionResultCode::kEmptyActionSequence
              errorMessage:kEmptyActionSequenceMessage
                    callID:request.callID]);
    return;
  }

  if (_activeCallbacks.find(taskID) != _activeCallbacks.end()) {
    completionBlock([[TTCActuationResponse alloc]
        initWithResultCode:actor::mojom::ActionResultCode::
                               kExecutionEngineExistingAction
              errorMessage:kConcurrentActionSequenceMessage
                    callID:request.callID]);
    return;
  }

  web::WebStateID webStateId = taskIt->second;
  std::vector<optimization_guide::proto::Action> actions;
  actions.reserve(request.actionProtos.count);

  for (NSData* data in request.actionProtos) {
    optimization_guide::proto::Action action;
    if (!action.ParseFromArray([data bytes], [data length])) {
      completionBlock([[TTCActuationResponse alloc]
          initWithResultCode:actor::mojom::ActionResultCode::kArgumentsInvalid
                errorMessage:kFailedToParseActionProtoMessage
                      callID:request.callID]);
      return;
    }
    InjectDataIntoAction(action, webStateId,
                         _browserID.value_or(SessionID::InvalidValue()));
    actions.push_back(action);
  }

  _activeCallbacks[taskID] = {
      base::BindOnce(completionBlock),
      request.callID,
  };

  __weak __typeof(self) weakSelf = self;
  NSString* callID = request.callID;

  actorService->PerformActions(
      taskID, actions, base::SysNSStringToUTF8(request.taskUpdate),
      base::BindOnce(^(actor::PerformActionsResult result) {
        [weakSelf handlePerformActionsResult:result
                                      taskID:taskID
                                      callID:callID];
      }));
}

- (void)stopTask:(actor::ActorTaskId)taskID
      withReason:(actor::ActorTaskStoppedReason)reason {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  auto taskIt = _taskToWebStateIDMap.find(taskID);
  if (taskIt == _taskToWebStateIDMap.end()) {
    return;
  }
  _taskToWebStateIDMap.erase(taskIt);

  auto callbackIt = _activeCallbacks.find(taskID);
  if (callbackIt != _activeCallbacks.end()) {
    ActiveActuation actuation = std::move(callbackIt->second);
    _activeCallbacks.erase(callbackIt);
    if (actuation.callback) {
      TTCActuationResponse* response = [[TTCActuationResponse alloc]
          initWithResultCode:actor::mojom::ActionResultCode::kTaskWentAway
                errorMessage:kTaskNotActiveOrStoppedMessage
                      callID:actuation.callID];
      std::move(actuation.callback).Run(response);
    }
  }

  actor::ActorService* actorService = [self actorService];
  if (actorService) {
    actorService->StopTask(taskID, reason);
  }
}

- (void)stopTask:(actor::ActorTaskId)taskID {
  [self stopTask:taskID
      withReason:actor::ActorTaskStoppedReason::kTaskComplete];
}

#pragma mark - Private

- (actor::ActorService*)actorService {
  if (_actorServiceForTesting) {
    return _actorServiceForTesting;
  }
  if (!_profile) {
    return nullptr;
  }
  return actor::ActorServiceFactory::GetForProfile(_profile);
}

- (void)handlePerformActionsResult:(const actor::PerformActionsResult&)result
                            taskID:(actor::ActorTaskId)taskID
                            callID:(NSString*)callID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  auto it = _activeCallbacks.find(taskID);
  if (it == _activeCallbacks.end()) {
    return;
  }
  ActiveActuation actuation = std::move(it->second);
  _activeCallbacks.erase(it);

  actor::mojom::ActionResultCode resultCode =
      actor::mojom::ActionResultCode::kOk;
  std::string errorMessage;

  for (const auto& actionResult : result.action_results) {
    if (!actionResult.tool_result.IsOk()) {
      resultCode = actionResult.tool_result.code();
      errorMessage =
          actor::GetToolExecutionResultMessage(actionResult.tool_result);
      break;
    }
  }

  if (actuation.callback) {
    TTCActuationResponse* response = [[TTCActuationResponse alloc]
        initWithResultCode:resultCode
              errorMessage:errorMessage.empty()
                               ? nil
                               : base::SysUTF8ToNSString(errorMessage)
                    callID:callID];
    std::move(actuation.callback).Run(response);
  }
}

#pragma mark - ActorTaskInterventionDelegate

- (void)actorTask:(actor::ActorTaskId)taskID
    selectFromSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
        completionHandler:
            (void (^)(ActorFormSuggestion* selectedSuggestion,
                      BOOL shouldStorePermission))completionHandler {
  completionHandler(suggestions.firstObject, NO);
}

- (void)actorTask:(actor::ActorTaskId)taskID
    requestUserInterventionWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                          buttonText:(NSString*)buttonText
                   completionHandler:(void (^)(void))completionHandler {
  if (completionHandler) {
    completionHandler();
  }
}

@end

@implementation TTCActuationHandler (Testing)

- (void)setActorServiceForTesting:(actor::ActorService*)actorService {
  _actorServiceForTesting = actorService;
}

@end
