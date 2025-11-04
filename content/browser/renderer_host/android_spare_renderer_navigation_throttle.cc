// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/android_spare_renderer_navigation_throttle.h"

#include "base/metrics/histogram_functions.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/common/content_features.h"

namespace content {

void AndroidSpareRendererNavigationThrottle::CreateAndAdd(
    NavigationThrottleRegistry& registry) {
  registry.AddThrottle(
      std::make_unique<AndroidSpareRendererNavigationThrottle>(registry));
}

AndroidSpareRendererNavigationThrottle::AndroidSpareRendererNavigationThrottle(
    NavigationThrottleRegistry& registry)
    : NavigationThrottle(registry) {}

AndroidSpareRendererNavigationThrottle::
    ~AndroidSpareRendererNavigationThrottle() {
  // If render_process_host_ is already destructed, `RenderProcessHostDestroyed`
  // will reset it. Thus it is safe to access the raw_ptr here.
  if (render_process_host_) {
    render_process_host_->RemoveObserver(this);
  }
}

NavigationThrottle::ThrottleCheckResult
AndroidSpareRendererNavigationThrottle::WillStartRequest() {
  RenderProcessHost* rph = GetSpeculativeRenderProcessHost();
  bool will_defer =
      rph && rph->ShouldThrottleNavigationForSpareRendererGraduation();
  base::UmaHistogramBoolean(
      "Navigation.AndroidSpareRendererNavigationThrottle.WillDefer",
      will_defer);
  if (!will_defer) {
    return NavigationThrottle::PROCEED;
  }
  rph->AddObserver(this);
  render_process_host_ = rph;
  if (base::TimeTicks::IsHighResolution()) {
    defer_timer_ = std::make_unique<base::ElapsedTimer>();
  }
  return NavigationThrottle::DEFER;
}

RenderProcessHost*
AndroidSpareRendererNavigationThrottle::GetSpeculativeRenderProcessHost() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  if (navigation_request->GetAssociatedRFHType() !=
      NavigationRequest::AssociatedRenderFrameHostType::SPECULATIVE) {
    return nullptr;
  }
  RenderFrameHostImpl* speculative_rfh = navigation_request->frame_tree_node()
                                             ->render_manager()
                                             ->speculative_frame_host();
  return speculative_rfh ? speculative_rfh->GetProcess() : nullptr;
}

void AndroidSpareRendererNavigationThrottle::SpareRendererPriorityGraduated(
    RenderProcessHost* host,
    bool is_alive) {
  // Because of the asynchronous process state notification on Android, the
  // process may become killed before RenderProcessExited signal arrives.
  // If the process is killed, we will wait for RenderProcessExited before
  // resuming the navigation.
  if (is_alive) {
    MaybeResume(/*is_alive=*/true);
  }
}

void AndroidSpareRendererNavigationThrottle::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  // Resume the navigation when the browser receives the ProcessDied signal
  // of the renderer. If the feature
  // ResumeNavigationWithSpeculativeRFHProcessGone is enabled, the browser is
  // able to resume the navigation with a new renderer process. Otherwise the
  // navigation will be aborted.
  MaybeResume(/*is_alive=*/false);
}

void AndroidSpareRendererNavigationThrottle::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  // RenderProcessHostDestroyed can be called without RenderProcessExited. We
  // need to resume the navigation even in that case.
  MaybeResume(/*is_alive=*/false);
}

void AndroidSpareRendererNavigationThrottle::MaybeResume(bool is_alive) {
  if (render_process_host_) {
    render_process_host_->RemoveObserver(this);
    base::UmaHistogramBoolean(
        "Navigation.AndroidSpareRendererNavigationThrottle."
        "ProcessAliveWhenResume",
        is_alive);
    if (defer_timer_) {
      base::UmaHistogramMicrosecondsTimes(
          "Navigation.AndroidSpareRendererNavigationThrottle.DeferTime",
          defer_timer_->Elapsed());
      defer_timer_.reset();
    }
    render_process_host_ = nullptr;
    Resume();
  }
}

const char* AndroidSpareRendererNavigationThrottle::GetNameForLogging() {
  return "AndroidSpareRendererNavigationThrottle";
}

}  // namespace content
