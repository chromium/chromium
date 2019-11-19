// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/webui_navigation_throttle.h"

#include "base/command_line.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"

namespace content {

WebUINavigationThrottle::WebUINavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

WebUINavigationThrottle::~WebUINavigationThrottle() {}

NavigationThrottle::ThrottleCheckResult
WebUINavigationThrottle::WillStartRequest() {
  // Allow only chrome: scheme documents to be navigated to.
  if (navigation_handle()->GetURL().SchemeIs(kChromeUIScheme))
    return PROCEED;

  return BLOCK_REQUEST;
}

const char* WebUINavigationThrottle::GetNameForLogging() {
  return "WebUINavigationThrottle";
}

// static
std::unique_ptr<NavigationThrottle>
WebUINavigationThrottle::CreateThrottleForNavigation(
    NavigationHandle* navigation_handle) {
  // The WebUI security model (which keeps renderes with WebUI bindings separate
  // from untrusted renderers) only makes sense in multi-process mode.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess))
    return nullptr;

  // Only throttle subframe navigations.
  if (navigation_handle->IsInMainFrame())
    return nullptr;

  RenderFrameHostImpl* parent = NavigationRequest::From(navigation_handle)
                                    ->frame_tree_node()
                                    ->parent()
                                    ->current_frame_host();

  // Throttle if the renderer process has WebUI bindings, or if the parent frame
  // is on a WebUI page.
  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          parent->GetProcess()->GetID()) ||
      parent->GetLastCommittedURL().SchemeIs(kChromeUIScheme)) {
    return std::make_unique<WebUINavigationThrottle>(navigation_handle);
  }

  return nullptr;
}

}  // namespace content
