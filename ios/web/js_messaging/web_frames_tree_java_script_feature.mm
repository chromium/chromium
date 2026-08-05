// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frames_tree_java_script_feature.h"

#import "base/strings/sys_string_conversions.h"
#import "base/unguessable_token.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace web {

namespace {

const char kFrameTreeScriptName[] = "frame_tree";
const char kFrameChildRegistrationHandlerName[] = "FrameChildRegistration";

NSString* GetSecretToken() {
  static NSString* const kSecretToken =
      base::SysUTF8ToNSString(base::UnguessableToken::Create().ToString());
  return kSecretToken;
}

}  // namespace

// static
WebFramesTreeJavaScriptFeature* WebFramesTreeJavaScriptFeature::GetInstance() {
  static base::NoDestructor<WebFramesTreeJavaScriptFeature> instance;
  return instance.get();
}

WebFramesTreeJavaScriptFeature::WebFramesTreeJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kFrameTreeScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kReinjectOnDocumentRecreation,
              base::BindRepeating(^NSDictionary<NSString*, NSString*>*() {
                return @{@"$(SECRET)" : GetSecretToken()};
              }))}) {}

WebFramesTreeJavaScriptFeature::~WebFramesTreeJavaScriptFeature() = default;

std::optional<std::string>
WebFramesTreeJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kFrameChildRegistrationHandlerName;
}

void WebFramesTreeJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& message) {
  // TODO(crbug.com/539923959): Implement child frame registration.
}

}  // namespace web
