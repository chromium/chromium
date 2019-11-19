// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// static
std::unique_ptr<PortalNavigationThrottle>
PortalNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  if (!IsEnabled() || !navigation_handle->IsInMainFrame())
    return nullptr;

  return base::WrapUnique(new PortalNavigationThrottle(navigation_handle));
}

// static
bool PortalNavigationThrottle::IsEnabled() {
  return Portal::IsEnabled() &&
         !base::FeatureList::IsEnabled(blink::features::kPortalsCrossOrigin);
}

PortalNavigationThrottle::PortalNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

PortalNavigationThrottle::~PortalNavigationThrottle() = default;

const char* PortalNavigationThrottle::GetNameForLogging() {
  return "PortalNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
PortalNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

NavigationThrottle::ThrottleCheckResult
PortalNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

NavigationThrottle::ThrottleCheckResult
PortalNavigationThrottle::WillStartOrRedirectRequest() {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(navigation_handle()->GetWebContents());
  Portal* portal = web_contents->portal();
  if (!portal)
    return PROCEED;

  url::Origin origin = url::Origin::Create(navigation_handle()->GetURL());
  url::Origin first_party_origin =
      portal->owner_render_frame_host()->GetLastCommittedOrigin();

  return origin == first_party_origin ? PROCEED : BLOCK_REQUEST;
}

}  // namespace content
