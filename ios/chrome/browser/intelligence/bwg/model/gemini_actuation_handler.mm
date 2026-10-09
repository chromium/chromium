// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"

#import <map>
#import <optional>
#import <set>
#import <string>
#import <utility>
#import <vector>

#import "base/base64.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_chat_message_data_types.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {

// Callback type invoked when an actuation request completes.
using ActuationCallback = base::OnceCallback<void(GeminiActuationResponse*)>;

// The MIME type for PNG screenshots.
constexpr char kPNGMimeType[] = "image/png";

// Returned to the SDK as the user's answer once a confirmation prompt has been
// accepted. Acceptance is the only answer the prompt can report for now.
// TODO(crbug.com/571050191): Get the string approved and decide whether it
// should be localized.
NSString* const kConfirmationUserResponse =
    @"I agree with what you wanted me to confirm. Please proceed.";

// Populates a TabObservation proto with data from a PageContext.
void PopulateTabObservationFromPageContext(
    optimization_guide::proto::TabObservation* tabObservation,
    const optimization_guide::proto::PageContext& pageContext) {
  if (pageContext.has_annotated_page_content()) {
    *tabObservation->mutable_annotated_page_content() =
        pageContext.annotated_page_content();
  }

  if (pageContext.has_tab_screenshot()) {
    std::string decodedScreenshot;
    if (base::Base64Decode(pageContext.tab_screenshot(), &decodedScreenshot)) {
      tabObservation->set_screenshot(decodedScreenshot.data(),
                                     decodedScreenshot.size());
      tabObservation->set_screenshot_mime_type(kPNGMimeType);
    }
  }

  bool hasContent = pageContext.has_annotated_page_content() ||
                    pageContext.has_tab_screenshot();

  tabObservation->set_result(
      hasContent ? optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK
                 : optimization_guide::proto::TabObservation::
                       TAB_OBSERVATION_FETCH_ERROR);
}

// Serializes a Protobuf message to NSData.
template <typename ProtoMessage>
NSData* SerializeProtoToNSData(const ProtoMessage& message) {
  std::string serialized;
  message.SerializeToString(&serialized);
  return [NSData dataWithBytes:serialized.data() length:serialized.size()];
}

// Returned when the task's actuated tab no longer exists.
GeminiActuationResponse* MakeTabWentAwayResponse() {
  return [[GeminiActuationResponse alloc]
      initWithResultCode:actor::mojom::ActionResultCode::kTabWentAway
            errorMessage:"The actuated tab is no longer available."];
}

// Returned when a request arrives while another one is pending for the task.
GeminiActuationResponse* MakeRequestInProgressResponse() {
  return [[GeminiActuationResponse alloc]
      initWithResultCode:actor::mojom::ActionResultCode::kArgumentsInvalid
            errorMessage:"An actuation request is already in progress for "
                         "this task."];
}

// Maps PageContextWrapperError enums to the corresponding TabObservationResult
// proto enums.
optimization_guide::proto::TabObservation::TabObservationResult
TabObservationResultFromPageContextWrapperError(PageContextWrapperError error) {
  switch (error) {
    case PageContextWrapperError::kGenericError:
    case PageContextWrapperError::kTimeout:
      return optimization_guide::proto::TabObservation::
          TAB_OBSERVATION_UNKNOWN_ERROR;

    case PageContextWrapperError::kAPCError:
    case PageContextWrapperError::kScreenshotError:
    case PageContextWrapperError::kPDFDataError:
    case PageContextWrapperError::kForceDetachError:
    case PageContextWrapperError::kInnerTextError:
    case PageContextWrapperError::kPageUnsafeError:
      return optimization_guide::proto::TabObservation::
          TAB_OBSERVATION_FETCH_ERROR;

    case PageContextWrapperError::kPageNotExtractableError:
      return optimization_guide::proto::TabObservation::
          TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE;
  }
}

// Populates a TabObservation proto using the data from a
// TabObservationResponse, handling errors appropriately.
void PopulateTabObservationFromResponse(
    optimization_guide::proto::TabObservation* tabObservation,
    const actor::TabObservationResponse& response) {
  tabObservation->set_id(response.tab_id.identifier());
  if (!response.web_state_exists) {
    tabObservation->set_result(optimization_guide::proto::TabObservation::
                                   TAB_OBSERVATION_TAB_WENT_AWAY);
  } else if (!response.page_context_response.has_value()) {
    tabObservation->set_result(TabObservationResultFromPageContextWrapperError(
        response.page_context_response.error()));
  } else {
    PopulateTabObservationFromPageContext(
        tabObservation, *response.page_context_response.value());
  }
}

// Populates an `ActionsResult` proto from `result` and sets `outResultCode`.
optimization_guide::proto::ActionsResult ActionsResultFromPerformActionsResult(
    const actor::PerformActionsResult& result,
    actor::mojom::ActionResultCode& outResultCode) {
  optimization_guide::proto::ActionsResult actionsResult;

  // Record the first failing action index and error message.
  std::optional<size_t> failedActionIndex;
  outResultCode = actor::mojom::ActionResultCode::kOk;
  for (size_t i = 0; i < result.action_results.size(); ++i) {
    const auto& actionResult = result.action_results[i];
    if (!actionResult.tool_result.IsOk()) {
      failedActionIndex = i;
      outResultCode = actionResult.tool_result.code();
      actionsResult.set_error_message(
          actor::GetToolExecutionResultMessage(actionResult.tool_result));
      break;
    }
  }

  actionsResult.set_action_result(static_cast<int32_t>(outResultCode));
  if (failedActionIndex.has_value()) {
    actionsResult.set_index_of_failed_action(
        static_cast<int32_t>(*failedActionIndex));
  }

  // Populate tab observations.
  for (const auto& observationResponse : result.page_contexts) {
    if (!observationResponse) {
      continue;
    }
    auto* tabObservationMessage = actionsResult.add_tabs();
    PopulateTabObservationFromResponse(tabObservationMessage,
                                       *observationResponse);
  }

  // TODO(crbug.com/504704411): Populate WindowObservation here.
  return actionsResult;
}

// Injects the current tab and window ID into the given action depending on its
// case.
// LINT.IfChange(InjectDataIntoAction)
void InjectDataIntoAction(optimization_guide::proto::Action& action,
                          web::WebStateID web_state_id,
                          SessionID window_id) {
  int32_t tab_id = web_state_id.identifier();
  switch (action.action_case()) {
    case optimization_guide::proto::Action::kNavigate:
      action.mutable_navigate()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kClick:
      action.mutable_click()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kBack:
      action.mutable_back()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kForward:
      action.mutable_forward()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kSelect:
      action.mutable_select()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kDragAndRelease:
      action.mutable_drag_and_release()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kType:
      action.mutable_type()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kWait:
      action.mutable_wait()->set_observe_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kScroll:
      action.mutable_scroll()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kScrollTo:
      action.mutable_scroll_to()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kAttemptLogin:
      action.mutable_attempt_login()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kAttemptFormFilling:
      action.mutable_attempt_form_filling()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kCloseTab:
      action.mutable_close_tab()->set_tab_id(tab_id);
      break;
    case optimization_guide::proto::Action::kCreateTab:
      if (window_id.is_valid()) {
        action.mutable_create_tab()->set_window_id(window_id.id());
      }
      break;
    case optimization_guide::proto::Action::kActivateTab:
      action.mutable_activate_tab()->set_tab_id(tab_id);
      break;
    default:
      break;
  }
}
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.mm:CreateTool)

// Parses serialized action protos from `request` and injects session data.
std::optional<std::vector<optimization_guide::proto::Action>>
ParseActionsFromRequest(GeminiActuationRequest* request,
                        web::WebStateID webStateID,
                        SessionID windowId) {
  if (!request.actionProtos) {
    return std::nullopt;
  }
  std::vector<optimization_guide::proto::Action> actions;
  actions.reserve(request.actionProtos.count);
  for (NSData* data in request.actionProtos) {
    optimization_guide::proto::Action action;
    if (!action.ParseFromArray([data bytes], [data length])) {
      return std::nullopt;
    }
    InjectDataIntoAction(action, webStateID, windowId);
    actions.push_back(action);
  }
  return actions;
}

}  // namespace

@interface GeminiActuationHandler () <ActorTaskUpdatesObserver>
@end

@implementation GeminiActuationHandler {
  // The ActorService to use for actuating tasks.
  raw_ptr<actor::ActorService> _actorService;

  // The WebStateList to obtain the active WebState.
  raw_ptr<WebStateList> _webStateList;

  // The Browser ID. We use std::optional here because SessionID is not
  // default-constructible and cannot be declared as an Objective-C instance
  // variable directly.
  std::optional<SessionID> _browserId;

  // Map from task IDs to WebState IDs.
  std::map<actor::ActorTaskId, web::WebStateID> _taskToWebStateIDMap;

  // Active callbacks awaiting completion, keyed by task ID.
  std::map<actor::ActorTaskId, ActuationCallback> _activeCallbacks;

  // Tasks whose callback in `_activeCallbacks` is pending the user's answer to
  // a confirmation prompt. A confirmation yield is answered in steps:
  // 1. `-handleConfirmationYieldWithTaskID:...` stores the callback and asks
  //    `ActorService` to interrupt the task with the prompt.
  // 2. Once the user accepts,
  //    `-actorTaskDidResolveConfirmationInterruptWithID:` requests a fresh
  //    observation of the actuated tab. Only tasks in this set are handled,
  //    since other clients of `ActorService` may interrupt.
  // 3. `-completeConfirmationForTaskID:webStateID:pageContextResponse:` builds
  //    the response and `-resolveConfirmationForTaskID:withResponse:` answers
  //    the callback.
  // A task stop or `-disconnect` answers the pending callback instead.
  std::set<actor::ActorTaskId> _confirmationTaskIDs;
}

#pragma mark - Public

- (instancetype)initWithActorService:(actor::ActorService*)actorService
                        webStateList:(WebStateList*)webStateList
                           browserId:(SessionID)browserId {
  self = [super init];
  if (self) {
    _actorService = actorService;
    _webStateList = webStateList;
    _browserId = browserId;
    if (_actorService) {
      _actorService->AddTaskUpdatesObserver(self);
    }
  }
  return self;
}

- (void)disconnect {
  if (_actorService) {
    _actorService->RemoveTaskUpdatesObserver(self);
    for (const auto& [taskID, _] : _taskToWebStateIDMap) {
      _actorService->StopTask(taskID, actor::ActorTaskStoppedReason::kShutdown);
    }
    _actorService = nullptr;
  }
  _webStateList = nullptr;
  _confirmationTaskIDs.clear();
  std::map<actor::ActorTaskId, ActuationCallback> callbacks =
      std::exchange(_activeCallbacks, {});
  for (auto& [taskID, callback] : callbacks) {
    if (callback) {
      std::move(callback).Run([[GeminiActuationResponse alloc]
          initWithResultCode:actor::mojom::ActionResultCode::kExecutorDestroyed
                errorMessage:"Session disconnected."]);
    }
  }
  _taskToWebStateIDMap.clear();
}

- (void)dealloc {
  if (_actorService) {
    _actorService->RemoveTaskUpdatesObserver(self);
  }
}

#pragma mark - GeminiChatMessageHandler

- (void)handleChatMessageRequest:(GeminiChatMessageRequest*)request
                      completion:(void (^)(GeminiChatMessageResponse* response))
                                     completion {
  // TODO(crbug.com/567037227): Answer pending clarification requests.
  // TODO(crbug.com/567036739): Stop paused tasks before sending.
  completion([[GeminiChatMessageResponse alloc] initWithShouldConsume:NO]);
}

#pragma mark - GeminiActuationDelegate

- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title {
  actor::ActorTaskId taskID = actor::ActorTaskId();
  if (!_webStateList || !_actorService) {
    return taskID;
  }

  // TODO(crbug.com/510404682): Don't use the active WebState, instead get the
  // tab ID from the Gemini SDK.
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return taskID;
  }

  // TODO(crbug.com/565875367): Set the conversation ID as the source ID.
  taskID = _actorService->CreateTask(
      base::SysNSStringToUTF8(title),
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic,
                            /*id=*/std::nullopt),
      /*allow_incognito_web_states=*/false);
  _actorService->AddControlledWebState(taskID, activeWebState);
  _taskToWebStateIDMap[taskID] = activeWebState->GetUniqueIdentifier();
  return taskID;
}

- (void)dispatchActuationRequest:(GeminiActuationRequest*)request
                       forTaskID:(actor::ActorTaskId)taskID
                 completionBlock:(void (^)(GeminiActuationResponse* response))
                                     completionBlock {
  CHECK(request);
  CHECK(completionBlock);

  if (!_actorService ||
      _taskToWebStateIDMap.find(taskID) == _taskToWebStateIDMap.end()) {
    completionBlock([[GeminiActuationResponse alloc]
        initWithResultCode:actor::mojom::ActionResultCode::kTaskWentAway
              errorMessage:"Task does not exist or has been stopped."]);
    return;
  }

  // Validate that actionProtos and yieldAction are mutually exclusive.
  const bool hasActions = request.actionProtos != nil;
  const bool hasYield = request.yieldAction != nil;
  if (hasActions == hasYield) {
    // TODO(crbug.com/556739755): Add monitoring for invalid actuation requests.
    completionBlock([[GeminiActuationResponse alloc]
        initWithResultCode:actor::mojom::ActionResultCode::kArgumentsInvalid
              errorMessage:"Invalid actuation request: actionProtos and "
                           "yieldAction are mutually exclusive."]);
    return;
  }

  // Route the request based on whether it is a yield action or action
  // execution.
  GeminiYieldAction* yieldAction = request.yieldAction;
  if (yieldAction) {
    switch (yieldAction.reason) {
      case GeminiYieldReason::kConfirmation:
        [self handleConfirmationYieldWithTaskID:taskID
                                    yieldAction:yieldAction
                                completionBlock:completionBlock];
        break;
      case GeminiYieldReason::kClarification:
        [self handleInterruptTaskWithID:taskID
                                 reason:actor::ActorTaskInterruptReason::
                                            kWaitingUserClarification
                            yieldAction:yieldAction
                        completionBlock:completionBlock];
        break;
      case GeminiYieldReason::kUserTakeover:
        [self handleInterruptTaskWithID:taskID
                                 reason:actor::ActorTaskInterruptReason::
                                            kWaitingUserTakeover
                            yieldAction:yieldAction
                        completionBlock:completionBlock];
        break;
      case GeminiYieldReason::kTaskComplete:
        [self handleStopTaskWithID:taskID
                            reason:actor::ActorTaskStoppedReason::kTaskComplete
                   completionBlock:completionBlock];
        break;
      case GeminiYieldReason::kIrrelevantUserInput:
        // TODO(crbug.com/559737665): Track Desktop experiment to pause or
        // re-prompt rather than stopping the task with a model error.
        [self handleStopTaskWithID:taskID
                            reason:actor::ActorTaskStoppedReason::kModelError
                   completionBlock:completionBlock];
        break;
      case GeminiYieldReason::kUnknownReason:
        [self handlePauseTaskWithID:taskID completionBlock:completionBlock];
        break;
    }
    return;
  }

  [self handlePerformActionsWithTaskID:taskID
                               request:request
                       completionBlock:completionBlock];
}

- (void)addTaskUpdatesObserver:(id<ActorTaskUpdatesObserver>)observer
                     forTaskID:(actor::ActorTaskId)taskID {
  // TODO(crbug.com/496163970): Implement and test.
}

- (void)setTaskInterventionDelegate:(id<ActorTaskInterventionDelegate>)delegate
                          forTaskID:(actor::ActorTaskId)taskID {
  // TODO(crbug.com/496163970): Implement and test.
}

#pragma mark - ActorTaskUpdatesObserver

// Aborts any pending request callback when a task stops externally.
- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  _confirmationTaskIDs.erase(taskID);
  auto it = _activeCallbacks.find(taskID);
  if (it != _activeCallbacks.end()) {
    ActuationCallback callback = std::move(it->second);
    _activeCallbacks.erase(it);
    std::move(callback).Run([[GeminiActuationResponse alloc]
        initWithResultCode:actor::mojom::ActionResultCode::kTaskWentAway
              errorMessage:"Task was stopped."]);
  }
  _taskToWebStateIDMap.erase(taskID);
}

// Resolves a pending confirmation request by capturing a fresh observation of
// the actuated tab. Step 2 of the confirmation flow described at
// `_confirmationTaskIDs`.
- (void)actorTaskDidResolveConfirmationInterruptWithID:
    (actor::ActorTaskId)taskID {
  // Early-return rather than CHECK: the interrupt may have been raised by
  // another client of `ActorService`.
  if (!_confirmationTaskIDs.contains(taskID)) {
    return;
  }
  // `-disconnect` clears both `_confirmationTaskIDs` and `_actorService`, so a
  // pending confirmation implies a live service.
  CHECK(_actorService);

  const web::WebStateID webStateID = [self webStateIDForTaskID:taskID];
  web::WebState* webState =
      webStateID.valid() ? _actorService->GetWebStateForID(webStateID, taskID)
                         : nullptr;
  if (!webState) {
    // TODO(crbug.com/510404682): Handle tab closure during task execution
    // gracefully rather than failing the request.
    [self resolveConfirmationForTaskID:taskID
                          withResponse:MakeTabWentAwayResponse()];
    return;
  }

  __weak GeminiActuationHandler* weakSelf = self;
  _actorService->RequestTabObservation(
      taskID, webState,
      base::BindOnce(^(PageContextWrapperCallbackResponse response) {
        [weakSelf completeConfirmationForTaskID:taskID
                                     webStateID:webStateID
                            pageContextResponse:std::move(response)];
      }));
}

#pragma mark - Private

// Executes actions on the task's controlled WebState.
- (void)handlePerformActionsWithTaskID:(actor::ActorTaskId)taskID
                               request:(GeminiActuationRequest*)request
                       completionBlock:
                           (void (^)(GeminiActuationResponse*))completionBlock {
  web::WebStateID webStateID = [self webStateIDForTaskID:taskID];
  if (!webStateID.valid() ||
      !_actorService->GetWebStateForID(webStateID, taskID)) {
    // TODO(crbug.com/510404682): Handle tab closure during task execution
    // gracefully rather than failing the request.
    if (completionBlock) {
      completionBlock(MakeTabWentAwayResponse());
    }
    return;
  }

  std::optional<std::vector<optimization_guide::proto::Action>> actions =
      ParseActionsFromRequest(request, webStateID,
                              _browserId.value_or(SessionID::InvalidValue()));
  if (!actions.has_value()) {
    if (completionBlock) {
      completionBlock([[GeminiActuationResponse alloc]
          initWithResultCode:actor::mojom::ActionResultCode::kArgumentsInvalid
                errorMessage:"Failed to parse action proto"]);
    }
    return;
  }

  if (_activeCallbacks.find(taskID) != _activeCallbacks.end()) {
    // TODO(crbug.com/556739755): Add monitoring for concurrent actuation
    // requests.
    if (completionBlock) {
      completionBlock(MakeRequestInProgressResponse());
    }
    return;
  }

  _activeCallbacks[taskID] = base::BindOnce(completionBlock);

  __weak GeminiActuationHandler* weakSelf = self;
  auto actionsCallback = base::BindOnce(^(actor::PerformActionsResult result) {
    [weakSelf handlePerformActionsResult:result taskID:taskID];
  });

  _actorService->PerformActions(taskID, std::move(*actions),
                                base::SysNSStringToUTF8(request.taskUpdate),
                                std::move(actionsCallback));
}

// Interrupts the task to prompt the user with `yieldAction.messageToUser` and
// holds `completionBlock` until the user answers the prompt.
- (void)handleConfirmationYieldWithTaskID:(actor::ActorTaskId)taskID
                              yieldAction:(GeminiYieldAction*)yieldAction
                          completionBlock:(void (^)(GeminiActuationResponse*))
                                              completionBlock {
  CHECK(yieldAction);
  // `messageToUser` is optional, but `ActorTask` stops a task asked to confirm
  // without a message, so report a nil or empty message as malformed instead.
  if (yieldAction.messageToUser.length == 0) {
    completionBlock([[GeminiActuationResponse alloc]
        initWithResultCode:actor::mojom::ActionResultCode::kArgumentsInvalid
              errorMessage:"Confirmation yield has no message."]);
    return;
  }

  web::WebStateID webStateID = [self webStateIDForTaskID:taskID];
  if (!webStateID.valid() ||
      !_actorService->GetWebStateForID(webStateID, taskID)) {
    // TODO(crbug.com/510404682): Handle tab closure during task execution
    // gracefully rather than failing the request.
    completionBlock(MakeTabWentAwayResponse());
    return;
  }

  // Replacing a pending callback would drop it unanswered.
  // TODO(crbug.com/556739755): Add monitoring for concurrent actuation
  // requests.
  if (_activeCallbacks.contains(taskID)) {
    completionBlock(MakeRequestInProgressResponse());
    return;
  }

  // Held before `InterruptTask`, which stops the task when no intervention
  // delegate is registered; the posted stop notification then answers the
  // request with `kTaskWentAway`.
  // TODO(crbug.com/548051839): An interrupt ignored because of the task's
  // current state leaves the request pending until the task stops or the
  // handler disconnects. Resolve it from the interrupt result instead.
  // TODO(crbug.com/567035844): Resolve the request when the user declines or
  // the prompt is dismissed unanswered.
  _activeCallbacks[taskID] = base::BindOnce(completionBlock);
  _confirmationTaskIDs.insert(taskID);
  _actorService->InterruptTask(
      taskID, actor::ActorTaskInterruptReason::kWaitingUserConfirmation,
      base::SysNSStringToUTF8(yieldAction.messageToUser));
}

// Resolves the pending confirmation for `taskID` with the user's acceptance and
// `pageContextResponse`, the fresh observation of the tab `webStateID`.
- (void)completeConfirmationForTaskID:(actor::ActorTaskId)taskID
                           webStateID:(web::WebStateID)webStateID
                  pageContextResponse:
                      (PageContextWrapperCallbackResponse)pageContextResponse {
  const actor::TabObservationResponse observation(
      webStateID, std::move(pageContextResponse), /*web_state_exists=*/true);
  optimization_guide::proto::ActionsResult actionsResult;
  actionsResult.set_action_result(
      static_cast<int32_t>(actor::mojom::ActionResultCode::kOk));
  PopulateTabObservationFromResponse(actionsResult.add_tabs(), observation);

  GeminiActuationResponse* response = [[GeminiActuationResponse alloc]
           initWithResultCode:actor::mojom::ActionResultCode::kOk
                 userResponse:kConfirmationUserResponse
      serializedActionsResult:SerializeProtoToNSData(actionsResult)];
  [self resolveConfirmationForTaskID:taskID withResponse:response];
}

// Answers the pending confirmation for `taskID` with `response`. No-op if the
// confirmation was already answered, e.g. by a task stop.
- (void)resolveConfirmationForTaskID:(actor::ActorTaskId)taskID
                        withResponse:(GeminiActuationResponse*)response {
  if (!_confirmationTaskIDs.erase(taskID)) {
    return;
  }
  auto it = _activeCallbacks.find(taskID);
  CHECK(it != _activeCallbacks.end());
  ActuationCallback callback = std::move(it->second);
  _activeCallbacks.erase(it);
  std::move(callback).Run(response);
}

// Interrupts the task with `reason` for user intervention and unblocks the
// caller.
- (void)handleInterruptTaskWithID:(actor::ActorTaskId)taskID
                           reason:(actor::ActorTaskInterruptReason)reason
                      yieldAction:(GeminiYieldAction*)yieldAction
                  completionBlock:
                      (void (^)(GeminiActuationResponse*))completionBlock {
  CHECK(yieldAction);
  // TODO(crbug.com/556739755): Wire up `yieldAction.messageToUser` to the user
  // intervention prompt / UI flow when intervention UI is integrated.
  _actorService->InterruptTask(taskID, reason);
  if (completionBlock) {
    completionBlock([[GeminiActuationResponse alloc]
             initWithResultCode:actor::mojom::ActionResultCode::kOk
                   userResponse:nil
        serializedActionsResult:nil]);
  }
}

// Stops the task and invokes `completionBlock`.
// `actorTaskDidStopWithID:` is posted, so the task is forgotten here directly
// to reject follow-up requests with `kTaskWentAway`. Any in-flight request is
// still completed by `actorTaskDidStopWithID:`.
- (void)handleStopTaskWithID:(actor::ActorTaskId)taskID
                      reason:(actor::ActorTaskStoppedReason)reason
             completionBlock:
                 (void (^)(GeminiActuationResponse*))completionBlock {
  _actorService->StopTask(taskID, reason);
  _taskToWebStateIDMap.erase(taskID);
  if (completionBlock) {
    completionBlock([[GeminiActuationResponse alloc]
             initWithResultCode:actor::mojom::ActionResultCode::kOk
                   userResponse:nil
        serializedActionsResult:nil]);
  }
}

// Pauses the task and immediately invokes `completionBlock` with `kOk`.
- (void)handlePauseTaskWithID:(actor::ActorTaskId)taskID
              completionBlock:
                  (void (^)(GeminiActuationResponse*))completionBlock {
  _actorService->PauseTask(taskID, /*from_actor=*/true);
  if (completionBlock) {
    completionBlock([[GeminiActuationResponse alloc]
             initWithResultCode:actor::mojom::ActionResultCode::kOk
                   userResponse:nil
        serializedActionsResult:nil]);
  }
}

// Handles the results of action execution for `taskID`, populates the
// GeminiActuationResponse, and invokes the active callback.
- (void)handlePerformActionsResult:(const actor::PerformActionsResult&)result
                            taskID:(actor::ActorTaskId)taskID {
  actor::mojom::ActionResultCode resultCode;
  optimization_guide::proto::ActionsResult actionsResult =
      ActionsResultFromPerformActionsResult(result, resultCode);
  NSData* data = SerializeProtoToNSData(actionsResult);
  GeminiActuationResponse* response =
      [[GeminiActuationResponse alloc] initWithResultCode:resultCode
                                             userResponse:nil
                                  serializedActionsResult:data];
  auto it = _activeCallbacks.find(taskID);
  if (it != _activeCallbacks.end()) {
    ActuationCallback callback = std::move(it->second);
    _activeCallbacks.erase(it);
    std::move(callback).Run(response);
  }
}

// TODO(crbug.com/559608376): Query the active WebStateID directly from
// ActorService rather than storing a static map, to support dynamic tab changes
// during actuation.
- (web::WebStateID)webStateIDForTaskID:(actor::ActorTaskId)taskID {
  auto it = _taskToWebStateIDMap.find(taskID);
  if (it == _taskToWebStateIDMap.end()) {
    return web::WebStateID();
  }
  return it->second;
}

@end
