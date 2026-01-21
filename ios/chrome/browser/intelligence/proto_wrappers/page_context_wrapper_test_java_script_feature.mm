// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_test_java_script_feature.h"

namespace {
const char kScriptName[] = "page_context_wrapper_test";
}  // namespace

PageContextWrapperTestJavaScriptFeature*
PageContextWrapperTestJavaScriptFeature::GetInstance() {
  static base::NoDestructor<PageContextWrapperTestJavaScriptFeature> instance;
  return instance.get();
}

PageContextWrapperTestJavaScriptFeature::
    PageContextWrapperTestJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {web::JavaScriptFeature::FeatureScript::CreateWithFilename(
              kScriptName,
              web::JavaScriptFeature::FeatureScript::InjectionTime::
                  kDocumentStart,
              web::JavaScriptFeature::FeatureScript::TargetFrames::
                  kAllFrames)}) {}

PageContextWrapperTestJavaScriptFeature::
    ~PageContextWrapperTestJavaScriptFeature() = default;
