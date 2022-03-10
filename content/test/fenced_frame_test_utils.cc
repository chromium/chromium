// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fenced_frame_test_utils.h"

#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "third_party/blink/public/common/features.h"

namespace content {

FrameTreeNode* GetFencedFrameRootNode(FrameTreeNode* node) {
  if (blink::features::kFencedFramesImplementationTypeParam.Get() ==
      blink::features::FencedFramesImplementationType::kShadowDOM) {
    return node;
  }

  int inner_node_id =
      node->current_frame_host()->inner_tree_main_frame_tree_node_id();
  return FrameTreeNode::GloballyFindByID(inner_node_id);
}

TestFencedFrameURLMappingResultObserver::
    TestFencedFrameURLMappingResultObserver() = default;

TestFencedFrameURLMappingResultObserver::
    ~TestFencedFrameURLMappingResultObserver() = default;

void TestFencedFrameURLMappingResultObserver::OnFencedFrameURLMappingComplete(
    absl::optional<GURL> mapped_url,
    absl::optional<AdAuctionData> ad_auction_data,
    absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
        pending_ad_components_map) {
  mapping_complete_observed_ = true;
  mapped_url_ = std::move(mapped_url);
  ad_auction_data_ = ad_auction_data;
  pending_ad_components_map_ = std::move(pending_ad_components_map);
}

}  // namespace content
