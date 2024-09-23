// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_METRICS_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_METRICS_UTILS_H_

#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"

namespace content {

class RenderFrameHostImpl;

// Records the AdClickMainFrameNavigation use counter for `initiator_frame`, and
// records the Navigation.MainFrame.FromAdClick UMA, if
// `initiator_activation_and_ad_status` indicates that the navigation is from an
// ad click. Precondition: The navigation is targeting the outermost main frame.
// It's only necessary to call this function for renderer-initiated navigations,
// as browser-initiated navigations can never be initiated from ad.
void MaybeRecordAdClickMainFrameNavigationMetrics(
    RenderFrameHostImpl* initiator_frame,
    blink::mojom::NavigationInitiatorActivationAndAdStatus
        initiator_activation_and_ad_status);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_METRICS_UTILS_H_
