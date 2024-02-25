// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/font_size/font_size_java_script_feature.h"

#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {
const char kFontSizeScript[] = "font_size";
}  // namespace

// static
FontSizeJavaScriptFeature* FontSizeJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FontSizeJavaScriptFeature> instance;
  return instance.get();
}

void FontSizeJavaScriptFeature::AdjustFontSize(web::WebState* web_state,
                                               int size) {
  for (web::WebFrame* frame :
       web_state->GetPageWorldWebFramesManager()->GetAllWebFrames()) {
    AdjustFontSize(frame, size);
  }
}

void FontSizeJavaScriptFeature::AdjustFontSize(web::WebFrame* web_frame,
                                               int size) {
  base::Value::List parameters;
  parameters.Append(size);
  CallJavaScriptFunction(web_frame, "font_size.adjustFontSize", parameters);
}

FontSizeJavaScriptFeature::FontSizeJavaScriptFeature()
    : web::JavaScriptFeature(web::ContentWorld::kIsolatedWorld,
                             {FeatureScript::CreateWithFilename(
                                 kFontSizeScript,
                                 FeatureScript::InjectionTime::kDocumentStart,
                                 FeatureScript::TargetFrames::kAllFrames)}) {}

FontSizeJavaScriptFeature::~FontSizeJavaScriptFeature() = default;
