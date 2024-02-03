// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/fullscreen/fullscreen_java_script_feature.h"

#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
const char kScriptName[] = "fullscreen";

const char kViewportConfigurationHandlerName[] = "FullscreenViewportHandler";

static const char kScriptMessageViewportFitCoverKey[] = "cover";
}  // namespace

namespace web {

// static
FullscreenJavaScriptFeature* FullscreenJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FullscreenJavaScriptFeature> instance;
  return instance.get();
}

FullscreenJavaScriptFeature::FullscreenJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}
FullscreenJavaScriptFeature::~FullscreenJavaScriptFeature() = default;

std::optional<std::string>
FullscreenJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kViewportConfigurationHandlerName;
}

void FullscreenJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  const base::Value::Dict* script_dict =
      script_message.body() ? script_message.body()->GetIfDict() : nullptr;
  if (!script_dict) {
    return;
  }

  std::optional<bool> viewport_fit_cover =
      script_dict->FindBool(kScriptMessageViewportFitCoverKey);
  if (viewport_fit_cover) {
    // TODO(crbug.com/1394631): Implement logic to correctly handle
    // viewport-fit:cover.
  }
}

}  // namespace web
