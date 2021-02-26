// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/java_script_console/java_script_console_feature.h"

#import <WebKit/WebKit.h>

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/web/java_script_console/java_script_console_feature_delegate.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_message.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "console_js";

const char kConsoleScriptHandlerName[] = "ConsoleMessageHandler";

static NSString* kConsoleMessageKey = @"message";
static NSString* kConsoleMessageLogLevelKey = @"log_level";
static NSString* kConsoleMessageUrlKey = @"url";
static NSString* kSenderFrameIdKey = @"sender_frame";
}  // namespace

JavaScriptConsoleFeature::JavaScriptConsoleFeature()
    : JavaScriptFeature(
          ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature(),
           web::java_script_features::GetMessageJavaScriptFeature()}) {}

JavaScriptConsoleFeature::~JavaScriptConsoleFeature() = default;

void JavaScriptConsoleFeature::SetDelegate(
    JavaScriptConsoleFeatureDelegate* delegate) {
  delegate_ = delegate;
}

base::Optional<std::string>
JavaScriptConsoleFeature::GetScriptMessageHandlerName() const {
  return kConsoleScriptHandlerName;
}

void JavaScriptConsoleFeature::ScriptMessageReceived(
    web::WebState* web_state,
    WKScriptMessage* script_message) {
  // Completely skip processing the message if no delegate exists.
  if (!delegate_) {
    return;
  }

  NSString* frame_id =
      base::mac::ObjCCast<NSString>(script_message.body[kSenderFrameIdKey]);
  if (!frame_id) {
    return;
  }

  web::WebFrame* sender_frame =
      web_state->GetWebFramesManager()->GetFrameWithId(
          base::SysNSStringToUTF8(frame_id));
  if (!sender_frame) {
    return;
  }

  NSString* log_message =
      base::mac::ObjCCast<NSString>(script_message.body[kConsoleMessageKey]);
  if (!log_message) {
    return;
  }

  NSString* log_level = base::mac::ObjCCast<NSString>(
      script_message.body[kConsoleMessageLogLevelKey]);
  if (!log_level) {
    return;
  }

  // At this point the message format has been validated and can be displayed.

  JavaScriptConsoleMessage frame_message;
  frame_message.level = log_level;
  frame_message.message = log_message;

  NSString* url_string =
      base::mac::ObjCCast<NSString>(script_message.body[kConsoleMessageUrlKey]);
  if (url_string.length) {
    frame_message.url = GURL(base::SysNSStringToUTF8(url_string));
  }

  delegate_->DidReceiveConsoleMessage(web_state, sender_frame, frame_message);
}
