// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/catch_gcrweb_script_errors_java_script_feature.h"

namespace {

const char kScriptName[] = "wrap_gcrweb_functions";

}  // namespace

namespace web {

// static
CatchGCrWebScriptErrorsJavaScriptFeature*
CatchGCrWebScriptErrorsJavaScriptFeature::GetInstance() {
  static base::NoDestructor<CatchGCrWebScriptErrorsJavaScriptFeature> instance;
  return instance.get();
}

CatchGCrWebScriptErrorsJavaScriptFeature::
    CatchGCrWebScriptErrorsJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAllContentWorlds,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}
CatchGCrWebScriptErrorsJavaScriptFeature::
    ~CatchGCrWebScriptErrorsJavaScriptFeature() = default;

}  // namespace web
