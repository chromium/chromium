// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
#define CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "net/base/net_errors.h"

namespace content {

class FrameTreeNode;
class RenderFrameHost;

// `node` is expected to be the child FrameTreeNode created in response to a
// <fencedframe> element being created. This method returns the FrameTreeNode of
// the fenced frame's inner FrameTree.
FrameTreeNode* GetFencedFrameRootNode(FrameTreeNode* node);

// Tests can use this class to observe and check the URL mapping result.
class TestFencedFrameURLMappingResultObserver
    : public FencedFrameURLMapping::MappingResultObserver {
 public:
  TestFencedFrameURLMappingResultObserver();
  ~TestFencedFrameURLMappingResultObserver() override;

  void OnFencedFrameURLMappingComplete(
      const std::optional<FencedFrameProperties>& properties) override;

  bool mapping_complete_observed() const { return mapping_complete_observed_; }

  const std::optional<FencedFrameProperties>& fenced_frame_properties() {
    return observed_fenced_frame_properties_;
  }

  std::optional<GURL> mapped_url() const {
    if (!observed_fenced_frame_properties_ ||
        !observed_fenced_frame_properties_->mapped_url()) {
      return std::nullopt;
    }
    return observed_fenced_frame_properties_->mapped_url()
        ->GetValueIgnoringVisibility();
  }

  std::optional<std::vector<std::pair<GURL, FencedFrameConfig>>>
  nested_urn_config_pairs() const {
    if (!observed_fenced_frame_properties_ ||
        !observed_fenced_frame_properties_->nested_urn_config_pairs()) {
      return std::nullopt;
    }
    return observed_fenced_frame_properties_->nested_urn_config_pairs()
        ->GetValueIgnoringVisibility();
  }

  std::optional<AdAuctionData> ad_auction_data() const {
    if (!observed_fenced_frame_properties_ ||
        !observed_fenced_frame_properties_->ad_auction_data()) {
      return std::nullopt;
    }
    return observed_fenced_frame_properties_->ad_auction_data()
        ->GetValueIgnoringVisibility();
  }

  const base::RepeatingClosure& on_navigate_callback() const {
    return observed_fenced_frame_properties_->on_navigate_callback();
  }

  FencedFrameReporter* fenced_frame_reporter() {
    if (!observed_fenced_frame_properties_) {
      return nullptr;
    }
    return observed_fenced_frame_properties_->fenced_frame_reporter().get();
  }

 private:
  bool mapping_complete_observed_ = false;
  std::optional<FencedFrameProperties> observed_fenced_frame_properties_;
};

class FencedFrameURLMappingTestPeer {
 public:
  FencedFrameURLMappingTestPeer() = delete;
  explicit FencedFrameURLMappingTestPeer(
      FencedFrameURLMapping* fenced_frame_url_mapping)
      : fenced_frame_url_mapping_(fenced_frame_url_mapping) {}

  // Insert urn mappings until it reaches the limit.
  void FillMap(const GURL& url);

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
