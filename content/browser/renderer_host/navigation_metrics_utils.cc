// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_metrics_utils.h"

#include "base/metrics/histogram_macros.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/url_constants.h"

namespace content {

void MaybeRecordAdClickMainFrameNavigationMetrics(
    RenderFrameHostImpl* initiator_frame,
    RenderFrameHostImpl* target_frame,
    const GURL& target_url,
    bool started_with_transient_activation,
    bool started_by_ad) {
  if (!initiator_frame || !started_by_ad) {
    return;
  }

  if (started_with_transient_activation) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        initiator_frame, blink::mojom::WebFeature::kAdClickMainFrameNavigation);

    UMA_HISTOGRAM_BOOLEAN("Navigation.MainFrame.FromAdClick", true);
    return;
  }

  if (!target_frame) {
    return;
  }

  // There is no gesture, but ad script is navigating. Filter out some likely
  // legit (e.g., maybe false positive) cases like same-site navs from ad
  // script.
  if (initiator_frame->GetSiteInstance()->IsSameSiteWithURL(target_url)) {
    return;
  }

  // Also ignore cases where the navigation is occurring in a new tab from the
  // opener without gesture as this is likely just the result of an ad being
  // clicked on.
  if (target_frame->IsOutermostMainFrame() &&
      target_frame->frame_tree_node()->opener() ==
          initiator_frame->frame_tree_node() &&
      (target_frame->GetLastCommittedURL().is_empty() ||
       target_frame->GetLastCommittedURL() == url::kAboutBlankURL)) {
    return;
  }

  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      initiator_frame,
      blink::mojom::WebFeature::kAdScriptMainFrameNavigationWithoutUserGesture);
}

}  // namespace content
