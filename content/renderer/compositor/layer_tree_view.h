// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_H_
#define CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/swap_promise_monitor.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
}  // namespace blink

namespace cc {
class AnimationHost;
class LayerTreeFrameSink;
class LayerTreeHost;
class LayerTreeSettings;
class TaskGraphRunner;
class UkmRecorderFactory;
}  // namespace cc

namespace content {
class LayerTreeViewDelegate;

class CONTENT_EXPORT LayerTreeView : public cc::LayerTreeHostClient,
                                     public cc::LayerTreeHostSingleThreadClient,
                                     public cc::LayerTreeHostSchedulingClient {
 public:
  // The |main_thread| is the task runner that the compositor will use for the
  // main thread (where it is constructed). The |compositor_thread| is the task
  // runner for the compositor thread, but is null if the compositor will run in
  // single-threaded mode (in tests only).
  LayerTreeView(LayerTreeViewDelegate* delegate,
                scoped_refptr<base::SingleThreadTaskRunner> main_thread,
                scoped_refptr<base::SingleThreadTaskRunner> compositor_thread,
                cc::TaskGraphRunner* task_graph_runner,
                blink::scheduler::WebThreadScheduler* scheduler);
  ~LayerTreeView() override;

  // The |ukm_recorder_factory| may be null to disable recording (in tests
  // only).
  void Initialize(const cc::LayerTreeSettings& settings,
                  std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory);

  // Drops any references back to the delegate in preparation for being
  // destroyed.
  void Disconnect();

  cc::AnimationHost* animation_host() { return animation_host_.get(); }

  void SetVisible(bool visible);

  // cc::LayerTreeHostClient implementation.
  void WillBeginMainFrame() override;
  void DidBeginMainFrame() override;
  void WillUpdateLayers() override;
  void DidUpdateLayers() override;
  void BeginMainFrame(const viz::BeginFrameArgs& args) override;
  void OnDeferMainFrameUpdatesChanged(bool) override;
  void OnDeferCommitsChanged(bool) override;
  void BeginMainFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override;
  void RecordManipulationTypeCounts(cc::ManipulationInfo info) override;
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override;
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override;
  void RequestNewLayerTreeFrameSink() override;
  void DidInitializeLayerTreeFrameSink() override;
  void DidFailToInitializeLayerTreeFrameSink() override;
  void WillCommit() override;
  void DidCommit() override;
  void DidCommitAndDrawFrame() override;
  void DidReceiveCompositorFrameAck() override {}
  void DidCompletePageScaleAnimation() override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) override;
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;

  // cc::LayerTreeHostSingleThreadClient implementation.
  void DidSubmitCompositorFrame() override;
  void DidLoseLayerTreeFrameSink() override;

  // cc::LayerTreeHostSchedulingClient implementation.
  void DidScheduleBeginMainFrame() override;
  void DidRunBeginMainFrame() override;

  void AddPresentationCallback(
      uint32_t frame_token,
      base::OnceCallback<void(base::TimeTicks)> callback);

  cc::LayerTreeHost* layer_tree_host() { return layer_tree_host_.get(); }
  const cc::LayerTreeHost* layer_tree_host() const {
    return layer_tree_host_.get();
  }

 protected:
  friend class RenderViewImplScaleFactorTest;

 private:
  void SetLayerTreeFrameSink(
      std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink);

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> compositor_thread_;
  cc::TaskGraphRunner* const task_graph_runner_;
  blink::scheduler::WebThreadScheduler* const web_main_thread_scheduler_;
  const std::unique_ptr<cc::AnimationHost> animation_host_;

  // The delegate_ becomes null when Disconnect() is called. After that, the
  // class should do nothing in calls from the LayerTreeHost, and just wait to
  // be destroyed. It is not expected to be used at all after Disconnect()
  // outside of handling/dropping LayerTreeHost client calls.
  LayerTreeViewDelegate* delegate_;
  std::unique_ptr<cc::LayerTreeHost> layer_tree_host_;

  // This class should do nothing and access no pointers once this value becomes
  // true.
  bool layer_tree_frame_sink_request_failed_while_invisible_ = false;

  base::circular_deque<
      std::pair<uint32_t,
                std::vector<base::OnceCallback<void(base::TimeTicks)>>>>
      presentation_callbacks_;

  base::WeakPtrFactory<LayerTreeView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LayerTreeView);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_H_
