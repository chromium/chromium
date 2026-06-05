// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"

#import <optional>

namespace actor {

ActorToolRequest::ActorToolRequest(optimization_guide::proto::Action action)
    : action_(std::move(action)) {}

ActorToolRequest::~ActorToolRequest() = default;

ToolType ActorToolRequest::GetToolType() const {
  // LINT.IfChange(GetToolType)
  switch (action_.action_case()) {
    case optimization_guide::proto::Action::kClick:
      return ToolType::kClick;
    case optimization_guide::proto::Action::kType:
      return ToolType::kType;
    case optimization_guide::proto::Action::kScroll:
      return ToolType::kScroll;
    case optimization_guide::proto::Action::kSelect:
      return ToolType::kSelect;
    case optimization_guide::proto::Action::kNavigate:
      return ToolType::kNavigate;
    case optimization_guide::proto::Action::kBack:
      return ToolType::kBack;
    case optimization_guide::proto::Action::kForward:
      return ToolType::kForward;
    case optimization_guide::proto::Action::kWait:
      return ToolType::kWait;
    case optimization_guide::proto::Action::kScrollTo:
      return ToolType::kScrollTo;
    default:
      return ToolType::kUnknown;
  }
  // LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.mm:CreateTool)
}

web::WebStateID ActorToolRequest::GetTargetWebStateId() const {
  // LINT.IfChange(GetTargetWebStateId)
  std::optional<int32_t> tab_id;
  switch (action_.action_case()) {
    case optimization_guide::proto::Action::kClick:
      if (action_.click().has_tab_id()) {
        tab_id = action_.click().tab_id();
      }
      break;
    case optimization_guide::proto::Action::kType:
      if (action_.type().has_tab_id()) {
        tab_id = action_.type().tab_id();
      }
      break;
    case optimization_guide::proto::Action::kScroll:
      if (action_.scroll().has_tab_id()) {
        tab_id = action_.scroll().tab_id();
      }
      break;
    case optimization_guide::proto::Action::kSelect:
      if (action_.select().has_tab_id()) {
        tab_id = action_.select().tab_id();
      }
      break;
    case optimization_guide::proto::Action::kNavigate:
      if (action_.navigate().has_tab_id()) {
        tab_id = action_.navigate().tab_id();
      }
      break;
    case optimization_guide::proto::Action::kBack:
      if (action_.back().has_tab_id()) {
        tab_id = action_.back().tab_id();
      }
      break;
    case optimization_guide::proto::Action::kForward:
      if (action_.forward().has_tab_id()) {
        tab_id = action_.forward().tab_id();
      }
      break;
    case optimization_guide::proto::Action::kWait:
      if (action_.wait().has_observe_tab_id()) {
        tab_id = action_.wait().observe_tab_id();
      }
      break;
    case optimization_guide::proto::Action::kScrollTo:
      if (action_.scroll_to().has_tab_id()) {
        tab_id = action_.scroll_to().tab_id();
      }
      break;
    default:
      break;
  }

  if (tab_id.has_value()) {
    return web::WebStateID::FromSerializedValue(tab_id.value());
  }
  return web::WebStateID();
  // LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.mm:CreateTool)
}

}  // namespace actor
