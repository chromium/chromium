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

FencedFrame* GetMatchingFencedFrameInOuterFrameTree(RenderFrameHostImpl* rfh) {
  EXPECT_EQ(blink::features::kFencedFramesImplementationTypeParam.Get(),
            blink::features::FencedFramesImplementationType::kMPArch);
  // `rfh` doesn't always have to be a root frame, since this needs to work
  // for arbitrary frames within a fenced frame.
  EXPECT_TRUE(rfh->frame_tree_node()->IsInFencedFrameTree());

  RenderFrameHostImpl* outer_delegate_frame =
      rfh->GetMainFrame()->GetParentOrOuterDocument();

  std::vector<FencedFrame*> fenced_frames =
      outer_delegate_frame->GetFencedFrames();
  EXPECT_FALSE(fenced_frames.empty());

  for (FencedFrame* fenced_frame : fenced_frames) {
    if (fenced_frame->GetInnerRoot() == rfh->GetMainFrame()) {
      return fenced_frame;
    }
  }

  NOTREACHED();
  return nullptr;
}

TestFencedFrameURLMappingResultObserver::
    TestFencedFrameURLMappingResultObserver() = default;

TestFencedFrameURLMappingResultObserver::
    ~TestFencedFrameURLMappingResultObserver() = default;

void TestFencedFrameURLMappingResultObserver::OnFencedFrameURLMappingComplete(
    absl::optional<GURL> mapped_url,
    absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
        pending_ad_components_map) {
  mapping_complete_observed_ = true;
  mapped_url_ = std::move(mapped_url);
  pending_ad_components_map_ = std::move(pending_ad_components_map);
}

FencedFrameNavigationObserver::FencedFrameNavigationObserver(
    RenderFrameHostImpl* fenced_frame_rfh)
    : frame_tree_node_(fenced_frame_rfh->frame_tree_node()) {
  EXPECT_TRUE(frame_tree_node_->IsInFencedFrameTree());

  if (blink::features::kFencedFramesImplementationTypeParam.Get() ==
      blink::features::FencedFramesImplementationType::kShadowDOM) {
    observer_for_shadow_dom_ =
        std::make_unique<TestFrameNavigationObserver>(fenced_frame_rfh);
    return;
  }

  fenced_frame_for_mparch_ =
      GetMatchingFencedFrameInOuterFrameTree(fenced_frame_rfh);
}

FencedFrameNavigationObserver::~FencedFrameNavigationObserver() = default;

void FencedFrameNavigationObserver::Wait(net::Error expected_net_error_code) {
  if (blink::features::kFencedFramesImplementationTypeParam.Get() ==
      blink::features::FencedFramesImplementationType::kShadowDOM) {
    DCHECK(observer_for_shadow_dom_);
    observer_for_shadow_dom_->Wait();
    EXPECT_EQ(observer_for_shadow_dom_->last_net_error_code(),
              expected_net_error_code);
    return;
  }

  DCHECK(fenced_frame_for_mparch_);

  // TODO(crbug.com/1257133): Once this bug is fixed, we can use
  // `TestFrameNavigationObserver` to tell us when the navigation has finished,
  // which actually exposes net::Error codes encountered during navigation.
  // Therefore once that bug is fixed, we can perform finer-grained error
  // code comparisons than the crude `RenderFrameHost::IsErrorDocument()` one
  // below.
  fenced_frame_for_mparch_->WaitForDidStopLoadingForTesting();

  EXPECT_EQ(frame_tree_node_->current_frame_host()->IsErrorDocument(),
            expected_net_error_code != net::OK);
}

}  // namespace content
