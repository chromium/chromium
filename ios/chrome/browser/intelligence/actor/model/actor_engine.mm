// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"

namespace actor {

ActorEngine::ActorEngine() : state_(State::kInit) {}

ActorEngine::~ActorEngine() = default;

void ActorEngine::Act(std::vector<std::unique_ptr<ActorTool>> actions,
                      PerformActionsCallback callback) {
  // TODO(crbug.com/503054406): Add guards for invalid start states.
  action_sequence_ = std::move(actions);
  completion_callback_ = std::move(callback);
  next_action_index_ = 0;
  action_results_.clear();
  ExecuteNextAction();
}

void ActorEngine::CancelOngoingAndPendingActions(
    ActorEngine::EngineResult reason) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  action_sequence_.clear();
  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(action_results_));
  }
}

void ActorEngine::ExecuteNextAction() {
  if (next_action_index_ >= action_sequence_.size()) {
    CompleteActions(ActionResult(ToolExecutionResult(base::ok())));
    return;
  }

  next_action_index_++;

  // TODO(crbug.com/496196533): Add pre-execution checks.
  state_ = State::kPreExecutionChecks;

  // TODO(crbug.com/503072595): Add tool verification.
  state_ = State::kToolVerify;

  // TODO(crbug.com/496195979): Add UI pre-invoke.
  state_ = State::kUiPreInvoke;
  FinishedUiPreInvoke(ActionResult(ToolExecutionResult(base::ok())));
}

void ActorEngine::FinishedUiPreInvoke(ActionResult result) {
  if (!result.tool_result.has_value()) {
    CompleteActions(std::move(result));
    return;
  }

  state_ = State::kToolInvoke;

  ActorTool* tool_ptr = action_sequence_[next_action_index_ - 1].get();
  tool_ptr->Execute(base::BindOnce(&ActorEngine::OnToolExecutionComplete,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ActorEngine::OnToolExecutionComplete(ToolExecutionResult tool_result) {
  FinishedToolInvoke(ActionResult(std::move(tool_result)));
}

void ActorEngine::FinishedToolInvoke(ActionResult result) {
  bool success = result.tool_result.has_value();

  if (!success) {
    CompleteActions(std::move(result));
    return;
  }

  action_results_.push_back(std::move(result));

  // TODO(crbug.com/496195979): Add UI post-invoke.
  state_ = State::kUiPostInvoke;
  FinishedUiPostInvoke(ActionResult(ToolExecutionResult(base::ok())));
}

void ActorEngine::FinishedUiPostInvoke(ActionResult result) {
  if (!result.tool_result.has_value()) {
    CompleteActions(std::move(result));
    return;
  }
  ExecuteNextAction();
}

void ActorEngine::CompleteActions(ActionResult result) {
  bool success = result.tool_result.has_value();

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

  state_ = success ? State::kCompleted : State::kFailed;

  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(action_results_));
  }
}

size_t ActorEngine::InProgressActionIndex() const {
  CHECK_GT(next_action_index_, 0ul);
  return next_action_index_ - 1;
}

}  // namespace actor
