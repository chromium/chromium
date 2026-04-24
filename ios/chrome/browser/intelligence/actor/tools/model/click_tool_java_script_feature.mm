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
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {
const char kScriptName[] = "click_tool";
}  // namespace

namespace actor {

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
    const optimization_guide::proto::ClickAction& action,
    ToolExecutionCallback callback) {
  CHECK(action.has_target());
  CHECK(action.has_click_count() && action.has_click_type());
  CHECK(action.target().has_coordinate() ||
        (action.target().has_content_node_id() &&
         action.target().has_document_identifier()));

  if (!target_frame) {
    std::move(callback).Run(ToolExecutionResult(
        ActorToolErrorCode::kActorTargetWebFrameInvalidated));
    return;
  }

  base::ListValue parameters;
  std::string function_name;

  if (action.target().has_content_node_id()) {
    parameters.Append(action.target().content_node_id());
    parameters.Append(static_cast<int>(action.click_type()));
    parameters.Append(static_cast<int>(action.click_count()));
    function_name = "click_tool.clickByNodeId";
  } else if (action.target().has_coordinate()) {
    parameters.Append(action.target().coordinate().x());
    parameters.Append(action.target().coordinate().y());
    parameters.Append(static_cast<int>(action.click_type()));
    parameters.Append(static_cast<int>(action.click_count()));
    parameters.Append(
        static_cast<int>(action.target().coordinate().pixel_type()));
    function_name = "click_tool.clickByCoordinate";
  } else {
    NOTREACHED();
  }

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      target_frame.get(), function_name, parameters,
      base::BindOnce(&ParseJavaScriptResult, std::move(cb_for_js)),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));

  if (!sent) {
    std::move(cb_for_error)
        .Run(ToolExecutionResult(
            ActorToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction));
  }
}

}  // namespace actor
