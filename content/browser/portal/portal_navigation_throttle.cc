// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/url_utils.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// A URL where developers can learn more about why this navigation throttle may
// have cancelled their request.
const char* GetBlockedInfoURL() {
  return "https://www.chromium.org/blink/origin-trials/portals";
}

}  // namespace

// static
std::unique_ptr<PortalNavigationThrottle>
PortalNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  if (!Portal::IsEnabled() || !navigation_handle->IsInMainFrame() ||
      navigation_handle->GetNavigatingFrameType() ==
          FrameType::kFencedFrameRoot) {
    return nullptr;
  }

  return base::WrapUnique(new PortalNavigationThrottle(navigation_handle));
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

  DCHECK_NE(navigation_handle()->GetNavigatingFrameType(),
            FrameType::kFencedFrameRoot);

  GURL url = navigation_handle()->GetURL();
  CHECK(!HasWebUIScheme(url))
      << "Portals should not even be able to attempt to reach WebUI";
  if (!url.SchemeIsHTTPOrHTTPS()) {
    base::StringPiece scheme = url.scheme_piece();
    portal->owner_render_frame_host()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf("Navigating a portal to scheme '%.*s' was blocked.",
                           static_cast<int>(scheme.size()), scheme.data()));
    return CANCEL;
  }

  if (!base::FeatureList::IsEnabled(blink::features::kPortalsCrossOrigin)) {
    url::Origin origin = url::Origin::Create(url);
    url::Origin first_party_origin =
        portal->owner_render_frame_host()->GetLastCommittedOrigin();

    if (origin != first_party_origin) {
      portal->owner_render_frame_host()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf(
              "Navigating a portal to cross-origin content (from %s) "
              "is not currently permitted and was blocked. "
              "See %s for more information.",
              origin.Serialize().c_str(), GetBlockedInfoURL()));
      return CANCEL;
    }
  }

  return PROCEED;
}

}  // namespace content
