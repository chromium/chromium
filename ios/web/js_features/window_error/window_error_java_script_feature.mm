// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/window_error_java_script_feature.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "error";

const char kWindowErrorResultHandlerName[] = "WindowErrorResultHandler";

static const char kScriptMessageResponseFilenameKey[] = "filename";
static const char kScriptMessageResponseLineNumberKey[] = "line_number";
static const char kScriptMessageResponseMessageKey[] = "message";
}  // namespace

namespace web {

WindowErrorJavaScriptFeature::ErrorDetails::ErrorDetails()
    : is_main_frame(true) {}
WindowErrorJavaScriptFeature::ErrorDetails::~ErrorDetails() = default;

WindowErrorJavaScriptFeature::WindowErrorJavaScriptFeature(
    base::RepeatingCallback<void(ErrorDetails)> callback)
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
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

absl::optional<std::string>
WindowErrorJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kWindowErrorResultHandlerName;
}

void WindowErrorJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  ErrorDetails details;

  if (!script_message.body() || !script_message.body()->is_dict()) {
    return;
  }

  std::string* filename =
      script_message.body()->FindStringKey(kScriptMessageResponseFilenameKey);
  if (filename) {
    details.filename = base::SysUTF8ToNSString(*filename);
  }

  auto line_number =
      script_message.body()->FindDoubleKey(kScriptMessageResponseLineNumberKey);
  if (line_number) {
    details.line_number = static_cast<int>(line_number.value());
  }

  std::string* log_message =
      script_message.body()->FindStringKey(kScriptMessageResponseMessageKey);
  if (log_message) {
    details.message = base::SysUTF8ToNSString(*log_message);
  }

  details.is_main_frame = script_message.is_main_frame();

  if (script_message.request_url()) {
    details.url = script_message.request_url().value();
  }

  callback_.Run(details);
}

}  // namespace web
