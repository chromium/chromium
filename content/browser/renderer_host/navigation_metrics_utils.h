// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_METRICS_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_METRICS_UTILS_H_

#include "url/gurl.h"

namespace content {

class RenderFrameHostImpl;

// Records the AdClickMainFrameNavigation use counter for `initiator_frame`, and
// records the Navigation.MainFrame.FromAdClick UMA, if the inputs
// (`started_with_transient_activation` and `started_by_ad`) indicate that the
// navigation is from an ad click. `target_frame` is the navigating frame and
// `target_url` is the url being navigated to.
//
// Precondition: The navigation is targeting the outermost main frame.
// It's only necessary to call this function for renderer-initiated navigations,
// as browser-initiated navigations can never be initiated from an ad.
void MaybeRecordAdClickMainFrameNavigationMetrics(
    RenderFrameHostImpl* initiator_frame,
    RenderFrameHostImpl* target_frame,
    const GURL& target_url,
    bool started_with_transient_activation,
    bool started_by_ad);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_METRICS_UTILS_H_
