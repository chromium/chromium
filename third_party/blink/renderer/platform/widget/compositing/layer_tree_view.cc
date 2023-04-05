// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/features.h"
#include "cc/base/region.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/metrics/web_vital_metrics.h"
#include "cc/tiles/raster_dark_mode_filter.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/presentation_time_callback_buffer.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"
#include "third_party/blink/renderer/platform/graphics/raster_dark_mode_filter_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/widget_scheduler.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {
class Layer;
}

namespace blink {

namespace {
// This factory is used to defer binding of the InterfacePtr to the compositor
// thread.
class UkmRecorderFactoryImpl : public cc::UkmRecorderFactory {
 public:
  UkmRecorderFactoryImpl() = default;
  ~UkmRecorderFactoryImpl() override = default;

  // This method gets called on the compositor thread.
  std::unique_ptr<ukm::UkmRecorder> CreateRecorder() override {
    mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;

    // Calling these methods on the compositor thread are thread safe.
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        factory.BindNewPipeAndPassReceiver());
    return ukm::MojoUkmRecorder::Create(*factory);
  }
};

}  // namespace

LayerTreeView::LayerTreeView(
    LayerTreeViewDelegate* delegate,
    scoped_refptr<scheduler::WidgetScheduler> scheduler)
    : widget_scheduler_(std::move(scheduler)),
      animation_host_(cc::AnimationHost::CreateMainInstance()),
      delegate_(delegate) {}

LayerTreeView::~LayerTreeView() = default;

void LayerTreeView::Initialize(
    const cc::LayerTreeSettings& settings,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_thread,
    cc::TaskGraphRunner* task_graph_runner) {
  DCHECK(delegate_);
  const bool is_threaded = !!compositor_thread;

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.scheduling_client = this;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner;
  params.main_task_runner = std::move(main_thread);
  params.mutator_host = animation_host_.get();
  params.dark_mode_filter = &RasterDarkModeFilterImpl::Instance();
  params.ukm_recorder_factory = std::make_unique<UkmRecorderFactoryImpl>();
  if (base::ThreadPoolInstance::Get()) {
    // The image worker thread needs to allow waiting since it makes discardable
    // shared memory allocations which need to make synchronous calls to the
    // IO thread.
    params.image_worker_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  }
  if (!is_threaded) {
    // Single-threaded web tests, and unit tests.
    layer_tree_host_ =
        cc::LayerTreeHost::CreateSingleThreaded(this, std::move(params));
  } else {
    layer_tree_host_ =
        cc::LayerTreeHost::CreateThreaded(std::move(compositor_thread), std::move(params));
  }
}

void LayerTreeView::Disconnect() {
  DCHECK(delegate_);
  // Drop compositor resources immediately, while keeping the compositor alive
  // until after this class is destroyed.
  layer_tree_host_->WaitForProtectedSequenceCompletion();
  layer_tree_host_->SetVisible(false);
  layer_tree_host_->ReleaseLayerTreeFrameSink();
  delegate_ = nullptr;
}

void LayerTreeView::SetVisible(bool visible) {
  DCHECK(delegate_);
  layer_tree_host_->SetVisible(visible);

  if (visible && layer_tree_frame_sink_request_failed_while_invisible_)
    DidFailToInitializeLayerTreeFrameSink();
}

void LayerTreeView::SetLayerTreeFrameSink(
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink,
    std::unique_ptr<cc::RenderFrameMetadataObserver>
        render_frame_metadata_observer) {
  DCHECK(delegate_);
  if (!layer_tree_frame_sink) {
    DidFailToInitializeLayerTreeFrameSink();
    return;
  }
  if (render_frame_metadata_observer) {
    layer_tree_host_->SetRenderFrameObserver(
        std::move(render_frame_metadata_observer));
  }
  layer_tree_host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void LayerTreeView::WillBeginMainFrame() {
  if (!delegate_)
    return;
  delegate_->WillBeginMainFrame();
}

void LayerTreeView::DidBeginMainFrame() {
  if (!delegate_)
    return;
  delegate_->DidBeginMainFrame();
}

void LayerTreeView::WillUpdateLayers() {
  if (!delegate_)
    return;
  delegate_->BeginUpdateLayers();
}

void LayerTreeView::DidUpdateLayers() {
  if (!delegate_)
    return;
  delegate_->EndUpdateLayers();
  // Dump property trees and layers if run with:
  //   --vmodule=layer_tree_view=3
  VLOG(3) << "After updating layers:\n"
          << "property trees:\n"
          << layer_tree_host_->property_trees()->ToString() << "\n"
          << "cc::Layers:\n"
          << layer_tree_host_->LayersAsString();
}

void LayerTreeView::BeginMainFrame(const viz::BeginFrameArgs& args) {
  if (!delegate_)
    return;
  widget_scheduler_->WillBeginFrame(args);
  delegate_->BeginMainFrame(args.frame_time);
}

void LayerTreeView::OnDeferMainFrameUpdatesChanged(bool status) {
  if (!delegate_)
    return;
  delegate_->OnDeferMainFrameUpdatesChanged(status);
}

void LayerTreeView::OnCommitRequested() {
  if (!delegate_)
    return;
  delegate_->OnCommitRequested();
}

void LayerTreeView::OnDeferCommitsChanged(
    bool status,
    cc::PaintHoldingReason reason,
    absl::optional<cc::PaintHoldingCommitTrigger> trigger) {
  if (!delegate_)
    return;
  delegate_->OnDeferCommitsChanged(status, reason, trigger);
}

void LayerTreeView::BeginMainFrameNotExpectedSoon() {
  if (!delegate_)
    return;
  widget_scheduler_->BeginFrameNotExpectedSoon();
}

void LayerTreeView::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  if (!delegate_)
    return;
  widget_scheduler_->BeginMainFrameNotExpectedUntil(time);
}

void LayerTreeView::UpdateLayerTreeHost() {
  if (!delegate_)
    return;
  delegate_->UpdateVisualState();
}

void LayerTreeView::ApplyViewportChanges(
    const cc::ApplyViewportChangesArgs& args) {
  if (!delegate_)
    return;
  delegate_->ApplyViewportChanges(args);
}

void LayerTreeView::UpdateCompositorScrollState(
    const cc::CompositorCommitData& commit_data) {
  if (!delegate_)
    return;
  delegate_->UpdateCompositorScrollState(commit_data);
}

void LayerTreeView::RequestNewLayerTreeFrameSink() {
  if (!delegate_)
    return;
  // When the compositor is not visible it would not request a
  // LayerTreeFrameSink so this is a race where it requested one on the
  // compositor thread while becoming non-visible on the main thread. In that
  // case, we can wait for it to become visible again before replying.
  if (!layer_tree_host_->IsVisible()) {
    layer_tree_frame_sink_request_failed_while_invisible_ = true;
    return;
  }

  delegate_->RequestNewLayerTreeFrameSink(base::BindOnce(
      &LayerTreeView::SetLayerTreeFrameSink, weak_factory_.GetWeakPtr()));
}

void LayerTreeView::DidInitializeLayerTreeFrameSink() {}

void LayerTreeView::DidFailToInitializeLayerTreeFrameSink() {
  if (!delegate_)
    return;
  // When the RenderWidget is made hidden while an async request for a
  // LayerTreeFrameSink is being processed, then if it fails we would arrive
  // here. Since the compositor does not request a LayerTreeFrameSink while not
  // visible, we can delay trying again until becoming visible again.
  if (!layer_tree_host_->IsVisible()) {
    layer_tree_frame_sink_request_failed_while_invisible_ = true;
    return;
  }
  layer_tree_frame_sink_request_failed_while_invisible_ = false;
  layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeView::RequestNewLayerTreeFrameSink,
                                weak_factory_.GetWeakPtr()));
}

void LayerTreeView::WillCommit(const cc::CommitState&) {
  if (!delegate_)
    return;
  delegate_->WillCommitCompositorFrame();
  if (base::FeatureList::IsEnabled(features::kNonBlockingCommit)) {
    widget_scheduler_->DidCommitFrameToCompositor();
  }
}

void LayerTreeView::DidCommit(base::TimeTicks commit_start_time,
                              base::TimeTicks commit_finish_time) {
  if (!delegate_)
    return;
  delegate_->DidCommitCompositorFrame(commit_start_time, commit_finish_time);
  if (!base::FeatureList::IsEnabled(features::kNonBlockingCommit)) {
    widget_scheduler_->DidCommitFrameToCompositor();
  }
}

void LayerTreeView::DidCommitAndDrawFrame() {
  if (!delegate_)
    return;
  delegate_->DidCommitAndDrawCompositorFrame();
}

void LayerTreeView::DidCompletePageScaleAnimation() {
  if (!delegate_)
    return;
  delegate_->DidCompletePageScaleAnimation();
}

void LayerTreeView::DidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  if (!delegate_)
    return;
  DCHECK(layer_tree_host_->GetTaskRunnerProvider()
             ->MainThreadTaskRunner()
             ->RunsTasksInCurrentSequence());
  // Only run callbacks on successful presentations.
  if (feedback.failed())
    return;
  while (!presentation_callbacks_.empty()) {
    const auto& front = presentation_callbacks_.begin();
    if (viz::FrameTokenGT(front->first, frame_token))
      break;
    for (auto& callback : front->second)
      std::move(callback).Run(feedback.timestamp);
    presentation_callbacks_.erase(front);
  }

#if BUILDFLAG(IS_APPLE)
  while (!core_animation_error_code_callbacks_.empty()) {
    const auto& front = core_animation_error_code_callbacks_.begin();
    if (viz::FrameTokenGT(front->first, frame_token))
      break;
    for (auto& callback : front->second)
      std::move(callback).Run(feedback.ca_layer_error_code);
    core_animation_error_code_callbacks_.erase(front);
  }
#endif
}

void LayerTreeView::RecordStartOfFrameMetrics() {
  if (!delegate_)
    return;
  delegate_->RecordStartOfFrameMetrics();
}

void LayerTreeView::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    cc::ActiveFrameSequenceTrackers trackers) {
  if (!delegate_)
    return;
  delegate_->RecordEndOfFrameMetrics(frame_begin_time, trackers);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
LayerTreeView::GetBeginMainFrameMetrics() {
  if (!delegate_)
    return nullptr;
  return delegate_->GetBeginMainFrameMetrics();
}

std::unique_ptr<cc::WebVitalMetrics> LayerTreeView::GetWebVitalMetrics() {
  if (!delegate_)
    return nullptr;
  return delegate_->GetWebVitalMetrics();
}

void LayerTreeView::NotifyThroughputTrackerResults(
    cc::CustomTrackerResults results) {
  NOTREACHED();
}

void LayerTreeView::DidObserveFirstScrollDelay(
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {
  if (!delegate_) {
    return;
  }
  delegate_->DidObserveFirstScrollDelay(first_scroll_delay,
                                        first_scroll_timestamp);
}

void LayerTreeView::RunPaintBenchmark(int repeat_count,
                                      cc::PaintBenchmarkResult& result) {
  if (delegate_)
    delegate_->RunPaintBenchmark(repeat_count, result);
}

void LayerTreeView::DidRunBeginMainFrame() {
  if (!delegate_)
    return;

  widget_scheduler_->DidRunBeginMainFrame();
}

void LayerTreeView::DidSubmitCompositorFrame() {}

void LayerTreeView::DidLoseLayerTreeFrameSink() {}

void LayerTreeView::ScheduleAnimationForWebTests() {
  if (!delegate_)
    return;

  delegate_->ScheduleAnimationForWebTests();
}

void LayerTreeView::AddPresentationCallback(
    uint32_t frame_token,
    base::OnceCallback<void(base::TimeTicks)> callback) {
  AddCallback(frame_token, std::move(callback), presentation_callbacks_);
}

#if BUILDFLAG(IS_APPLE)
void LayerTreeView::AddCoreAnimationErrorCodeCallback(
    uint32_t frame_token,
    base::OnceCallback<void(gfx::CALayerResult)> callback) {
  AddCallback(frame_token, std::move(callback),
              core_animation_error_code_callbacks_);
}
#endif

template <typename Callback>
void LayerTreeView::AddCallback(
    uint32_t frame_token,
    Callback callback,
    base::circular_deque<std::pair<uint32_t, std::vector<Callback>>>&
        callbacks) {
  DCHECK(delegate_);
  if (!callbacks.empty()) {
    auto& previous = callbacks.back();
    uint32_t previous_frame_token = previous.first;
    if (previous_frame_token == frame_token) {
      previous.second.push_back(std::move(callback));
      DCHECK_LE(previous.second.size(), 250u);
      return;
    }
    DCHECK(viz::FrameTokenGT(frame_token, previous_frame_token));
  }
  std::vector<Callback> new_callbacks;
  new_callbacks.push_back(std::move(callback));
  callbacks.emplace_back(frame_token, std::move(new_callbacks));
  DCHECK_LE(callbacks.size(),
            cc::PresentationTimeCallbackBuffer::kMaxBufferSize);
}

}  // namespace blink
