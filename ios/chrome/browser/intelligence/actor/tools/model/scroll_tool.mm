// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_tool.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_constants.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

ScrollTool::~ScrollTool() = default;

// static
std::unique_ptr<ScrollTool> ScrollTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::ScrollAction& action) {
  ActionTarget target = ActionTarget::FromProto(action.target());
  return std::unique_ptr<ScrollTool>(
      new ScrollTool(web_state, action, std::move(target)));
}

void ScrollTool::Validate(ToolExecutionCallback callback) {
  if (!action_.has_direction() || !action_.has_distance()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  // The tool will fall back to targeting the document root if a target is not
  // specified, so an empty target should have passed the validation.
  std::move(callback).Run(ToolExecutionResult::Ok());
}

void ScrollTool::Execute(ToolExecutionCallback callback) {
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

  // Fall back to targeting the document root if a target is not specified.
  // This follows the behavior of Desktop's ScrollTool, see
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/actor_proto_conversion.cc;l=264-277;drc=4a530ad3251da1da3fbde56051d440a7df0a60bd
  if (!target_.is_valid()) {
    optimization_guide::proto::ActionTarget target_proto;
    target_proto.set_content_node_id(kRootElementDomNodeId);
    target_proto.mutable_document_identifier()->set_serialized_token(
        frames_manager->GetMainWebFrame()->GetFrameId());

    target_ = ActionTarget::FromProto(target_proto);

    OnTargetFrameResolved(
        std::move(callback),
        base::ok(ActionTargetJavaScriptFeature::TargetFrameResult{
            frames_manager->GetMainWebFrame(), target_}));
    return;
  }

  ResolveTargetFrame(
      web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(), target_,
      base::BindOnce(&ScrollTool::OnTargetFrameResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

base::WeakPtr<web::WebState> ScrollTool::GetTargetWebState() const {
  return web_state_;
}

ToolType ScrollTool::GetToolType() const {
  return ToolType::kScroll;
}

ScrollTool::ScrollTool(base::WeakPtr<web::WebState> web_state,
                       const optimization_guide::proto::ScrollAction& action,
                       ActionTarget target)
    : action_(action),
      target_(std::move(target)),
      web_state_(web_state),
      js_feature_(ScrollToolJavaScriptFeature::GetInstance()) {}

void ScrollTool::OnTargetFrameResolved(
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

  js_feature_->Scroll(target_web_frame->AsWeakPtr(), target_frame.target,
                      action_.direction(), action_.distance(),
                      std::move(callback));
}

}  // namespace actor
