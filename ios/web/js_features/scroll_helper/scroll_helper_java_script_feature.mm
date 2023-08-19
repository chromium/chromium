// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/scroll_helper/scroll_helper_java_script_feature.h"

#import "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace {
const char kScrollHelperScript[] = "scroll_helper";
}  // namespace

namespace web {

ScrollHelperJavaScriptFeature::ScrollHelperJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScrollHelperScript,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

ScrollHelperJavaScriptFeature::~ScrollHelperJavaScriptFeature() = default;

void ScrollHelperJavaScriptFeature::SetWebViewScrollViewIsDragging(
    WebState* web_state,
    bool dragging) {
  WebFrame* main_frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!main_frame)
    return;
  auto parameters = base::Value::List().Append(dragging);
  CallJavaScriptFunction(main_frame, "setWebViewScrollViewIsDragging",
                         parameters);
}

}  // namespace web
