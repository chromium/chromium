// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool.h"

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

ClickTool::~ClickTool() = default;

// static
std::unique_ptr<ClickTool> ClickTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::ClickAction& action) {
  return std::unique_ptr<ClickTool>(new ClickTool(web_state, action));
}

void ClickTool::Validate(ToolExecutionCallback callback) {
  if (!action_.has_click_count() || !action_.has_click_type()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  if (!action_.has_target()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  const optimization_guide::proto::ActionTarget& target = action_.target();
  // TODO(crbug.com/537772128): Share common target validation logic.
  // Callers must either target by coordinate or (document_identifier, node_id).
  if (target.has_content_node_id() && !target.has_document_identifier()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }
  bool can_target_by_coordinate = target.has_coordinate();
  bool can_target_by_node_id =
      target.has_content_node_id() && target.has_document_identifier();
  if (!can_target_by_coordinate && !can_target_by_node_id) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }
  if (can_target_by_coordinate && can_target_by_node_id) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  std::move(callback).Run(ToolExecutionResult::Ok());
}

void ClickTool::Execute(ToolExecutionCallback callback) {
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

  ResolveTargetFrame(web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(),
                     action_.target(),
                     base::BindOnce(&ClickTool::OnTargetFrameResolved,
                                    weak_ptr_factory_.GetWeakPtr(), action_,
                                    std::move(callback)));
}

base::WeakPtr<web::WebState> ClickTool::GetTargetWebState() const {
  return web_state_;
}

ToolType ClickTool::GetToolType() const {
  return ToolType::kClick;
}

void ClickTool::OnTargetFrameResolved(
    const optimization_guide::proto::ClickAction& action,
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

  // Update the target with the potentially translated coordinates relative
  // to the target frame.
  optimization_guide::proto::ClickAction new_action = action;
  *new_action.mutable_target() = target_frame.target;

  js_feature_->Click(target_web_frame->AsWeakPtr(), new_action,
                     std::move(callback));
}

ClickTool::ClickTool(base::WeakPtr<web::WebState> web_state,
                     const optimization_guide::proto::ClickAction& action)
    : action_(action),
      web_state_(web_state),
      js_feature_(ClickToolJavaScriptFeature::GetInstance()) {}

}  // namespace actor
