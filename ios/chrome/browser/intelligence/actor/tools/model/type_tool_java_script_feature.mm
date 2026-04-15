// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {
const char kScriptName[] = "type_tool";
}  // namespace

namespace actor {

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
    const optimization_guide::proto::TypeAction& action,
    ToolExecutionCallback callback) {
  CHECK(action.has_target());
  CHECK(action.has_text() && action.has_mode());
  CHECK(action.target().has_coordinate() ||
        (action.target().has_content_node_id() &&
         action.target().has_document_identifier()));

  if (!target_frame) {
    std::move(callback).Run(base::unexpected(
        ActorToolError{ActorToolErrorCode::kActorTargetWebFrameInvalidated}));
    return;
  }

  base::ListValue parameters;
  std::string function_name;

  if (action.target().has_content_node_id()) {
    parameters.Append(action.target().content_node_id());
    parameters.Append(action.text());
    parameters.Append(static_cast<int>(action.mode()));
    parameters.Append(action.follow_by_enter());
    function_name = "type_tool.typeByNodeId";
  } else {
    parameters.Append(action.target().coordinate().x());
    parameters.Append(action.target().coordinate().y());
    parameters.Append(
        static_cast<int>(action.target().coordinate().pixel_type()));
    parameters.Append(action.text());
    parameters.Append(static_cast<int>(action.mode()));
    parameters.Append(action.follow_by_enter());
    function_name = "type_tool.typeByCoordinate";
  }

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      target_frame.get(), function_name, parameters,
      base::BindOnce(&ParseJavaScriptResult, std::move(cb_for_js)),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));

  if (!sent) {
    std::move(cb_for_error)
        .Run(base::unexpected(ActorToolError{
            ActorToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction}));
  }
}

}  // namespace actor
