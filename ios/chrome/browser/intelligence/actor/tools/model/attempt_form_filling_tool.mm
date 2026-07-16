// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool.h"

#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/profile_context_resolver.h"
#import "ios/web/public/web_state.h"

namespace actor {

// static
base::expected<std::unique_ptr<AttemptFormFillingTool>, ToolExecutionResult>
AttemptFormFillingTool::Create(
    const optimization_guide::proto::AttemptFormFillingAction& action,
    ToolDelegate* tool_delegate,
    const ProfileContextResolver& profile_context_resolver) {
  if (!action.has_tab_id()) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kCreationMissingRequiredFields));
  }

  base::expected<ProfileContextResolver::TabResolutionResult,
                 ToolExecutionResult>
      resolution_result = profile_context_resolver.ResolveTab(action.tab_id());
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }

  return std::unique_ptr<AttemptFormFillingTool>(new AttemptFormFillingTool(
      action, resolution_result.value().web_state, tool_delegate));
}

AttemptFormFillingTool::AttemptFormFillingTool(
    const optimization_guide::proto::AttemptFormFillingAction& action,
    base::WeakPtr<web::WebState> web_state,
    ToolDelegate* tool_delegate)
    : action_(action), web_state_(web_state), tool_delegate_(tool_delegate) {}

AttemptFormFillingTool::~AttemptFormFillingTool() = default;

void AttemptFormFillingTool::Validate(ToolExecutionCallback callback) {
  std::move(callback).Run(ToolExecutionResult::Ok());
}

void AttemptFormFillingTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  std::move(callback).Run(ToolExecutionResult::Ok());
}

base::WeakPtr<web::WebState> AttemptFormFillingTool::GetTargetWebState() const {
  return web_state_;
}

ToolType AttemptFormFillingTool::GetToolType() const {
  return ToolType::kAttemptFormFilling;
}

}  // namespace actor
