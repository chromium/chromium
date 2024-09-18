// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/partitioned_popins/partitioned_popins_navigation_throttle.h"

#include "base/feature_list.h"
#include "content/browser/renderer_host/partitioned_popins/partitioned_popins_policy.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
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
  // Partitioned popins are special modal popups that are partitioned as
  // though they were an iframe embedded in the opener. All main-frame
  // navigations and redirects must set a request header to notify the loaded
  // site they are in a partitioned popin and not a standard popup.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  navigation_handle()->SetRequestHeader("Sec-Popin-Context", "partitioned");
  return PROCEED;
}

NavigationThrottle::ThrottleCheckResult
PartitionedPopinsNavigationThrottle::WillRedirectRequest() {
  // Partitioned popin top-frames must respond with a policy that permits the
  // top-frame-origin of the popin opener.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  if (DoesPopinPolicyBlockResponse()) {
    return BLOCK_REQUEST;
  }
  // Partitioned popin top-frames cannot redirect to HTTP pages, if this occurs
  // we need to block the request.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  if (!navigation_handle()->GetURL().SchemeIs(url::kHttpsScheme)) {
    return BLOCK_REQUEST;
  }
  return PROCEED;
}

NavigationThrottle::ThrottleCheckResult
PartitionedPopinsNavigationThrottle::WillProcessResponse() {
  // Partitioned popin top-frames must respond with a policy that permits the
  // top-frame-origin of the popin opener.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  if (DoesPopinPolicyBlockResponse()) {
    return BLOCK_RESPONSE;
  }
  return PROCEED;
}

PartitionedPopinsNavigationThrottle::PartitionedPopinsNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

bool PartitionedPopinsNavigationThrottle::DoesPopinPolicyBlockResponse() {
  if (base::FeatureList::IsEnabled(
          features::kPartitionedPopinsHeaderPolicyBypass)) {
    return false;
  }
  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();
  const WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(navigation_handle()->GetWebContents());
  if (!web_contents || !response_headers ||
      !response_headers->HasHeader("Popin-Policy")) {
    return true;
  }
  std::string untrusted_popin_policy;
  response_headers->GetNormalizedHeader("Popin-Policy",
                                        &untrusted_popin_policy);
  PartitionedPopinsPolicy policy(untrusted_popin_policy);
  if (policy.wildcard) {
    return false;
  }
  for (const url::Origin& policy_origin : policy.origins) {
    if (policy_origin == web_contents->PartitionedPopinOpener()
                             ->GetMainFrame()
                             ->GetLastCommittedOrigin()) {
      return false;
    }
  }
  return true;
}

}  // namespace content
