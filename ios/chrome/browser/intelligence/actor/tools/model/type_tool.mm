// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

TypeTool::~TypeTool() = default;

// static
base::expected<std::unique_ptr<TypeTool>, ActorToolError> TypeTool::Create(
    const optimization_guide::proto::TypeAction& action,
    ProfileIOS* profile) {
  if (!action.has_tab_id()) {
    return base::unexpected(
        ActorToolError{ActorToolErrorCode::kCreationMissingRequiredFields});
  }

  auto resolution_result = ResolveTab(action.tab_id(), profile);
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }

  if (!action.has_text() || !action.has_mode()) {
    return base::unexpected(
        ActorToolError{ActorToolErrorCode::kCreationMissingRequiredFields});
  }

  if (!action.has_target()) {
    return base::unexpected(
        ActorToolError{ActorToolErrorCode::kCreationMissingRequiredFields});
  }

  const auto& target = action.target();
  bool can_target_by_coordinate = target.has_coordinate();
  bool can_target_by_node_id =
      target.has_content_node_id() && target.has_document_identifier();

  if (!can_target_by_coordinate && !can_target_by_node_id) {
    return base::unexpected(
        ActorToolError{ActorToolErrorCode::kCreationMissingRequiredFields});
  }

  return std::unique_ptr<TypeTool>(
      new TypeTool(action, resolution_result.value().web_state));
}

void TypeTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(ActorToolErrorCode::kExecutionMissingDependencies));
    return;
  }
  web::WebFramesManager* frames_manager =
      js_feature_->GetWebFramesManager(web_state_.get());
  if (!frames_manager || !frames_manager->GetMainWebFrame()) {
    std::move(callback).Run(
        ToolExecutionResult(ActorToolErrorCode::kExecutionMissingDependencies));
    return;
  }

  ResolveTargetFrame(web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(),
                     action_.target(),
                     base::BindOnce(&TypeTool::OnTargetFrameResolved,
                                    weak_ptr_factory_.GetWeakPtr(), action_,
                                    std::move(callback)));
}

TypeTool::TypeTool(const optimization_guide::proto::TypeAction& action,
                   base::WeakPtr<web::WebState> web_state)
    : action_(action),
      web_state_(web_state),
      js_feature_(TypeToolJavaScriptFeature::GetInstance()) {}

void TypeTool::OnTargetFrameResolved(
    optimization_guide::proto::TypeAction action,
    ToolExecutionCallback callback,
    base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                   ActorToolError> result) {
  if (!result.has_value()) {
    std::move(callback).Run(ToolExecutionResult(result.error().external_code,
                                                result.error().message));
    return;
  }

  ActionTargetJavaScriptFeature::TargetFrameResult target_frame =
      result.value();
  web::WebFrame* target_web_frame = target_frame.frame;
  if (!target_web_frame) {
    std::move(callback).Run(
        ToolExecutionResult(ActorToolErrorCode::kExecutionMissingDependencies));
    return;
  }

  // Update the target with the potentially translated coordinates relative
  // to the target frame.
  *action.mutable_target() = target_frame.target;

  js_feature_->Type(target_web_frame->AsWeakPtr(), action, std::move(callback));
}

}  // namespace actor
