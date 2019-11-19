// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_STUB_LAYER_TREE_VIEW_DELEGATE_H_
#define CONTENT_TEST_STUB_LAYER_TREE_VIEW_DELEGATE_H_

#include "cc/paint/element_id.h"
#include "content/renderer/compositor/layer_tree_view_delegate.h"

namespace cc {
struct ApplyViewportChangesArgs;
}

namespace content {

class StubLayerTreeViewDelegate : public LayerTreeViewDelegate {
 public:
  // LayerTreeViewDelegate implementation.
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs&) override {}
  void RecordManipulationTypeCounts(cc::ManipulationInfo info) override {}
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override {}
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override {}
  void BeginMainFrame(base::TimeTicks frame_time) override {}
  void OnDeferMainFrameUpdatesChanged(bool) override {}
  void OnDeferCommitsChanged(bool) override {}
  void DidBeginMainFrame() override {}
  void RecordStartOfFrameMetrics() override {}
  void RecordEndOfFrameMetrics(base::TimeTicks) override {}
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;
  void BeginUpdateLayers() override {}
  void EndUpdateLayers() override {}
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void DidCommitAndDrawCompositorFrame() override {}
  void WillCommitCompositorFrame() override {}
  void DidCommitCompositorFrame() override {}
  void DidCompletePageScaleAnimation() override {}
  void UpdateVisualState() override {}
  void WillBeginCompositorFrame() override {}
};

}  // namespace content

#endif  // CONTENT_TEST_STUB_LAYER_TREE_VIEW_DELEGATE_H_
