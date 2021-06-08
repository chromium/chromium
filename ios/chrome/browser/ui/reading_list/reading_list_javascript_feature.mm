// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_javascript_feature.h"

#include "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "distiller_js";
const char kScriptHandlerName[] = "ReadingListDOMMessageHandler";
}  // namespace

ReadingListJavaScriptFeature::ReadingListJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

ReadingListJavaScriptFeature::~ReadingListJavaScriptFeature() = default;

absl::optional<std::string>
ReadingListJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void ReadingListJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    // Ignore malformed responses.
    return;
  }

  // TODO(crbug.com/1195978): pass result into
  // dom_distiller::CalculateDerivedFeatures().
}
