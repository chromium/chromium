// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/print/print_java_script_feature.h"

#import "base/values.h"
#import "ios/chrome/browser/web/model/print/print_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "print";
const char kScriptHandlerName[] = "PrintMessageHandler";
}  // namespace

PrintJavaScriptFeature::PrintJavaScriptFeature()
    : JavaScriptFeature(
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

PrintJavaScriptFeature::~PrintJavaScriptFeature() = default;

std::optional<std::string> PrintJavaScriptFeature::GetScriptMessageHandlerName()
    const {
  return kScriptHandlerName;
}

void PrintJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  DCHECK(web_state);

  if (!message.is_main_frame() && !message.is_user_interacting()) {
    // Ignore non user-initiated window.print() calls from iframes, to prevent
    // abusive behavior from web sites.
    return;
  }

  PrintTabHelper* helper = PrintTabHelper::GetOrCreateForWebState(web_state);
  if (!helper) {
    return;
  }

  helper->Print();
}
