// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/untrusted_web_ui_navigation_throttle.h"

#include <optional>

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/url_constants.h"
#include "url/origin.h"

namespace content {

namespace {

bool IsHostingUntrustedWebUI(RenderFrameHostImpl* rfh) {
  return rfh &&
         (rfh->GetLastCommittedURL().SchemeIs(kChromeUIUntrustedScheme) ||
          rfh->GetLastCommittedOrigin().scheme() == kChromeUIUntrustedScheme);
}

}  // namespace

// static
void UntrustedWebUINavigationThrottle::MaybeCreateAndAdd(
    NavigationThrottleRegistry& registry) {
  NavigationHandle* handle = &registry.GetNavigationHandle();
  // Only renderer-initiated navigations can be unexpected / malicious from
  // untrusted web content. Browser-initiated navigations are always allowed.
  if (!handle->IsRendererInitiated()) {
    return;
  }

  NavigationRequest* request = NavigationRequest::From(handle);
  RenderFrameHostImpl* current_rfh =
      request->frame_tree_node()->current_frame_host();
  // Only create this throttle if the target frame is currently hosting
  // chrome-untrusted:// WebUI content.
  if (!IsHostingUntrustedWebUI(current_rfh)) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<UntrustedWebUINavigationThrottle>(registry));
}

UntrustedWebUINavigationThrottle::UntrustedWebUINavigationThrottle(
    NavigationThrottleRegistry& registry)
    : NavigationThrottle(registry) {}

UntrustedWebUINavigationThrottle::~UntrustedWebUINavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
UntrustedWebUINavigationThrottle::WillStartRequest() {
  return ProcessNavigation();
}

NavigationThrottle::ThrottleCheckResult
UntrustedWebUINavigationThrottle::WillRedirectRequest() {
  return ProcessNavigation();
}

NavigationThrottle::ThrottleCheckResult
UntrustedWebUINavigationThrottle::WillCommitWithoutUrlLoader() {
  return ProcessNavigation();
}

const char* UntrustedWebUINavigationThrottle::GetNameForLogging() {
  return "UntrustedWebUINavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
UntrustedWebUINavigationThrottle::ProcessNavigation() {
  CHECK(navigation_handle()->IsRendererInitiated());

  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  RenderFrameHostImpl* current_rfh =
      request->frame_tree_node()->current_frame_host();
  // If the target frame is not currently hosting chrome-untrusted:// content,
  // there is nothing to throttle.
  if (!IsHostingUntrustedWebUI(current_rfh)) {
    return NavigationThrottle::PROCEED;
  }

  // Check the initiating RenderFrameHostImpl if available.
  if (request->GetInitiatorFrameToken().has_value()) {
    RenderFrameHostImpl* initiator_rfh = RenderFrameHostImpl::FromFrameToken(
        request->GetInitiatorProcessId(),
        request->GetInitiatorFrameToken().value());
    if (initiator_rfh) {
      // 1. Allow if the initiator is the exact same frame (self-navigation).
      if (initiator_rfh == current_rfh) {
        return NavigationThrottle::PROCEED;
      }

      // 2. Allow if the initiator is an ancestor frame of the target frame
      // (e.g. chrome:// parent embedding and navigating an inner
      // chrome-untrusted:// iframe).
      for (RenderFrameHostImpl* parent =
               current_rfh->GetParentOrOuterDocument();
           parent; parent = parent->GetParentOrOuterDocument()) {
        if (parent == initiator_rfh) {
          return NavigationThrottle::PROCEED;
        }
      }
    }
  }

  const std::optional<url::Origin>& initiator_origin =
      navigation_handle()->GetInitiatorOrigin();
  if (!initiator_origin.has_value()) {
    return NavigationThrottle::CANCEL_AND_IGNORE;
  }

  // 3. Allow if the initiator is same-origin with the current document
  // (e.g. same-origin popup or same-origin sibling frame).
  if (initiator_origin->IsSameOriginWith(
          current_rfh->GetLastCommittedOrigin())) {
    return NavigationThrottle::PROCEED;
  }

  // Block unexpected cross-origin renderer-initiated navigation of the
  // chrome-untrusted:// frame.
  return NavigationThrottle::CANCEL_AND_IGNORE;
}

}  // namespace content
