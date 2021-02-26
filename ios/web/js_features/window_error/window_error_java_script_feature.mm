// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/window_error_java_script_feature.h"

#import <WebKit/WebKit.h>

#include "ios/web/public/js_messaging/java_script_feature_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "error_js";

const char kWindowErrorResultHandlerName[] = "WindowErrorResultHandler";

static NSString* kScriptMessageResponseFilenameKey = @"filename";
static NSString* kScriptMessageResponseLineNumberKey = @"line_number";
static NSString* kScriptMessageResponseMessageKey = @"message";
}  // namespace

namespace web {

WindowErrorJavaScriptFeature::ErrorDetails::ErrorDetails()
    : is_main_frame(true) {}
WindowErrorJavaScriptFeature::ErrorDetails::~ErrorDetails() = default;

WindowErrorJavaScriptFeature::WindowErrorJavaScriptFeature(
    base::RepeatingCallback<void(ErrorDetails)> callback)
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature()}),
      callback_(std::move(callback)) {
  DCHECK(callback_);
}
WindowErrorJavaScriptFeature::~WindowErrorJavaScriptFeature() = default;

base::Optional<std::string>
WindowErrorJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kWindowErrorResultHandlerName;
}

void WindowErrorJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    WKScriptMessage* script_message) {
  ErrorDetails details;

  id filename = script_message.body[kScriptMessageResponseFilenameKey];
  if (filename && [filename isKindOfClass:[NSString class]]) {
    details.filename = filename;
  }

  id line_number = script_message.body[kScriptMessageResponseLineNumberKey];
  if (line_number && [line_number isKindOfClass:[NSNumber class]]) {
    details.line_number = [line_number intValue];
  }

  id message = script_message.body[kScriptMessageResponseMessageKey];
  if (message && [message isKindOfClass:[NSString class]]) {
    details.message = message;
  }

  WKFrameInfo* frame_info = script_message.frameInfo;
  if (frame_info) {
    details.is_main_frame = frame_info.isMainFrame;

    if (frame_info.request.URL) {
      details.url = net::GURLWithNSURL(frame_info.request.URL);
    }
  }

  callback_.Run(details);
}

}  // namespace web
