// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

SelectTool::~SelectTool() = default;

// static
std::unique_ptr<SelectTool> SelectTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::SelectAction& action) {
  ActionTarget target = ActionTarget::FromProto(action.target());
  std::optional<std::string> value;
  if (action.has_value()) {
    value = action.value();
  }
  return std::unique_ptr<SelectTool>(
      new SelectTool(web_state, std::move(value), std::move(target)));
}

void SelectTool::Validate(ToolExecutionCallback callback) {
  if (!value_.has_value()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  if (!target_.is_valid()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  std::move(callback).Run(ToolExecutionResult::Ok());
}

void SelectTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  web::WebFramesManager* frames_manager =
      js_feature_->GetWebFramesManager(web_state_.get());
  if (!frames_manager || !frames_manager->GetMainWebFrame()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  ResolveTargetFrame(
      web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(), target_,
      base::BindOnce(&SelectTool::OnTargetFrameResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

base::WeakPtr<web::WebState> SelectTool::GetTargetWebState() const {
  return web_state_;
}

ToolType SelectTool::GetToolType() const {
  return ToolType::kSelect;
}

SelectTool::SelectTool(base::WeakPtr<web::WebState> web_state,
                       std::optional<std::string> value,
                       ActionTarget target)
    : value_(std::move(value)),
      target_(std::move(target)),
      web_state_(web_state),
      js_feature_(SelectToolJavaScriptFeature::GetInstance()) {}

void SelectTool::OnTargetFrameResolved(
    ToolExecutionCallback callback,
    base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                   ToolExecutionResult> result) {
  if (!result.has_value()) {
    std::move(callback).Run(result.error());
    return;
  }

  ActionTargetJavaScriptFeature::TargetFrameResult targeting_result =
      result.value();
  web::WebFrame* target_web_frame = targeting_result.frame;
  if (!target_web_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  target_frame_ = target_web_frame->AsWeakPtr();

  CHECK(value_.has_value());

  js_feature_->Select(target_web_frame->AsWeakPtr(), targeting_result.target,
                      value_.value(), std::move(callback));
}

}  // namespace actor
