// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"

#import "base/functional/bind.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

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

// TODO(crbug.com/503841160): Log the proper WebState URLs.
// Logs an engine state transition to the journal.
void LogEngineStateTransition(AggregatedJournal* journal,
                              ActorTaskId task_id,
                              ActorEngine::State old_state,
                              ActorEngine::State new_state) {
  CHECK(journal);

  std::vector<JournalDetails> details = {
      {"current_state", ActorEngineStateToString(old_state)},
      {"new_state", ActorEngineStateToString(new_state)}};

  journal->Log(GURL(), task_id, "ExecutionEngine::StateChange",
               std::move(details));
}

// TODO(crbug.com/503841160): Log the proper WebState URLs.
// Logs the start of the Act sequence to the journal.
void LogActStart(AggregatedJournal* journal,
                 ActorTaskId task_id,
                 const std::vector<std::unique_ptr<ActorTool>>& actions) {
  CHECK(journal);

  std::vector<JournalDetails> details;
  for (size_t i = 0; i < actions.size(); ++i) {
    details.push_back({base::StringPrintf("Actions[%zu]", i),
                       base::StringPrintf("Tool %zu", i)});
  }

  journal->Log(GURL(), task_id, "ExecutionEngine::Act", std::move(details));
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
      std::vector<JournalDetails>());
}

// Ends a pending async entry in the journal, adding error details if any.
void EndAsyncEntry(AggregatedJournal::PendingAsyncEntry* entry,
                   const ToolExecutionResult& tool_result) {
  CHECK(entry);

  std::vector<JournalDetails> details;
  if (!tool_result.IsOk()) {
    details.push_back({"error", GetToolExecutionResultMessage(tool_result)});
  }
  entry->EndEntry(std::move(details));
}

}  // namespace

ActorEngine::ActorEngine(ActorTaskId task_id, AggregatedJournal* journal)
    : state_(State::kInit), task_id_(task_id), journal_(journal) {}

ActorEngine::~ActorEngine() = default;

void ActorEngine::Act(std::vector<std::unique_ptr<ActorTool>> actions,
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

  if (current_async_entry_) {
    current_async_entry_->EndEntry({{"status", "pending action cancelled"}});
    current_async_entry_.reset();
  }

  SetState(State::kFailed);

  std::vector<JournalDetails> details = {
      {"reason", EngineResultToString(reason)}};
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
  SetState(State::kUiPreInvoke);
  FinishedUiPreInvoke(ActionResult(ToolExecutionResult::Ok()));
}

void ActorEngine::FinishedUiPreInvoke(ActionResult result) {
  if (!result.tool_result.IsOk()) {
    CompleteActions(std::move(result));
    return;
  }

  SetState(State::kToolInvoke);

  ActorTool* tool_ptr = action_sequence_[InProgressActionIndex()].get();

  current_async_entry_ = CreateToolExecutionAsyncEntry(journal_, task_id_,
                                                       InProgressActionIndex());

  tool_ptr->Execute(base::BindOnce(&ActorEngine::OnToolExecutionComplete,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ActorEngine::OnToolExecutionComplete(ToolExecutionResult tool_result) {
  CHECK(current_async_entry_);
  EndAsyncEntry(current_async_entry_.get(), tool_result);
  current_async_entry_.reset();

  FinishedToolInvoke(ActionResult(std::move(tool_result)));
}

void ActorEngine::FinishedToolInvoke(ActionResult result) {
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

  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(action_results_));
  }
}

size_t ActorEngine::InProgressActionIndex() const {
  CHECK_GT(next_action_index_, 0ul);
  return next_action_index_ - 1;
}

}  // namespace actor
