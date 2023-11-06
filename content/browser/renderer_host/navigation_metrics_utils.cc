// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_metrics_utils.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {

void MaybeRecordAdClickMainFrameNavigationUseCounter(
    RenderFrameHostImpl* initiator_frame,
    blink::mojom::NavigationInitiatorActivationAndAdStatus
        initiator_activation_and_ad_status) {
  if (!initiator_frame) {
    return;
  }

  if (initiator_activation_and_ad_status ==
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kStartedWithTransientActivationFromAd) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        initiator_frame, blink::mojom::WebFeature::kAdClickMainFrameNavigation);
  }
}

}  // namespace content
