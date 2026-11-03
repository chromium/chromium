// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

#import <utility>

#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

bool IsTerminalState(ActorTaskState state) {
  switch (state) {
    case ActorTaskState::kCancelled:
    case ActorTaskState::kFinished:
    case ActorTaskState::kFailed:
      return true;
    case ActorTaskState::kInit:
    case ActorTaskState::kActing:
    case ActorTaskState::kReflecting:
    case ActorTaskState::kPausedByActor:
    case ActorTaskState::kPausedByUser:
    case ActorTaskState::kWaitingOnUser:
      return false;
  }
}

ActionResult::ActionResult(ToolExecutionResult result)
    : tool_result(std::move(result)) {}
ActionResult::~ActionResult() = default;
ActionResult::ActionResult(ActionResult&&) = default;
ActionResult& ActionResult::operator=(ActionResult&&) = default;

std::vector<ActionResult> MakeActionResults(mojom::ActionResultCode code) {
  std::vector<ActionResult> results;
  results.emplace_back(ToolExecutionResult(code));
  return results;
}

// TabObservationResponse.
TabObservationResponse::TabObservationResponse() = default;
TabObservationResponse::TabObservationResponse(
    web::WebStateID tab_id,
    PageContextWrapperCallbackResponse page_context_response,
    bool web_state_exists)
    : tab_id(tab_id),
      page_context_response(std::move(page_context_response)),
      web_state_exists(web_state_exists) {}
TabObservationResponse::~TabObservationResponse() = default;
TabObservationResponse::TabObservationResponse(TabObservationResponse&&) =
    default;
TabObservationResponse& TabObservationResponse::operator=(
    TabObservationResponse&&) = default;

// PerformActions.
PerformActionsResult::PerformActionsResult() = default;
PerformActionsResult::~PerformActionsResult() = default;
PerformActionsResult::PerformActionsResult(PerformActionsResult&&) = default;
PerformActionsResult& PerformActionsResult::operator=(PerformActionsResult&&) =
    default;

// ConfirmationInterrupt.
ConfirmationInterrupt::ConfirmationInterrupt() = default;
ConfirmationInterrupt::ConfirmationInterrupt(const ConfirmationInterrupt&) =
    default;
ConfirmationInterrupt& ConfirmationInterrupt::operator=(
    const ConfirmationInterrupt&) = default;
ConfirmationInterrupt::ConfirmationInterrupt(ConfirmationInterrupt&&) noexcept =
    default;
ConfirmationInterrupt& ConfirmationInterrupt::operator=(
    ConfirmationInterrupt&&) noexcept = default;
ConfirmationInterrupt::~ConfirmationInterrupt() = default;

// InterruptTaskResult.
InterruptTaskResult::InterruptTaskResult(
    InterruptResponse response,
    std::unique_ptr<TabObservationResponse> observation)
    : response(response), observation(std::move(observation)) {}
InterruptTaskResult::~InterruptTaskResult() = default;
InterruptTaskResult::InterruptTaskResult(InterruptTaskResult&&) noexcept =
    default;
InterruptTaskResult& InterruptTaskResult::operator=(
    InterruptTaskResult&&) noexcept = default;

}  // namespace actor
