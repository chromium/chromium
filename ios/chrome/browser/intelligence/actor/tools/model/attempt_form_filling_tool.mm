// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool.h"

#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/web_state.h"

namespace actor {

// static
base::expected<std::unique_ptr<AttemptFormFillingTool>, ToolExecutionResult>
AttemptFormFillingTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::AttemptFormFillingAction& action,
    ToolDelegate* tool_delegate) {
  return std::unique_ptr<AttemptFormFillingTool>(
      new AttemptFormFillingTool(web_state, action, tool_delegate));
}

AttemptFormFillingTool::AttemptFormFillingTool(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::AttemptFormFillingAction& action,
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
