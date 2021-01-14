// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_util_impl.h"

#import <Foundation/Foundation.h>

#include "ios/web/public/js_messaging/java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kBaseScriptName[] = "base_js";
const char kCommonScriptName[] = "common_js";
const char kMessageScriptName[] = "message_js";
}  // namespace

namespace web {
namespace java_script_features {

std::vector<JavaScriptFeature*> GetBuiltInJavaScriptFeatures() {
  return {};
}

JavaScriptFeature* GetBaseJavaScriptFeature() {
  // Static storage is ok for |base_feature| as it holds no state.
  static std::unique_ptr<JavaScriptFeature> base_feature = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    std::vector<const JavaScriptFeature::FeatureScript> feature_scripts = {
        JavaScriptFeature::FeatureScript::CreateWithFilename(
            kBaseScriptName,
            JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
            JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

    base_feature = std::make_unique<JavaScriptFeature>(
        JavaScriptFeature::ContentWorld::kAnyContentWorld, feature_scripts);
  });
  return base_feature.get();
}

JavaScriptFeature* GetCommonJavaScriptFeature() {
  // Static storage is ok for |common_feature| as it holds no state.
  static std::unique_ptr<JavaScriptFeature> common_feature = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    std::vector<const JavaScriptFeature::FeatureScript> feature_scripts = {
        JavaScriptFeature::FeatureScript::CreateWithFilename(
            kCommonScriptName,
            JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
            JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

    std::vector<const JavaScriptFeature*> dependencies = {
        GetBaseJavaScriptFeature()};

    common_feature = std::make_unique<JavaScriptFeature>(
        JavaScriptFeature::ContentWorld::kAnyContentWorld, feature_scripts,
        dependencies);
  });
  return common_feature.get();
}

JavaScriptFeature* GetMessageJavaScriptFeature() {
  // Static storage is ok for |message_feature| as it holds no state.
  static std::unique_ptr<JavaScriptFeature> message_feature = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    std::vector<const JavaScriptFeature::FeatureScript> feature_scripts = {
        JavaScriptFeature::FeatureScript::CreateWithFilename(
            kMessageScriptName,
            JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
            JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

    std::vector<const JavaScriptFeature*> dependencies = {
        GetCommonJavaScriptFeature()};

    message_feature = std::make_unique<JavaScriptFeature>(
        JavaScriptFeature::ContentWorld::kAnyContentWorld, feature_scripts,
        dependencies);
  });
  return message_feature.get();
}

}  // namespace java_script_features
}  // namespace web
