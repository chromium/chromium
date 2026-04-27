// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

namespace actor {

ActionResult::ActionResult(ToolExecutionResult result)
    : tool_result(std::move(result)) {}
ActionResult::~ActionResult() = default;
ActionResult::ActionResult(ActionResult&&) = default;
ActionResult& ActionResult::operator=(ActionResult&&) = default;

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

}  // namespace actor
