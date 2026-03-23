// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool.h"

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

ClickTool::~ClickTool() = default;

// static
base::expected<std::unique_ptr<ClickTool>, ActuationError> ClickTool::Create(
    const optimization_guide::proto::ClickAction& action,
    ProfileIOS* profile) {
  if (!action.has_tab_id()) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingRequiredFields});
  }

  auto resolution_result = ResolveTab(action.tab_id(), profile);
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }

  if (!action.has_click_count() || !action.has_click_type()) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingRequiredFields});
  }

  if (!action.has_target() || !action.target().has_coordinate()) {
    // Bling currently only uses the Coordinate field of the ActionTarget.
    // TODO(crbug.com/476461762): add support for (document_identifier,
    // dom_node_id).
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingRequiredFields});
  }

  return std::unique_ptr<ClickTool>(
      new ClickTool(action, resolution_result.value().web_state));
}

void ClickTool::Execute(ActuationCallback callback) {
  if (!web_state_) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kExecutionMissingDependencies}));
    return;
  }
  web::WebFramesManager* frames_manager =
      js_feature_->GetWebFramesManager(web_state_.get());
  if (!frames_manager || !frames_manager->GetMainWebFrame()) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kExecutionMissingDependencies}));
    return;
  }

  ResolveTargetFrame(web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(),
                     action_.target(),
                     base::BindOnce(&ClickTool::OnTargetFrameResolved,
                                    weak_ptr_factory_.GetWeakPtr(), action_,
                                    std::move(callback)));
}

void ClickTool::OnTargetFrameResolved(
    const optimization_guide::proto::ClickAction& action,
    ActuationCallback callback,
    base::expected<ActuationTargetJavaScriptFeature::TargetFrameResult,
                   ActuationError> result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }

  ActuationTargetJavaScriptFeature::TargetFrameResult target_frame =
      result.value();
  web::WebFrame* target_web_frame = target_frame.frame;
  if (!target_web_frame) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kExecutionMissingDependencies}));
    return;
  }

  // Update the target with the potentially translated coordinates relative
  // to the target frame.
  optimization_guide::proto::ClickAction new_action = action;
  *new_action.mutable_target() = target_frame.target;

  js_feature_->Click(target_web_frame, new_action, std::move(callback));
}

ClickTool::ClickTool(const optimization_guide::proto::ClickAction& action,
                     base::WeakPtr<web::WebState> web_state)
    : action_(action),
      web_state_(web_state),
      js_feature_(ClickToolJavaScriptFeature::GetInstance()) {}
