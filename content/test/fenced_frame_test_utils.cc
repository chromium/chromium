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

using SharedStorageReportingMap = base::flat_map<std::string, ::GURL>;

FrameTreeNode* GetFencedFrameRootNode(FrameTreeNode* node) {
  if (blink::features::kFencedFramesImplementationTypeParam.Get() ==
      blink::features::FencedFramesImplementationType::kShadowDOM) {
    return node;
  }

  int inner_node_id =
      node->current_frame_host()->inner_tree_main_frame_tree_node_id();
  return FrameTreeNode::GloballyFindByID(inner_node_id);
}

void SimulateSharedStorageURNMappingComplete(
    FencedFrameURLMapping& fenced_frame_url_mapping,
    const GURL& urn_uuid,
    const GURL& mapped_url,
    const url::Origin& shared_storage_origin,
    double budget_to_charge,
    const std::string& report_event,
    const GURL& report_url) {
  FencedFrameURLMapping::SharedStorageBudgetMetadata budget_metadata = {
      .origin = shared_storage_origin, .budget_to_charge = budget_to_charge};

  SharedStorageReportingMap reporting_map(
      {std::make_pair(report_event, report_url)});

  fenced_frame_url_mapping.OnSharedStorageURNMappingResultDetermined(
      urn_uuid, FencedFrameURLMapping::SharedStorageURNMappingResult(
                    mapped_url, budget_metadata, reporting_map));
}

TestFencedFrameURLMappingResultObserver::
    TestFencedFrameURLMappingResultObserver() = default;

TestFencedFrameURLMappingResultObserver::
    ~TestFencedFrameURLMappingResultObserver() = default;

void TestFencedFrameURLMappingResultObserver::OnFencedFrameURLMappingComplete(
    absl::optional<GURL> mapped_url,
    absl::optional<AdAuctionData> ad_auction_data,
    absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
        pending_ad_components_map,
    ReportingMetadata& reporting_metadata) {
  mapping_complete_observed_ = true;
  mapped_url_ = std::move(mapped_url);
  ad_auction_data_ = ad_auction_data;
  pending_ad_components_map_ = std::move(pending_ad_components_map);
  reporting_metadata_ = reporting_metadata;
}

}  // namespace content
