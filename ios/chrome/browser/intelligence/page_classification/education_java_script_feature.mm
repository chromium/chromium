// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/education_java_script_feature.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {

constexpr char kScriptName[] = "education_page_detector";
constexpr char kExtractDOMFeaturesFunction[] =
    "education_page_detector.extractDOMFeatures";

void ProcessDOMFeaturesResult(
    base::OnceCallback<void(std::optional<EducationDOMFeatures>)> callback,
    const base::Value* response) {
  if (!response || !response->is_dict()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const base::DictValue& dict = response->GetDict();
  std::optional<int> word_count = dict.FindInt("word_count");
  std::optional<int> heading_count = dict.FindInt("heading_count");

  if (!word_count || !heading_count) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  EducationDOMFeatures features;
  features.word_count = *word_count;
  features.heading_count = *heading_count;
  std::move(callback).Run(features);
}

}  // namespace

// static
EducationJavaScriptFeature* EducationJavaScriptFeature::GetInstance() {
  static base::NoDestructor<EducationJavaScriptFeature> instance;
  return instance.get();
}

EducationJavaScriptFeature::EducationJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {web::JavaScriptFeature::FeatureScript::CreateWithFilename(
              kScriptName,
              web::JavaScriptFeature::FeatureScript::InjectionTime::
                  kDocumentStart,
              web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame,
              web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
                  kInjectOncePerWindow)}) {}

EducationJavaScriptFeature::~EducationJavaScriptFeature() = default;

void EducationJavaScriptFeature::ExtractDOMFeatures(
    web::WebFrame* frame,
    base::TimeDelta timeout,
    base::OnceCallback<void(std::optional<EducationDOMFeatures>)> callback) {
  if (!frame) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  CallJavaScriptFunction(
      frame, kExtractDOMFeaturesFunction, {},
      base::BindOnce(&ProcessDOMFeaturesResult, std::move(callback)), timeout);
}
