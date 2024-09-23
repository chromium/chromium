// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"

#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/common/features.h"
#import "ios/web/js_features/context_menu/context_menu_params_utils.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace {
const char kContextMenuJavaScriptFeatureKeyName[] =
    "context_menu_java_script_feature";

const char kAllFramesContextMenuScript[] = "all_frames_context_menu";
const char kMainFrameContextMenuScript[] = "main_frame_context_menu";

const char kFindElementResultHandlerName[] = "FindElementResultHandler";
}

namespace web {

ContextMenuJavaScriptFeature::ContextMenuJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
               kAllFramesContextMenuScript,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames),
           FeatureScript::CreateWithFilename(
               kMainFrameContextMenuScript,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kMainFrame)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}
ContextMenuJavaScriptFeature::~ContextMenuJavaScriptFeature() = default;

// static
ContextMenuJavaScriptFeature* ContextMenuJavaScriptFeature::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  ContextMenuJavaScriptFeature* feature =
      static_cast<ContextMenuJavaScriptFeature*>(
          browser_state->GetUserData(kContextMenuJavaScriptFeatureKeyName));
  if (!feature) {
    feature = new ContextMenuJavaScriptFeature();
    browser_state->SetUserData(kContextMenuJavaScriptFeatureKeyName,
                               base::WrapUnique(feature));
  }
  return feature;
}

void ContextMenuJavaScriptFeature::GetElementAtPoint(
    WebState* web_state,
    std::string requestID,
    CGPoint point,
    ElementDetailsCallback callback) {
  callbacks_[requestID] = std::move(callback);

  WebFrame* main_frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  base::Value::List parameters;
  parameters.Append(requestID);
  parameters.Append(point.x);
  parameters.Append(point.y);
  CallJavaScriptFunction(main_frame, "contextMenu.findElementAtPoint",
                         parameters);
}

std::optional<std::string>
ContextMenuJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kFindElementResultHandlerName;
}

void ContextMenuJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& message) {
  if (!message.body()) {
    // Ignore malformed responses.
    return;
  }
  const auto* dict = message.body()->GetIfDict();
  if (!dict) {
    // Ignore malformed responses.
    return;
  }

  const std::string* request_id = dict->FindString("requestId");
  if (!request_id || request_id->empty()) {
    // Ignore malformed responses.
    return;
  }

  auto callback_it = callbacks_.find(*request_id);
  if (callback_it == callbacks_.end()) {
    return;
  }

  ElementDetailsCallback callback = std::move(callback_it->second);
  if (callback.is_null()) {
    return;
  }

  web::ContextMenuParams params =
      web::ContextMenuParamsFromElementDictionary(*dict);
  params.is_main_frame = message.is_main_frame();

  std::move(callback).Run(*request_id, params);
}

}  // namespace web
