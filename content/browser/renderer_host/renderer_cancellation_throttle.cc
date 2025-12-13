// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_cancellation_throttle.h"

#include "base/metrics/histogram_functions.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"

namespace content {

namespace {

// Default timeout for the cancellation window. From gathering data through the
// "Navigation.RendererInitiatedCancellation.DeferStartToCancellationWindowEnd"
// UMA, 99% of navigations' cancellation window ends in under 2000ms, and all
// cancellation windows end in under 10000ms, so setting this to 11000ms.
const base::FeatureParam<base::TimeDelta> throttle_timeout{
    &features::kRendererCancellationThrottleImprovements, "timeout",
    base::Milliseconds(11000)};

}  // namespace

// static
void RendererCancellationThrottle::MaybeCreateAndAdd(
    NavigationThrottleRegistry& registry) {
  NavigationRequest* request =
      NavigationRequest::From(&registry.GetNavigationHandle());
  if (request->ShouldWaitForRendererCancellationWindowToEnd()) {
    registry.AddThrottle(
        std::make_unique<RendererCancellationThrottle>(registry));
  }
}

RendererCancellationThrottle::RendererCancellationThrottle(
    NavigationThrottleRegistry& registry)
    : NavigationThrottle(registry) {}

RendererCancellationThrottle::~RendererCancellationThrottle() {
  if (defer_start_time_ == base::TimeTicks()) {
    return;
  }
  base::UmaHistogramBoolean(
      "Navigation.RendererCancellationThrottle.NavigationCancelled",
      !did_resume_navigation_);
  if (!did_resume_navigation_) {
    base::UmaHistogramTimes(
        "Navigation.RendererCancellationThrottle.NavigationCancelled."
        "TimeUntilCancel",
        base::TimeTicks::Now() - defer_start_time_);
  }
}

NavigationThrottle::ThrottleCheckResult
RendererCancellationThrottle::WillProcessResponse() {
  return WaitForRendererCancellationIfNeeded();
}

NavigationThrottle::ThrottleCheckResult
RendererCancellationThrottle::WillCommitWithoutUrlLoader() {
  return WaitForRendererCancellationIfNeeded();
}

NavigationThrottle::ThrottleCheckResult
RendererCancellationThrottle::WaitForRendererCancellationIfNeeded() {
  if (base::FeatureList::IsEnabled(
          features::kSkipRendererCancellationThrottle)) {
    return NavigationThrottle::PROCEED;
  }

  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  DCHECK(request);
  if (request->renderer_cancellation_window_ended()) {
    // The cancellation window had already ended, so the navigation doesn't need
    // deferring.
    return NavigationThrottle::PROCEED;
  }

  if (!request->GetRenderFrameHost() ||
      request->GetRenderFrameHost()->GetSiteInstance()->group() !=
          request->frame_tree_node()
              ->current_frame_host()
              ->GetSiteInstance()
              ->group()) {
    // Only defer same-SiteInstanceGroup navigations, as only those navigations
    // were previously guaranteed to be cancelable from the same JS task it
    // started on (see class level comment for more details).
    return NavigationThrottle::PROCEED;
  }

  // Start the cancellation timeout, to warn users of an unresponsive renderer
  // if the cancellation window is longer than the set time limit.
  defer_start_time_ = base::TimeTicks::Now();
  RestartTimeout();
  // Wait for the navigation cancellation window to end before continuing.
  request->set_renderer_cancellation_window_ended_callback(base::BindOnce(
      &RendererCancellationThrottle::NavigationCancellationWindowEnded,
      base::Unretained(this)));

  return NavigationThrottle::DEFER;
}

void RendererCancellationThrottle::NavigationCancellationWindowEnded() {
  if (did_resume_navigation_) {
    // The timeout handler already resumed the navigation.
    return;
  }

  base::UmaHistogramBoolean(
      "Navigation.RendererCancellationThrottle.NotCancelled.TimeoutIsHit",
      false);
  base::UmaHistogramTimes(
      "Navigation.RendererCancellationThrottle.TimeUntilWindowEnd",
      base::TimeTicks::Now() - defer_start_time_);
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  CHECK(request->renderer_cancellation_window_ended());

  // Stop the timeout and notify that renderer is responsive if necessary.
  renderer_cancellation_timeout_timer_.Stop();

  if (!base::FeatureList::IsEnabled(
          features::kRendererCancellationThrottleImprovements)) {
    request->GetRenderFrameHost()
        ->GetRenderWidgetHost()
        ->RendererIsResponsive();
  }
  did_resume_navigation_ = true;
  Resume();
}

void RendererCancellationThrottle::SetOnTimeoutCallbackForTesting(
    base::OnceClosure callback) {
  on_timeout_callback_for_testing_ = std::move(callback);
}

void RendererCancellationThrottle::OnTimeout() {
  if (on_timeout_callback_for_testing_) {
    std::move(on_timeout_callback_for_testing_).Run();
  }
  base::UmaHistogramBoolean(
      "Navigation.RendererCancellationThrottle.NotCancelled.TimeoutIsHit",
      true);
  if (base::FeatureList::IsEnabled(
          features::kRendererCancellationThrottleImprovements)) {
    // Resume the navigation once it hits the timeout, without marking the
    // renderer as unresponsive.
    did_resume_navigation_ = true;
    Resume();
    return;
  }
  // Warn that the renderer is unresponsive.
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  DCHECK(request);

  auto* previous_rfh =
      RenderFrameHostImpl::FromID(request->GetPreviousRenderFrameHostId());
  if (!previous_rfh) {
    return;
  }

  previous_rfh->GetRenderWidgetHost()->RendererIsUnresponsive(
      RenderWidgetHostImpl::RendererIsUnresponsiveReason::
          kRendererCancellationThrottleTimeout,
      base::BindRepeating(&RendererCancellationThrottle::RestartTimeout,
                          weak_factory_.GetWeakPtr()));
}

void RendererCancellationThrottle::RestartTimeout() {
  renderer_cancellation_timeout_timer_.Start(
      FROM_HERE, throttle_timeout.Get(),
      base::BindOnce(&RendererCancellationThrottle::OnTimeout,
                     base::Unretained(this)));
}

const char* RendererCancellationThrottle::GetNameForLogging() {
  return "RendererCancellationThrottle";
}

}  // namespace content
