// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/tool_controller.h"

#import <ostream>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/no_destructor.h"
#import "base/not_fatal_until.h"
#import "base/state_transitions.h"
#import "base/strings/stringprintf.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/journal_details_builder.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/web_state.h"

namespace actor {

namespace {

std::string StateToString(ToolController::State state) {
  switch (state) {
    case ToolController::State::kInit:
      return "INIT";
    case ToolController::State::kReady:
      return "READY";
    case ToolController::State::kCreating:
      return "CREATING";
    case ToolController::State::kValidating:
      return "VALIDATING";
    case ToolController::State::kPostValidate:
      return "POST_VALIDATE";
    case ToolController::State::kInvokable:
      return "INVOKABLE";
    case ToolController::State::kPreInvoke:
      return "PREINVOKE";
    case ToolController::State::kInvoking:
      return "INVOKING";
    case ToolController::State::kPostInvoke:
      return "POSTINVOKE";
  }
}

// Runs `callback` with `result` unless `controller` was cancelled or destroyed.
// A free function so that no `ToolController` method is on the stack if
// `callback` destroys the controller.
void RunResultIfNotCancelled(base::WeakPtr<ToolController> controller,
                             ToolController::ResultCallback callback,
                             ToolExecutionResult result) {
  if (!controller) {
    return;
  }
  std::move(callback).Run(std::move(result));
}

// Posts `callback` with `result` so that the caller is never re-entered on the
// controller's stack. The result is dropped if `controller` is cancelled first.
void PostResult(base::WeakPtr<ToolController> controller,
                ToolController::ResultCallback callback,
                ToolExecutionResult result) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RunResultIfNotCancelled, std::move(controller),
                                std::move(callback), std::move(result)));
}

}  // namespace

ToolController::ActiveState::ActiveState(
    std::unique_ptr<ActorTool> tool,
    ResultCallback completion_callback,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry)
    : tool(std::move(tool)),
      completion_callback(std::move(completion_callback)),
      journal_entry(std::move(journal_entry)) {
  CHECK(this->tool);
  CHECK(!this->completion_callback.is_null());
}

ToolController::ActiveState::~ActiveState() = default;

ToolController::ToolController(ToolDelegate* tool_delegate)
    : tool_delegate_(tool_delegate) {
  CHECK(tool_delegate_);
  state_ = State::kReady;
}

ToolController::~ToolController() {
  // Not `Cancel()`, as it journals through `tool_delegate_`, whose owner may be
  // partially destroyed.
  ResetActiveTool();
}

void ToolController::SetState(State state) {
  GURL journal_url;
  if (active_state_ && active_state_->tool) {
    if (base::WeakPtr<web::WebState> target_web_state =
            active_state_->tool->GetTargetWebState()) {
      journal_url = target_web_state->GetLastCommittedURL();
    }
  }

  std::vector<mojom::JournalDetailsPtr> details =
      JournalDetailsBuilder()
          .Add("current_state", StateToString(state_))
          .Add("new_state", StateToString(state))
          .Build();

  journal().Log(journal_url, task_id(), "ToolControllerStateChange",
                std::move(details));

  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          {State::kInit, {State::kCreating}},
          {State::kReady, {State::kCreating}},
          {State::kCreating, {State::kValidating, State::kReady}},
          {State::kValidating, {State::kPostValidate, State::kReady}},
          {State::kPostValidate, {State::kInvokable, State::kReady}},
          {State::kInvokable, {State::kPreInvoke, State::kReady}},
          {State::kPreInvoke, {State::kInvoking, State::kReady}},
          {State::kInvoking, {State::kPostInvoke, State::kReady}},
          {State::kPostInvoke, {State::kReady}},
      }));
  CHECK(transitions->IsTransitionValid(state_, state),
        base::NotFatalUntil::M160)
      << "Invalid transition: " << state_ << " -> " << state;
  state_ = state;
}

std::ostream& operator<<(std::ostream& o, const ToolController::State& s) {
  return o << StateToString(s);
}

void ToolController::ReplyAndSetState(ResultCallback callback,
                                      ToolExecutionResult result,
                                      State state) {
  PostResult(weak_ptr_factory_.GetWeakPtr(), std::move(callback),
             std::move(result));
  SetState(state);
}

void ToolController::CreateToolAndValidate(const ActorToolRequest& request,
                                           ResultCallback callback) {
  CHECK_EQ(state_, State::kReady);
  SetState(State::kCreating);
  std::string tool_name =
      ActorActionCaseToToolName(request.action().action_case())
          .value_or("unknown tool");

  LogToolExecutionResult(journal(), GURL(), task_id(),
                         "Attempting to create tool: " + tool_name,
                         ToolExecutionResult::Ok());

  ActorToolFactory& factory = tool_delegate_->GetToolFactory();
  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> tool_result =
      factory.CreateTool(request, tool_delegate_);

  if (!tool_result.has_value()) {
    LogToolExecutionResult(journal(), GURL(), task_id(),
                           "Failed to create tool request: " + tool_name,
                           tool_result.error());
    ReplyAndSetState(std::move(callback), std::move(tool_result).error(),
                     State::kReady);
    return;
  }

  std::unique_ptr<ActorTool> tool = std::move(tool_result).value();
  CHECK(tool);

  GURL journal_url;
  if (tool->GetTargetWebState()) {
    journal_url = tool->GetTargetWebState()->GetLastCommittedURL();
  }

  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry =
      StartAsyncJournalEntry(journal(), journal_url, task_id(), tool_name,
                             "Execute Tool", tool->DebugString());

  active_state_.emplace(std::move(tool), std::move(callback),
                        std::move(journal_entry));

  SetState(State::kValidating);
  // Posted so that the tool is never destroyed while it is on the stack, even
  // if it runs the callback synchronously.
  active_state_->tool->Validate(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ToolController::PostValidate, weak_ptr_factory_.GetWeakPtr())));
}

void ToolController::PostValidate(ToolExecutionResult result) {
  CHECK(active_state_);
  if (!result.IsOk()) {
    CompleteToolRequest(std::move(result));
    return;
  }
  SetState(State::kPostValidate);
  // TODO(crbug.com/520098751): Call ActorTool::UpdateTaskBeforeInvoke here.
  PostUpdateTask(ToolExecutionResult::Ok());
}

void ToolController::PostUpdateTask(ToolExecutionResult result) {
  CHECK(active_state_);

  if (!result.IsOk()) {
    CompleteToolRequest(std::move(result));
    return;
  }
  ReplyAndSetState(std::move(active_state_->completion_callback),
                   ToolExecutionResult::Ok(), State::kInvokable);
}

void ToolController::Invoke(ResultCallback result_callback) {
  CHECK(active_state_);
  SetState(State::kPreInvoke);
  active_state_->completion_callback = std::move(result_callback);

  // TODO(crbug.com/520098751): Call ActorTool::TimeOfUseValidation here.

  SetState(State::kInvoking);
  // Created per tool so that no observation state carries over from the
  // previous tool.
  if (IsPageStabilityEnabled()) {
    observation_delayer_ =
        std::make_unique<ObservationDelayController>(task_id(), &journal());
  }
  // Posted so that the tool is never destroyed while it is on the stack, even
  // if it runs the callback synchronously.
  active_state_->tool->Execute(base::BindPostTaskToCurrentDefault(
      base::BindOnce(&ToolController::DidFinishToolExecution,
                     weak_ptr_factory_.GetWeakPtr())));
}

void ToolController::Cancel() {
  // Drops results that are posted but not yet delivered, even when idle.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (state_ == State::kInit || state_ == State::kReady) {
    return;
  }
  ResetActiveTool();
  SetState(State::kReady);
}

void ToolController::FailCurrentTool(mojom::ActionResultCode code) {
  if (state_ != State::kInvoking) {
    return;
  }

  // Posted so that the tool is never cancelled while it is on the stack.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ToolController::FailCurrentToolInternal,
                                weak_ptr_factory_.GetWeakPtr(), code));
}

void ToolController::FailCurrentToolInternal(mojom::ActionResultCode code) {
  if (state_ != State::kInvoking) {
    return;
  }

  PostInvokeTool(ToolExecutionResult(code));
}

void ToolController::DidFinishToolExecution(ToolExecutionResult result) {
  CHECK(active_state_);

  if (!IsPageStabilityEnabled() || !result.requires_page_stabilization() ||
      !observation_delayer_) {
    PostInvokeTool(std::move(result));
    return;
  }

  WaitForObservation(std::move(result));
}

void ToolController::WaitForObservation(ToolExecutionResult result) {
  CHECK(active_state_);
  CHECK(observation_delayer_);
  if (base::WeakPtr<web::WebState> target_web_state =
          active_state_->tool->GetTargetWebState()) {
    observation_delayer_->Wait(
        target_web_state, active_state_->tool->GetTargetWebFrame(),
        base::BindOnce(&ToolController::ObservationDelayComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(result)));
  } else {
    PostInvokeTool(std::move(result));
  }
}

void ToolController::ObservationDelayComplete(
    ToolExecutionResult action_result,
    ObservationDelayController::Result observation_result) {
  // TODO(crbug.com/498991756): Record UMA about what observation_result is.
  switch (observation_result) {
    case ObservationDelayController::Result::kOk:
      PostInvokeTool(std::move(action_result));
      break;
    case ObservationDelayController::Result::kPageNavigated: {
      size_t last_navigation_count = observation_delayer_->NavigationCount();
      GURL journal_url;
      if (active_state_ && active_state_->tool) {
        if (base::WeakPtr<web::WebState> target_web_state =
                active_state_->tool->GetTargetWebState()) {
          journal_url = target_web_state->GetLastCommittedURL();
        }
      }
      journal().Log(journal_url, task_id(),
                    "ToolController Restarting Observation", /*details=*/{});
      observation_delayer_ =
          std::make_unique<ObservationDelayController>(task_id(), &journal());
      observation_delayer_->SetNavigationCount(last_navigation_count + 1);
      WaitForObservation(std::move(action_result));
      break;
    }
  }
}

void ToolController::PostInvokeTool(ToolExecutionResult result) {
  CHECK(active_state_);
  if (!result.IsOk()) {
    CompleteToolRequest(std::move(result));
    return;
  }

  SetState(State::kPostInvoke);
  // TODO(crbug.com/520098751): Call ActorTool::UpdateTaskAfterInvoke here.
  CompleteToolRequest(std::move(result));
}

void ToolController::CompleteToolRequest(ToolExecutionResult result) {
  CHECK(active_state_);

  // Drops callbacks bound to the completed tool.
  weak_ptr_factory_.InvalidateWeakPtrs();

  EndAsyncJournalEntry(active_state_->journal_entry.get(), result);

  ReplyAndSetState(std::move(active_state_->completion_callback),
                   std::move(result), State::kReady);

  ResetActiveTool();
}

void ToolController::ResetActiveTool() {
  observation_delayer_.reset();
  if (active_state_) {
    active_state_->tool->Cancel();
  }
  active_state_.reset();
}

ActorTaskId ToolController::task_id() const {
  return tool_delegate_->GetTaskId();
}

AggregatedJournal& ToolController::journal() const {
  return tool_delegate_->GetJournal();
}

}  // namespace actor
