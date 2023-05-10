// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mixed_content_navigation_throttle.h"

#include <vector>

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"

namespace content {

// static
std::unique_ptr<NavigationThrottle>
MixedContentNavigationThrottle::CreateThrottleForNavigation(
    NavigationHandle* navigation_handle) {
  return std::make_unique<MixedContentNavigationThrottle>(navigation_handle);
}

MixedContentNavigationThrottle::MixedContentNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

MixedContentNavigationThrottle::~MixedContentNavigationThrottle() {}

NavigationThrottle::ThrottleCheckResult
MixedContentNavigationThrottle::WillStartRequest() {
  bool should_block =
      checker_.ShouldBlockNavigation(*navigation_handle(), false);
  return should_block ? CANCEL : PROCEED;
}

NavigationThrottle::ThrottleCheckResult
MixedContentNavigationThrottle::WillRedirectRequest() {
  // Upon redirects the same checks are to be executed as for requests.
  bool should_block =
      checker_.ShouldBlockNavigation(*navigation_handle(), true);
  if (!should_block) {
    MaybeHandleCertificateError();
    return PROCEED;
  }
  return CANCEL;
}

NavigationThrottle::ThrottleCheckResult
MixedContentNavigationThrottle::WillProcessResponse() {
  MaybeHandleCertificateError();
  return PROCEED;
}

const char* MixedContentNavigationThrottle::GetNameForLogging() {
  return "MixedContentNavigationThrottle";
}

void MixedContentNavigationThrottle::MaybeHandleCertificateError() {
  // The outermost main frame certificate errors are handled separately in
  // SSLManager.
  if (navigation_handle()->IsInOutermostMainFrame()) {
    return;
  }

  // If there was no SSL info, then it was not an HTTPS resource load, and we
  // can ignore it.
  if (!navigation_handle()->GetSSLInfo()) {
    return;
  }

  if (!net::IsCertStatusError(navigation_handle()->GetSSLInfo()->cert_status)) {
    return;
  }

  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  RenderFrameHostImpl* rfh = request->frame_tree_node()->current_frame_host();
  rfh->OnDidRunContentWithCertificateErrors();
}

}  // namespace content
