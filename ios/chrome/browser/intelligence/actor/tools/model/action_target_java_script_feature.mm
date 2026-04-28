// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"

#import <optional>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "base/values.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

namespace {
const char kScriptName[] = "action_target";

struct ChildFrameData {
  std::string remote_frame_token;
  double frame_x;
  double frame_y;
};

/**
 * Parses and validates the result of action_target.resolveTargetIframe.
 */
base::expected<std::optional<ChildFrameData>, ToolExecutionResult>
ParseResolveTargetIframeResult(const base::Value* result) {
  if (!result || !result->is_dict()) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kJavascriptFeatureGotInvalidResult));
  }
  const base::DictValue& result_dict = result->GetDict();

  std::optional<bool> success = result_dict.FindBool("success");
  if (!success.value_or(false)) {
    const std::string* error_message = result_dict.FindString("message");
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution,
        error_message ? *error_message : "Unknown error in JS."));
  }

  const base::DictValue* child_frame = result_dict.FindDict("childFrame");
  if (!child_frame) {
    return std::nullopt;
  }

  const std::string* remote_frame_token =
      child_frame->FindString("remoteFrameToken");
  if (!remote_frame_token) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kJavascriptFeatureGotInvalidResult));
  }

  std::optional<double> frame_x = child_frame->FindDouble("frameX");
  std::optional<double> frame_y = child_frame->FindDouble("frameY");
  if (!frame_x || !frame_y) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kJavascriptFeatureGotInvalidResult));
  }

  return ChildFrameData{*remote_frame_token, *frame_x, *frame_y};
}

}  // namespace

// static
ActionTargetJavaScriptFeature* ActionTargetJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ActionTargetJavaScriptFeature> instance;
  return instance.get();
}

void ActionTargetJavaScriptFeature::GetTargetFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    const optimization_guide::proto::ActionTarget& target,
    TargetFrameCallback callback,
    int depth) {
  CHECK(web_frame);
  CHECK(web_state);
  CHECK(target.has_coordinate() || target.has_document_identifier());

  if (depth >= kMaxTargetIframeDepth) {
    std::move(callback).Run(base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kActorTargetMaxDepthExceeded)));
    return;
  }

  if (target.has_document_identifier()) {
    GetTargetFrameByDocumentIdentifier(web_state, target, std::move(callback));
    return;
  }

  GetTargetFrameByCoordinate(web_state, web_frame, target, std::move(callback),
                             depth);
}

ActionTargetJavaScriptFeature::ActionTargetJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

ActionTargetJavaScriptFeature::~ActionTargetJavaScriptFeature() = default;

void ActionTargetJavaScriptFeature::GetTargetFrameByDocumentIdentifier(
    web::WebState* web_state,
    const optimization_guide::proto::ActionTarget& target,
    TargetFrameCallback callback) {
  auto target_frame = GetWebFrameByRemoteFrameToken(
      web_state, target.document_identifier().serialized_token());

  if (!target_frame.has_value()) {
    std::move(callback).Run(base::unexpected(target_frame.error()));
    return;
  }

  std::move(callback).Run(TargetFrameResult{*target_frame, target});
}

void ActionTargetJavaScriptFeature::GetTargetFrameByCoordinate(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    const optimization_guide::proto::ActionTarget& target,
    TargetFrameCallback callback,
    int depth) {
  base::ListValue parameters;
  parameters.Append(target.coordinate().x());
  parameters.Append(target.coordinate().y());
  parameters.Append(static_cast<int>(target.coordinate().pixel_type()));

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      web_frame, "action_target.resolveTargetIframe", parameters,
      base::BindOnce(&ActionTargetJavaScriptFeature::OnTargetIframeResolved,
                     base::Unretained(GetInstance()), target,
                     web_state->GetWeakPtr(), web_frame->AsWeakPtr(),
                     std::move(cb_for_js), depth),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));
  if (!sent) {
    std::move(cb_for_error)
        .Run(base::unexpected(ToolExecutionResult(
            InternalToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction)));
  }
}

void ActionTargetJavaScriptFeature::OnTargetIframeResolved(
    optimization_guide::proto::ActionTarget target,
    base::WeakPtr<web::WebState> web_state,
    base::WeakPtr<web::WebFrame> current_frame,
    TargetFrameCallback callback,
    int depth,
    const base::Value* result) {
  if (!web_state) {
    std::move(callback).Run(
        base::expected<TargetFrameResult, ToolExecutionResult>(
            base::unexpected(ToolExecutionResult(
                InternalToolErrorCode::kActorTargetWebStateDestroyed))));
    return;
  }

  if (!current_frame) {
    std::move(callback).Run(base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kActorTargetFrameNotFoundById)));
    return;
  }

  auto parsed_result = ParseResolveTargetIframeResult(result);
  if (!parsed_result.has_value()) {
    std::move(callback).Run(base::unexpected(parsed_result.error()));
    return;
  }

  if (!parsed_result->has_value()) {
    std::move(callback).Run(TargetFrameResult{current_frame.get(), target});
    return;
  }

  ChildFrameData child_data = parsed_result->value();
  auto target_frame = GetWebFrameByRemoteFrameToken(
      web_state.get(), child_data.remote_frame_token);

  if (!target_frame.has_value()) {
    std::move(callback).Run(base::unexpected(target_frame.error()));
    return;
  }

  optimization_guide::proto::ActionTarget child_target = target;
  child_target.mutable_coordinate()->set_x(child_data.frame_x);
  child_target.mutable_coordinate()->set_y(child_data.frame_y);

  // Recurse into the frame to find nested frames.
  GetTargetFrame(web_state.get(), *target_frame, child_target,
                 std::move(callback), depth + 1);
}

base::expected<web::WebFrame*, ToolExecutionResult>
ActionTargetJavaScriptFeature::GetWebFrameByRemoteFrameToken(
    web::WebState* web_state,
    const std::string& remote_frame_token) {
  std::optional<base::UnguessableToken> remote_token =
      autofill::DeserializeJavaScriptFrameId(remote_frame_token);
  if (!remote_token) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kActorTargetInvalidRemoteFrameToken));
  }

  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state);

  // We can assume that the frame has already been registered during an
  // earlier call to fetch the page's APC.
  std::optional<autofill::LocalFrameToken> local_token =
      registrar->LookupChildFrame(autofill::RemoteFrameToken(*remote_token));

  if (!local_token) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kActorTargetFrameNotRegistered));
  }

  web::WebFrame* target_frame =
      GetWebFramesManager(web_state)->GetFrameWithId(local_token->ToString());

  if (!target_frame) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kActorTargetFrameNotFoundById));
  }

  return target_frame;
}

}  // namespace actor
