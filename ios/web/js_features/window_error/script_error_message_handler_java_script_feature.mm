// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/script_error_message_handler_java_script_feature.h"

#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/location.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/js_features/window_error/script_error_stack_util.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "net/base/apple/url_conversions.h"

namespace {
const char kWindowErrorResultHandlerName[] = "WindowErrorResultHandler";

static const char kScriptMessageResponseFilenameKey[] = "filename";
static const char kScriptMessageResponseLineNumberKey[] = "line_number";
static const char kScriptMessageResponseMessageKey[] = "message";
static const char kScriptMessageResponseStackKey[] = "stack";

constexpr unsigned long kStackMaxSize = 1024;
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
  UMA_HISTOGRAM_BOOLEAN("IOS.JavaScript.ErrorHasFilename", has_scriptname);

  if (base::FeatureList::IsEnabled(features::kLogJavaScriptErrors) &&
      !has_scriptname) {
    if (log_message) {
      SCOPED_CRASH_KEY_STRING256("Javascript", "error", *log_message);

      static auto* const stack_crash_key = base::debug::AllocateCrashKeyString(
          "Javascript-stack", base::debug::CrashKeySize::Size1024);

      script_error_stack_util::FrameComponents top_stack_frame;

      if (stack) {
        std::string stack_crash_key_value =
            script_error_stack_util::FilterForUsefulStackFrames(*stack);
        if (stack_crash_key_value.length() > kStackMaxSize) {
          stack_crash_key_value = script_error_stack_util::TruncateMiddle(
              stack_crash_key_value, kStackMaxSize);
        }
        base::debug::SetCrashKeyString(stack_crash_key, stack_crash_key_value);

        top_stack_frame =
            script_error_stack_util::TopFrameComponentsFromStack(*stack);
      }

      if (top_stack_frame.function_name.length() > 0) {
        int reported_line = top_stack_frame.line;
        // If the script appears to be minimized, use the column number instead
        // of the line as all minimized scripts are only 1 line long.
        if (top_stack_frame.line == 1 &&
            top_stack_frame.function_name.length() == 1) {
          reported_line = top_stack_frame.column;
        }
        base::debug::DumpWithoutCrashing(base::Location::Current(
            top_stack_frame.function_name.c_str(),
            top_stack_frame.file_name.c_str(), reported_line));
      } else {
        base::debug::DumpWithoutCrashing();
      }

      base::debug::ClearCrashKeyString(stack_crash_key);
    }
  }

  callback_.Run(details);
}

}  // namespace web
