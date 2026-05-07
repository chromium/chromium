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
base::expected<std::unique_ptr<ClickTool>, ToolExecutionResult>
ClickTool::Create(const optimization_guide::proto::ClickAction& action,
                  ProfileIOS* profile) {
  if (!action.has_tab_id()) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
  }

  auto resolution_result = ResolveTab(action.tab_id(), profile);
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }

  if (!action.has_click_count() || !action.has_click_type()) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
  }

  if (!action.has_target()) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
  }

  const auto& target = action.target();
  // Callers must either target by coordinate or (document_identifier, node_id).
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
  return std::unique_ptr<ClickTool>(
      new ClickTool(action, resolution_result.value().web_state));
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

optimization_guide::proto::Action::ActionCase ClickTool::GetActionCase() const {
  return optimization_guide::proto::Action::kClick;
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

ClickTool::ClickTool(const optimization_guide::proto::ClickAction& action,
                     base::WeakPtr<web::WebState> web_state)
    : action_(action),
      web_state_(web_state),
      js_feature_(ClickToolJavaScriptFeature::GetInstance()) {}

}  // namespace actor
