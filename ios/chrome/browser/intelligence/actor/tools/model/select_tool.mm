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
#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

SelectTool::~SelectTool() = default;

// static
base::expected<std::unique_ptr<SelectTool>, ToolExecutionResult>
SelectTool::Create(const optimization_guide::proto::SelectAction& action,
                   ProfileIOS* profile) {
  if (!action.has_tab_id()) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
  }

  auto resolution_result = ResolveTab(action.tab_id(), profile);
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }

  if (!action.has_value()) {
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

  return std::unique_ptr<SelectTool>(
      new SelectTool(action, resolution_result.value().web_state));
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
      web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(),
      action_.target(),
      base::BindOnce(&SelectTool::OnTargetFrameResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

base::WeakPtr<web::WebState> SelectTool::GetTargetWebState() const {
  return web_state_;
}

optimization_guide::proto::Action::ActionCase SelectTool::GetActionCase()
    const {
  return optimization_guide::proto::Action::kSelect;
}

SelectTool::SelectTool(const optimization_guide::proto::SelectAction& action,
                       base::WeakPtr<web::WebState> web_state)
    : action_(action),
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

  ActionTargetJavaScriptFeature::TargetFrameResult& targeting_result =
      result.value();
  web::WebFrame* target_web_frame = targeting_result.frame;
  if (!target_web_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  target_frame_ = target_web_frame->AsWeakPtr();

  // Update the target with the potentially translated coordinates relative
  // to the target frame.
  *action_.mutable_target() = std::move(targeting_result.target);

  js_feature_->Select(target_web_frame->AsWeakPtr(), action_,
                      std::move(callback));
}

}  // namespace actor
