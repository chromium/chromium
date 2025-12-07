// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/error_event_listener_java_script_feature.h"

namespace {
const char kScriptName[] = "error";
}  // namespace

namespace web {

// static
ErrorEventListenerJavaScriptFeature*
ErrorEventListenerJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ErrorEventListenerJavaScriptFeature> instance;
  return instance.get();
}

ErrorEventListenerJavaScriptFeature::ErrorEventListenerJavaScriptFeature()
    : JavaScriptFeature(
          // Errors reported to the JavaScript error event listeners span across
          // content worlds so listening in a single world is sufficient for all
          // errors to be reported. Additionally, note that this is why this
          // script can't be registered in
          // `ScriptErrorMessageHandlerJavaScriptFeature` as errors would be
          // double reported when using `kAllContentWorlds`.
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)}) {}
ErrorEventListenerJavaScriptFeature::~ErrorEventListenerJavaScriptFeature() =
    default;

}  // namespace web
