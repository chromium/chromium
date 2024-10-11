// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_url_loader_throttle.h"

#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"

namespace content {

PrerenderURLLoaderThrottle::PrerenderURLLoaderThrottle(
    FrameTreeNodeId frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

// static
std::unique_ptr<PrerenderURLLoaderThrottle>
PrerenderURLLoaderThrottle::MaybeCreate(FrameTreeNodeId frame_tree_node_id) {
  if (!base::FeatureList::IsEnabled(
          features::kPrerender2FallbackPrefetchSpecRules)) {
    return nullptr;
  }

  auto* frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node) {
    return nullptr;
  }

  PrerenderHostRegistry* prerender_host_registry =
      frame_tree_node->current_frame_host()
          ->delegate()
          ->GetPrerenderHostRegistry();
  if (!prerender_host_registry) {
    return nullptr;
  }

  PrerenderHost* prerender_host =
      prerender_host_registry->FindNonReservedHostById(frame_tree_node_id);
  if (!prerender_host) {
    return nullptr;
  }

  if (!prerender_host->ShouldAbortNavigationBecausePrefetchUnavailable()) {
    return nullptr;
  }

  // If the prefetch ahead of prerender "failed", `PrerenderURLLoaderThrottle`
  // is added to the `ThrottlingURLLoader` for the corresponding prerendering
  // navigation, and the `PrerenderURLLoaderThrottle` always cancels the network
  // request before it starts, which cancels the prerender.
  return std::make_unique<PrerenderURLLoaderThrottle>(frame_tree_node_id);
}

void PrerenderURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  delegate_->CancelWithError(net::ERR_ABORTED);

  auto* frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!frame_tree_node) {
    return;
  }

  PrerenderHostRegistry* prerender_host_registry =
      frame_tree_node->current_frame_host()
          ->delegate()
          ->GetPrerenderHostRegistry();
  if (!prerender_host_registry) {
    return;
  }

  prerender_host_registry->CancelHost(
      frame_tree_node_id_,
      PrerenderCancellationReason(
          PrerenderFinalStatus::kPrerenderFailedDuringPrefetch));
}

}  // namespace content
