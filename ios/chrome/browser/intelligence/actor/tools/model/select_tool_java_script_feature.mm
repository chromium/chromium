// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool_java_script_feature.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace actor {

namespace {

mojom::ActionResultCode ToActionResultCode(int code) {
  auto result_code = static_cast<SelectToolResultCode>(code);
  switch (result_code) {
    case SelectToolResultCode::kOk:
      return mojom::ActionResultCode::kOk;
    case SelectToolResultCode::kSelectInvalidElement:
      return mojom::ActionResultCode::kSelectInvalidElement;
    case SelectToolResultCode::kElementDisabled:
      return mojom::ActionResultCode::kElementDisabled;
    case SelectToolResultCode::kSelectOptionDisabled:
      return mojom::ActionResultCode::kSelectOptionDisabled;
    case SelectToolResultCode::kSelectNoSuchOption:
      return mojom::ActionResultCode::kSelectNoSuchOption;
    case SelectToolResultCode::kCoordinatesOutOfBounds:
      return mojom::ActionResultCode::kCoordinatesOutOfBounds;
    case SelectToolResultCode::kInvalidDomNodeId:
      return mojom::ActionResultCode::kInvalidDomNodeId;
  }
  NOTREACHED();
}

}  // namespace

namespace {
const char kScriptName[] = "select_tool";
}  // namespace

// static
SelectToolJavaScriptFeature* SelectToolJavaScriptFeature::GetInstance() {
  static base::NoDestructor<SelectToolJavaScriptFeature> instance;
  return instance.get();
}

void SelectToolJavaScriptFeature::Select(
    base::WeakPtr<web::WebFrame> target_frame,
    const optimization_guide::proto::SelectAction& action,
    ToolExecutionCallback callback) {
  CHECK(action.has_target());
  CHECK(action.has_value());
  CHECK(action.target().has_coordinate() ||
        (action.target().has_content_node_id() &&
         action.target().has_document_identifier()));

  if (!target_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  base::ListValue parameters;
  std::string function_name;

  if (action.target().has_content_node_id()) {
    parameters.Append(action.target().content_node_id());
    parameters.Append(action.value());
    function_name = "select_tool.selectByNodeId";
  } else {
    parameters.Append(action.target().coordinate().x());
    parameters.Append(action.target().coordinate().y());
    parameters.Append(
        static_cast<int>(action.target().coordinate().pixel_type()));
    parameters.Append(action.value());
    function_name = "select_tool.selectByCoordinate";
  }

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      target_frame.get(), function_name, parameters,
      base::BindOnce(
          [](ToolExecutionCallback callback, const base::Value* result) {
            std::move(callback).Run(ParseJavaScriptResultWithResultCode(
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

SelectToolJavaScriptFeature::SelectToolJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

SelectToolJavaScriptFeature::~SelectToolJavaScriptFeature() = default;

}  // namespace actor
