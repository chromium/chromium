// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COOKIE_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_COOKIE_UTILS_H_

#include "content/browser/renderer_host/navigation_request.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"

namespace content {

class RenderFrameHostImpl;
struct CookieAccessDetails;

// Sorts cookies into allowed (cookies that were included for the access
// attempt) and blocked (cookies that were excluded because they were
// blocked by the user's preferences or 3PCD). Cookies that are excluded
// independently of the user's cookie blocking settings are not included in
// either of the outputs.
void SplitCookiesIntoAllowedAndBlocked(
    const network::mojom::CookieAccessDetailsPtr& cookie_details,
    CookieAccessDetails* allowed,
    CookieAccessDetails* blocked);

// Logs cookie warnings to DevTools Issues Panel and logs events to UseCounters
// and UKM. Does not log to the JS console.
// TODO(crbug.com/40632967): Remove when no longer needed.
void EmitCookieWarningsAndMetrics(
    RenderFrameHostImpl* rfh,
    NavigationRequest* navigation_request,
    const network::mojom::CookieAccessDetailsPtr& cookie_details);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COOKIE_UTILS_H_
