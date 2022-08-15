// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
#define CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "net/base/net_errors.h"

namespace content {

class FrameTreeNode;

// `node` is expected to be the child FrameTreeNode created in response to a
// <fencedframe> element being created. This method:
//    - Returns `node` if we're in the ShadowDOM version
//    - Returns the FrameTreeNode of the fenced frame's inner FrameTree, if
//    we're in the MPArch version of fenced frames
FrameTreeNode* GetFencedFrameRootNode(FrameTreeNode* node);

void SimulateSharedStorageURNMappingComplete(
    FencedFrameURLMapping& fenced_frame_url_mapping,
    const GURL& urn_uuid,
    const GURL& mapped_url,
    const url::Origin& shared_storage_origin,
    double budget_to_charge,
    const std::string& report_event = "",
    const GURL& report_url = GURL());

// Tests can use this class to observe and check the URL mapping result.
class TestFencedFrameURLMappingResultObserver
    : public FencedFrameURLMapping::MappingResultObserver {
 public:
  TestFencedFrameURLMappingResultObserver();
  ~TestFencedFrameURLMappingResultObserver() override;

  void OnFencedFrameURLMappingComplete(
      const absl::optional<FencedFrameURLMapping::FencedFrameProperties>&
          properties) override;

  bool mapping_complete_observed() const { return mapping_complete_observed_; }

  const absl::optional<GURL>& mapped_url() const { return mapped_url_; }

  const absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>&
  pending_ad_components_map() const {
    return pending_ad_components_map_;
  }

  const absl::optional<AdAuctionData> ad_auction_data() const {
    return ad_auction_data_;
  }

  ReportingMetadata reporting_metadata() { return reporting_metadata_; }

 private:
  bool mapping_complete_observed_ = false;
  absl::optional<GURL> mapped_url_;
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
      pending_ad_components_map_;
  absl::optional<AdAuctionData> ad_auction_data_;
  ReportingMetadata reporting_metadata_;
};

}  // namespace content

#endif  // CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
