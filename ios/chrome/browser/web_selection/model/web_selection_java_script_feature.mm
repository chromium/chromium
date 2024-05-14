// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_selection/model/web_selection_java_script_feature.h"

#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/chrome/browser/web_selection/model/web_selection_java_script_feature_observer.h"
#import "ios/chrome/browser/web_selection/model/web_selection_response.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "web_selection";
const char kWebSelectionFunctionName[] = "webSelection.getSelectedText";
const char kScriptHandlerName[] = "WebSelection";

WebSelectionResponse* ParseResponse(base::WeakPtr<web::WebState> weak_web_state,
                                    const base::Value::Dict& dict) {
  web::WebState* web_state = weak_web_state.get();
  if (!web_state) {
    return [WebSelectionResponse invalidResponse];
  }
  return [WebSelectionResponse selectionResponseWithDict:dict
                                                webState:web_state];
}

}  // namespace

// TODO(crbug.com/40256864): migrate to kIsolatedWorld.
WebSelectionJavaScriptFeature::WebSelectionJavaScriptFeature()
    : JavaScriptFeature(web::ContentWorld::kPageContentWorld,
                        {FeatureScript::CreateWithFilename(
                            kScriptName,
                            FeatureScript::InjectionTime::kDocumentStart,
                            FeatureScript::TargetFrames::kAllFrames,
                            FeatureScript::ReinjectionBehavior::
                                kReinjectOnDocumentRecreation)},
                        {}) {}

void WebSelectionJavaScriptFeature::AddObserver(
    WebSelectionJavaScriptFeatureObserver* observer) {
  observers_.AddObserver(observer);
}

void WebSelectionJavaScriptFeature::RemoveObserver(
    WebSelectionJavaScriptFeatureObserver* observer) {
  observers_.RemoveObserver(observer);
}

WebSelectionJavaScriptFeature::~WebSelectionJavaScriptFeature() = default;

// static
WebSelectionJavaScriptFeature* WebSelectionJavaScriptFeature::GetInstance() {
  static base::NoDestructor<WebSelectionJavaScriptFeature> instance;
  return instance.get();
}

bool WebSelectionJavaScriptFeature::GetSelectedText(web::WebState* web_state) {
  DCHECK(web_state);
  web::WebFrame* frame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();

  if (!frame) {
    return false;
  }
  return CallJavaScriptFunction(frame, kWebSelectionFunctionName,
                                /* parameters= */ {});
}

// pragma mark - web::JavaScriptFeature methods

std::optional<std::string>
WebSelectionJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void WebSelectionJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  if (observers_.empty()) {
    // There is no observer waiting for selection retrieval. Ignore the message.
    return;
  }
  base::Value* response = script_message.body();
  if (!response || !response->is_dict()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  WebSelectionResponse* web_response =
      ParseResponse(web_state->GetWeakPtr(), response->GetDict());
  if (![web_response isValid]) {
    // No not forward invalid response.
    return;
  }
  for (auto& observer : observers_) {
    observer.OnSelectionRetrieved(web_state, web_response);
  }
}
