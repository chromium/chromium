// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool_java_script_feature.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {
const char kScriptName[] = "click_tool";
}  // namespace

namespace actor {

namespace {

mojom::ActionResultCode ToActionResultCode(int code) {
  auto result_code = static_cast<ClickToolResultCode>(code);
  switch (result_code) {
    case ClickToolResultCode::kOk:
      return mojom::ActionResultCode::kOk;
    case ClickToolResultCode::kCoordinatesOutOfBounds:
      return mojom::ActionResultCode::kCoordinatesOutOfBounds;
    case ClickToolResultCode::kInvalidDomNodeId:
      return mojom::ActionResultCode::kInvalidDomNodeId;
    case ClickToolResultCode::kElementDisabled:
      return mojom::ActionResultCode::kElementDisabled;
    case ClickToolResultCode::kClickSuppressed:
      return mojom::ActionResultCode::kClickSuppressed;
  }
  NOTREACHED();
}

}  // namespace

// static
ClickToolJavaScriptFeature* ClickToolJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ClickToolJavaScriptFeature> instance;
  return instance.get();
}

ClickToolJavaScriptFeature::ClickToolJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

ClickToolJavaScriptFeature::~ClickToolJavaScriptFeature() = default;

void ClickToolJavaScriptFeature::Click(
    base::WeakPtr<web::WebFrame> target_frame,
    const ActionTarget& target,
    optimization_guide::proto::ClickAction_ClickType click_type,
    int click_count,
    ToolExecutionCallback callback) {
  CHECK(target.node_id().has_value() || target.coordinate().has_value());

  if (!target_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  base::ListValue parameters;
  std::string function_name;

  if (target.node_id().has_value()) {
    parameters.Append(target.node_id()->content_node_id);
    parameters.Append(static_cast<int>(click_type));
    parameters.Append(click_count);
    function_name = "click_tool.clickByNodeId";
  } else if (target.coordinate().has_value()) {
    parameters.Append(target.coordinate()->x);
    parameters.Append(target.coordinate()->y);
    parameters.Append(static_cast<int>(click_type));
    parameters.Append(click_count);
    parameters.Append(static_cast<int>(target.coordinate()->pixel_type));
    function_name = "click_tool.clickByCoordinate";
  } else {
    NOTREACHED();
  }

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      target_frame.get(), function_name, parameters,
      base::BindOnce(
          [](ToolExecutionCallback cb, const base::Value* result) {
            std::move(cb).Run(ParseJavaScriptResultWithResultCode(
                &ToActionResultCode, result));
          },
          std::move(cb_for_js)),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));

  if (!sent) {
    std::move(cb_for_error)
        .Run(ToolExecutionResult(
            InternalToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction));
  }
}

}  // namespace actor
