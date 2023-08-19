// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
#define CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "net/base/net_errors.h"

namespace content {

class FrameTreeNode;
class RenderFrameHost;
class MappingResultObserver;

// `node` is expected to be the child FrameTreeNode created in response to a
// <fencedframe> element being created. This method returns the FrameTreeNode of
// the fenced frame's inner FrameTree.
FrameTreeNode* GetFencedFrameRootNode(FrameTreeNode* node);

void SimulateSharedStorageURNMappingComplete(
    FencedFrameURLMapping& fenced_frame_url_mapping,
    const GURL& urn_uuid,
    const GURL& mapped_url,
    const url::Origin& shared_storage_origin,
    double budget_to_charge,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter = nullptr);

// Tests can use this class to observe and check the URL mapping result.
class TestFencedFrameURLMappingResultObserver
    : public FencedFrameURLMapping::MappingResultObserver {
 public:
  TestFencedFrameURLMappingResultObserver();
  ~TestFencedFrameURLMappingResultObserver() override;

  void OnFencedFrameURLMappingComplete(
      const absl::optional<FencedFrameProperties>& properties) override;

  bool mapping_complete_observed() const { return mapping_complete_observed_; }

  const absl::optional<FencedFrameProperties>& fenced_frame_properties() {
    return observed_fenced_frame_properties_;
  }

  absl::optional<GURL> mapped_url() const {
    if (!observed_fenced_frame_properties_ ||
        !observed_fenced_frame_properties_->mapped_url_) {
      return absl::nullopt;
    }
    return observed_fenced_frame_properties_->mapped_url_
        ->GetValueIgnoringVisibility();
  }

  absl::optional<std::vector<std::pair<GURL, FencedFrameConfig>>>
  nested_urn_config_pairs() const {
    if (!observed_fenced_frame_properties_ ||
        !observed_fenced_frame_properties_->nested_urn_config_pairs_) {
      return absl::nullopt;
    }
    return observed_fenced_frame_properties_->nested_urn_config_pairs_
        ->GetValueIgnoringVisibility();
  }

  absl::optional<AdAuctionData> ad_auction_data() const {
    if (!observed_fenced_frame_properties_ ||
        !observed_fenced_frame_properties_->ad_auction_data_) {
      return absl::nullopt;
    }
    return observed_fenced_frame_properties_->ad_auction_data_
        ->GetValueIgnoringVisibility();
  }

  const base::RepeatingClosure& on_navigate_callback() const {
    return observed_fenced_frame_properties_->on_navigate_callback_;
  }

  FencedFrameReporter* fenced_frame_reporter() {
    if (!observed_fenced_frame_properties_) {
      return nullptr;
    }
    return observed_fenced_frame_properties_->fenced_frame_reporter_.get();
  }

 private:
  bool mapping_complete_observed_ = false;
  absl::optional<FencedFrameProperties> observed_fenced_frame_properties_;
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

  // TODO(crbug.com/1422301): This method allows setting of an arbitrary id of
  // the fenced frame mapping. It is used to test that the auction fails if
  // there is a mismatch between the fenced frame mapping used at the beginning
  // of the auction and at the end of the auction. Once the root cause is known
  // and the issue fixed, remove `SetId()` and `GetNextId()`.
  void SetId(FencedFrameURLMapping::Id id);

  FencedFrameURLMapping::Id GetNextId() const;

 private:
  raw_ptr<FencedFrameURLMapping> fenced_frame_url_mapping_;
};

// TODO(xiaochenzh): Once fenced frame size freezing has no time gap, remove
// this.
// This function keeps polling the evaluation result of the given script until
// it returns true or times out.
// Currently this is only used to check the fenced frame size freezing behavior.
// The size freezing only takes effect after layout has happened.
bool PollUntilEvalToTrue(const std::string& script, RenderFrameHost* rfh);

}  // namespace content

#endif  // CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
