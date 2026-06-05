// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"

#import "base/functional/bind.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/journal_details_builder.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_controller.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/web_state.h"

namespace actor {

namespace {

// Returns the string representation of the ActorEngine::State.
std::string ActorEngineStateToString(ActorEngine::State state) {
  switch (state) {
    case ActorEngine::State::kUnknown:
      return "Unknown";
    case ActorEngine::State::kInit:
      return "Init";
    case ActorEngine::State::kPreExecutionChecks:
      return "PreExecutionChecks";
    case ActorEngine::State::kToolVerify:
      return "ToolVerify";
    case ActorEngine::State::kUiPreInvoke:
      return "UiPreInvoke";
    case ActorEngine::State::kToolInvoke:
      return "ToolInvoke";
    case ActorEngine::State::kUiPostInvoke:
      return "UiPostInvoke";
    case ActorEngine::State::kCompleted:
      return "Completed";
    case ActorEngine::State::kFailed:
      return "Failed";
  }
}

// Returns the string representation of the ActorEngine::EngineResult.
std::string EngineResultToString(ActorEngine::EngineResult result) {
  switch (result) {
    case ActorEngine::EngineResult::kUnknown:
      return "Unknown";
    case ActorEngine::EngineResult::kSuccess:
      return "Success";
    case ActorEngine::EngineResult::kFailed:
      return "Failed";
    case ActorEngine::EngineResult::kTimeout:
      return "Timeout";
    case ActorEngine::EngineResult::kCancelled:
      return "Cancelled";
  }
}

// Waits or immediately finishes tool execution based on the `tool_result`.
void WaitOrImmediatelyFinishTool(ActorTool* tool,
                                 base::OnceClosure on_delay_complete,
                                 ToolExecutionResult tool_result,
                                 ObservationDelayController* delay_controller) {
  // TODO(crbug.com/504625981): Move tool-specific state machine
  // into an iOS version of chrome/browser/actor/tools/tool_controller.h.
  if (!tool || !delay_controller || !IsPageStabilityEnabled() ||
      !tool_result.requires_page_stabilization()) {
    std::move(on_delay_complete).Run();
    return;
  }
  delay_controller->Wait(
      /*web_state=*/tool->GetTargetWebState(),
      /*web_frame=*/tool->GetTargetWebFrame(),
      base::BindOnce(
          [](base::OnceClosure callback,
             ObservationDelayController::Result delay_result) {
            // TODO(crbug.com/498991756): Record UMA about what delay_result is.
            std::move(callback).Run();
          },
          std::move(on_delay_complete)));
}

// TODO(crbug.com/503841160): Log the proper WebState URLs.
// Logs an engine state transition to the journal.
void LogEngineStateTransition(AggregatedJournal* journal,
                              ActorTaskId task_id,
                              ActorEngine::State old_state,
                              ActorEngine::State new_state) {
  CHECK(journal);

  std::vector<mojom::JournalDetailsPtr> details =
      JournalDetailsBuilder()
          .Add("current_state", ActorEngineStateToString(old_state))
          .Add("new_state", ActorEngineStateToString(new_state))
          .Build();

  journal->Log(GURL(), task_id, "ExecutionEngine::StateChange",
               std::move(details));
}

// TODO(crbug.com/503841160): Log the proper WebState URLs.
// Logs the start of the Act sequence to the journal.
void LogActStart(
    AggregatedJournal* journal,
    ActorTaskId task_id,
    const std::vector<std::unique_ptr<ActorToolRequest>>& actions) {
  CHECK(journal);

  JournalDetailsBuilder builder;
  for (size_t i = 0; i < actions.size(); ++i) {
    builder.Add(base::StringPrintf("Actions[%zu]", i),
                base::StringPrintf("Tool %zu", i));
  }

  journal->Log(GURL(), task_id, "ExecutionEngine::Act",
               std::move(builder).Build());
}

// TODO(crbug.com/503841160): Log the proper WebState URLs.
// Creates a pending async entry for tool execution in the journal.
std::unique_ptr<AggregatedJournal::PendingAsyncEntry>
CreateToolExecutionAsyncEntry(AggregatedJournal* journal,
                              ActorTaskId task_id,
                              size_t action_index) {
  CHECK(journal);

  return journal->CreatePendingAsyncEntry(
      GURL(), task_id, 0, base::StringPrintf("Execute Tool %zu", action_index),
      /*details=*/{});
}

// Ends a pending async entry in the journal, adding error details if any.
void EndAsyncEntry(AggregatedJournal::PendingAsyncEntry* entry,
                   const ToolExecutionResult& tool_result) {
  CHECK(entry);

  JournalDetailsBuilder builder;
  if (!tool_result.IsOk()) {
    builder.AddError(GetToolExecutionResultMessage(tool_result));
  }
  entry->EndEntry(std::move(builder).Build());
}

// Logs a ToolExecutionResult immediately to the journal.
//
// Note that `result` has a success code and can be used to carry errors
// made before the tool was even executed.
void LogToolExecutionResult(AggregatedJournal* journal,
                            ActorTaskId task_id,
                            std::string_view details_key,
                            std::string_view event_name,
                            const ToolExecutionResult& result) {
  CHECK(journal);
  std::vector<mojom::JournalDetailsPtr> details;

  if (result.IsOk()) {
    details = JournalDetailsBuilder()
                  .Add(details_key, GetToolExecutionResultMessage(result))
                  .Build();
  } else {
    details = JournalDetailsBuilder()
                  .AddError(GetToolExecutionResultMessage(result))
                  .Build();
  }
  journal->Log(GURL(), task_id, event_name, std::move(details));
}

// Returns the WebStateID for the target WebState of `action`, or an invalid
// WebStateID if `action` is null or has no target WebState.
web::WebStateID GetWebStateIDForAction(const ActorToolRequest* action) {
  if (!action) {
    return web::WebStateID();
  }
  return action->GetTargetWebStateId();
}

}  // namespace

ActorEngine::ActorEngine(ActorTaskId task_id,
                         AggregatedJournal* journal,
                         ExecutionUpdatesDelegate* execution_updates_delegate,
                         ActorToolFactory* tool_factory)
    : tool_factory_(tool_factory),
      state_(State::kInit),
      task_id_(task_id),
      journal_(journal),
      execution_updates_delegate_(execution_updates_delegate) {
  CHECK(execution_updates_delegate_);
}

ActorEngine::~ActorEngine() = default;

void ActorEngine::Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
                      ActCallback callback) {
  // TODO(crbug.com/503054406): Add guards for invalid start states.
  action_sequence_ = std::move(actions);
  completion_callback_ = std::move(callback);
  next_action_index_ = 0;
  action_results_.clear();

  LogActStart(journal_, task_id_, action_sequence_);

  ExecuteNextAction();
}

void ActorEngine::CancelOngoingAndPendingActions(
    ActorEngine::EngineResult reason) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  action_sequence_.clear();
  current_tool_.reset();

  if (current_async_entry_) {
    current_async_entry_->EndEntry(
        JournalDetailsBuilder()
            .Add("status", "pending action cancelled")
            .Build());
    current_async_entry_.reset();
  }

  SetState(State::kFailed);

  std::vector<mojom::JournalDetailsPtr> details =
      JournalDetailsBuilder()
          .Add("reason", EngineResultToString(reason))
          .Build();
  journal_->Log(GURL(), task_id_, "ExecutionEngine::Cancel",
                std::move(details));

  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(action_results_));
  }
}

void ActorEngine::SetState(State new_state) {
  LogEngineStateTransition(journal_, task_id_, state_, new_state);
  state_ = new_state;
}

void ActorEngine::ExecuteNextAction() {
  if (next_action_index_ >= action_sequence_.size()) {
    CompleteActions(ActionResult(ToolExecutionResult::Ok()));
    return;
  }

  next_action_index_++;

  // TODO(crbug.com/496196533): Add pre-execution checks.
  SetState(State::kPreExecutionChecks);

  // TODO(crbug.com/503072595): Add tool verification.
  SetState(State::kToolVerify);

  // TODO(crbug.com/496195979): Add UI pre-invoke.
  UiPreInvoke();
}

void ActorEngine::UiPreInvoke() {
  SetState(State::kUiPreInvoke);

  const ActorToolRequest* action =
      action_sequence_[InProgressActionIndex()].get();
  if (!action) {
    FinishedUiPreInvoke(ActionResult(
        ToolExecutionResult(mojom::ActionResultCode::kToolUnknown)));
    return;
  }

  execution_updates_delegate_->OnWillExecuteTool(
      action->GetToolType(), GetWebStateIDForAction(action));

  FinishedUiPreInvoke(ActionResult(ToolExecutionResult::Ok()));
}

void ActorEngine::FinishedUiPreInvoke(ActionResult result) {
  if (!result.tool_result.IsOk()) {
    CompleteActions(std::move(result));
    return;
  }

  SetState(State::kToolInvoke);

  const ActorToolRequest* action =
      action_sequence_[InProgressActionIndex()].get();

  // TODO(crbug.com/504625981): Move tool instantiation/controller behavior into
  // the tools layer.
  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult>
      maybe_created_tool = tool_factory_->CreateTool(action->action());

  ToolExecutionResult create_tool_result = ToolExecutionResult::Ok();
  if (!maybe_created_tool.has_value()) {
    create_tool_result = maybe_created_tool.error();
    CompleteActions(ActionResult(create_tool_result));
    return;
  }
  LogToolExecutionResult(
      journal_, task_id_,
      /*detail_key=*/"ActorEngine::FinishedUiPreInvoke",
      /*event_name=*/
      base::StringPrintf("CreateTool #%zu", InProgressActionIndex()),
      create_tool_result);

  current_tool_ = std::move(maybe_created_tool).value();
  current_async_entry_ = CreateToolExecutionAsyncEntry(journal_, task_id_,
                                                       InProgressActionIndex());

  current_tool_->Execute(base::BindOnce(&ActorEngine::OnToolExecutionComplete,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        current_tool_.get()));
}

void ActorEngine::OnToolExecutionComplete(ActorTool* tool,
                                          ToolExecutionResult tool_result) {
  CHECK(current_async_entry_);
  EndAsyncEntry(current_async_entry_.get(), tool_result);
  current_async_entry_.reset();

  if (IsPageStabilityEnabled() && tool_result.requires_page_stabilization()) {
    observation_delay_controller_ =
        std::make_unique<ObservationDelayController>(task_id_, journal_);
  }

  // Regardless of the observation delay outcome, the initial `tool_result` is
  // used by the engine.
  base::OnceClosure on_delay_complete =
      base::BindOnce(&ActorEngine::FinishedToolInvoke,
                     weak_ptr_factory_.GetWeakPtr(), ActionResult(tool_result));
  WaitOrImmediatelyFinishTool(tool, std::move(on_delay_complete), tool_result,
                              observation_delay_controller_.get());
}

void ActorEngine::FinishedToolInvoke(ActionResult result) {
  current_tool_.reset();
  bool success = result.tool_result.IsOk();

  if (!success) {
    CompleteActions(std::move(result));
    return;
  }

  action_results_.push_back(std::move(result));

  // TODO(crbug.com/496195979): Add UI post-invoke.
  SetState(State::kUiPostInvoke);
  FinishedUiPostInvoke(ActionResult(ToolExecutionResult::Ok()));
}

void ActorEngine::FinishedUiPostInvoke(ActionResult result) {
  if (!result.tool_result.IsOk()) {
    CompleteActions(std::move(result));
    return;
  }
  ExecuteNextAction();
}

void ActorEngine::CompleteActions(ActionResult result) {
  bool success = result.tool_result.IsOk();

  // Successful tool results are already appended in `FinishedToolInvoke`,
  // therefore only record/overwrite the result if it is a failure.
  if (!success) {
    size_t index = InProgressActionIndex();
    if (action_results_.size() == index) {
      // This is the first result for the current action, append to results
      // vector.
      action_results_.push_back(std::move(result));
    } else if (action_results_.size() > index) {
      // A result was already recorded for this action. Overwrite the success
      // result with the failure.
      action_results_[index] = std::move(result);
    }
  }

  SetState(success ? State::kCompleted : State::kFailed);

  action_sequence_.clear();

  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(action_results_));
  }
}

size_t ActorEngine::InProgressActionIndex() const {
  CHECK_GT(next_action_index_, 0ul);
  return next_action_index_ - 1;
}

}  // namespace actor
