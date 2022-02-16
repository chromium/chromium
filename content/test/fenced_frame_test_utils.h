// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
#define CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "net/base/net_errors.h"

namespace content {

class FencedFrame;
class RenderFrameHostImpl;
class FrameTreeNode;
class TestFrameNavigationObserver;

// `node` is expected to be the child FrameTreeNode created in response to a
// <fencedframe> element being created. This method:
//    - Returns `node` if we're in the ShadowDOM version
//    - Returns the FrameTreeNode of the fenced frame's inner FrameTree, if
//    we're in the MPArch version of fenced frames
FrameTreeNode* GetFencedFrameRootNode(FrameTreeNode* node);

// This method takes in a RenderFrameHostImpl that must be inside a fenced frame
// FrameTree, and returns the FencedFrame* object that represents this inner
// FrameTree from the outer FrameTree.
FencedFrame* GetMatchingFencedFrameInOuterFrameTree(RenderFrameHostImpl* rfh);

// Tests can use this class to observe and check the URL mapping result.
class TestFencedFrameURLMappingResultObserver
    : public FencedFrameURLMapping::MappingResultObserver {
 public:
  TestFencedFrameURLMappingResultObserver();
  ~TestFencedFrameURLMappingResultObserver() override;

  void OnFencedFrameURLMappingComplete(
      absl::optional<GURL> mapped_url,
      absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
          pending_ad_components_map) override;

  bool mapping_complete_observed() const { return mapping_complete_observed_; }

  const absl::optional<GURL>& mapped_url() const { return mapped_url_; }

  const absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>&
  pending_ad_components_map() const {
    return pending_ad_components_map_;
  }

 private:
  bool mapping_complete_observed_ = false;
  absl::optional<GURL> mapped_url_;
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
      pending_ad_components_map_;
};

class FencedFrameNavigationObserver {
 public:
  explicit FencedFrameNavigationObserver(RenderFrameHostImpl* fenced_frame_rfh);

  ~FencedFrameNavigationObserver();

  void Wait(net::Error expected_net_error_code);

 private:
  FrameTreeNode* frame_tree_node_ = nullptr;

  // For the ShadowDOM version of fenced frames, we can just use a
  // `TestFrameNavigationObserver` as normal directly on the frame that is
  // navigating.
  std::unique_ptr<TestFrameNavigationObserver> observer_for_shadow_dom_;

  // For the MPArch version of fenced frames, rely on
  // FencedFrame::WaitForDidStopLoadingForTesting. `TestFrameNavigationObserver`
  // does not fully work inside of a fenced frame FrameTree: `WaitForCommit()`
  // works, but `Wait()` always times out because it expects to hear the
  // DidFinishedLoad event from the outer WebContents, which is not communicated
  // by nested FrameTrees.
  FencedFrame* fenced_frame_for_mparch_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_TEST_FENCED_FRAME_TEST_UTILS_H_
