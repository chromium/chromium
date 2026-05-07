// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"

#import <map>
#import <string>
#import <vector>

#import "base/barrier_callback.h"
#import "base/base64.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {

// The MIME type for PNG screenshots.
const char kPNGMimeType[] = "image/png";

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

// Processes the responses from PageContextWrapper by populating the
// TabObservation protos, serializes them, and calls the completion block.
void ProcessContextsAndComplete(
    std::vector<std::unique_ptr<actor::TabObservationResponse>> responses,
    void (^completionBlock)(NSArray<NSData*>*)) {
  if (!completionBlock) {
    return;
  }

  NSMutableArray<NSData*>* serializedTabObservations = [NSMutableArray array];
  for (const auto& response : responses) {
    optimization_guide::proto::TabObservation tabObservation;
    PopulateTabObservationFromResponse(&tabObservation, *response);
    [serializedTabObservations
        addObject:SerializeProtoToNSData(tabObservation)];
  }

  completionBlock(serializedTabObservations);
}

// Creates a serialized ActionsResult representing a failure.
NSData* CreateSerializedFailureActionsResult(const std::string& error_message) {
  optimization_guide::proto::ActionsResult actionsResult;
  actionsResult.set_action_result(actor::kActionResultFailure);
  actionsResult.set_error_message(error_message);
  return SerializeProtoToNSData(actionsResult);
}

// Injects the tab ID into the given action depending on its case.
// LINT.IfChange(InjectTabIdIntoAction)
void InjectTabIdIntoAction(optimization_guide::proto::Action& action,
                           web::WebStateID web_state_id) {
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
    default:
      break;
  }
}
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.mm:CreateTool)

}  // namespace

@implementation GeminiActuationHandler {
  // The ActorService to use for actuating tasks.
  raw_ptr<actor::ActorService> _actorService;

  // The WebStateList to obtain the active WebState.
  raw_ptr<WebStateList> _webStateList;

  // Map from task IDs to WebState IDs.
  std::map<actor::ActorTaskId, web::WebStateID> _taskToWebStateIDMap;
}

- (instancetype)initWithActorService:(actor::ActorService*)actorService
                        webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _actorService = actorService;
    _webStateList = webStateList;
  }
  return self;
}

#pragma mark - Private

// Handles the results of action execution, populates the ActionsResult proto,
// and invokes the completion block with the serialized proto.
- (void)handleActionResults:(actor::PerformActionsResult)result
                     taskID:(actor::ActorTaskId)taskID
            completionBlock:(void (^)(NSData*))completionBlock {
  if (!completionBlock) {
    return;
  }

  optimization_guide::proto::ActionsResult actionsResult;

  // Populate action results.
  bool overallSuccess = true;
  int32_t failedActionIndex = -1;
  for (size_t i = 0; i < result.action_results.size(); ++i) {
    const auto& actionResult = result.action_results[i];
    if (!actionResult.tool_result.IsOk()) {
      overallSuccess = false;
      failedActionIndex = i;
      actionsResult.set_error_message(
          actor::GetToolExecutionResultMessage(actionResult.tool_result));
      break;
    }
  }

  actionsResult.set_action_result(overallSuccess ? actor::kActionResultSuccess
                                                 : actor::kActionResultFailure);
  if (failedActionIndex != -1) {
    actionsResult.set_index_of_failed_action(failedActionIndex);
  }

  // Populate tab observations.
  for (const auto& observationResponse : result.page_contexts) {
    auto* tabObservationMessage = actionsResult.add_tabs();
    PopulateTabObservationFromResponse(tabObservationMessage,
                                       *observationResponse);
  }

  // TODO(crbug.com/504704411): Populate WindowObservation here.

  NSData* data = SerializeProtoToNSData(actionsResult);
  completionBlock(data);
}

#pragma mark - GeminiActuationDelegate

- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title {
  actor::ActorTaskId taskID = actor::ActorTaskId();
  if (!_webStateList) {
    return taskID;
  }

  // TODO(crbug.com/510404682): Don't use the active WebState, instead get the
  // tab ID from the Gemini SDK.
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return taskID;
  }

  taskID = _actorService->CreateTask(base::SysNSStringToUTF8(title),
                                     /*allow_incognito_web_states=*/false);
  _taskToWebStateIDMap[taskID] = activeWebState->GetUniqueIdentifier();
  return taskID;
}

- (void)addTaskUpdatesObserver:(id<ActorTaskUpdatesObserver>)observer
                     forTaskID:(actor::ActorTaskId)taskID {
  // TODO(crbug.com/496163970): Implement and test.
}

- (void)setTaskInterventionDelegate:(id<ActorTaskInterventionDelegate>)delegate
                          forTaskID:(actor::ActorTaskId)taskID {
  // TODO(crbug.com/496163970): Implement and test.
}

- (void)performActionsWithTaskID:(actor::ActorTaskId)taskID
                      taskUpdate:(NSString*)taskUpdate
          serializedActionProtos:(NSArray<NSData*>*)serializedActionProtos
                 completionBlock:(void (^)(NSData* serializedActionsResult))
                                     completionBlock {
  if (!completionBlock) {
    return;
  }

  auto it = _taskToWebStateIDMap.find(taskID);
  if (it == _taskToWebStateIDMap.end()) {
    completionBlock(CreateSerializedFailureActionsResult(
        "Failed to perform actions: Task ID not found."));
    return;
  }
  web::WebStateID webStateId = it->second;

  std::vector<optimization_guide::proto::Action> actions;
  for (NSData* data in serializedActionProtos) {
    optimization_guide::proto::Action action;
    if (!action.ParseFromArray([data bytes], [data length])) {
      completionBlock(
          CreateSerializedFailureActionsResult("Failed to parse action proto"));
      return;
    }
    InjectTabIdIntoAction(action, webStateId);
    actions.push_back(action);
  }

  auto toolsResult = _actorService->CreateActorTools(actions, taskID);
  if (!toolsResult.has_value()) {
    completionBlock(CreateSerializedFailureActionsResult(
        actor::GetToolExecutionResultMessage(toolsResult.error())));
    return;
  }

  __weak GeminiActuationHandler* weakSelf = self;
  _actorService->PerformActions(
      taskID, std::move(toolsResult.value()),
      base::SysNSStringToUTF8(taskUpdate),
      base::BindOnce(
          [](__weak GeminiActuationHandler* weakSelf, actor::ActorTaskId taskID,
             void (^completionBlock)(NSData*),
             actor::PerformActionsResult result) {
            GeminiActuationHandler* strongSelf = weakSelf;
            if (!strongSelf) {
              if (completionBlock) {
                completionBlock(CreateSerializedFailureActionsResult(
                    "Handler destroyed before actions completed"));
              }
              return;
            }
            [strongSelf handleActionResults:std::move(result)
                                     taskID:taskID
                            completionBlock:completionBlock];
          },
          weakSelf, taskID, completionBlock));
}

- (void)requestActionablePageContextForWebStateIDs:
            (NSArray<NSNumber*>*)webStateIDs
                                            taskID:(actor::ActorTaskId)taskID
                                   completionBlock:
                                       (void (^)(NSArray<NSData*>*
                                                     serializedTabObservations))
                                           completionBlock {
  if (!completionBlock) {
    return;
  }

  if ([webStateIDs count] == 0) {
    completionBlock(@[]);
    return;
  }

  NSSet<NSNumber*>* uniqueWebStateIDs = [NSSet setWithArray:webStateIDs];

  auto barrier =
      base::BarrierCallback<std::unique_ptr<actor::TabObservationResponse>>(
          [uniqueWebStateIDs count],
          base::BindOnce(
              [](void (^completionBlock)(NSArray<NSData*>*),
                 std::vector<std::unique_ptr<actor::TabObservationResponse>>
                     responses) {
                ProcessContextsAndComplete(std::move(responses),
                                           completionBlock);
              },
              completionBlock));

  for (NSNumber* nsId in uniqueWebStateIDs) {
    web::WebStateID webStateId =
        web::WebStateID::FromSerializedValue([nsId intValue]);
    web::WebState* webState =
        _actorService->GetWebStateForID(webStateId, taskID);
    if (webState) {
      _actorService->RequestTabObservation(
          taskID, webState,
          base::BindOnce(
              [](web::WebStateID webStateId,
                 base::RepeatingCallback<void(
                     std::unique_ptr<actor::TabObservationResponse>)> barrier,
                 PageContextWrapperCallbackResponse response) {
                barrier.Run(std::make_unique<actor::TabObservationResponse>(
                    webStateId, std::move(response), true));
              },
              webStateId, barrier));
    } else {
      barrier.Run(std::make_unique<actor::TabObservationResponse>(
          webStateId, base::unexpected(PageContextWrapperError::kGenericError),
          false));
    }
  }
}

- (void)pauseTaskWithID:(actor::ActorTaskId)taskID {
  _actorService->PauseTask(taskID, /*from_actor=*/true);
}

- (void)stopTaskWithID:(actor::ActorTaskId)taskID
                reason:(actor::ActorTaskStoppedReason)reason {
  _actorService->StopTask(taskID, reason);
  _taskToWebStateIDMap.erase(taskID);
}

@end
