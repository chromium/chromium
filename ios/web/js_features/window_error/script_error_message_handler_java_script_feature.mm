// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/script_error_message_handler_java_script_feature.h"

#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/location.h"
#import "base/metrics/histogram_macros.h"
#import "ios/web/common/features.h"
#import "ios/web/js_features/window_error/script_error_details.h"
#import "ios/web/js_features/window_error/web_js_error_report_processor.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"

namespace {
const char kWindowErrorResultHandlerName[] = "WindowErrorResultHandler";

static const char kScriptMessageResponseApiNameKey[] = "api";
static const char kScriptMessageResponseLineNumberKey[] = "line_number";
static const char kScriptMessageResponseMessageKey[] = "message";
static const char kScriptMessageResponseStackKey[] = "stack";
static const char kScriptMessageResponseCrashKeys[] = "crashKeys";

}  // namespace

namespace web {

ScriptErrorMessageHandlerJavaScriptFeature::
    ScriptErrorMessageHandlerJavaScriptFeature(
        base::RepeatingCallback<void(ScriptErrorDetails)> callback)
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
  ScriptErrorDetails details(script_message.is_main_frame());

  const base::DictValue* script_dict =
      script_message.body() ? script_message.body()->GetIfDict() : nullptr;
  if (!script_dict) {
    return;
  }

  auto line_number =
      script_dict->FindDouble(kScriptMessageResponseLineNumberKey);
  if (line_number) {
    details.line_number = static_cast<int>(line_number.value());
  }

  const std::string* log_message =
      script_dict->FindString(kScriptMessageResponseMessageKey);
  if (log_message) {
    details.message = *log_message;
  }

  const std::string* api =
      script_dict->FindString(kScriptMessageResponseApiNameKey);
  if (api) {
    details.api = *api;
  }

  const std::string* stack =
      script_dict->FindString(kScriptMessageResponseStackKey);
  if (stack) {
    details.stack = *stack;
  }

  if (script_message.request_url()) {
    details.url = script_message.request_url().value();
  }

  const base::DictValue* crash_keys =
      script_dict->FindDict(kScriptMessageResponseCrashKeys);
  if (crash_keys) {
    for (auto ck = crash_keys->begin(); ck != crash_keys->end(); ++ck) {
      details.crash_keys.insert({ck->first, ck->second.GetString()});
    }
  }

  if (log_message &&
      base::FeatureList::IsEnabled(features::kLogCrWebJavaScriptErrors)) {
    WebJsErrorReportProcessor::FromBrowserState(web_state->GetBrowserState())
        ->ReportJavaScriptError(details);
  }

  callback_.Run(std::move(details));
}

}  // namespace web
