// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/partitioned_popins/partitioned_popins_navigation_throttle.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {

// static
std::unique_ptr<PartitionedPopinsNavigationThrottle>
PartitionedPopinsNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  CHECK(navigation_handle);
  // Only the outermost frame in a partitioned popin needs the throttle.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(navigation_handle->GetWebContents());
  if (navigation_handle->IsInOutermostMainFrame() && web_contents &&
      web_contents->IsPartitionedPopin()) {
    return base::WrapUnique(
        new PartitionedPopinsNavigationThrottle(navigation_handle));
  }
  return nullptr;
}

const char* PartitionedPopinsNavigationThrottle::GetNameForLogging() {
  return "PartitionedPopinsNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
PartitionedPopinsNavigationThrottle::WillStartRequest() {
  // Partitioned popin top-frames cannot navigate to HTTP pages, if this occurs
  // we need to block the request.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  if (!navigation_handle()->GetURL().SchemeIs(url::kHttpsScheme)) {
    return BLOCK_REQUEST;
  }
  return PROCEED;
}

NavigationThrottle::ThrottleCheckResult
PartitionedPopinsNavigationThrottle::WillRedirectRequest() {
  // Partitioned popin top-frames cannot redirect to HTTP pages, if this occurs
  // we need to block the request.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  if (!navigation_handle()->GetURL().SchemeIs(url::kHttpsScheme)) {
    return BLOCK_REQUEST;
  }
  return PROCEED;
}

PartitionedPopinsNavigationThrottle::PartitionedPopinsNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

}  // namespace content
