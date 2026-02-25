// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool_java_script_feature.h"

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {
const char kScriptName[] = "click_tool";
}  // namespace

// static
ClickToolJavaScriptFeature* ClickToolJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ClickToolJavaScriptFeature> instance;
  return instance.get();
}

// Note: Error strings in this file are for debugging purposes and are not
// displayed to the user.
ClickToolJavaScriptFeature::ClickToolJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              // TODO (crbug.com/476090817) - Inject in all frames once we can
              // reliably identify iframes in JS and native code.
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

ClickToolJavaScriptFeature::~ClickToolJavaScriptFeature() = default;

void ClickToolJavaScriptFeature::Click(
    web::WebFrame* web_frame,
    const optimization_guide::proto::ClickAction& action,
    ActuationTool::ActuationCallback callback) {
  CHECK(web_frame);
  CHECK(action.has_target() && action.target().has_coordinate());
  CHECK(action.has_click_count() && action.has_click_type());

  base::ListValue parameters;
  parameters.Append(action.target().coordinate().x());
  parameters.Append(action.target().coordinate().y());
  parameters.Append(static_cast<int>(action.click_type()));
  parameters.Append(static_cast<int>(action.click_count()));
  parameters.Append(
      static_cast<int>(action.target().coordinate().pixel_type()));

  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      web_frame, "click_tool.clickByCoordinate", parameters,
      base::BindOnce(&ClickToolJavaScriptFeature::ProcessClickResult,
                     base::Unretained(GetInstance()), std::move(cb_for_js)),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));

  if (!sent) {
    std::move(cb_for_error)
        .Run(base::unexpected(ActuationError{
            ActuationErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction}));
  }
}

void ClickToolJavaScriptFeature::ProcessClickResult(
    ActuationTool::ActuationCallback callback,
    const base::Value* click_result) {
  if (!click_result || !click_result->is_dict()) {
    std::move(callback).Run(base::unexpected(ActuationError{
        ActuationErrorCode::kJavascriptFeatureGotInvalidResult}));
    return;
  }
  const base::DictValue& result_dict = click_result->GetDict();
  bool success = result_dict.FindBool("success").value_or(false);
  const std::string* error_message = result_dict.FindString("message");
  if (!success) {
    // TODO: (crbug.com/476090817) - Add support for actuating in iframes here.
    std::move(callback).Run(base::unexpected(ActuationError{
        ActuationErrorCode::kJavascriptFeatureFailedInJavaScriptExecution,
        error_message ? *error_message : "Unknown error in JS."}));
    return;
  }
  std::move(callback).Run(base::ok());
}
