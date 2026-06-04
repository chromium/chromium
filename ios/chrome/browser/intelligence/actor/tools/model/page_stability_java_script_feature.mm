// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_java_script_feature.h"

#import <WebKit/WebKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {
// The name of the TS file used for this JavaScriptFeature.
const char kScriptName[] = "page_stability";

// A placeholder to inject the duration to monitor mutations after an
// interaction.
const char kIntervalDurationMsPlaceholder[] = "{{INTERVAL_DURATION_MS}}";

// A placeholder to inject the threshold for throttling interactions that come
// in quick succession.
const char kThrottleThresholdMsPlaceholder[] = "{{THROTTLE_THRESHOLD_MS}}";

// A placeholder to inject whether the script captures page stability metrics.
const char kPageStabilityMetricsEnabledPlaceholder[] =
    "window.pageStabilityMetricsEnabled";

// The minimum time required between processing user interactions. Any
// subsequent interactions occurring within this window of the last accepted
// interaction are ignored to prevent overlapping measurement intervals from
// firing too rapidly.
constexpr base::TimeDelta kPageStabilityThrottleThreshold =
    base::Milliseconds(500);

// UMA histogram key for mutation count.
const char kMutationCountHistogram[] =
    "IOS.Actor.PageStability.InteractionMetrics.MutationCount";

// UMA histogram key for time to first mutation.
const char kTimeToFirstMutationHistogram[] =
    "IOS.Actor.PageStability.InteractionMetrics.TimeToFirstMutation";

// UMA histogram key for time to last mutation.
const char kTimeToLastMutationHistogram[] =
    "IOS.Actor.PageStability.InteractionMetrics.TimeToLastMutation";

// Provides the values to be injected into the script on creation.
web::JavaScriptFeature::FeatureScript::PlaceholderReplacements
GetReplacements() {
  return @{
    base::SysUTF8ToNSString(kIntervalDurationMsPlaceholder) :
        base::SysUTF8ToNSString(base::NumberToString(
            GetPageStabilityIntervalDuration().InMilliseconds())),
    base::SysUTF8ToNSString(kThrottleThresholdMsPlaceholder) :
        base::SysUTF8ToNSString(base::NumberToString(
            kPageStabilityThrottleThreshold.InMilliseconds())),
    base::SysUTF8ToNSString(kPageStabilityMetricsEnabledPlaceholder) :
            IsPageStabilityMetricsEnabled() ? @"true" : @"false",
  };
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
  bool sent = CallAsyncJavaScriptFunction(
      target_frame.get(), /*name=*/"page_stability.waitForStability",
      parameters,
      /*callback=*/
      base::BindOnce(&PageStabilityJavaScriptFeature::OnStabilityResult,
                     // Safe because this is a singleton and will remain alive
                     // while this function is executing.
                     base::Unretained(this), target_frame,
                     std::move(callback)));

  if (!sent) {
    std::move(callback).Run(ToolExecutionResult(
        mojom::ActionResultCode::kArgumentsInvalid,
        InternalToolErrorCode::
            kJavascriptFeatureFailedToCallJavaScriptFunction));
  }
}

std::optional<std::string>
PageStabilityJavaScriptFeature::GetScriptMessageHandlerName() const {
  return "PageStabilityMetricsHandler";
}

void PageStabilityJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!IsPageStabilityMetricsEnabled()) {
    return;
  }

  if (!message.body() || !message.body()->is_dict()) {
    return;
  }

  const base::DictValue& dict = message.body()->GetDict();
  std::optional<double> mutation_count = dict.FindDouble("mutationCount");
  std::optional<double> time_to_first_mutation =
      dict.FindDouble("timeToFirstMutation");
  std::optional<double> time_to_last_mutation =
      dict.FindDouble("timeToLastMutation");

  if (!mutation_count.has_value() || !time_to_first_mutation.has_value() ||
      !time_to_last_mutation.has_value()) {
    return;
  }
  base::UmaHistogramCounts10000(kMutationCountHistogram,
                                static_cast<int>(mutation_count.value()));

  // A value of -1 indicates no mutations occurred.
  if (time_to_first_mutation.value() >= 0) {
    base::UmaHistogramTimes(
        kTimeToFirstMutationHistogram,
        base::Milliseconds(static_cast<int>(time_to_first_mutation.value())));
  }
  if (time_to_last_mutation.value() >= 0) {
    base::UmaHistogramTimes(
        kTimeToLastMutationHistogram,
        base::Milliseconds(static_cast<int>(time_to_last_mutation.value())));
  }
}

PageStabilityJavaScriptFeature::PageStabilityJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              // Reinject since this script sets up event listeners.
              FeatureScript::ReinjectionBehavior::kReinjectOnDocumentRecreation,
              base::BindRepeating(&GetReplacements))}) {}

PageStabilityJavaScriptFeature::~PageStabilityJavaScriptFeature() = default;

void PageStabilityJavaScriptFeature::OnStabilityResult(
    base::WeakPtr<web::WebFrame> target_frame,
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
