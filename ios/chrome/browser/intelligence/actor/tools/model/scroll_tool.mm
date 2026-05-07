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
base::expected<std::unique_ptr<ScrollTool>, ToolExecutionResult>
ScrollTool::Create(const optimization_guide::proto::ScrollAction& action,
                   ProfileIOS* profile) {
  if (!action.has_tab_id()) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
  }

  auto resolution_result = ResolveTab(action.tab_id(), profile);
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }

  if (!action.has_direction() || !action.has_distance()) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
  }

  if (action.has_target()) {
    const auto& target = action.target();
    // Callers must either target by coordinate or (document_identifier,
    // node_id).
    if (target.has_content_node_id() && !target.has_document_identifier()) {
      return base::unexpected(
          ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    }
    bool can_target_by_coordinate = target.has_coordinate();
    bool can_target_by_node_id =
        target.has_content_node_id() && target.has_document_identifier();
    if (!can_target_by_coordinate && !can_target_by_node_id) {
      return base::unexpected(
          ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    }
    if (can_target_by_coordinate && can_target_by_node_id) {
      return base::unexpected(
          ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    }
  }

  return std::unique_ptr<ScrollTool>(
      new ScrollTool(action, resolution_result.value().web_state));
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
  if (!action_.has_target()) {
    auto* target = action_.mutable_target();
    target->set_content_node_id(kRootElementDomNodeId);
    target->mutable_document_identifier()->set_serialized_token(
        frames_manager->GetMainWebFrame()->GetFrameId());
    OnTargetFrameResolved(
        action_, std::move(callback),
        base::ok(ActionTargetJavaScriptFeature::TargetFrameResult{
            frames_manager->GetMainWebFrame(), *target}));
    return;
  }

  ResolveTargetFrame(web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(),
                     action_.target(),
                     base::BindOnce(&ScrollTool::OnTargetFrameResolved,
                                    weak_ptr_factory_.GetWeakPtr(), action_,
                                    std::move(callback)));
}

base::WeakPtr<web::WebState> ScrollTool::GetTargetWebState() const {
  return web_state_;
}

optimization_guide::proto::Action::ActionCase ScrollTool::GetActionCase()
    const {
  return optimization_guide::proto::Action::kScroll;
}

ScrollTool::ScrollTool(const optimization_guide::proto::ScrollAction& action,
                       base::WeakPtr<web::WebState> web_state)
    : action_(action),
      web_state_(web_state),
      js_feature_(ScrollToolJavaScriptFeature::GetInstance()) {}

void ScrollTool::OnTargetFrameResolved(
    optimization_guide::proto::ScrollAction action,
    ToolExecutionCallback callback,
    base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                   ToolExecutionResult> result) {
  if (!result.has_value()) {
    std::move(callback).Run(result.error());
    return;
  }

  const ActionTargetJavaScriptFeature::TargetFrameResult& target_frame =
      result.value();
  web::WebFrame* target_web_frame = target_frame.frame;
  if (!target_web_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  target_frame_ = target_web_frame->AsWeakPtr();

  // Update the target with the potentially translated coordinates relative
  // to the target frame.
  *action.mutable_target() = target_frame.target;

  js_feature_->Scroll(target_web_frame->AsWeakPtr(), action,
                      std::move(callback));
}

}  // namespace actor
