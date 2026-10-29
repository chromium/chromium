// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

#import <stdint.h>

#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/to_string.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/sessions/core/session_id.h"
#import "components/ttc/app/public/tool_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_browser_utils.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {

// Internal title used when creating the session's Actor task; not surfaced in
// user-facing UI.
constexpr char kTtcTaskTitle[] = "TTC Session";

// Returns the `SessionID` of the regular `Browser` owning `web_state`, or an
// invalid `SessionID` if `web_state` is not attached to a regular browser.
SessionID GetBrowserIdForWebState(web::WebState* web_state) {
  if (!web_state) {
    return SessionID::InvalidValue();
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  if (!profile) {
    return SessionID::InvalidValue();
  }
  BrowserAndIndex browser_and_index = actor::FindBrowserAndIndexFromProfile(
      profile, web_state->GetUniqueIdentifier(), /*include_incognito=*/false);
  if (!browser_and_index.browser) {
    return SessionID::InvalidValue();
  }
  ActorBrowserAgent* agent =
      ActorBrowserAgent::FromBrowser(browser_and_index.browser);
  return agent ? agent->browser_id() : SessionID::InvalidValue();
}

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

// Completes `async_entry` with `result_code` and `error_message`.
void EndActuationAsyncEntry(
    std::unique_ptr<TTCJournal::PendingAsyncEntry> async_entry,
    actor::mojom::ActionResultCode result_code,
    std::string_view error_message) {
  if (!async_entry) {
    return;
  }
  TTCJournalDetailsBuilder details;
  details.Add("result_code", static_cast<int64_t>(result_code));
  if (result_code == actor::mojom::ActionResultCode::kOk) {
    details.SetSummary("Tool finished: ok");
  } else {
    std::string error = error_message.empty() ? base::ToString(result_code)
                                              : std::string(error_message);
    details.SetSummary(base::StrCat({"Tool finished: ", error}))
        .AddError(error);
  }
  async_entry->EndEntry(std::move(details).Build());
}

// Completes `async_entry` as cancelled.
void CancelActuationAsyncEntry(
    std::unique_ptr<TTCJournal::PendingAsyncEntry> async_entry) {
  if (!async_entry) {
    return;
  }
  async_entry->EndEntry(TTCJournalDetailsBuilder()
                            .SetSummary("Tool finished: cancelled")
                            .Add("result", "cancelled")
                            .Build());
}

}  // namespace

// Default intervention delegate for TTC tasks.
@interface TtcTaskInterventionHandler : NSObject <ActorTaskInterventionDelegate>
@end

#pragma mark - Initializers

TtcActuationHandler::TtcActuationHandler(actor::ActorService* actor_service)
    : actor_service_(actor_service),
      intervention_delegate_([[TtcTaskInterventionHandler alloc] init]) {}

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
  intervention_delegate_ = nil;

  actor::ActorTaskId task_id = std::exchange(task_id_, actor::ActorTaskId());
  actor::ActorService* actor_service = std::exchange(actor_service_, nullptr);
  ActuationCallback callback = std::move(active_callback_);
  CancelActuationAsyncEntry(std::move(active_async_entry_));
  journal_.reset();

  if (!task_id.is_null() && actor_service) {
    actor_service->StopTask(task_id, actor::ActorTaskStoppedReason::kShutdown);
  }

  if (callback) {
    std::move(callback).Run(ttc::ToolResponse::Error(
        actor::mojom::ActionResultCode::kExecutorDestroyed));
  }
}

#pragma mark - Public Actuation

void TtcActuationHandler::ProcessToolCall(const ttc::ToolRequest& tool_request,
                                          web::WebState* active_web_state,
                                          ActuationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  auto request = TtcToolValidator::CreateActuationRequest(tool_request);
  if (!request.has_value()) {
    actor::mojom::ActionResultCode code =
        request.error() == ttc::kErrorMessageUnknownTool
            ? actor::mojom::ActionResultCode::kToolUnknown
            : actor::mojom::ActionResultCode::kArgumentsInvalid;
    FailWithError(std::move(callback), code, request.error());
    return;
  }

  DispatchActuationRequest(*request, active_web_state, std::move(callback));
}

void TtcActuationHandler::DispatchActuationRequest(
    const TtcActuationRequest& request,
    web::WebState* active_web_state,
    ActuationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  if (!actor_service_) {
    FailWithError(std::move(callback),
                  actor::mojom::ActionResultCode::kTaskWentAway);
    return;
  }

  // The task can be stopped externally (e.g., if the user closes a controlled
  // tab or stops the task from the UI), in which case it is replaced by a new
  // one and any in-flight callback is failed with `kTaskWentAway`.
  if (!task_id_.is_null() &&
      !actor_service_->GetActiveTaskState().has_value()) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    task_id_ = actor::ActorTaskId();
    ActuationCallback active_callback = std::move(active_callback_);
    CancelActuationAsyncEntry(std::move(active_async_entry_));
    if (active_callback) {
      std::move(active_callback)
          .Run(ttc::ToolResponse::Error(
              actor::mojom::ActionResultCode::kTaskWentAway));
    }
  }

  web::BrowserState* browser_state =
      active_web_state ? active_web_state->GetBrowserState() : nullptr;
  if (!browser_state || browser_state->IsOffTheRecord()) {
    FailWithError(std::move(callback),
                  actor::mojom::ActionResultCode::kTabWentAway);
    return;
  }

  if (request.action_protos.empty()) {
    FailWithError(std::move(callback),
                  actor::mojom::ActionResultCode::kEmptyActionSequence);
    return;
  }

  if (active_callback_) {
    FailWithError(
        std::move(callback),
        actor::mojom::ActionResultCode::kExecutionEngineExistingAction);
    return;
  }

  web::WebStateID web_state_id = active_web_state->GetUniqueIdentifier();
  SessionID window_id = GetBrowserIdForWebState(active_web_state);
  std::vector<optimization_guide::proto::Action> actions;
  actions.reserve(request.action_protos.size());

  for (const std::string& proto_bytes : request.action_protos) {
    optimization_guide::proto::Action action;
    if (!action.ParseFromString(proto_bytes)) {
      FailWithError(std::move(callback),
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
      return;
    }
    InjectDataIntoAction(action, web_state_id, window_id);
    actions.push_back(std::move(action));
  }

  if (task_id_.is_null()) {
    task_id_ = actor_service_->CreateTask(
        kTtcTaskTitle,
        actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTtc,
                              /*id=*/std::nullopt),
        /*allow_incognito_web_states=*/false);
    if (task_id_.is_null()) {
      FailWithError(std::move(callback),
                    actor::mojom::ActionResultCode::kTaskWentAway);
      return;
    }
    actor_service_->SetTaskInterventionDelegate(task_id_,
                                                intervention_delegate_);
  }
  actor_service_->AddControlledWebState(task_id_, active_web_state);

  active_callback_ = std::move(callback);
  active_async_entry_ = BeginActuationAsyncEntry(actions);

  actor_service_->PerformActions(
      task_id_, std::move(actions), request.task_update,
      base::BindOnce(&TtcActuationHandler::OnPerformActionsFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TtcActuationHandler::StopTask(actor::ActorTaskStoppedReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_ptr_factory_.InvalidateWeakPtrs();

  actor::ActorTaskId task_id = std::exchange(task_id_, actor::ActorTaskId());
  ActuationCallback callback = std::move(active_callback_);
  CancelActuationAsyncEntry(std::move(active_async_entry_));

  if (!task_id.is_null() && actor_service_) {
    actor_service_->StopTask(task_id, reason);
  }

  if (callback) {
    std::move(callback).Run(ttc::ToolResponse::Error(
        actor::mojom::ActionResultCode::kTaskWentAway));
  }
}

#pragma mark - Private

void TtcActuationHandler::OnPerformActionsFinished(
    actor::PerformActionsResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!active_callback_) {
    return;
  }
  ActuationCallback callback = std::move(active_callback_);
  std::unique_ptr<TTCJournal::PendingAsyncEntry> async_entry =
      std::move(active_async_entry_);

  actor::mojom::ActionResultCode result_code =
      actor::mojom::ActionResultCode::kOk;
  std::string error_message;

  for (const auto& action_result : result.action_results) {
    if (!action_result.tool_result.IsOk()) {
      result_code = action_result.tool_result.code();
      error_message =
          actor::GetToolExecutionResultMessage(action_result.tool_result);
      break;
    }
  }

  // Resets `task_id_` if the task was torn down mid-execution so the next
  // actuation request lazily creates a replacement task.
  if (result_code == actor::mojom::ActionResultCode::kTaskWentAway) {
    task_id_ = actor::ActorTaskId();
  }

  EndActuationAsyncEntry(std::move(async_entry), result_code, error_message);

  std::move(callback).Run(result_code == actor::mojom::ActionResultCode::kOk
                              ? ttc::ToolResponse::Success()
                              : ttc::ToolResponse::Error(result_code));
}

#pragma mark - Journal Logging

void TtcActuationHandler::FailWithError(ActuationCallback callback,
                                        actor::mojom::ActionResultCode code,
                                        std::string_view reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (journal_) {
    std::string error =
        reason.empty() ? base::ToString(code) : std::string(reason);
    journal_->LogError(TTCJournalComponent::kActuation,
                       "ActuationDispatchRejected",
                       TTCJournalDetailsBuilder()
                           .SetSummary(base::StrCat({"Tool rejected: ", error}))
                           .Add("result_code", static_cast<int64_t>(code))
                           .AddError(error)
                           .Build());
  }
  std::move(callback).Run(ttc::ToolResponse::Error(code));
}

std::unique_ptr<TTCJournal::PendingAsyncEntry>
TtcActuationHandler::BeginActuationAsyncEntry(
    const std::vector<optimization_guide::proto::Action>& actions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!journal_) {
    return nullptr;
  }
  std::vector<std::string> action_names;
  action_names.reserve(actions.size());
  for (const optimization_guide::proto::Action& action : actions) {
    action_names.push_back(
        actor::ActorActionCaseToToolName(action.action_case())
            .value_or("unknown"));
  }
  std::string joined_names = base::JoinString(action_names, ", ");
  return journal_->BeginAsyncEvent(
      TTCJournalComponent::kActuation, "ActuationExecute",
      TTCJournalDetailsBuilder()
          .SetSummary(base::StrCat({"Tool started: ", joined_names}))
          .Add("task_id", task_id_.value())
          .Add("actions", actions.size())
          .Build());
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
