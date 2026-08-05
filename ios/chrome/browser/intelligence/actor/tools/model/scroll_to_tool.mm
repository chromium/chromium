// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_to_tool.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

ScrollToTool::~ScrollToTool() = default;

// static
std::unique_ptr<ScrollToTool> ScrollToTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::ScrollToAction& action) {
  ActionTarget target = ActionTarget::FromProto(action.target());
  return std::unique_ptr<ScrollToTool>(
      new ScrollToTool(web_state, std::move(target)));
}

void ScrollToTool::Validate(ToolExecutionCallback callback) {
  if (!target_.is_valid()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  std::move(callback).Run(ToolExecutionResult::Ok());
}

void ScrollToTool::Execute(ToolExecutionCallback callback) {
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
      base::BindOnce(&ScrollToTool::OnTargetFrameResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

base::WeakPtr<web::WebState> ScrollToTool::GetTargetWebState() const {
  return web_state_;
}

ToolType ScrollToTool::GetToolType() const {
  return ToolType::kScrollTo;
}

ScrollToTool::ScrollToTool(base::WeakPtr<web::WebState> web_state,
                           ActionTarget target)
    : target_(std::move(target)),
      web_state_(web_state),
      js_feature_(ScrollToolJavaScriptFeature::GetInstance()) {}

void ScrollToTool::OnTargetFrameResolved(
    ToolExecutionCallback callback,
    base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                   ToolExecutionResult> result) {
  if (!result.has_value()) {
    std::move(callback).Run(result.error());
    return;
  }

  ActionTargetJavaScriptFeature::TargetFrameResult target_frame =
      result.value();
  web::WebFrame* target_web_frame = target_frame.frame;
  if (!target_web_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  target_frame_ = target_web_frame->AsWeakPtr();

  js_feature_->ScrollTo(target_web_frame->AsWeakPtr(), target_frame.target,
                        std::move(callback));
}

}  // namespace actor
