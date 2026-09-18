// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/drag_and_release_tool.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/drag_and_release_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

DragAndReleaseTool::~DragAndReleaseTool() = default;

// static
std::unique_ptr<DragAndReleaseTool> DragAndReleaseTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::DragAndReleaseAction& action) {
  ActionTarget from_target = ActionTarget::FromProto(action.from_target());
  ActionTarget to_target = ActionTarget::FromProto(action.to_target());
  return base::WrapUnique(new DragAndReleaseTool(
      web_state, std::move(from_target), std::move(to_target)));
}

void DragAndReleaseTool::Validate(ToolExecutionCallback callback) {
  if (!from_target_.is_valid() || !to_target_.is_valid()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  std::move(callback).Run(ToolExecutionResult::Ok());
}

void DragAndReleaseTool::Execute(ToolExecutionCallback callback) {
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
      web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(), from_target_,
      base::BindOnce(&DragAndReleaseTool::OnFromTargetFrameResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

base::WeakPtr<web::WebState> DragAndReleaseTool::GetTargetWebState() const {
  return web_state_;
}

ToolType DragAndReleaseTool::GetToolType() const {
  return ToolType::kDragAndRelease;
}

DragAndReleaseTool::DragAndReleaseTool(base::WeakPtr<web::WebState> web_state,
                                       ActionTarget from_target,
                                       ActionTarget to_target)
    : from_target_(std::move(from_target)),
      to_target_(std::move(to_target)),
      web_state_(web_state),
      js_feature_(DragAndReleaseToolJavaScriptFeature::GetInstance()) {}

void DragAndReleaseTool::OnFromTargetFrameResolved(
    ToolExecutionCallback callback,
    base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                   ToolExecutionResult> result) {
  if (!result.has_value()) {
    std::move(callback).Run(result.error());
    return;
  }

  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  ActionTargetJavaScriptFeature::TargetFrameResult from_target_result =
      std::move(result.value());
  web::WebFrame* source_web_frame = from_target_result.frame;
  if (!source_web_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  base::WeakPtr<web::WebFrame> from_frame = source_web_frame->AsWeakPtr();
  ActionTarget from_target = std::move(from_target_result.target);

  web::WebFramesManager* frames_manager =
      js_feature_->GetWebFramesManager(web_state_.get());
  if (!frames_manager || !frames_manager->GetMainWebFrame()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  ResolveTargetFrame(
      web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(), to_target_,
      base::BindOnce(&DragAndReleaseTool::OnToTargetFrameResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     from_frame, std::move(from_target)));
}

void DragAndReleaseTool::OnToTargetFrameResolved(
    ToolExecutionCallback callback,
    base::WeakPtr<web::WebFrame> from_frame,
    ActionTarget from_target,
    base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                   ToolExecutionResult> result) {
  if (!result.has_value()) {
    std::move(callback).Run(result.error());
    return;
  }

  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  ActionTargetJavaScriptFeature::TargetFrameResult to_target_result =
      std::move(result.value());
  web::WebFrame* to_web_frame = to_target_result.frame;
  if (!to_web_frame || !from_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  if (from_frame.get() != to_web_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kNotImplemented,
                            /*requires_page_stabilization=*/false,
                            "Cross-frame drag and release is not supported."));
    return;
  }

  target_frame_ = from_frame;

  js_feature_->DragAndRelease(from_frame, from_target, to_target_result.target,
                              std::move(callback));
}

}  // namespace actor
