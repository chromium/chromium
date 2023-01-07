// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/java_script_console/java_script_console_feature.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/web/java_script_console/java_script_console_feature_delegate.h"
#import "ios/chrome/browser/web/java_script_console/java_script_console_message.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "console";

const char kConsoleScriptHandlerName[] = "ConsoleMessageHandler";

const char kConsoleMessageKey[] = "message";
const char kConsoleMessageLogLevelKey[] = "log_level";
const char kConsoleMessageUrlKey[] = "url";
const char kSenderFrameIdKey[] = "sender_frame";
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

absl::optional<std::string>
JavaScriptConsoleFeature::GetScriptMessageHandlerName() const {
  return kConsoleScriptHandlerName;
}

void JavaScriptConsoleFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  // Completely skip processing the message if no delegate exists.
  if (!delegate_) {
    return;
  }

  if (!script_message.body() || !script_message.body()->is_dict()) {
    return;
  }

  std::string* frame_id =
      script_message.body()->FindStringKey(kSenderFrameIdKey);
  if (!frame_id) {
    return;
  }

  web::WebFrame* sender_frame =
      web_state->GetWebFramesManager()->GetFrameWithId(*frame_id);
  if (!sender_frame) {
    return;
  }

  std::string* log_message =
      script_message.body()->FindStringKey(kConsoleMessageKey);
  if (!log_message) {
    return;
  }

  std::string* log_level =
      script_message.body()->FindStringKey(kConsoleMessageLogLevelKey);
  if (!log_level) {
    return;
  }

  // At this point the message format has been validated and can be displayed.

  JavaScriptConsoleMessage frame_message;
  frame_message.level = base::SysUTF8ToNSString(*log_level);
  frame_message.message = base::SysUTF8ToNSString(*log_message);

  std::string* url_string =
      script_message.body()->FindStringKey(kConsoleMessageUrlKey);
  if (url_string && !url_string->empty()) {
    frame_message.url = GURL(*url_string);
  }

  delegate_->DidReceiveConsoleMessage(web_state, sender_frame, frame_message);
}
