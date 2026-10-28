// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"

#import <stdint.h>

#import <optional>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/memory/weak_ptr.h"
#import "base/numerics/safe_conversions.h"
#import "base/timer/elapsed_timer.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_constants.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace actor {

namespace {

mojom::ActionResultCode ToActionResultCode(int code) {
  auto result_code = static_cast<TypeToolResultCode>(code);
  switch (result_code) {
    case TypeToolResultCode::kOk:
      return mojom::ActionResultCode::kOk;
    case TypeToolResultCode::kCoordinatesOutOfBounds:
      return mojom::ActionResultCode::kCoordinatesOutOfBounds;
    case TypeToolResultCode::kInvalidDomNodeId:
      return mojom::ActionResultCode::kInvalidDomNodeId;
    case TypeToolResultCode::kTypeTargetNotElement:
      return mojom::ActionResultCode::kTypeTargetNotElement;
    case TypeToolResultCode::kTypeTargetNotFocusable:
      return mojom::ActionResultCode::kTypeTargetNotFocusable;
    case TypeToolResultCode::kInvalidArguments:
      return mojom::ActionResultCode::kArgumentsInvalid;
    case TypeToolResultCode::kElementDisabled:
      return mojom::ActionResultCode::kElementDisabled;
  }
  NOTREACHED();
}

// Extracts the `fieldRendererId` returned by `type_tool.type` if possible or
// returns `autofill::FieldRendererId()` null sentinel.
autofill::FieldRendererId ExtractFieldRendererId(const base::Value* result) {
  if (!result || !result->is_dict()) {
    return autofill::FieldRendererId();
  }
  static constexpr char kFieldRendererIdKey[] = "fieldRendererId";
  if (std::optional<double> id =
          result->GetDict().FindDouble(kFieldRendererIdKey);
      id.has_value() && *id >= 1 &&
      base::IsValueInRangeForNumericType<uint32_t>(*id)) {
    return autofill::FieldRendererId(base::checked_cast<uint32_t>(*id));
  }
  return autofill::FieldRendererId();
}

const char kScriptName[] = "type_tool";

}  // namespace

// static
TypeToolJavaScriptFeature* TypeToolJavaScriptFeature::GetInstance() {
  static base::NoDestructor<TypeToolJavaScriptFeature> instance;
  return instance.get();
}

TypeToolJavaScriptFeature::TypeToolJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

TypeToolJavaScriptFeature::~TypeToolJavaScriptFeature() = default;

void TypeToolJavaScriptFeature::Type(
    base::WeakPtr<web::WebFrame> target_frame,
    const ActionTarget& target,
    const std::string& text,
    optimization_guide::proto::TypeAction_TypeMode mode,
    bool follow_by_enter,
    ToolExecutionCallback callback) {
  // `base::Unretained(this)` is safe because `TypeToolJavaScriptFeature` is a
  // `base::NoDestructor` singleton that is never destroyed.
  Type(target_frame, target, text, mode,
       base::BindOnce(&TypeToolJavaScriptFeature::OnTypeCompleted,
                      base::Unretained(this), target_frame, follow_by_enter,
                      std::move(callback)));
}

void TypeToolJavaScriptFeature::Type(
    base::WeakPtr<web::WebFrame> target_frame,
    const ActionTarget& target,
    const std::string& text,
    optimization_guide::proto::TypeAction_TypeMode mode,
    TypeExecutionCallback callback) {
  CHECK(target.is_valid());

  if (!target_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway),
        autofill::FieldRendererId());
    return;
  }

  base::ListValue parameters;
  parameters.Append(target.ToDictValue());
  parameters.Append(text);
  parameters.Append(static_cast<int>(mode));

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      target_frame.get(), "type_tool.type", parameters,
      base::BindOnce(
          [](TypeExecutionCallback callback, base::ElapsedTimer timer,
             const base::Value* result) {
            std::move(callback).Run(ParseJavaScriptResultWithResultCode(
                                        ActorJavaScriptCall::kType,
                                        &ToActionResultCode, result, timer),
                                    ExtractFieldRendererId(result));
          },
          std::move(cb_for_js), base::ElapsedTimer()),
      kActorJavaScriptExecutionTimeout);

  if (!sent) {
    std::move(cb_for_error)
        .Run(ToolExecutionResult(
                 InternalToolErrorCode::
                     kJavascriptFeatureFailedToCallJavaScriptFunction),
             autofill::FieldRendererId());
  }
}

void TypeToolJavaScriptFeature::OnTypeCompleted(
    base::WeakPtr<web::WebFrame> target_frame,
    bool follow_by_enter,
    ToolExecutionCallback callback,
    ToolExecutionResult result,
    autofill::FieldRendererId field_id) {
  if (result.IsOk() && follow_by_enter && !field_id.is_null()) {
    SubmitWithEnterFallback(
        std::move(target_frame), field_id,
        base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
  std::move(callback).Run(std::move(result));
}

void TypeToolJavaScriptFeature::SubmitWithEnterFallback(
    base::WeakPtr<web::WebFrame> target_frame,
    autofill::FieldRendererId field_id,
    base::OnceClosure callback) {
  if (!target_frame || field_id.is_null()) {
    std::move(callback).Run();
    return;
  }

  base::ListValue parameters;
  parameters.Append(static_cast<double>(field_id.value()));

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      target_frame.get(), "type_tool.submitWithEnterFallback", parameters,
      base::BindOnce(
          [](base::ElapsedTimer timer, const base::Value* result) {
            ParseJavaScriptResultWithResultCode(
                ActorJavaScriptCall::kTypeSubmitWithEnterFallback,
                &ToActionResultCode, result, timer);
          },
          base::ElapsedTimer())
          .Then(std::move(cb_for_js)),
      kActorJavaScriptExecutionTimeout);

  if (!sent) {
    std::move(cb_for_error).Run();
  }
}

}  // namespace actor
