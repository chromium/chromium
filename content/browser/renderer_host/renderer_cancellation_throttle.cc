// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_cancellation_throttle.h"

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"

namespace content {

namespace {

// Default timeout for the cancellation window. From gathering data through the
// "Navigation.RendererInitiatedCancellation.DeferStartToCancellationWindowEnd"
// UMA, 99% of navigations' cancellation window ends in under 2000ms, and all
// cancellation windows end in under 10000ms, so setting this to 11000ms.
constexpr base::TimeDelta kDefaultCancellationTimeout = base::Seconds(11);
base::TimeDelta g_cancellation_timeout = kDefaultCancellationTimeout;

}  // namespace

// static
std::unique_ptr<RendererCancellationThrottle>
RendererCancellationThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  NavigationRequest* request = NavigationRequest::From(handle);
  if (request->ShouldWaitForRendererCancellationWindowToEnd()) {
    return std::make_unique<RendererCancellationThrottle>(handle);
  }
  return nullptr;
}

// static
void RendererCancellationThrottle::SetCancellationTimeoutForTesting(
    base::TimeDelta timeout) {
  if (timeout.is_zero()) {
    g_cancellation_timeout = kDefaultCancellationTimeout;
  } else {
    g_cancellation_timeout = timeout;
  }
}

RendererCancellationThrottle::RendererCancellationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

RendererCancellationThrottle::~RendererCancellationThrottle() = default;

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
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  DCHECK(request);
  if (request->renderer_cancellation_window_ended()) {
    // The cancellation window had already ended, so the navigation doesn't need
    // deferring.
    return NavigationThrottle::PROCEED;
  }

  if (!request->GetRenderFrameHost() ||
      request->GetRenderFrameHost()->GetSiteInstance() !=
          request->frame_tree_node()->current_frame_host()->GetSiteInstance()) {
    // Only defer same-SiteInstance navigations, as only those navigations
    // were previously guaranteed to be cancelable from the same JS task it
    // started on (see comment in the header file for more details).
    return NavigationThrottle::PROCEED;
  }

  // Start the cancellation timeout, to warn users of an unresponsive renderer
  // if the cancellation window is longer than the set time limit.
  RestartTimeout();
  // Wait for the navigation cancellation window to end before continuing.
  request->set_renderer_cancellation_window_ended_callback(base::BindOnce(
      &RendererCancellationThrottle::NavigationCancellationWindowEnded,
      base::Unretained(this)));

  return NavigationThrottle::DEFER;
}

void RendererCancellationThrottle::NavigationCancellationWindowEnded() {
  CHECK(NavigationRequest::From(navigation_handle())
            ->renderer_cancellation_window_ended());
  // Stop the timeout and notify that renderer is responsive if necessary.
  renderer_cancellation_timeout_timer_.Stop();
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  request->GetRenderFrameHost()->GetRenderWidgetHost()->RendererIsResponsive();

  Resume();
}

void RendererCancellationThrottle::OnTimeout() {
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
      FROM_HERE, g_cancellation_timeout,
      base::BindOnce(&RendererCancellationThrottle::OnTimeout,
                     base::Unretained(this)));
}

const char* RendererCancellationThrottle::GetNameForLogging() {
  return "RendererCancellationThrottle";
}

}  // namespace content
