// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"

#import <optional>
#import <string>
#import <utility>

#import "base/timer/elapsed_timer.h"
#import "base/values.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"

namespace actor {

namespace {

constexpr std::string_view kResultCodeKey = "resultCode";
constexpr std::string_view kMessageKey = "message";

}  // namespace

ToolExecutionResult ParseJavaScriptResultWithResultCode(
    ActorJavaScriptCall call,
    base::FunctionRef<mojom::ActionResultCode(int)> result_code_translator,
    const base::Value* result,
    const base::ElapsedTimer& timer) {
  if (!result) {
    // `result` is nullptr if the JavaScript function call timed out. See
    // https://source.chromium.org/chromium/chromium/src/+/main:ios/web/public/js_messaging/web_frame.h;l=65-68;drc=2acee4f42bc58706d4ec89a8c5323e90b454ab3c.
    RecordJavaScriptResultCode(call, mojom::ActionResultCode::kToolTimeout);
    return ToolExecutionResult(mojom::ActionResultCode::kToolTimeout);
  }
  if (!result->is_dict()) {
    return ToolExecutionResult(
        InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
  }
  const base::DictValue& result_dict = result->GetDict();
  std::optional<double> error_code_double =
      result_dict.FindDouble(kResultCodeKey);
  if (!error_code_double) {
    return ToolExecutionResult(
        InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
  }
  int error_code = static_cast<int>(*error_code_double);
  mojom::ActionResultCode external_code = result_code_translator(error_code);
  RecordJavaScriptExecutionDuration(call, timer.Elapsed());
  RecordJavaScriptResultCode(call, external_code);
  bool requires_page_stabilization =
      (external_code == mojom::ActionResultCode::kOk);
  if (const std::string* message = result_dict.FindString(kMessageKey);
      message) {
    return ToolExecutionResult(external_code, requires_page_stabilization,
                               *message);
  }
  return ToolExecutionResult(external_code, requires_page_stabilization);
}

}  // namespace actor
