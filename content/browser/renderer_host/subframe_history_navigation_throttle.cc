// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/subframe_history_navigation_throttle.h"

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

SubframeHistoryNavigationThrottle::SubframeHistoryNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

SubframeHistoryNavigationThrottle::~SubframeHistoryNavigationThrottle() =
    default;

NavigationThrottle::ThrottleCheckResult
SubframeHistoryNavigationThrottle::WillStartRequest() {
  // This will defer cross-document subframe history requests.
  return DEFER;
}

NavigationThrottle::ThrottleCheckResult
SubframeHistoryNavigationThrottle::WillCommitWithoutUrlLoader() {
  // This will defer same-document subframe history commits.
  return DEFER;
}

const char* SubframeHistoryNavigationThrottle::GetNameForLogging() {
  return "SubframeHistoryNavigationThrottle";
}

void SubframeHistoryNavigationThrottle::Resume() {
  NavigationThrottle::Resume();
}

void SubframeHistoryNavigationThrottle::Cancel() {
  CancelDeferredNavigation(CANCEL_AND_IGNORE);
}

// static
std::unique_ptr<NavigationThrottle>
SubframeHistoryNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  // `main_frame_same_document_history_token()` will only be set if the
  // navigation is a main-frame same-document history navigation and it might
  // be cancelable by a navigate event. There's no need to defer the subframe
  // navigation unless the navigate event might cancel the entire history
  // traversal.
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  auto main_frame_token = request->main_frame_same_document_history_token();
  if (!main_frame_token) {
    return nullptr;
  }
  DCHECK(!request->IsInMainFrame());

  RenderFrameHostImpl* root_frame_host =
      request->frame_tree_node()->frame_tree().root()->current_frame_host();
  NavigationRequest* root_frame_navigation_request =
      root_frame_host->GetSameDocumentNavigationRequest(*main_frame_token);
  // If the main-frame same-document history navigation already completed, the
  // throttle is no longer necessary. This can happen when `request` is
  // cross-document and had to wait on a slow beforeunload handler.
  if (!root_frame_navigation_request) {
    return nullptr;
  }
  auto throttle = std::make_unique<SubframeHistoryNavigationThrottle>(request);
  root_frame_navigation_request->AddDeferredSubframeNavigationThrottle(
      throttle->weak_factory_.GetWeakPtr());
  return throttle;
}

}  // namespace content
