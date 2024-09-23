// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/script_error_message_handler_java_script_feature.h"

#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "net/base/apple/url_conversions.h"

namespace {
const char kWindowErrorResultHandlerName[] = "WindowErrorResultHandler";

static const char kScriptMessageResponseFilenameKey[] = "filename";
static const char kScriptMessageResponseLineNumberKey[] = "line_number";
static const char kScriptMessageResponseMessageKey[] = "message";
static const char kScriptMessageResponseStackKey[] = "stack";
}  // namespace

namespace web {

ScriptErrorMessageHandlerJavaScriptFeature::ErrorDetails::ErrorDetails()
    : is_main_frame(true) {}
ScriptErrorMessageHandlerJavaScriptFeature::ErrorDetails::~ErrorDetails() =
    default;

ScriptErrorMessageHandlerJavaScriptFeature::
    ScriptErrorMessageHandlerJavaScriptFeature(
        base::RepeatingCallback<void(ErrorDetails)> callback)
    : JavaScriptFeature(ContentWorld::kAllContentWorlds, {}),
      callback_(std::move(callback)) {
  DCHECK(callback_);
}
ScriptErrorMessageHandlerJavaScriptFeature::
    ~ScriptErrorMessageHandlerJavaScriptFeature() = default;

std::optional<std::string>
ScriptErrorMessageHandlerJavaScriptFeature::GetScriptMessageHandlerName()
    const {
  return kWindowErrorResultHandlerName;
}

void ScriptErrorMessageHandlerJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  ErrorDetails details;

  const base::Value::Dict* script_dict =
      script_message.body() ? script_message.body()->GetIfDict() : nullptr;
  if (!script_dict) {
    return;
  }

  const std::string* filename =
      script_dict->FindString(kScriptMessageResponseFilenameKey);
  if (filename) {
    details.filename = base::SysUTF8ToNSString(*filename);
  }

  auto line_number =
      script_dict->FindDouble(kScriptMessageResponseLineNumberKey);
  if (line_number) {
    details.line_number = static_cast<int>(line_number.value());
  }

  const std::string* log_message =
      script_dict->FindString(kScriptMessageResponseMessageKey);
  if (log_message) {
    details.message = base::SysUTF8ToNSString(*log_message);
  }

  const std::string* stack =
      script_dict->FindString(kScriptMessageResponseStackKey);
  if (stack) {
    details.stack = base::SysUTF8ToNSString(*stack);
  }

  details.is_main_frame = script_message.is_main_frame();

  if (script_message.request_url()) {
    details.url = script_message.request_url().value();
  }

  bool has_scriptname = details.filename && details.filename.length > 0;
  UMA_HISTOGRAM_BOOLEAN("IOS.Javascript.ErrorHasFilename", has_scriptname);

  if (base::FeatureList::IsEnabled(features::kLogJavaScriptErrors) &&
      !has_scriptname) {
    if (log_message) {
      SCOPED_CRASH_KEY_STRING256("Javascript", "error", *log_message);
      const std::string stack_error_key = stack ? *stack : "";
      SCOPED_CRASH_KEY_STRING256("Javascript", "stack", stack_error_key);
      base::debug::DumpWithoutCrashing();
    }
  }

  callback_.Run(details);
}

}  // namespace web
