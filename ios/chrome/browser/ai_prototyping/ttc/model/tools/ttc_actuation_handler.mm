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
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/sessions/core/session_id.h"
#import "components/ttc/app/public/tool_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {

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

// Default intervention delegate for TTC tasks.
@interface TtcTaskInterventionHandler : NSObject <ActorTaskInterventionDelegate>
@end

#pragma mark - Initializers

TtcActuationHandler::TtcActuationHandler(actor::ActorService* actor_service,
                                         WebStateList* web_state_list,
                                         SessionID browser_id)
    : intervention_delegate_([[TtcTaskInterventionHandler alloc] init]) {
  if (actor_service && web_state_list && browser_id.is_valid()) {
    actor_service_ = actor_service;
    web_state_list_ = web_state_list;
    browser_id_ = browser_id;
  }
}

TtcActuationHandler::~TtcActuationHandler() {
  Disconnect();
}

#pragma mark - Properties

TTCJournal* TtcActuationHandler::journal() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return journal_.get();
}

void TtcActuationHandler::set_journal(TTCJournal* journal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  journal_ = journal ? journal->GetWeakPtr() : nullptr;
}

#pragma mark - Teardown

void TtcActuationHandler::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_ptr_factory_.InvalidateWeakPtrs();
  web_state_list_ = nullptr;
  browser_id_.reset();
  intervention_delegate_ = nil;

  if (actor_service_) {
    for (const auto& [task_id, _] : task_to_web_state_id_map_) {
      actor_service_->StopTask(task_id,
                               actor::ActorTaskStoppedReason::kShutdown);
    }
    actor_service_ = nullptr;
  }

  task_to_web_state_id_map_.clear();

  std::map<actor::ActorTaskId, ActuationCallback> callbacks =
      std::exchange(active_callbacks_, {});
  for (auto& [task_id, callback] : callbacks) {
    if (callback) {
      std::move(callback).Run(ttc::ToolResponse::Error(
          actor::mojom::ActionResultCode::kExecutorDestroyed));
    }
  }
  journal_.reset();
}

#pragma mark - Public Actuation

actor::ActorTaskId TtcActuationHandler::CreateTask(const std::string& title) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  actor::ActorTaskId task_id = actor::ActorTaskId();
  if (!web_state_list_ || !browser_id_.has_value() || !actor_service_) {
    return task_id;
  }

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  if (!active_web_state) {
    return task_id;
  }

  if (active_web_state->GetBrowserState() &&
      active_web_state->GetBrowserState()->IsOffTheRecord()) {
    return task_id;
  }

  task_id = actor_service_->CreateTask(
      title,
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTtc,
                            /*id=*/std::nullopt),
      /*allow_incognito_web_states=*/false);
  actor_service_->AddControlledWebState(task_id, active_web_state);
  actor_service_->SetTaskInterventionDelegate(task_id, intervention_delegate_);
  task_to_web_state_id_map_[task_id] = active_web_state->GetUniqueIdentifier();
  return task_id;
}

void TtcActuationHandler::ProcessToolCall(const ttc::ToolRequest& tool_request,
                                          actor::ActorTaskId task_id,
                                          ActuationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  auto request = TtcToolValidator::CreateActuationRequest(tool_request);
  if (!request.has_value()) {
    actor::mojom::ActionResultCode code =
        request.error() == ttc::kErrorMessageUnknownTool
            ? actor::mojom::ActionResultCode::kToolUnknown
            : actor::mojom::ActionResultCode::kArgumentsInvalid;
    std::move(callback).Run(ttc::ToolResponse::Error(code));
    return;
  }

  DispatchActuationRequest(*request, task_id, std::move(callback));
}

void TtcActuationHandler::DispatchActuationRequest(
    const TtcActuationRequest& request,
    actor::ActorTaskId task_id,
    ActuationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  auto task_it = task_to_web_state_id_map_.find(task_id);
  if (!actor_service_ || task_it == task_to_web_state_id_map_.end()) {
    std::move(callback).Run(ttc::ToolResponse::Error(
        actor::mojom::ActionResultCode::kTaskWentAway));
    return;
  }

  if (request.action_protos.empty()) {
    std::move(callback).Run(ttc::ToolResponse::Error(
        actor::mojom::ActionResultCode::kEmptyActionSequence));
    return;
  }

  if (active_callbacks_.find(task_id) != active_callbacks_.end()) {
    std::move(callback).Run(ttc::ToolResponse::Error(
        actor::mojom::ActionResultCode::kExecutionEngineExistingAction));
    return;
  }

  web::WebStateID web_state_id = task_it->second;
  std::vector<optimization_guide::proto::Action> actions;
  actions.reserve(request.action_protos.size());

  for (const std::string& proto_bytes : request.action_protos) {
    optimization_guide::proto::Action action;
    if (!action.ParseFromString(proto_bytes)) {
      std::move(callback).Run(ttc::ToolResponse::Error(
          actor::mojom::ActionResultCode::kArgumentsInvalid));
      return;
    }
    InjectDataIntoAction(action, web_state_id,
                         browser_id_.value_or(SessionID::InvalidValue()));
    actions.push_back(std::move(action));
  }

  active_callbacks_[task_id] = std::move(callback);

  actor_service_->PerformActions(
      task_id, actions, request.task_update,
      base::BindOnce(&TtcActuationHandler::OnPerformActionsFinished,
                     weak_ptr_factory_.GetWeakPtr(), task_id));
}

void TtcActuationHandler::StopTask(actor::ActorTaskId task_id,
                                   actor::ActorTaskStoppedReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto task_it = task_to_web_state_id_map_.find(task_id);
  if (task_it == task_to_web_state_id_map_.end()) {
    return;
  }
  task_to_web_state_id_map_.erase(task_it);

  auto callback_it = active_callbacks_.find(task_id);
  if (callback_it != active_callbacks_.end()) {
    ActuationCallback callback = std::move(callback_it->second);
    active_callbacks_.erase(callback_it);
    if (callback) {
      std::move(callback).Run(ttc::ToolResponse::Error(
          actor::mojom::ActionResultCode::kTaskWentAway));
    }
  }

  if (actor_service_) {
    actor_service_->StopTask(task_id, reason);
  }
}

#pragma mark - Private

void TtcActuationHandler::OnPerformActionsFinished(
    actor::ActorTaskId task_id,
    actor::PerformActionsResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = active_callbacks_.find(task_id);
  if (it == active_callbacks_.end()) {
    return;
  }
  ActuationCallback callback = std::move(it->second);
  active_callbacks_.erase(it);

  actor::mojom::ActionResultCode result_code =
      actor::mojom::ActionResultCode::kOk;

  for (const auto& action_result : result.action_results) {
    if (!action_result.tool_result.IsOk()) {
      result_code = action_result.tool_result.code();
      break;
    }
  }

  if (callback) {
    std::move(callback).Run(result_code == actor::mojom::ActionResultCode::kOk
                                ? ttc::ToolResponse::Success()
                                : ttc::ToolResponse::Error(result_code));
  }
}

@implementation TtcTaskInterventionHandler

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
