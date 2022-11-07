// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
#define CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "net/base/net_errors.h"

namespace content {

class FrameTreeNode;
class MappingResultObserver;

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

  const absl::optional<FencedFrameURLMapping::FencedFrameProperties>&
  fenced_frame_properties() {
    return fenced_frame_properties_;
  }

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
  absl::optional<FencedFrameURLMapping::FencedFrameProperties>
      fenced_frame_properties_;
  absl::optional<GURL> mapped_url_;
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
      pending_ad_components_map_;
  absl::optional<AdAuctionData> ad_auction_data_;
  ReportingMetadata reporting_metadata_;
};

class FencedFrameURLMappingTestPeer {
 public:
  FencedFrameURLMappingTestPeer() = delete;
  explicit FencedFrameURLMappingTestPeer(
      FencedFrameURLMapping* fenced_frame_url_mapping)
      : fenced_frame_url_mapping_(fenced_frame_url_mapping) {}

  bool HasObserver(const GURL& urn_uuid,
                   FencedFrameURLMapping::MappingResultObserver* observer);

  // Returns as an out parameter the `ReportingMetadata`'s map for value
  // `"shared-storage-select-url"` associated with `urn_uuid`, or leaves the out
  // parameter unchanged if there's no shared storage reporting metadata
  // associated (i.e. `urn_uuid` did not originate from shared storage or else
  // there was no metadata passed from JavaScript). Precondition: `urn_uuid`
  // exists in `urn_uuid_to_url_map_`.
  void GetSharedStorageReportingMap(
      const GURL& urn_uuid,
      SharedStorageReportingMap* out_reporting_map);

  // Insert urn mappings until it reaches the limit.
  void FillMap(const GURL& url);

 private:
  FencedFrameURLMapping* fenced_frame_url_mapping_;
};

}  // namespace content

#endif  // CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
