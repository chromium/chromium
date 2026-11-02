// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_METRICS_H_

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
