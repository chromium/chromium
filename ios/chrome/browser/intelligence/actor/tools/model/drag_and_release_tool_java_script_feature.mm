// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/drag_and_release_tool_java_script_feature.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/values.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace actor {

namespace {

constexpr char kScriptName[] = "drag_and_release_tool";

mojom::ActionResultCode ToActionResultCode(int code) {
  auto result_code = static_cast<DragAndReleaseToolResultCode>(code);
  switch (result_code) {
    case DragAndReleaseToolResultCode::kOk:
      return mojom::ActionResultCode::kOk;
    case DragAndReleaseToolResultCode::kFromCoordinatesOutOfBounds:
      return mojom::ActionResultCode::kDragAndReleaseFromOffscreen;
    case DragAndReleaseToolResultCode::kToCoordinatesOutOfBounds:
      return mojom::ActionResultCode::kDragAndReleaseToOffscreen;
    case DragAndReleaseToolResultCode::kFromInvalidDomNodeId:
    case DragAndReleaseToolResultCode::kToInvalidDomNodeId:
      return mojom::ActionResultCode::kInvalidDomNodeId;
    case DragAndReleaseToolResultCode::kDragSuppressed:
      return mojom::ActionResultCode::kDragAndReleaseDownSuppressed;
    case DragAndReleaseToolResultCode::kFromElementDisabled:
    case DragAndReleaseToolResultCode::kToElementDisabled:
      return mojom::ActionResultCode::kElementDisabled;
  }
  NOTREACHED();
}

}  // namespace

// static
DragAndReleaseToolJavaScriptFeature*
DragAndReleaseToolJavaScriptFeature::GetInstance() {
  static base::NoDestructor<DragAndReleaseToolJavaScriptFeature> instance;
  return instance.get();
}

void DragAndReleaseToolJavaScriptFeature::DragAndRelease(
    base::WeakPtr<web::WebFrame> target_frame,
    const ActionTarget& from_target,
    const ActionTarget& to_target,
    ToolExecutionCallback callback) {
  CHECK(from_target.is_valid());
  CHECK(to_target.is_valid());

  if (!target_frame) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  base::ListValue parameters;
  parameters.Append(from_target.ToDictValue());
  parameters.Append(to_target.ToDictValue());

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      target_frame.get(), "drag_and_release_tool.dragAndRelease", parameters,
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

DragAndReleaseToolJavaScriptFeature::DragAndReleaseToolJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

DragAndReleaseToolJavaScriptFeature::~DragAndReleaseToolJavaScriptFeature() =
    default;

}  // namespace actor
