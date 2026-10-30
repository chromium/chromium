// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/stringprintf.h"
#import "base/types/expected.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/passwords/model/password_native_keystroke_autologin.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

TypeTool::~TypeTool() = default;

// static
std::unique_ptr<TypeTool> TypeTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::TypeAction& action) {
  ActionTarget target = ActionTarget::FromProto(action.target());
  return std::unique_ptr<TypeTool>(
      new TypeTool(web_state, action, std::move(target)));
}

void TypeTool::Validate(ToolExecutionCallback callback) {
  if (!action_.has_text() || !action_.has_mode()) {
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

void TypeTool::Execute(ToolExecutionCallback callback) {
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

  callback_ = std::move(callback);
  ResolveTargetFrame(web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(),
                     target_,
                     base::BindOnce(&TypeTool::OnTargetFrameResolved,
                                    weak_ptr_factory_.GetWeakPtr()));
}

base::WeakPtr<web::WebState> TypeTool::GetTargetWebState() const {
  return web_state_;
}

ToolType TypeTool::GetToolType() const {
  return ToolType::kType;
}

std::string TypeTool::DebugString() const {
  return base::StringPrintf(
      "TypeTool[%s;text(%s);mode(%s);FollowByEnter(%s)]",
      target_.DebugString().c_str(), action_.text().c_str(),
      optimization_guide::proto::TypeAction::TypeMode_Name(action_.mode())
          .c_str(),
      action_.follow_by_enter() ? "true" : "false");
}

TypeTool::TypeTool(base::WeakPtr<web::WebState> web_state,
                   const optimization_guide::proto::TypeAction& action,
                   ActionTarget target)
    : action_(action),
      target_(std::move(target)),
      web_state_(web_state),
      js_feature_(TypeToolJavaScriptFeature::GetInstance()) {}

void TypeTool::OnTargetFrameResolved(
    base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                   ToolExecutionResult> result) {
  if (!result.has_value()) {
    std::move(callback_).Run(result.error());
    return;
  }

  ActionTargetJavaScriptFeature::TargetFrameResult target_frame =
      result.value();
  web::WebFrame* target_web_frame = target_frame.frame;
  if (!target_web_frame) {
    std::move(callback_).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  target_frame_ = target_web_frame->AsWeakPtr();

  js_feature_->Type(target_web_frame->AsWeakPtr(), target_frame.target,
                    action_.text(), action_.mode(),
                    base::BindOnce(&TypeTool::OnTypeCompleted,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void TypeTool::OnTypeCompleted(ToolExecutionResult result,
                               autofill::FieldRendererId field_id) {
  if (!result.IsOk() || !action_.follow_by_enter() || !web_state_ ||
      !target_frame_ || field_id.is_null()) {
    std::move(callback_).Run(std::move(result));
    return;
  }

  TriggerAutoSubmissionWithResult(
      web_state_, target_frame_->GetFrameId(), field_id,
      base::BindOnce(&TypeTool::OnAutoSubmissionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result),
                     field_id),
      /*should_prevent_keyboard=*/false);
}

void TypeTool::OnAutoSubmissionCompleted(
    ToolExecutionResult result,
    autofill::FieldRendererId field_id,
    AutoSubmissionState auto_submission_state) {
  if (auto_submission_state.keystroke_dispatched || !target_frame_ ||
      field_id.is_null()) {
    std::move(callback_).Run(std::move(result));
    return;
  }

  js_feature_->SubmitWithEnterFallback(
      target_frame_, field_id,
      base::BindOnce(std::move(callback_), std::move(result)));
}

}  // namespace actor
