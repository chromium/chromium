// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/test/controllable_actor_tool.h"

#import <utility>

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"

namespace actor {

ControllableToolHooks::ControllableToolHooks() = default;

ControllableToolHooks::~ControllableToolHooks() = default;

ControllableActorTool::ControllableActorTool(
    ControllableToolHooks& hooks,
    ToolDelegate* tool_delegate,
    web::WebStateID target_web_state_id)
    : hooks_(hooks), target_web_state_id_(target_web_state_id) {
  CHECK(!hooks_->tool_delegate);
  hooks_->tool_delegate = tool_delegate;
  hooks_->tool_destroyed = false;
}

ControllableActorTool::~ControllableActorTool() {
  hooks_->validate_callback.Reset();
  hooks_->execute_callback.Reset();
  hooks_->on_execute.Reset();
  hooks_->tool_delegate = nullptr;
  hooks_->tool_destroyed = true;
}

void ControllableActorTool::Validate(ToolExecutionCallback callback) {
  hooks_->validate_callback = std::move(callback);
}

void ControllableActorTool::Execute(ToolExecutionCallback callback) {
  hooks_->executed_action_ids.push_back(target_web_state_id_);
  hooks_->execute_callback = std::move(callback);
  if (hooks_->on_execute) {
    std::move(hooks_->on_execute).Run();
  }
}

base::WeakPtr<web::WebState> ControllableActorTool::GetTargetWebState() const {
  return nullptr;
}

ToolType ControllableActorTool::GetToolType() const {
  return ToolType::kWait;
}

std::string ControllableActorTool::DebugString() const {
  return "ControllableActorTool";
}

ControllableActorToolFactory::ControllableActorToolFactory(
    ProfileIOS* profile,
    ControllableToolHooks& hooks)
    : ActorToolFactory(profile), hooks_(hooks) {}

ControllableActorToolFactory::~ControllableActorToolFactory() = default;

base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult>
ControllableActorToolFactory::CreateTool(const ActorToolRequest& request,
                                         ToolDelegate* tool_delegate) {
  return std::make_unique<ControllableActorTool>(*hooks_, tool_delegate,
                                                 request.GetTargetWebStateId());
}

}  // namespace actor
