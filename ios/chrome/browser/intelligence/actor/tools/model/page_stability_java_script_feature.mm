// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_java_script_feature.h"

#import <WebKit/WebKit.h>

#import "base/functional/callback_helpers.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/types/expected.h"
#import "base/values.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {

// The name of the TS file used for this JavaScriptFeature.
const char kScriptName[] = "page_stability";

// Helper to extract the result dictionary from a JavaScript callback response.
// If an error occurred or result is missing/invalid, returns a
// `ToolExecutionResult` error.
base::expected<const base::DictValue*, actor::ToolExecutionResult>
ExtractResultDict(const base::Value* result, NSError* error) {
  if (error) {
    if ([error.domain isEqualToString:WKErrorDomain] &&
        error.code == WKErrorJavaScriptInvalidFrameTarget) {
      return base::unexpected(actor::ToolExecutionResult(
          actor::mojom::ActionResultCode::kFrameWentAway));
    }
    std::string error_msg = base::StringPrintf(
        "JavaScript execution failed: %s (Domain: %s, Code: %ld)",
        base::SysNSStringToUTF8(error.localizedDescription).c_str(),
        base::SysNSStringToUTF8(error.domain).c_str(),
        static_cast<long>(error.code));
    return base::unexpected(actor::ToolExecutionResult(
        actor::mojom::ActionResultCode::kArgumentsInvalid,
        actor::InternalToolErrorCode::
            kJavascriptFeatureFailedInJavaScriptExecution,
        false, error_msg));
  }
  if (!result) {
    // `result` is nullptr if the JavaScript function call timed out.
    return base::unexpected(actor::ToolExecutionResult(
        actor::mojom::ActionResultCode::kToolTimeout));
  }
  if (!result->is_dict()) {
    return base::unexpected(actor::ToolExecutionResult(
        actor::InternalToolErrorCode::kJavascriptFeatureGotInvalidResult));
  }
  return &result->GetDict();
}

}  // namespace

namespace actor {

// static
PageStabilityJavaScriptFeature* PageStabilityJavaScriptFeature::GetInstance() {
  static base::NoDestructor<PageStabilityJavaScriptFeature> instance;
  return instance.get();
}

void PageStabilityJavaScriptFeature::WaitForStability(
    base::WeakPtr<web::WebFrame> target_frame,
    base::OnceCallback<void(ToolExecutionResult)> callback) {
  if (!target_frame || !target_frame->GetWebFrameInternal()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }
  base::DictValue parameters;
  parameters.Set(
      "windowDurationMs",
      static_cast<int>(GetActorPageStabilityWindowDuration().InMilliseconds()));
  parameters.Set("mutationCap", GetActorPageStabilityMutationCap());
  parameters.Set(
      "timeoutMs",
      static_cast<int>(GetActorPageStabilityTimeout().InMilliseconds()));
  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallAsyncJavaScriptFunction(
      target_frame.get(), /*name=*/"page_stability.waitForStability",
      parameters,
      /*callback=*/
      base::BindOnce(&PageStabilityJavaScriptFeature::OnStabilityResult,
                     // Safe because this is a singleton and will remain alive
                     // while this function is executing.
                     base::Unretained(this), std::move(cb_for_js)));

  if (!sent) {
    std::move(cb_for_error)
        .Run(ToolExecutionResult(
            mojom::ActionResultCode::kArgumentsInvalid,
            InternalToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction));
  }
}

void PageStabilityJavaScriptFeature::CancelWaitForStability(
    web::WebFrame* target_frame) {
  if (!target_frame || !target_frame->GetBrowserState() ||
      !target_frame->GetWebFrameInternal()) {
    return;
  }
  CallJavaScriptFunction(target_frame, "page_stability.cancelWaitForStability",
                         /*parameters=*/{});
}

void PageStabilityJavaScriptFeature::WaitForLcp(
    base::WeakPtr<web::WebFrame> target_frame,
    base::TimeDelta timeout,
    base::OnceCallback<void(ToolExecutionResult)> callback) {
  if (!target_frame || !target_frame->GetWebFrameInternal()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }
  base::DictValue parameters;
  parameters.Set("timeoutMs", static_cast<int>(timeout.InMilliseconds()));
  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallAsyncJavaScriptFunction(
      target_frame.get(), "page_stability.waitForLcp", parameters,
      base::BindOnce(&PageStabilityJavaScriptFeature::OnLcpResult,
                     base::Unretained(this), std::move(cb_for_js)));
  if (!sent) {
    std::move(cb_for_error)
        .Run(ToolExecutionResult(
            mojom::ActionResultCode::kArgumentsInvalid,
            InternalToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction));
  }
}

void PageStabilityJavaScriptFeature::CancelWaitForLcp(
    base::WeakPtr<web::WebFrame> target_frame) {
  if (!target_frame || !target_frame->GetBrowserState()) {
    return;
  }
  CallJavaScriptFunction(target_frame.get(), "page_stability.cancelWaitForLcp",
                         /*parameters=*/{});
}

PageStabilityJavaScriptFeature::PageStabilityJavaScriptFeature()
    : web::JavaScriptFeature(web::ContentWorld::kIsolatedWorld,
                             {FeatureScript::CreateWithFilename(
                                 kScriptName,
                                 FeatureScript::InjectionTime::kDocumentStart,
                                 FeatureScript::TargetFrames::kAllFrames,
                                 // Reinject since this script registers APIs.
                                 FeatureScript::ReinjectionBehavior::
                                     kReinjectOnDocumentRecreation)}) {}

PageStabilityJavaScriptFeature::~PageStabilityJavaScriptFeature() = default;

void PageStabilityJavaScriptFeature::OnStabilityResult(
    base::OnceCallback<void(ToolExecutionResult)> callback,
    const base::Value* result,
    NSError* error) {
  auto dict_or_error = ExtractResultDict(result, error);
  if (!dict_or_error.has_value()) {
    std::move(callback).Run(std::move(dict_or_error.error()));
    return;
  }
  const base::DictValue* result_dict = dict_or_error.value();
  std::optional<bool> settled = result_dict->FindBool("settled");
  if (!settled.has_value() || !settled.value()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kToolTimeout));
    return;
  }
  std::move(callback).Run(ToolExecutionResult::Ok());
}

void PageStabilityJavaScriptFeature::OnLcpResult(
    base::OnceCallback<void(ToolExecutionResult)> callback,
    const base::Value* result,
    NSError* error) {
  auto dict_or_error = ExtractResultDict(result, error);
  if (!dict_or_error.has_value()) {
    std::move(callback).Run(std::move(dict_or_error.error()));
    return;
  }
  const base::DictValue* result_dict = dict_or_error.value();
  std::optional<bool> lcp_received = result_dict->FindBool("lcpReceived");
  if (!lcp_received.has_value()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }
  if (!lcp_received.value()) {
    // TODO(crbug.com/498991756) - Log UMA when lcpReceived is false.
  }
  std::move(callback).Run(ToolExecutionResult::Ok());
}

}  // namespace actor
