// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_METRICS_H_

#import <optional>
#import <string_view>

#import "base/time/time.h"
#import "components/actor/public/mojom/actor_types.mojom-forward.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

// Page Stability Histograms.
// Based on Desktop's `Actor.RendererPageStability.*` metrics in
// `components/actor/core/page_stability_metrics_common.h`.
inline constexpr std::string_view kIOSActorPageStabilityOutcomeMetricName =
    "IOS.Actor.PageStability.Outcome";
inline constexpr std::string_view
    kIOSActorPageStabilityTotalTimeToStableMetricName =
        "IOS.Actor.PageStability.TotalTimeToStable";
inline constexpr std::string_view
    kIOSActorPageStabilityTimeFromMonitoringToStableMetricName =
        "IOS.Actor.PageStability.TimeFromMonitoringToStable";
inline constexpr std::string_view
    kIOSActorPageStabilityTotalTimeToWebFrameGoingAwayMetricName =
        "IOS.Actor.PageStability.TotalTimeToWebFrameGoingAway";

// Observation Delay Histograms.
// Based on Desktop's `Actor.ObservationDelay.*` metrics in
// `chrome/browser/actor/tools/observation_delay_metrics.h`.
inline constexpr std::string_view
    kIOSActorObservationDelayStateDurationWaitForPageStabilityMetricName =
        "IOS.Actor.ObservationDelay.StateDuration.WaitForPageStability";
inline constexpr std::string_view
    kIOSActorObservationDelayStateDurationWaitForLoadCompletionMetricName =
        "IOS.Actor.ObservationDelay.StateDuration.WaitForLoadCompletion";
inline constexpr std::string_view
    kIOSActorObservationDelayStateDurationDelayForLcpMetricName =
        "IOS.Actor.ObservationDelay.StateDuration.DelayForLcp";
inline constexpr std::string_view
    kIOSActorObservationDelayStateDurationWaitForAutofillPredictionsMetricName =
        "IOS.Actor.ObservationDelay.StateDuration.WaitForAutofillPredictions";
inline constexpr std::string_view
    kIOSActorObservationDelayTotalWaitDurationMetricName =
        "IOS.Actor.ObservationDelay.TotalWaitDuration";
inline constexpr std::string_view
    kIOSActorObservationDelayDidTimeoutMetricName =
        "IOS.Actor.ObservationDelay.DidTimeout";
inline constexpr std::string_view
    kIOSActorObservationDelayDidNavigateMetricName =
        "IOS.Actor.ObservationDelay.DidNavigate";
inline constexpr std::string_view
    kIOSActorObservationDelayLcpDelayNeededMetricName =
        "IOS.Actor.ObservationDelay.LcpDelayNeeded";

// Tool Execution & Page Stabilization Timing Histogram Prefixes.
// Based on Desktop's `Actor.Tools.ExecutionDuration.{ToolRequest}` and
// `Actor.Tools.PageStabilization.{ToolRequest}` metrics in
// `chrome/browser/actor/actor_metrics.cc`.
inline constexpr char kIOSActorToolsExecutionDurationPrefixMetricName[] =
    "IOS.Actor.Tools.ExecutionDuration.";
inline constexpr char kIOSActorToolsPageStabilizationPrefixMetricName[] =
    "IOS.Actor.Tools.PageStabilization.";

// Tool Action Result Code Histograms.
// Based on Desktop's `Actor.ExecutionEngine.Action.ResultCode` metric in
// `chrome/browser/actor/actor_metrics.cc`.
inline constexpr char kIOSActorExecutionEngineActionResultCodeMetricName[] =
    "IOS.Actor.ExecutionEngine.Action.ResultCode";
inline constexpr char kIOSActorToolsActionResultCodePrefixMetricName[] =
    "IOS.Actor.Tools.ActionResultCode.";

// Target Resolution & Validation Histograms.
// Based on Desktop's `Actor.DomNodeResult` and target resolution metrics in
// `chrome/renderer/actor/tool_utils.cc` and
// `chrome/browser/actor/tools/page_target_util.cc`.
inline constexpr char kIOSActorToolsActionTargetTypeMetricName[] =
    "IOS.Actor.Tools.ActionTargetType";
inline constexpr char kIOSActorToolsTargetFrameResolutionResultMetricName[] =
    "IOS.Actor.Tools.TargetFrameResolutionResult";
inline constexpr char kIOSActorToolsTargetIframeDepthMetricName[] =
    "IOS.Actor.Tools.TargetIframeDepth";

// Represents the type of page target specified by the model for a web tool.
// Logged for the IOS.Actor.Tools.ActionTargetType histogram.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSActorActionTargetType)
enum class ActionTargetType {
  kDomNodeId = 0,
  kCoordinate = 1,
  kMaxValue = kCoordinate,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSActorActionTargetType)

// Records the execution duration and post-execution page stabilization duration
// for a completed tool invocation.
void RecordToolTimings(std::string_view tool_name,
                       base::TimeDelta execution_duration,
                       base::TimeDelta page_stabilization_duration);

// Records the external `mojom::ActionResultCode` (both aggregate and per-tool)
// for a completed tool request.
void RecordToolActionResultCode(std::string_view tool_name,
                                const ToolExecutionResult& result);

// Records the external `mojom::ActionResultCode` for an action execution
// outcome (Desktop equivalent: `Actor.ExecutionEngine.Action.ResultCode`).
void RecordActionResultCode(mojom::ActionResultCode result_code);

// Records whether a web tool targeted a DOM node ID or viewport coordinates.
void RecordActionTargetType(ActionTargetType target_type);

// Records the result code of resolving the target `WebFrame` for an
// `ActionTarget` and, on success, the optional iframe nesting depth (`0` for
// main frame).
void RecordTargetFrameResolutionResult(
    mojom::ActionResultCode result_code,
    std::optional<int> iframe_depth = std::nullopt);

// Returns the full histogram name for recording the execution duration of
// `call` (e.g. `IOS.Actor.Tools.JavaScriptExecutionDuration.Click`).
std::string_view GetJavaScriptExecutionDurationHistogramName(
    ActorJavaScriptCall call);

// Returns the full histogram name for recording the `mojom::ActionResultCode`
// of `call` (e.g. `IOS.Actor.Tools.JavaScriptResultCode.Click`).
std::string_view GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall call);

// Records the execution duration of an Actor JavaScript call for `call`.
// Should only be called when the JavaScript execution completed successfully or
// returned an error payload (not on timeout or malformed payloads).
void RecordJavaScriptExecutionDuration(ActorJavaScriptCall call,
                                       base::TimeDelta duration);

// Records the `mojom::ActionResultCode` of an Actor JavaScript call for
// `call`.
void RecordJavaScriptResultCode(ActorJavaScriptCall call,
                                mojom::ActionResultCode code);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_METRICS_H_
