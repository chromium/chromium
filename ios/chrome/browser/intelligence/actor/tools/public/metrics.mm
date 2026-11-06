// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"

#import <optional>
#import <string_view>
#import <utility>

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/time/time.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

// LINT.IfChange(IOSActorJavaScriptCall)
std::string_view GetJavaScriptExecutionDurationHistogramName(
    ActorJavaScriptCall call) {
  switch (call) {
    case ActorJavaScriptCall::kActionTarget:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.ActionTarget";
    case ActorJavaScriptCall::kAttemptFormFilling:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.AttemptFormFilling";
    case ActorJavaScriptCall::kClick:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.Click";
    case ActorJavaScriptCall::kDragAndRelease:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.DragAndRelease";
    case ActorJavaScriptCall::kScroll:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.Scroll";
    case ActorJavaScriptCall::kScrollTo:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.ScrollTo";
    case ActorJavaScriptCall::kSelect:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.Select";
    case ActorJavaScriptCall::kType:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.Type";
    case ActorJavaScriptCall::kTypeSubmitWithEnterFallback:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration."
             "TypeSubmitWithEnterFallback";
  }
  NOTREACHED();
}

std::string_view GetJavaScriptResultCodeHistogramName(
    ActorJavaScriptCall call) {
  switch (call) {
    case ActorJavaScriptCall::kActionTarget:
      return "IOS.Actor.Tools.JavaScriptResultCode.ActionTarget";
    case ActorJavaScriptCall::kAttemptFormFilling:
      return "IOS.Actor.Tools.JavaScriptResultCode.AttemptFormFilling";
    case ActorJavaScriptCall::kClick:
      return "IOS.Actor.Tools.JavaScriptResultCode.Click";
    case ActorJavaScriptCall::kDragAndRelease:
      return "IOS.Actor.Tools.JavaScriptResultCode.DragAndRelease";
    case ActorJavaScriptCall::kScroll:
      return "IOS.Actor.Tools.JavaScriptResultCode.Scroll";
    case ActorJavaScriptCall::kScrollTo:
      return "IOS.Actor.Tools.JavaScriptResultCode.ScrollTo";
    case ActorJavaScriptCall::kSelect:
      return "IOS.Actor.Tools.JavaScriptResultCode.Select";
    case ActorJavaScriptCall::kType:
      return "IOS.Actor.Tools.JavaScriptResultCode.Type";
    case ActorJavaScriptCall::kTypeSubmitWithEnterFallback:
      return "IOS.Actor.Tools.JavaScriptResultCode."
             "TypeSubmitWithEnterFallback";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/histograms.xml:IOSActorJavaScriptCall,//ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h:IOSActorJavaScriptCall)

void RecordJavaScriptExecutionDuration(ActorJavaScriptCall call,
                                       base::TimeDelta duration) {
  base::UmaHistogramTimes(GetJavaScriptExecutionDurationHistogramName(call),
                          duration);
}

void RecordJavaScriptResultCode(ActorJavaScriptCall call,
                                mojom::ActionResultCode code) {
  base::UmaHistogramSparse(GetJavaScriptResultCodeHistogramName(call),
                           std::to_underlying(code));
}

void RecordToolTimings(std::string_view tool_name,
                       base::TimeDelta execution_duration,
                       base::TimeDelta page_stabilization_duration) {
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kIOSActorToolsExecutionDurationPrefixMetricName, tool_name}),
      execution_duration);
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kIOSActorToolsPageStabilizationPrefixMetricName, tool_name}),
      page_stabilization_duration);
}

void RecordToolActionResultCode(std::string_view tool_name,
                                const ToolExecutionResult& result) {
  const int external_code = std::to_underlying(result.code());
  base::UmaHistogramSparse(kIOSActorExecutionEngineActionResultCodeMetricName,
                           external_code);
  if (!tool_name.empty()) {
    base::UmaHistogramSparse(
        base::StrCat(
            {kIOSActorToolsActionResultCodePrefixMetricName, tool_name}),
        external_code);
  }
}

void RecordActionResultCode(mojom::ActionResultCode result_code) {
  base::UmaHistogramSparse(kIOSActorExecutionEngineActionResultCodeMetricName,
                           std::to_underlying(result_code));
}

void RecordActionTargetType(ActionTargetType target_type) {
  base::UmaHistogramEnumeration(kIOSActorToolsActionTargetTypeMetricName,
                                target_type);
}

void RecordTargetFrameResolutionResult(mojom::ActionResultCode result_code,
                                       std::optional<int> iframe_depth) {
  base::UmaHistogramSparse(kIOSActorToolsTargetFrameResolutionResultMetricName,
                           std::to_underlying(result_code));
  if (iframe_depth.has_value()) {
    base::UmaHistogramCounts100(kIOSActorToolsTargetIframeDepthMetricName,
                                *iframe_depth);
  }
}
}  // namespace actor
