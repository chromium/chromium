// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_url_loader_throttle.h"

#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"

namespace content {

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

  return std::make_unique<PrerenderURLLoaderThrottle>();
}

void PrerenderURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  delegate_->CancelWithError(net::ERR_ABORTED);
}

}  // namespace content
