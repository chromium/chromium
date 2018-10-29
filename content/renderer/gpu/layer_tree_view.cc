// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/layer_tree_view.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/region.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "content/renderer/gpu/layer_tree_view_delegate.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/blink.h"
#include "ui/gfx/presentation_feedback.h"

namespace base {
class Value;
}

namespace cc {
class Layer;
}

namespace content {
namespace {

using ReportTimeCallback = blink::WebLayerTreeView::ReportTimeCallback;

// Enables using presentation times instead of swap times in swap promises.
// Currently, these promises are only used by Paint Timing, but they will be
// used by other APIs such as Event Timing.
const base::Feature kUsePresentationTimeInSwapPromise = {
    "UsePresentationTimeInSwapPromise", base::FEATURE_DISABLED_BY_DEFAULT};

class ReportTimeSwapPromise : public cc::SwapPromise {
 public:
  ReportTimeSwapPromise(ReportTimeCallback callback,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        base::WeakPtr<LayerTreeView> layer_tree_view);
  ~ReportTimeSwapPromise() override;

  void DidActivate() override {}
  void WillSwap(viz::CompositorFrameMetadata* metadata) override;
  void DidSwap() override;
  void DidNotSwap(DidNotSwapReason reason) override;
  int64_t TraceId() const override;

 private:
  ReportTimeCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<LayerTreeView> layer_tree_view_;

  DISALLOW_COPY_AND_ASSIGN(ReportTimeSwapPromise);
};

ReportTimeSwapPromise::ReportTimeSwapPromise(
    ReportTimeCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<LayerTreeView> layer_tree_view)
    : callback_(std::move(callback)),
      task_runner_(std::move(task_runner)),
      layer_tree_view_(std::move(layer_tree_view)) {}

ReportTimeSwapPromise::~ReportTimeSwapPromise() {}

void ReportTimeSwapPromise::WillSwap(viz::CompositorFrameMetadata* metadata) {
  DCHECK_GT(metadata->frame_token, 0u);
  metadata->request_presentation_feedback = true;
  if (!base::FeatureList::IsEnabled(kUsePresentationTimeInSwapPromise))
    return;

  // If using presentation timestamp, post task here calling
  // LayerTreeView::AddPresentationCallback.
  auto* task_runner = task_runner_.get();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LayerTreeView::AddPresentationCallback, layer_tree_view_,
          metadata->frame_token,
          base::BindOnce(std::move(callback_),
                         blink::WebLayerTreeView::SwapResult::kDidSwap)));
}

void ReportTimeSwapPromise::DidSwap() {
  if (base::FeatureList::IsEnabled(kUsePresentationTimeInSwapPromise))
    return;

  // If using swap timestamp, the swap promise should return the current time.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_),
                                blink::WebLayerTreeView::SwapResult::kDidSwap,
                                base::TimeTicks::Now()));
}

void ReportTimeSwapPromise::DidNotSwap(
    cc::SwapPromise::DidNotSwapReason reason) {
  blink::WebLayerTreeView::SwapResult result;
  switch (reason) {
    case cc::SwapPromise::DidNotSwapReason::SWAP_FAILS:
      result = blink::WebLayerTreeView::SwapResult::kDidNotSwapSwapFails;
      break;
    case cc::SwapPromise::DidNotSwapReason::COMMIT_FAILS:
      result = blink::WebLayerTreeView::SwapResult::kDidNotSwapCommitFails;
      break;
    case cc::SwapPromise::DidNotSwapReason::COMMIT_NO_UPDATE:
      result = blink::WebLayerTreeView::SwapResult::kDidNotSwapCommitNoUpdate;
      break;
    case cc::SwapPromise::DidNotSwapReason::ACTIVATION_FAILS:
      result = blink::WebLayerTreeView::SwapResult::kDidNotSwapActivationFails;
      break;
  }
  // During a failed swap, return the current time regardless of whether we're
  // using presentation or swap timestamps.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback_), result,
                                                   base::TimeTicks::Now()));
}

int64_t ReportTimeSwapPromise::TraceId() const {
  return 0;
}

}  // namespace

LayerTreeView::LayerTreeView(
    LayerTreeViewDelegate* delegate,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_thread,
    cc::TaskGraphRunner* task_graph_runner,
    blink::scheduler::WebThreadScheduler* scheduler)
    : delegate_(delegate),
      main_thread_(std::move(main_thread)),
      compositor_thread_(std::move(compositor_thread)),
      task_graph_runner_(task_graph_runner),
      web_main_thread_scheduler_(scheduler),
      animation_host_(cc::AnimationHost::CreateMainInstance()),
      weak_factory_(this) {}

LayerTreeView::~LayerTreeView() = default;

void LayerTreeView::Initialize(
    const cc::LayerTreeSettings& settings,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) {
  const bool is_threaded = !!compositor_thread_;

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner_;
  params.main_task_runner = main_thread_;
  params.mutator_host = animation_host_.get();
  params.ukm_recorder_factory = std::move(ukm_recorder_factory);
  if (base::TaskScheduler::GetInstance()) {
    // The image worker thread needs to allow waiting since it makes discardable
    // shared memory allocations which need to make synchronous calls to the
    // IO thread.
    params.image_worker_task_runner = base::CreateSequencedTaskRunnerWithTraits(
        {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  }
  if (!is_threaded) {
    // Single-threaded layout tests, and unit tests.
    layer_tree_host_ =
        cc::LayerTreeHost::CreateSingleThreaded(this, std::move(params));
  } else {
    layer_tree_host_ = cc::LayerTreeHost::CreateThreaded(compositor_thread_,
                                                         std::move(params));
  }
}

void LayerTreeView::SetNeverVisible() {
  DCHECK(!layer_tree_host_->IsVisible());
  never_visible_ = true;
}

const base::WeakPtr<cc::InputHandler>& LayerTreeView::GetInputHandler() {
  return layer_tree_host_->GetInputHandler();
}

void LayerTreeView::SetNeedsDisplayOnAllLayers() {
  layer_tree_host_->SetNeedsDisplayOnAllLayers();
}

void LayerTreeView::SetRasterizeOnlyVisibleContent() {
  cc::LayerTreeDebugState current = layer_tree_host_->GetDebugState();
  current.rasterize_only_visible_content = true;
  layer_tree_host_->SetDebugState(current);
}

void LayerTreeView::SetNeedsRedrawRect(gfx::Rect damage_rect) {
  layer_tree_host_->SetNeedsRedrawRect(damage_rect);
}

bool LayerTreeView::IsSurfaceSynchronizationEnabled() const {
  return layer_tree_host_->GetSettings().enable_surface_synchronization;
}

void LayerTreeView::SetNeedsForcedRedraw() {
  layer_tree_host_->SetNeedsCommitWithForcedRedraw();
}

std::unique_ptr<cc::SwapPromiseMonitor>
LayerTreeView::CreateLatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency) {
  return std::make_unique<cc::LatencyInfoSwapPromiseMonitor>(
      latency, layer_tree_host_->GetSwapPromiseManager(), nullptr);
}

void LayerTreeView::QueueSwapPromise(
    std::unique_ptr<cc::SwapPromise> swap_promise) {
  layer_tree_host_->QueueSwapPromise(std::move(swap_promise));
}

int LayerTreeView::GetSourceFrameNumber() const {
  return layer_tree_host_->SourceFrameNumber();
}

void LayerTreeView::NotifyInputThrottledUntilCommit() {
  layer_tree_host_->NotifyInputThrottledUntilCommit();
}

const cc::Layer* LayerTreeView::GetRootLayer() const {
  return layer_tree_host_->root_layer();
}

int LayerTreeView::ScheduleMicroBenchmark(
    const std::string& name,
    std::unique_ptr<base::Value> value,
    base::OnceCallback<void(std::unique_ptr<base::Value>)> callback) {
  return layer_tree_host_->ScheduleMicroBenchmark(name, std::move(value),
                                                  std::move(callback));
}

bool LayerTreeView::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return layer_tree_host_->SendMessageToMicroBenchmark(id, std::move(value));
}

void LayerTreeView::SetViewportSizeAndScale(
    const gfx::Size& device_viewport_size,
    float device_scale_factor,
    const viz::LocalSurfaceId& local_surface_id,
    base::TimeTicks allocation_time) {
  layer_tree_host_->SetViewportSizeAndScale(device_viewport_size,
                                            device_scale_factor,
                                            local_surface_id, allocation_time);
}

void LayerTreeView::RequestNewLocalSurfaceId() {
  layer_tree_host_->RequestNewLocalSurfaceId();
}

void LayerTreeView::SetViewportVisibleRect(const gfx::Rect& visible_rect) {
  layer_tree_host_->SetViewportVisibleRect(visible_rect);
}

viz::FrameSinkId LayerTreeView::GetFrameSinkId() {
  return frame_sink_id_;
}

void LayerTreeView::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  layer_tree_host_->SetRootLayer(std::move(layer));
}

void LayerTreeView::ClearRootLayer() {
  layer_tree_host_->SetRootLayer(nullptr);
}

cc::AnimationHost* LayerTreeView::CompositorAnimationHost() {
  return animation_host_.get();
}

gfx::Size LayerTreeView::GetViewportSize() const {
  return layer_tree_host_->device_viewport_size();
}

void LayerTreeView::SetBackgroundColor(SkColor color) {
  layer_tree_host_->set_background_color(color);
}

void LayerTreeView::SetVisible(bool visible) {
  if (never_visible_)
    return;

  layer_tree_host_->SetVisible(visible);

  if (visible && layer_tree_frame_sink_request_failed_while_invisible_)
    DidFailToInitializeLayerTreeFrameSink();
}

void LayerTreeView::SetPageScaleFactorAndLimits(float page_scale_factor,
                                                float minimum,
                                                float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(page_scale_factor, minimum,
                                                maximum);
}

void LayerTreeView::StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                            bool use_anchor,
                                            float new_page_scale,
                                            double duration_sec) {
  base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
      duration_sec * base::Time::kMicrosecondsPerSecond);
  layer_tree_host_->StartPageScaleAnimation(target_offset, use_anchor,
                                            new_page_scale, duration);
}

bool LayerTreeView::HasPendingPageScaleAnimation() const {
  return layer_tree_host_->HasPendingPageScaleAnimation();
}

void LayerTreeView::HeuristicsForGpuRasterizationUpdated(
    bool matches_heuristics) {
  layer_tree_host_->SetHasGpuRasterizationTrigger(matches_heuristics);
}

void LayerTreeView::SetNeedsBeginFrame() {
  layer_tree_host_->SetNeedsAnimate();
}

void LayerTreeView::RegisterViewportLayers(const ViewportLayers& layers) {
  cc::LayerTreeHost::ViewportLayers viewport_layers;
  viewport_layers.overscroll_elasticity_element_id =
      layers.overscroll_elasticity_element_id;
  viewport_layers.page_scale = layers.page_scale;
  viewport_layers.inner_viewport_container = layers.inner_viewport_container;
  viewport_layers.outer_viewport_container = layers.outer_viewport_container;
  viewport_layers.inner_viewport_scroll = layers.inner_viewport_scroll;
  viewport_layers.outer_viewport_scroll = layers.outer_viewport_scroll;
  layer_tree_host_->RegisterViewportLayers(viewport_layers);
}

void LayerTreeView::ClearViewportLayers() {
  layer_tree_host_->RegisterViewportLayers(cc::LayerTreeHost::ViewportLayers());
}

void LayerTreeView::RegisterSelection(const cc::LayerSelection& selection) {
  layer_tree_host_->RegisterSelection(selection);
}

void LayerTreeView::ClearSelection() {
  layer_tree_host_->RegisterSelection(cc::LayerSelection());
}

void LayerTreeView::SetMutatorClient(
    std::unique_ptr<cc::LayerTreeMutator> client) {
  TRACE_EVENT0("cc", "LayerTreeView::setMutatorClient");
  layer_tree_host_->SetLayerTreeMutator(std::move(client));
}

void LayerTreeView::ForceRecalculateRasterScales() {
  layer_tree_host_->SetNeedsRecalculateRasterScales();
}

void LayerTreeView::SetEventListenerProperties(
    cc::EventListenerClass event_class,
    cc::EventListenerProperties properties) {
  layer_tree_host_->SetEventListenerProperties(event_class, properties);
}

cc::EventListenerProperties LayerTreeView::EventListenerProperties(
    cc::EventListenerClass event_class) const {
  return layer_tree_host_->event_listener_properties(event_class);
}

void LayerTreeView::SetHaveScrollEventHandlers(bool has_handlers) {
  layer_tree_host_->SetHaveScrollEventHandlers(has_handlers);
}

bool LayerTreeView::HaveScrollEventHandlers() const {
  return layer_tree_host_->have_scroll_event_handlers();
}

bool LayerTreeView::CompositeIsSynchronous() const {
  if (!compositor_thread_) {
    DCHECK(!layer_tree_host_->GetSettings().single_thread_proxy_scheduler);
    return true;
  }
  return false;
}

void LayerTreeView::LayoutAndPaintAsync(base::OnceClosure callback) {
  DCHECK(layout_and_paint_async_callback_.is_null());
  layout_and_paint_async_callback_ = std::move(callback);

  if (CompositeIsSynchronous()) {
    // The LayoutAndPaintAsyncCallback is invoked in WillCommit, which is
    // dispatched after layout and paint for all compositing modes.
    const bool raster = false;
    layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&LayerTreeView::SynchronouslyComposite,
                                  weak_factory_.GetWeakPtr(), raster, nullptr));
  } else {
    layer_tree_host_->SetNeedsCommit();
  }
}

void LayerTreeView::SetLayerTreeFrameSink(
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  if (!layer_tree_frame_sink) {
    DidFailToInitializeLayerTreeFrameSink();
    return;
  }
  layer_tree_host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void LayerTreeView::InvokeLayoutAndPaintCallback() {
  if (!layout_and_paint_async_callback_.is_null())
    std::move(layout_and_paint_async_callback_).Run();
}

void LayerTreeView::CompositeAndReadbackAsync(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  DCHECK(layout_and_paint_async_callback_.is_null());
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
      layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner();
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> callback,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), result->AsSkBitmap()));
              },
              std::move(callback), std::move(main_thread_task_runner)));
  auto swap_promise =
      delegate_->RequestCopyOfOutputForLayoutTest(std::move(request));

  // Force a commit to happen. The temporary copy output request will
  // be installed after layout which will happen as a part of the commit, for
  // widgets that delay the creation of their output surface.
  if (CompositeIsSynchronous()) {
    // Since the composite is required for a pixel dump, we need to raster.
    // Note that we defer queuing the SwapPromise until the requested Composite
    // with rasterization is done.
    const bool raster = true;
    layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&LayerTreeView::SynchronouslyComposite,
                                  weak_factory_.GetWeakPtr(), raster,
                                  std::move(swap_promise)));
  } else {
    // Force a redraw to ensure that the copy swap promise isn't cancelled due
    // to no damage.
    SetNeedsForcedRedraw();
    layer_tree_host_->QueueSwapPromise(std::move(swap_promise));
    layer_tree_host_->SetNeedsCommit();
  }
}

void LayerTreeView::UpdateAllLifecyclePhasesAndCompositeForTesting(
    bool do_raster) {
  SynchronouslyComposite(do_raster, nullptr /* swap_promise */);
}

void LayerTreeView::SynchronouslyComposite(
    bool raster,
    std::unique_ptr<cc::SwapPromise> swap_promise) {
  DCHECK(CompositeIsSynchronous());
  if (!layer_tree_host_->IsVisible())
    return;

  if (in_synchronous_compositor_update_) {
    // LayoutTests can use a nested message loop to pump frames while inside a
    // frame, but the compositor does not support this. In this case, we only
    // run blink's lifecycle updates.
    delegate_->BeginMainFrame(base::TimeTicks::Now());
    delegate_->UpdateVisualState();
    return;
  }

  if (swap_promise) {
    // Force a redraw to ensure that the copy swap promise isn't cancelled due
    // to no damage.
    SetNeedsForcedRedraw();
    layer_tree_host_->QueueSwapPromise(std::move(swap_promise));
  }

  DCHECK(!in_synchronous_compositor_update_);
  base::AutoReset<bool> inside_composite(&in_synchronous_compositor_update_,
                                         true);
  layer_tree_host_->Composite(base::TimeTicks::Now(), raster);
}

std::unique_ptr<cc::ScopedDeferCommits> LayerTreeView::DeferCommits() {
  return layer_tree_host_->DeferCommits();
}

int LayerTreeView::LayerTreeId() const {
  return layer_tree_host_->GetId();
}

void LayerTreeView::SetShowFPSCounter(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->GetDebugState();
  debug_state.show_fps_counter = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void LayerTreeView::SetShowPaintRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->GetDebugState();
  debug_state.show_paint_rects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void LayerTreeView::SetShowDebugBorders(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->GetDebugState();
  if (show)
    debug_state.show_debug_borders.set();
  else
    debug_state.show_debug_borders.reset();
  layer_tree_host_->SetDebugState(debug_state);
}

void LayerTreeView::SetShowScrollBottleneckRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->GetDebugState();
  debug_state.show_touch_event_handler_rects = show;
  debug_state.show_wheel_event_handler_rects = show;
  debug_state.show_non_fast_scrollable_rects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void LayerTreeView::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate) {
  layer_tree_host_->UpdateBrowserControlsState(constraints, current, animate);
}

void LayerTreeView::SetBrowserControlsHeight(float top_height,
                                             float bottom_height,
                                             bool shrink) {
  layer_tree_host_->SetBrowserControlsHeight(top_height, bottom_height, shrink);
}

void LayerTreeView::SetBrowserControlsShownRatio(float ratio) {
  layer_tree_host_->SetBrowserControlsShownRatio(ratio);
}

void LayerTreeView::RequestDecode(const cc::PaintImage& image,
                                  base::OnceCallback<void(bool)> callback) {
  layer_tree_host_->QueueImageDecode(image, std::move(callback));

  // If we're compositing synchronously, the SetNeedsCommit call which will be
  // issued by |layer_tree_host_| is not going to cause a commit, due to the
  // fact that this would make layout tests slow and cause flakiness. However,
  // in this case we actually need a commit to transfer the decode requests to
  // the impl side. So, force a commit to happen.
  if (CompositeIsSynchronous()) {
    const bool raster = true;
    layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&LayerTreeView::SynchronouslyComposite,
                                  weak_factory_.GetWeakPtr(), raster, nullptr));
  }
}

void LayerTreeView::SetOverscrollBehavior(
    const cc::OverscrollBehavior& behavior) {
  layer_tree_host_->SetOverscrollBehavior(behavior);
}

void LayerTreeView::WillBeginMainFrame() {
  delegate_->WillBeginCompositorFrame();
}

void LayerTreeView::DidBeginMainFrame() {}

void LayerTreeView::BeginMainFrame(const viz::BeginFrameArgs& args) {
  web_main_thread_scheduler_->WillBeginFrame(args);
  delegate_->BeginMainFrame(args.frame_time);
}

void LayerTreeView::BeginMainFrameNotExpectedSoon() {
  web_main_thread_scheduler_->BeginFrameNotExpectedSoon();
}

void LayerTreeView::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  web_main_thread_scheduler_->BeginMainFrameNotExpectedUntil(time);
}

void LayerTreeView::UpdateLayerTreeHost() {
  delegate_->UpdateVisualState();
}

void LayerTreeView::ApplyViewportChanges(
    const cc::ApplyViewportChangesArgs& args) {
  delegate_->ApplyViewportChanges(args);
}

void LayerTreeView::RecordWheelAndTouchScrollingCount(
    bool has_scrolled_by_wheel,
    bool has_scrolled_by_touch) {
  delegate_->RecordWheelAndTouchScrollingCount(has_scrolled_by_wheel,
                                               has_scrolled_by_touch);
}

void LayerTreeView::RequestNewLayerTreeFrameSink() {
  // If the host is closing, then no more compositing is possible.  This
  // prevents shutdown races between handling the close message and
  // the CreateLayerTreeFrameSink task.
  if (delegate_->IsClosing())
    return;
  delegate_->RequestNewLayerTreeFrameSink(base::BindOnce(
      &LayerTreeView::SetLayerTreeFrameSink, weak_factory_.GetWeakPtr()));
}

void LayerTreeView::DidInitializeLayerTreeFrameSink() {}

void LayerTreeView::DidFailToInitializeLayerTreeFrameSink() {
  if (!layer_tree_host_->IsVisible()) {
    layer_tree_frame_sink_request_failed_while_invisible_ = true;
    return;
  }
  layer_tree_frame_sink_request_failed_while_invisible_ = false;
  layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeView::RequestNewLayerTreeFrameSink,
                                weak_factory_.GetWeakPtr()));
}

void LayerTreeView::WillCommit() {
  InvokeLayoutAndPaintCallback();
}

void LayerTreeView::DidCommit() {
  delegate_->DidCommitCompositorFrame();
  web_main_thread_scheduler_->DidCommitFrameToCompositor();
}

void LayerTreeView::DidCommitAndDrawFrame() {
  delegate_->DidCommitAndDrawCompositorFrame();
}

void LayerTreeView::DidCompletePageScaleAnimation() {
  delegate_->DidCompletePageScaleAnimation();
}

void LayerTreeView::DidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(layer_tree_host_->GetTaskRunnerProvider()
             ->MainThreadTaskRunner()
             ->RunsTasksInCurrentSequence());
  while (!presentation_callbacks_.empty()) {
    const auto& front = presentation_callbacks_.begin();
    if (viz::FrameTokenGT(front->first, frame_token))
      break;
    for (auto& callback : front->second)
      std::move(callback).Run(feedback.timestamp);
    presentation_callbacks_.erase(front);
  }
}

void LayerTreeView::RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) {
  delegate_->RecordEndOfFrameMetrics(frame_begin_time);
}

void LayerTreeView::RequestScheduleAnimation() {
  delegate_->RequestScheduleAnimation();
}

void LayerTreeView::DidSubmitCompositorFrame() {}

void LayerTreeView::DidLoseLayerTreeFrameSink() {}

void LayerTreeView::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
}

void LayerTreeView::SetRasterColorSpace(const gfx::ColorSpace& color_space) {
  layer_tree_host_->SetRasterColorSpace(color_space);
}

void LayerTreeView::ClearCachesOnNextCommit() {
  layer_tree_host_->ClearCachesOnNextCommit();
}

void LayerTreeView::SetContentSourceId(uint32_t id) {
  layer_tree_host_->SetContentSourceId(id);
}

void LayerTreeView::NotifySwapTime(ReportTimeCallback callback) {
  QueueSwapPromise(std::make_unique<ReportTimeSwapPromise>(
      std::move(callback),
      layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner(),
      weak_factory_.GetWeakPtr()));
}

void LayerTreeView::RequestBeginMainFrameNotExpected(bool new_state) {
  layer_tree_host_->RequestBeginMainFrameNotExpected(new_state);
}

const cc::LayerTreeSettings& LayerTreeView::GetLayerTreeSettings() const {
  return layer_tree_host_->GetSettings();
}

void LayerTreeView::SetRenderFrameObserver(
    std::unique_ptr<cc::RenderFrameMetadataObserver> observer) {
  layer_tree_host_->SetRenderFrameObserver(std::move(observer));
}

void LayerTreeView::AddPresentationCallback(
    uint32_t frame_token,
    base::OnceCallback<void(base::TimeTicks)> callback) {
  if (!presentation_callbacks_.empty()) {
    auto& previous = presentation_callbacks_.back();
    uint32_t previous_frame_token = previous.first;
    if (previous_frame_token == frame_token) {
      previous.second.push_back(std::move(callback));
      DCHECK_LE(previous.second.size(), 250u);
      return;
    }
    DCHECK(viz::FrameTokenGT(frame_token, previous_frame_token));
  }
  std::vector<base::OnceCallback<void(base::TimeTicks)>> callbacks;
  callbacks.push_back(std::move(callback));
  presentation_callbacks_.push_back({frame_token, std::move(callbacks)});
  DCHECK_LE(presentation_callbacks_.size(), 25u);
}

void LayerTreeView::SetURLForUkm(const GURL& url) {
  layer_tree_host_->SetURLForUkm(url);
}

}  // namespace content
