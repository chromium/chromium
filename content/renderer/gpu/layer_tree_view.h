// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_LAYER_TREE_VIEW_H_
#define CONTENT_RENDERER_GPU_LAYER_TREE_VIEW_H_

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
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "ui/gfx/geometry/rect.h"

class GURL;

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
}  // namespace blink

namespace cc {
class AnimationHost;
class InputHandler;
class Layer;
class LayerTreeFrameSink;
class LayerTreeHost;
class LayerTreeSettings;
class RenderFrameMetadataObserver;
class TaskGraphRunner;
class UkmRecorderFactory;
class ScopedDeferCommits;
}  // namespace cc

namespace gfx {
class ColorSpace;
class Size;
}

namespace ui {
class LatencyInfo;
}

namespace content {
class LayerTreeViewDelegate;

class CONTENT_EXPORT LayerTreeView
    : public blink::WebLayerTreeView,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient {
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
  //  only).
  void Initialize(const cc::LayerTreeSettings& settings,
                  std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory);

  void SetNeverVisible();
  const base::WeakPtr<cc::InputHandler>& GetInputHandler();
  void SetNeedsDisplayOnAllLayers();
  void SetRasterizeOnlyVisibleContent();
  void SetNeedsRedrawRect(gfx::Rect damage_rect);

  bool IsSurfaceSynchronizationEnabled() const;

  // Like setNeedsRedraw but forces the frame to be drawn, without early-outs.
  // Redraw will be forced after the next commit
  void SetNeedsForcedRedraw();
  // Calling CreateLatencyInfoSwapPromiseMonitor() to get a scoped
  // LatencyInfoSwapPromiseMonitor. During the life time of the
  // LatencyInfoSwapPromiseMonitor, if SetNeedsCommit() or
  // SetNeedsUpdateLayers() is called on LayerTreeHost, the original latency
  // info will be turned into a LatencyInfoSwapPromise.
  std::unique_ptr<cc::SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency);
  // Calling QueueSwapPromise() to directly queue a SwapPromise into
  // LayerTreeHost.
  void QueueSwapPromise(std::unique_ptr<cc::SwapPromise> swap_promise);
  int GetSourceFrameNumber() const;
  void NotifyInputThrottledUntilCommit();
  const cc::Layer* GetRootLayer() const;
  int ScheduleMicroBenchmark(
      const std::string& name,
      std::unique_ptr<base::Value> value,
      base::OnceCallback<void(std::unique_ptr<base::Value>)> callback);
  bool SendMessageToMicroBenchmark(int id, std::unique_ptr<base::Value> value);
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  void SetRasterColorSpace(const gfx::ColorSpace& color_space);
  void ClearCachesOnNextCommit();
  void SetContentSourceId(uint32_t source_id);
  void SetViewportSizeAndScale(const gfx::Size& device_viewport_size,
                               float device_scale_factor,
                               const viz::LocalSurfaceId& local_surface_id,
                               base::TimeTicks allocation_time);
  void RequestNewLocalSurfaceId();
  void SetViewportVisibleRect(const gfx::Rect& visible_rect);
  void SetURLForUkm(const GURL& url);

  // blink::WebLayerTreeView implementation.
  viz::FrameSinkId GetFrameSinkId() override;
  void SetRootLayer(scoped_refptr<cc::Layer> layer) override;
  void ClearRootLayer() override;
  cc::AnimationHost* CompositorAnimationHost() override;
  gfx::Size GetViewportSize() const override;
  void SetBackgroundColor(SkColor color) override;
  void SetVisible(bool visible) override;
  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float minimum,
                                   float maximum) override;
  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool use_anchor,
                               float new_page_scale,
                               double duration_sec) override;
  bool HasPendingPageScaleAnimation() const override;
  void HeuristicsForGpuRasterizationUpdated(bool matches_heuristics) override;
  void SetNeedsBeginFrame() override;
  void LayoutAndPaintAsync(base::OnceClosure callback) override;
  void CompositeAndReadbackAsync(
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  // Synchronously performs the complete set of document lifecycle phases,
  // including updates to the compositor state, optionally including
  // rasterization.
  void UpdateAllLifecyclePhasesAndCompositeForTesting(bool do_raster) override;
  std::unique_ptr<cc::ScopedDeferCommits> DeferCommits() override;
  void RegisterViewportLayers(const ViewportLayers& viewport_layers) override;
  void ClearViewportLayers() override;
  void RegisterSelection(const cc::LayerSelection& selection) override;
  void ClearSelection() override;
  void SetMutatorClient(std::unique_ptr<cc::LayerTreeMutator>) override;
  void ForceRecalculateRasterScales() override;
  void SetEventListenerProperties(
      cc::EventListenerClass eventClass,
      cc::EventListenerProperties properties) override;
  cc::EventListenerProperties EventListenerProperties(
      cc::EventListenerClass eventClass) const override;
  void SetHaveScrollEventHandlers(bool) override;
  bool HaveScrollEventHandlers() const override;
  int LayerTreeId() const override;
  void SetShowFPSCounter(bool show) override;
  void SetShowPaintRects(bool show) override;
  void SetShowDebugBorders(bool show) override;
  void SetShowScrollBottleneckRects(bool show) override;
  void NotifySwapTime(ReportTimeCallback callback) override;

  void UpdateBrowserControlsState(cc::BrowserControlsState constraints,
                                  cc::BrowserControlsState current,
                                  bool animate) override;
  void SetBrowserControlsHeight(float top_height,
                                float bottom_height,
                                bool shrink) override;
  void SetBrowserControlsShownRatio(float) override;
  void RequestDecode(const cc::PaintImage& image,
                     base::OnceCallback<void(bool)> callback) override;

  void SetOverscrollBehavior(const cc::OverscrollBehavior&) override;

  // cc::LayerTreeHostClient implementation.
  void WillBeginMainFrame() override;
  void DidBeginMainFrame() override;
  void BeginMainFrame(const viz::BeginFrameArgs& args) override;
  void BeginMainFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override;
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override;
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
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) override;

  // cc::LayerTreeHostSingleThreadClient implementation.
  void RequestScheduleAnimation() override;
  void DidSubmitCompositorFrame() override;
  void DidLoseLayerTreeFrameSink() override;
  void RequestBeginMainFrameNotExpected(bool new_state) override;

  const cc::LayerTreeSettings& GetLayerTreeSettings() const;

  // Sets the RenderFrameMetadataObserver, which is sent to the compositor
  // thread for binding.
  void SetRenderFrameObserver(
      std::unique_ptr<cc::RenderFrameMetadataObserver> observer);

  void AddPresentationCallback(
      uint32_t frame_token,
      base::OnceCallback<void(base::TimeTicks)> callback);

  cc::LayerTreeHost* layer_tree_host() { return layer_tree_host_.get(); }

 protected:
  friend class RenderViewImplScaleFactorTest;

 private:
  void SetLayerTreeFrameSink(
      std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink);
  void InvokeLayoutAndPaintCallback();
  bool CompositeIsSynchronous() const;
  void SynchronouslyComposite(bool raster,
                              std::unique_ptr<cc::SwapPromise> swap_promise);

  LayerTreeViewDelegate* const delegate_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> compositor_thread_;
  cc::TaskGraphRunner* const task_graph_runner_;
  blink::scheduler::WebThreadScheduler* const web_main_thread_scheduler_;
  const std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> layer_tree_host_;
  bool never_visible_ = false;

  bool layer_tree_frame_sink_request_failed_while_invisible_ = false;

  bool in_synchronous_compositor_update_ = false;
  base::OnceClosure layout_and_paint_async_callback_;

  viz::FrameSinkId frame_sink_id_;
  base::circular_deque<
      std::pair<uint32_t,
                std::vector<base::OnceCallback<void(base::TimeTicks)>>>>
      presentation_callbacks_;

  base::WeakPtrFactory<LayerTreeView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeView);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_LAYER_TREE_VIEW_H_
