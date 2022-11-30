// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/scroll_helper/scroll_helper_java_script_feature.h"

#import "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  WebFrame* main_frame = GetMainFrame(web_state);
  if (!main_frame)
    return;
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(dragging));
  CallJavaScriptFunction(main_frame, "setWebViewScrollViewIsDragging",
                         parameters);
}

}  // namespace web
