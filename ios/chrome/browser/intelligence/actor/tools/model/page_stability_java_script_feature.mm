// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_java_script_feature.h"

#import <WebKit/WebKit.h>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {
// The name of the TS file used for this JavaScriptFeature.
const char kScriptName[] = "page_stability";
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
  bool sent = CallAsyncJavaScriptFunction(
      target_frame.get(), /*name=*/"page_stability.waitForStability",
      parameters,
      /*callback=*/
      base::BindOnce(&PageStabilityJavaScriptFeature::OnStabilityResult,
                     // Safe because this is a singleton and will remain alive
                     // while this function is executing.
                     base::Unretained(this), std::move(callback)));

  if (!sent) {
    std::move(callback).Run(ToolExecutionResult(
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
  if (error) {
    mojom::ActionResultCode external_code =
        mojom::ActionResultCode::kArgumentsInvalid;
    InternalToolErrorCode internal_code =
        InternalToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution;

    if ([error.domain isEqualToString:WKErrorDomain]) {
      if (error.code == WKErrorJavaScriptInvalidFrameTarget) {
        std::move(callback).Run(
            ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
        return;
      }
    }

    std::string error_msg = base::StringPrintf(
        "JavaScript execution failed: %s (Domain: %s, Code: %ld)",
        base::SysNSStringToUTF8(error.localizedDescription).c_str(),
        base::SysNSStringToUTF8(error.domain).c_str(),
        static_cast<long>(error.code));

    std::move(callback).Run(
        ToolExecutionResult(external_code, internal_code, false, error_msg));
    return;
  }
  if (!result) {
    // `result` is nullptr if the JavaScript function call timed out. See
    // https://source.chromium.org/chromium/chromium/src/+/main:ios/web/public/js_messaging/web_frame.h;l=65-68;drc=2acee4f42bc58706d4ec89a8c5323e90b454ab3c.
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kToolTimeout));
    return;
  }
  if (!result->is_dict()) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kJavascriptFeatureGotInvalidResult));
    return;
  }
  const base::DictValue& result_dict = result->GetDict();
  std::optional<bool> settled = result_dict.FindBool("settled");
  if (!settled.has_value() || !settled.value()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kToolTimeout));
    return;
  }
  std::move(callback).Run(ToolExecutionResult::Ok());
}

}  // namespace actor
