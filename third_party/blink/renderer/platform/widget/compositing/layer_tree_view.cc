// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/region.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {
class Layer;
}

namespace blink {

LayerTreeView::LayerTreeView(
    LayerTreeViewDelegate* delegate,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_thread,
    cc::TaskGraphRunner* task_graph_runner,
    scheduler::WebThreadScheduler* scheduler)
    : main_thread_(std::move(main_thread)),
      compositor_thread_(std::move(compositor_thread)),
      task_graph_runner_(task_graph_runner),
      web_main_thread_scheduler_(scheduler),
      animation_host_(cc::AnimationHost::CreateMainInstance()),
      delegate_(delegate) {}

LayerTreeView::~LayerTreeView() = default;

void LayerTreeView::Initialize(
    const cc::LayerTreeSettings& settings,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) {
  DCHECK(delegate_);
  const bool is_threaded = !!compositor_thread_;

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.scheduling_client = this;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner_;
  params.main_task_runner = main_thread_;
  params.mutator_host = animation_host_.get();
  params.ukm_recorder_factory = std::move(ukm_recorder_factory);
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
    layer_tree_host_ = cc::LayerTreeHost::CreateThreaded(compositor_thread_,
                                                         std::move(params));
  }
}

void LayerTreeView::Disconnect() {
  DCHECK(delegate_);
  // Drop compositor resources immediately, while keeping the compositor alive
  // until after this class is destroyed.
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
  if (web_main_thread_scheduler_)
    web_main_thread_scheduler_->WillBeginFrame(args);
  delegate_->BeginMainFrame(args.frame_time);
}

void LayerTreeView::OnDeferMainFrameUpdatesChanged(bool status) {
  if (!delegate_)
    return;
  delegate_->OnDeferMainFrameUpdatesChanged(status);
}

void LayerTreeView::OnDeferCommitsChanged(bool status) {
  if (!delegate_)
    return;
  delegate_->OnDeferCommitsChanged(status);
}

void LayerTreeView::BeginMainFrameNotExpectedSoon() {
  if (!delegate_ || !web_main_thread_scheduler_)
    return;
  web_main_thread_scheduler_->BeginFrameNotExpectedSoon();
}

void LayerTreeView::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  if (!delegate_ || !web_main_thread_scheduler_)
    return;
  web_main_thread_scheduler_->BeginMainFrameNotExpectedUntil(time);
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

void LayerTreeView::RecordManipulationTypeCounts(cc::ManipulationInfo info) {
  if (!delegate_)
    return;
  delegate_->RecordManipulationTypeCounts(info);
}

void LayerTreeView::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  if (!delegate_)
    return;
  delegate_->SendOverscrollEventFromImplSide(overscroll_delta,
                                             scroll_latched_element_id);
}

void LayerTreeView::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  if (!delegate_)
    return;
  delegate_->SendScrollEndEventFromImplSide(scroll_latched_element_id);
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

void LayerTreeView::WillCommit() {
  if (!delegate_)
    return;
  delegate_->WillCommitCompositorFrame();
}

void LayerTreeView::DidCommit(base::TimeTicks commit_start_time) {
  if (!delegate_)
    return;
  delegate_->DidCommitCompositorFrame(commit_start_time);
  if (web_main_thread_scheduler_)
    web_main_thread_scheduler_->DidCommitFrameToCompositor();
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
  while (!presentation_callbacks_.empty()) {
    const auto& front = presentation_callbacks_.begin();
    if (viz::FrameTokenGT(front->first, frame_token))
      break;
    for (auto& callback : front->second)
      std::move(callback).Run(feedback.timestamp);
    presentation_callbacks_.erase(front);
  }
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

void LayerTreeView::DidScheduleBeginMainFrame() {
  if (!delegate_ || !web_main_thread_scheduler_)
    return;
  web_main_thread_scheduler_->DidScheduleBeginMainFrame();
}

void LayerTreeView::DidRunBeginMainFrame() {
  if (!delegate_ || !web_main_thread_scheduler_)
    return;
  web_main_thread_scheduler_->DidRunBeginMainFrame();
}

void LayerTreeView::DidSubmitCompositorFrame() {}

void LayerTreeView::DidLoseLayerTreeFrameSink() {}

void LayerTreeView::AddPresentationCallback(
    uint32_t frame_token,
    base::OnceCallback<void(base::TimeTicks)> callback) {
  DCHECK(delegate_);
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

}  // namespace blink
