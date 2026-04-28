// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_tool_java_script_feature.h"

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
const char kScriptName[] = "scroll_tool";
}  // namespace

namespace actor {

// static
ScrollToolJavaScriptFeature* ScrollToolJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ScrollToolJavaScriptFeature> instance;
  return instance.get();
}

ScrollToolJavaScriptFeature::ScrollToolJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

ScrollToolJavaScriptFeature::~ScrollToolJavaScriptFeature() = default;

void ScrollToolJavaScriptFeature::Scroll(
    base::WeakPtr<web::WebFrame> target_frame,
    const optimization_guide::proto::ScrollAction& action,
    ToolExecutionCallback callback) {
  CHECK(action.has_target());
  CHECK(action.has_direction() && action.has_distance());
  ExecuteScrollAction(target_frame, action.target(),
                      /*direction_and_distance=*/
                      std::make_pair(action.direction(), action.distance()),
                      std::move(callback));
}

void ScrollToolJavaScriptFeature::ScrollTo(
    base::WeakPtr<web::WebFrame> target_frame,
    const optimization_guide::proto::ScrollToAction& action,
    ToolExecutionCallback callback) {
  CHECK(action.has_target());
  ExecuteScrollAction(target_frame, action.target(),
                      /*direction_and_distance=*/std::nullopt,
                      std::move(callback));
}

void ScrollToolJavaScriptFeature::ExecuteScrollAction(
    base::WeakPtr<web::WebFrame> web_frame,
    const optimization_guide::proto::ActionTarget& target,
    std::optional<
        std::pair<optimization_guide::proto::ScrollAction_ScrollDirection, int>>
        direction_and_distance,
    ToolExecutionCallback callback) {
  CHECK(target.has_coordinate() ||
        (target.has_content_node_id() && target.has_document_identifier()));

  if (!web_frame) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kActorTargetWebFrameInvalidated));
    return;
  }

  base::ListValue parameters;
  std::string function_name;

  if (target.has_content_node_id()) {
    function_name = "scroll_tool.scrollByNodeId";
    parameters.Append(target.content_node_id());
  } else {
    function_name = "scroll_tool.scrollByCoordinate";
    parameters.Append(target.coordinate().x());
    parameters.Append(target.coordinate().y());
    parameters.Append(static_cast<int>(target.coordinate().pixel_type()));
  }

  if (direction_and_distance.has_value()) {
    parameters.Append(static_cast<int>(direction_and_distance->first));
    parameters.Append(direction_and_distance->second);
  }

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      web_frame.get(), function_name, parameters,
      base::BindOnce(&ParseJavaScriptResult, std::move(cb_for_js)),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));

  if (!sent) {
    std::move(cb_for_error)
        .Run(ToolExecutionResult(
            InternalToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction));
  }
}

}  // namespace actor
