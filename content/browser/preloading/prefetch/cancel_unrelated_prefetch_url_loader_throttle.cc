// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/cancel_unrelated_prefetch_url_loader_throttle.h"

#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

CancelUnrelatedPrefetchURLLoaderThrottle::
    CancelUnrelatedPrefetchURLLoaderThrottle(FrameTreeNodeId frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

// static
std::unique_ptr<CancelUnrelatedPrefetchURLLoaderThrottle>
CancelUnrelatedPrefetchURLLoaderThrottle::MaybeCreate(
    FrameTreeNodeId frame_tree_node_id) {
  if (!base::FeatureList::IsEnabled(
          features::kPrefetchCancelUnrelatedPrefetch)) {
    return nullptr;
  }

  auto* frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node) {
    return nullptr;
  }

  // Only non-prerender navigations are targets.

  if (!frame_tree_node->IsOutermostMainFrame()) {
    return nullptr;
  }

  PrerenderHostId prerender_host_id =
      frame_tree_node->frame_tree().delegate()->GetPrerenderHostId();
  if (prerender_host_id) {
    return nullptr;
  }

  return std::make_unique<CancelUnrelatedPrefetchURLLoaderThrottle>(
      frame_tree_node_id);
}

void CancelUnrelatedPrefetchURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  auto* frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  CHECK(frame_tree_node);

  auto* prefetch_service =
      PrefetchService::GetFromFrameTreeNodeId(frame_tree_node_id_);
  if (!prefetch_service) {
    return;
  }

  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  if (!navigation_request) {
    return;
  }
  const std::optional<blink::DocumentToken>& initiator_document_token =
      navigation_request->GetInitiatorDocumentToken();

  prefetch_service->CancelUnrelatedPrefetchForNavigation(
      initiator_document_token);
}

}  // namespace content
