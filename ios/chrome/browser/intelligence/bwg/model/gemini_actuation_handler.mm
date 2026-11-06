// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"

#import <map>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/base64.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
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
#import "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#import "url/gurl.h"
#import "url/origin.h"

@interface GeminiActuationHandler () <ActorTaskUpdatesObserver>

// Forgets `taskID` if `response` says it went away, so that follow-up requests
// are rejected.
- (void)forgetTaskIfWentAway:(actor::ActorTaskId)taskID
                withResponse:(GeminiActuationResponse*)response;

@end

namespace {

// The MIME type for PNG screenshots.
constexpr char kPNGMimeType[] = "image/png";

// Returned to the SDK as the user's answer once a confirmation prompt has been
// accepted. Acceptance is the only answer the prompt can report for now.
// TODO(crbug.com/571050191): Get the string approved and decide whether it
// should be localized.
NSString* const kConfirmationUserResponse =
    @"I agree with what you wanted me to confirm. Please proceed.";

// Title of confirmation prompts.
// TODO(crbug.com/573515833): Localize the title.
constexpr char kConfirmationTitle[] = "Input needed";

// Label of the confirming button of confirmation prompts.
// TODO(crbug.com/573515833): Localize the button text.
constexpr char kConfirmationButtonText[] = "Continue";

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

// Returned when the task's actuated tab no longer exists.
GeminiActuationResponse* MakeTabWentAwayResponse() {
  return [[GeminiActuationResponse alloc]
      initWithResultCode:actor::mojom::ActionResultCode::kTabWentAway
            errorMessage:"The actuated tab is no longer available."];
}

// Returned when the yield reason is not supported yet.
GeminiActuationResponse* MakeUnsupportedYieldResponse() {
  return [[GeminiActuationResponse alloc]
      initWithResultCode:actor::mojom::ActionResultCode::kNotImplemented
            errorMessage:"The yield reason is not supported yet."];
}

// Returned when the task is stopped or no longer exists.
GeminiActuationResponse* MakeTaskWentAwayResponse() {
  return [[GeminiActuationResponse alloc]
      initWithResultCode:actor::mojom::ActionResultCode::kTaskWentAway
            errorMessage:"Task was stopped."];
}

// Returns the response for a request that failed with `error`.
GeminiActuationResponse* MakeErrorResponse(actor::ActorTaskError error) {
  switch (error) {
    case actor::ActorTaskError::kRejected:
      // TODO(crbug.com/578539528): Map rejection causes to dedicated result
      // codes and error messages.
      return [[GeminiActuationResponse alloc]
          initWithResultCode:actor::mojom::ActionResultCode::
                                 kInvalidTaskStateForAct
                errorMessage:"Task rejected the request."];
    case actor::ActorTaskError::kTaskWentAway:
      return MakeTaskWentAwayResponse();
  }
  NOTREACHED();
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

// Returns the response for an accepted confirmation, carrying `observation` of
// the actuated tab.
GeminiActuationResponse* MakeConfirmedResponse(
    const actor::TabObservationResponse* observation) {
  if (!observation) {
    // TODO(crbug.com/510404682): Handle tab closure during task execution
    // gracefully rather than failing the request.
    return MakeTabWentAwayResponse();
  }
  optimization_guide::proto::ActionsResult actions_result;
  actions_result.set_action_result(
      static_cast<int32_t>(actor::mojom::ActionResultCode::kOk));
  PopulateTabObservationFromResponse(actions_result.add_tabs(), *observation);
  return [[GeminiActuationResponse alloc]
           initWithResultCode:actor::mojom::ActionResultCode::kOk
                 userResponse:kConfirmationUserResponse
      serializedActionsResult:SerializeProtoToNSData(actions_result)];
}

// Returns the response for a successful confirmation interrupt.
GeminiActuationResponse* MakeConfirmationResponse(
    const actor::InterruptTaskResult& result) {
  switch (result.response) {
    case actor::InterruptResponse::kConfirmed:
      return MakeConfirmedResponse(result.observation.get());
  }
  NOTREACHED();
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

// Returns the response for the `result` of an act.
GeminiActuationResponse* MakePerformActionsResponse(
    const actor::PerformActionsResult& result) {
  actor::mojom::ActionResultCode result_code;
  optimization_guide::proto::ActionsResult actions_result =
      ActionsResultFromPerformActionsResult(result, result_code);
  return [[GeminiActuationResponse alloc]
           initWithResultCode:result_code
                 userResponse:nil
      serializedActionsResult:SerializeProtoToNSData(actions_result)];
}

// Returns the response for the `result` of a confirmation interrupt.
GeminiActuationResponse* MakeConfirmationResultResponse(
    const actor::ActorTaskResult<actor::InterruptTaskResult>& result) {
  if (!result.has_value()) {
    return MakeErrorResponse(result.error());
  }
  return MakeConfirmationResponse(result.value());
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

// Answers `completion_block` with `response`, forgetting the task on `handler`
// if it went away. Not a method of `handler`, as `handler` is nil when
// `disconnect` stopped the task: the posted `kTaskWentAway` reply runs after
// the handler is deallocated. The cleanup is then skipped, but the SDK still
// gets its answer.
void AnswerRequest(GeminiActuationHandler* handler,
                   actor::ActorTaskId task_id,
                   void (^completion_block)(GeminiActuationResponse*),
                   GeminiActuationResponse* response) {
  [handler forgetTaskIfWentAway:task_id withResponse:response];
  if (completion_block) {
    completion_block(response);
  }
}
// Parses serialized origin strings from `origin_strings` into valid, non-opaque
// `url::Origin` instances.
absl::flat_hash_set<url::Origin> ParseWritableMainframeOrigins(
    NSArray<NSString*>* origin_strings) {
  absl::flat_hash_set<url::Origin> origins;
  for (NSString* origin_string in origin_strings) {
    GURL url(base::SysNSStringToUTF8(origin_string));
    if (!url.is_valid()) {
      continue;
    }
    url::Origin origin = url::Origin::Create(url);
    if (!origin.opaque()) {
      origins.insert(std::move(origin));
    }
  }
  return origins;
}

}  // namespace

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
    // TODO(crbug.com/582694320): Add monitoring for invalid actuation requests.
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
      case GeminiYieldReason::kUserTakeover:
      case GeminiYieldReason::kUnknownReason:
        // TODO(crbug.com/567097523): Interrupt and prompt the user for
        // clarification once `ActorTask` supports it.
        // TODO(crbug.com/567096140): Interrupt and hand control to the user
        // once `ActorTask` supports takeover.
        // TODO(crbug.com/567040651): Pause the task until the user resumes it
        // once `ActorTask` supports pausing.
        completionBlock(MakeUnsupportedYieldResponse());
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

// Forgets tasks stopped elsewhere, so that follow-up requests are rejected.
- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  _taskToWebStateIDMap.erase(taskID);
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

  // TODO(crbug.com/568339898): Reject requests sent while the previous reply's
  // tab observation runs, once `ActorTask` covers it.
  __weak GeminiActuationHandler* weakSelf = self;
  auto actionsCallback = base::BindOnce(^(actor::PerformActionsResult result) {
    AnswerRequest(weakSelf, taskID, completionBlock,
                  MakePerformActionsResponse(result));
  });

  _actorService->PerformActions(
      taskID, std::move(*actions), base::SysNSStringToUTF8(request.taskUpdate),
      ParseWritableMainframeOrigins(request.addedWritableMainframeOrigins),
      std::move(actionsCallback));
}

// Asks the user to confirm `yieldAction.messageToUser` and answers with their
// choice.
- (void)handleConfirmationYieldWithTaskID:(actor::ActorTaskId)taskID
                              yieldAction:(GeminiYieldAction*)yieldAction
                          completionBlock:(void (^)(GeminiActuationResponse*))
                                              completionBlock {
  CHECK(yieldAction);
  // A missing `messageToUser` is considered malformed for an intervention.
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

  actor::ConfirmationInterrupt confirmation;
  confirmation.title = kConfirmationTitle;
  confirmation.subtitle = base::SysNSStringToUTF8(yieldAction.messageToUser);
  confirmation.button_text = kConfirmationButtonText;
  // TODO(crbug.com/568339898): Reject requests sent while the previous reply's
  // tab observation runs, once `ActorTask` owns observations.
  __weak GeminiActuationHandler* weakSelf = self;
  _actorService->InterruptTask(
      taskID, std::move(confirmation),
      base::BindOnce(
          ^(actor::ActorTaskResult<actor::InterruptTaskResult> result) {
            AnswerRequest(weakSelf, taskID, completionBlock,
                          MakeConfirmationResultResponse(result));
          }));
}

- (void)forgetTaskIfWentAway:(actor::ActorTaskId)taskID
                withResponse:(GeminiActuationResponse*)response {
  if (response.resultCode == actor::mojom::ActionResultCode::kTaskWentAway) {
    _taskToWebStateIDMap.erase(taskID);
  }
}

// Stops the task and invokes `completionBlock`.
// `actorTaskDidStopWithID:` is posted, so the task is forgotten here directly
// to reject follow-up requests with `kTaskWentAway`. Any in-flight request is
// still answered with `kTaskWentAway`.
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
