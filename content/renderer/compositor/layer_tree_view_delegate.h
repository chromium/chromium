// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_DELEGATE_H_
#define CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "cc/trees/layer_tree_host_client.h"

namespace cc {
class LayerTreeFrameSink;
struct BeginMainFrameMetrics;
struct ElementId;
}  // namespace cc

namespace content {

// Consumers of LayerTreeView implement this delegate in order to
// transport compositing information across processes.
class LayerTreeViewDelegate {
 public:
  using LayerTreeFrameSinkCallback =
      base::OnceCallback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>;

  // Report viewport related properties during a commit from the compositor
  // thread.
  virtual void ApplyViewportChanges(
      const cc::ApplyViewportChangesArgs& args) = 0;

  // Record use counts of different methods of scrolling (e.g. wheel, touch,
  // precision touchpad, etc.).
  virtual void RecordManipulationTypeCounts(cc::ManipulationInfo info) = 0;

  // Send overscroll DOM event when overscrolling has happened on the compositor
  // thread.
  virtual void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) = 0;

  // Send scrollend DOM event when gesture scrolling on the compositor thread
  // has finished.
  virtual void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) = 0;

  // Notifies that the compositor has issued a BeginMainFrame.
  virtual void BeginMainFrame(base::TimeTicks frame_time) = 0;

  virtual void OnDeferMainFrameUpdatesChanged(bool) = 0;
  virtual void OnDeferCommitsChanged(bool) = 0;

  // Notifies that the layer tree host has completed a call to
  // RequestMainFrameUpdate in response to a BeginMainFrame.
  virtual void DidBeginMainFrame() = 0;

  // Requests a LayerTreeFrameSink to submit CompositorFrames to.
  virtual void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) = 0;

  // Notifies that the draw commands for a committed frame have been issued.
  virtual void DidCommitAndDrawCompositorFrame() = 0;

  // Notifies that a compositor frame commit operation is about to start.
  virtual void WillCommitCompositorFrame() = 0;

  // Notifies about a compositor frame commit operation having finished.
  virtual void DidCommitCompositorFrame() = 0;

  // Called by the compositor when page scale animation completed.
  virtual void DidCompletePageScaleAnimation() = 0;

  // Requests that a UMA and UKM metrics be recorded for the total frame time
  // and the portion of frame time spent in various sub-systems.
  // Call RecordStartOfFrameMetrics when a main frame is starting, and call
  // RecordEndOfFrameMetrics as soon as the total frame time becomes known for
  // a given frame. For example, ProxyMain::BeginMainFrame calls
  // RecordStartOfFrameMetrics just be WillBeginCompositorFrame() and
  // RecordEndOfFrameMetrics immediately before aborting or committing a frame
  // (at the same time Tracing measurements are taken).
  virtual void RecordStartOfFrameMetrics() = 0;
  virtual void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) = 0;
  // Return metrics information for the stages of BeginMainFrame. This is
  // ultimately implemented by Blink's LocalFrameUKMAggregator. It must be a
  // distinct call from the FrameMetrics above because the BeginMainFrameMetrics
  // for compositor latency must be gathered before the layer tree is
  // committed to the compositor, which is before the call to
  // RecordEndOfFrameMetrics.
  virtual std::unique_ptr<cc::BeginMainFrameMetrics>
  GetBeginMainFrameMetrics() = 0;

  // Notification of the beginning and end of LayerTreeHost::UpdateLayers, for
  // metrics collection.
  virtual void BeginUpdateLayers() = 0;
  virtual void EndUpdateLayers() = 0;

  // Requests a visual frame-based update to the state of the delegate if there
  // is an update available.
  virtual void UpdateVisualState() = 0;

  // Indicates that the compositor is about to begin a frame. This is primarily
  // to signal to flow control mechanisms that a frame is beginning, not to
  // perform actual painting work. When |record_main_frame_metrics| is true
  // we are in a frame that shoujld capture metrics data, and the local frame's
  // UKM aggregator must be informed that the frame is starting.
  virtual void WillBeginCompositorFrame() = 0;

 protected:
  virtual ~LayerTreeViewDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_DELEGATE_H_
