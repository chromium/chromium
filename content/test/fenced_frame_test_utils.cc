// Copyright 2022 The Chromium Authors
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
  SharedStorageBudgetMetadata budget_metadata = {
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
    const absl::optional<FencedFrameProperties>& properties) {
  mapping_complete_observed_ = true;
  observed_fenced_frame_properties_ = properties;
}

bool FencedFrameURLMappingTestPeer::HasObserver(
    const GURL& urn_uuid,
    FencedFrameURLMapping::MappingResultObserver* observer) {
  return fenced_frame_url_mapping_->IsPendingMapped(urn_uuid) &&
         fenced_frame_url_mapping_->pending_urn_uuid_to_url_map_.at(urn_uuid)
             .count(observer);
}

void FencedFrameURLMappingTestPeer::GetSharedStorageReportingMap(
    const GURL& urn_uuid,
    SharedStorageReportingMap* out_reporting_map) {
  DCHECK(out_reporting_map);

  auto urn_it = fenced_frame_url_mapping_->urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(urn_it != fenced_frame_url_mapping_->urn_uuid_to_url_map_.end());

  if (!urn_it->second.reporting_metadata_.has_value())
    return;

  auto data_it =
      urn_it->second.reporting_metadata_->GetValueIgnoringVisibility()
          .metadata.find(blink::FencedFrame::ReportingDestination::
                             kSharedStorageSelectUrl);

  if (data_it !=
      urn_it->second.reporting_metadata_->GetValueIgnoringVisibility()
          .metadata.end())
    *out_reporting_map = data_it->second;
}

void FencedFrameURLMappingTestPeer::FillMap(const GURL& url) {
  while (!fenced_frame_url_mapping_->IsFull()) {
    auto it = fenced_frame_url_mapping_->AddMappingForUrl(url);
    DCHECK(it.has_value());
  }

  DCHECK(fenced_frame_url_mapping_->IsFull());
}

}  // namespace content
