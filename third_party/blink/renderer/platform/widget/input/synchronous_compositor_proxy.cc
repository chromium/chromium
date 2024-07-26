// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/synchronous_compositor_proxy.h"

#include "base/functional/bind.h"
#include "base/memory/shared_memory_mapping.h"
#include "components/viz/common/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

SynchronousCompositorProxy::SynchronousCompositorProxy(
    InputHandlerProxy* input_handler_proxy)
    : input_handler_proxy_(input_handler_proxy),
      viz_frame_submission_enabled_(
          features::IsUsingVizFrameSubmissionForWebView()),
      page_scale_factor_(0.f),
      min_page_scale_factor_(0.f),
      max_page_scale_factor_(0.f),
      need_invalidate_count_(0u),
      invalidate_needs_draw_(false),
      did_activate_pending_tree_count_(0u) {
  DCHECK(input_handler_proxy_);
}

SynchronousCompositorProxy::~SynchronousCompositorProxy() {
  // The LayerTreeFrameSink is destroyed/removed by the compositor before
  // shutting down everything.
  DCHECK_EQ(layer_tree_frame_sink_, nullptr);
  input_handler_proxy_->SetSynchronousInputHandler(nullptr);
}

void SynchronousCompositorProxy::Init() {
  input_handler_proxy_->SetSynchronousInputHandler(this);
}

void SynchronousCompositorProxy::SetLayerTreeFrameSink(
    SynchronousLayerTreeFrameSink* layer_tree_frame_sink) {
  DCHECK_NE(layer_tree_frame_sink_, layer_tree_frame_sink);
  DCHECK(layer_tree_frame_sink);
  if (layer_tree_frame_sink_) {
    layer_tree_frame_sink_->SetSyncClient(nullptr);
  }
  layer_tree_frame_sink_ = layer_tree_frame_sink;
  use_in_process_zero_copy_software_draw_ =
      layer_tree_frame_sink_->UseZeroCopySoftwareDraw();
  layer_tree_frame_sink_->SetSyncClient(this);
  LayerTreeFrameSinkCreated();
  if (begin_frame_paused_)
    layer_tree_frame_sink_->SetBeginFrameSourcePaused(true);
}

void SynchronousCompositorProxy::UpdateRootLayerState(
    const gfx::PointF& total_scroll_offset,
    const gfx::PointF& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  if (total_scroll_offset_ != total_scroll_offset ||
      max_scroll_offset_ != max_scroll_offset ||
      scrollable_size_ != scrollable_size ||
      page_scale_factor_ != page_scale_factor ||
      min_page_scale_factor_ != min_page_scale_factor ||
      max_page_scale_factor_ != max_page_scale_factor) {
    total_scroll_offset_ = total_scroll_offset;
    max_scroll_offset_ = max_scroll_offset;
    scrollable_size_ = scrollable_size;
    page_scale_factor_ = page_scale_factor;
    min_page_scale_factor_ = min_page_scale_factor;
    max_page_scale_factor_ = max_page_scale_factor;

    SendAsyncRendererStateIfNeeded();
  }
}

void SynchronousCompositorProxy::Invalidate(bool needs_draw) {
  ++need_invalidate_count_;
  invalidate_needs_draw_ |= needs_draw;
  SendAsyncRendererStateIfNeeded();
}

void SynchronousCompositorProxy::DidActivatePendingTree() {
  ++did_activate_pending_tree_count_;
  SendAsyncRendererStateIfNeeded();
}

mojom::blink::SyncCompositorCommonRendererParamsPtr
SynchronousCompositorProxy::PopulateNewCommonParams() {
  mojom::blink::SyncCompositorCommonRendererParamsPtr params =
      mojom::blink::SyncCompositorCommonRendererParams::New();
  params->version = ++version_;
  params->total_scroll_offset = total_scroll_offset_;
  params->max_scroll_offset = max_scroll_offset_;
  params->scrollable_size = scrollable_size_;
  params->page_scale_factor = page_scale_factor_;
  params->min_page_scale_factor = min_page_scale_factor_;
  params->max_page_scale_factor = max_page_scale_factor_;
  params->need_invalidate_count = need_invalidate_count_;
  params->invalidate_needs_draw = invalidate_needs_draw_;
  params->did_activate_pending_tree_count = did_activate_pending_tree_count_;
  return params;
}

void SynchronousCompositorProxy::DemandDrawHwAsync(
    mojom::blink::SyncCompositorDemandDrawHwParamsPtr params) {
  DemandDrawHw(
      std::move(params),
      base::BindOnce(&SynchronousCompositorProxy::SendDemandDrawHwAsyncReply,
                     base::Unretained(this)));
}

void SynchronousCompositorProxy::DemandDrawHw(
    mojom::blink::SyncCompositorDemandDrawHwParamsPtr params,
    DemandDrawHwCallback callback) {
  invalidate_needs_draw_ = false;
  hardware_draw_reply_ = std::move(callback);

  if (layer_tree_frame_sink_) {
    layer_tree_frame_sink_->DemandDrawHw(
        params->viewport_size, params->viewport_rect_for_tile_priority,
        params->transform_for_tile_priority, params->need_new_local_surface_id);
  }

  // Ensure that a response is always sent even if the reply hasn't
  // generated a compostior frame.
  if (hardware_draw_reply_) {
    // Did not swap.
    std::move(hardware_draw_reply_)
        .Run(PopulateNewCommonParams(), 0u, 0u, std::nullopt, std::nullopt,
             std::nullopt);
  }
}

void SynchronousCompositorProxy::WillSkipDraw() {
  if (layer_tree_frame_sink_) {
    layer_tree_frame_sink_->WillSkipDraw();
  }
}

struct SynchronousCompositorProxy::SharedMemoryWithSize {
  base::WritableSharedMemoryMapping shared_memory;
  const size_t buffer_size;
  bool zeroed;

  SharedMemoryWithSize(base::WritableSharedMemoryMapping shm_mapping,
                       size_t buffer_size)
      : shared_memory(std::move(shm_mapping)),
        buffer_size(buffer_size),
        zeroed(true) {}
};

void SynchronousCompositorProxy::ZeroSharedMemory() {
  // It is possible for this to get called twice, eg. if draw is called before
  // the LayerTreeFrameSink is ready. Just ignore duplicated calls rather than
  // inventing a complicated system to avoid it.
  if (software_draw_shm_->zeroed)
    return;

  base::span<uint8_t> mem(software_draw_shm_->shared_memory);
  std::ranges::fill(mem.first(software_draw_shm_->buffer_size), 0u);
  software_draw_shm_->zeroed = true;
}

void SynchronousCompositorProxy::DemandDrawSw(
    mojom::blink::SyncCompositorDemandDrawSwParamsPtr params,
    DemandDrawSwCallback callback) {
  invalidate_needs_draw_ = false;

  software_draw_reply_ = std::move(callback);
  if (layer_tree_frame_sink_) {
    if (use_in_process_zero_copy_software_draw_) {
      layer_tree_frame_sink_->DemandDrawSwZeroCopy();
    } else {
      DoDemandDrawSw(std::move(params));
    }
  }

  // Ensure that a response is always sent even if the reply hasn't
  // generated a compostior frame.
  if (software_draw_reply_) {
    // Did not swap.
    std::move(software_draw_reply_)
        .Run(PopulateNewCommonParams(), 0u, std::nullopt);
  }
}

void SynchronousCompositorProxy::DoDemandDrawSw(
    mojom::blink::SyncCompositorDemandDrawSwParamsPtr params) {
  DCHECK(layer_tree_frame_sink_);
  DCHECK(software_draw_shm_->zeroed);
  software_draw_shm_->zeroed = false;

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params->size.width(), params->size.height());
  size_t stride = info.minRowBytes();
  size_t buffer_size = info.computeByteSize(stride);
  DCHECK_EQ(software_draw_shm_->buffer_size, buffer_size);

  base::span<uint8_t> mem(software_draw_shm_->shared_memory);
  CHECK_GE(mem.size(), buffer_size);
  SkBitmap bitmap;
  if (!bitmap.installPixels(info, mem.data(), stride)) {
    return;
  }
  SkCanvas canvas(bitmap);
  canvas.clipRect(gfx::RectToSkRect(params->clip));
  canvas.concat(gfx::TransformToFlattenedSkMatrix(params->transform));

  layer_tree_frame_sink_->DemandDrawSw(&canvas);
}

void SynchronousCompositorProxy::SubmitCompositorFrame(
    uint32_t layer_tree_frame_sink_id,
    const viz::LocalSurfaceId& local_surface_id,
    std::optional<viz::CompositorFrame> frame,
    std::optional<viz::HitTestRegionList> hit_test_region_list) {
  // Verify that exactly one of these is true.
  DCHECK(hardware_draw_reply_.is_null() ^ software_draw_reply_.is_null());
  mojom::blink::SyncCompositorCommonRendererParamsPtr common_renderer_params =
      PopulateNewCommonParams();

  if (hardware_draw_reply_) {
    // For viz the CF was submitted directly via CompositorFrameSink
    DCHECK(frame || viz_frame_submission_enabled_);
    DCHECK(local_surface_id.is_valid());
    std::move(hardware_draw_reply_)
        .Run(std::move(common_renderer_params), layer_tree_frame_sink_id,
             NextMetadataVersion(), local_surface_id, std::move(frame),
             std::move(hit_test_region_list));
  } else if (software_draw_reply_) {
    DCHECK(frame);
    std::move(software_draw_reply_)
        .Run(std::move(common_renderer_params), NextMetadataVersion(),
             std::move(frame->metadata));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void SynchronousCompositorProxy::SetNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames)
    return;
  needs_begin_frames_ = needs_begin_frames;
  if (host_)
    host_->SetNeedsBeginFrames(needs_begin_frames);
}

void SynchronousCompositorProxy::SinkDestroyed() {
  layer_tree_frame_sink_ = nullptr;
}

void SynchronousCompositorProxy::SetThreadIds(
    const Vector<base::PlatformThreadId>& thread_ids) {
  if (thread_ids_ == thread_ids) {
    return;
  }
  thread_ids_ = thread_ids;
  if (host_) {
    host_->SetThreadIds(thread_ids_);
  }
}

void SynchronousCompositorProxy::SetBeginFrameSourcePaused(bool paused) {
  begin_frame_paused_ = paused;
  if (layer_tree_frame_sink_)
    layer_tree_frame_sink_->SetBeginFrameSourcePaused(paused);
}

void SynchronousCompositorProxy::BeginFrame(
    const viz::BeginFrameArgs& args,
    const HashMap<uint32_t, viz::FrameTimingDetails>& timing_details) {
  if (layer_tree_frame_sink_) {
    base::flat_map<uint32_t, viz::FrameTimingDetails> timings;
    for (const auto& pair : timing_details) {
      timings[pair.key] = pair.value;
    }
    layer_tree_frame_sink_->DidPresentCompositorFrame(timings);
    if (needs_begin_frames_)
      layer_tree_frame_sink_->BeginFrame(args);
  }

  SendBeginFrameResponse(PopulateNewCommonParams());
}

void SynchronousCompositorProxy::SetScroll(
    const gfx::PointF& new_total_scroll_offset) {
  if (total_scroll_offset_ == new_total_scroll_offset)
    return;
  total_scroll_offset_ = new_total_scroll_offset;
  input_handler_proxy_->SynchronouslySetRootScrollOffset(total_scroll_offset_);
}

void SynchronousCompositorProxy::SetMemoryPolicy(uint32_t bytes_limit) {
  if (!layer_tree_frame_sink_)
    return;
  layer_tree_frame_sink_->SetMemoryPolicy(bytes_limit);
}

void SynchronousCompositorProxy::ReclaimResources(
    uint32_t layer_tree_frame_sink_id,
    Vector<viz::ReturnedResource> resources) {
  if (!layer_tree_frame_sink_)
    return;
  layer_tree_frame_sink_->ReclaimResources(layer_tree_frame_sink_id,
                                           std::move(resources));
}

void SynchronousCompositorProxy::OnCompositorFrameTransitionDirectiveProcessed(
    uint32_t layer_tree_frame_sink_id,
    uint32_t sequence_id) {
  if (!layer_tree_frame_sink_)
    return;
  layer_tree_frame_sink_->OnCompositorFrameTransitionDirectiveProcessed(
      layer_tree_frame_sink_id, sequence_id);
}

void SynchronousCompositorProxy::SetSharedMemory(
    base::WritableSharedMemoryRegion shm_region,
    SetSharedMemoryCallback callback) {
  bool success = false;
  mojom::blink::SyncCompositorCommonRendererParamsPtr common_renderer_params;
  if (shm_region.IsValid()) {
    base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
    if (shm_mapping.IsValid()) {
      software_draw_shm_ = std::make_unique<SharedMemoryWithSize>(
          std::move(shm_mapping), shm_mapping.size());
      common_renderer_params = PopulateNewCommonParams();
      success = true;
    }
  }
  if (!common_renderer_params) {
    common_renderer_params =
        mojom::blink::SyncCompositorCommonRendererParams::New();
  }
  std::move(callback).Run(success, std::move(common_renderer_params));
}

void SynchronousCompositorProxy::ZoomBy(float zoom_delta,
                                        const gfx::Point& anchor,
                                        ZoomByCallback callback) {
  zoom_by_reply_ = std::move(callback);
  input_handler_proxy_->SynchronouslyZoomBy(zoom_delta, anchor);
  std::move(zoom_by_reply_).Run(PopulateNewCommonParams());
}

uint32_t SynchronousCompositorProxy::NextMetadataVersion() {
  return ++metadata_version_;
}

void SynchronousCompositorProxy::SendDemandDrawHwAsyncReply(
    mojom::blink::SyncCompositorCommonRendererParamsPtr,
    uint32_t layer_tree_frame_sink_id,
    uint32_t metadata_version,
    const std::optional<viz::LocalSurfaceId>& local_surface_id,
    std::optional<viz::CompositorFrame> frame,
    std::optional<viz::HitTestRegionList> hit_test_region_list) {
  control_host_->ReturnFrame(layer_tree_frame_sink_id, metadata_version,
                             local_surface_id, std::move(frame),
                             std::move(hit_test_region_list));
}

void SynchronousCompositorProxy::SendBeginFrameResponse(
    mojom::blink::SyncCompositorCommonRendererParamsPtr param) {
  control_host_->BeginFrameResponse(std::move(param));
}

void SynchronousCompositorProxy::SendAsyncRendererStateIfNeeded() {
  if (hardware_draw_reply_ || software_draw_reply_ || zoom_by_reply_ || !host_)
    return;

  host_->UpdateState(PopulateNewCommonParams());
}

void SynchronousCompositorProxy::LayerTreeFrameSinkCreated() {
  DCHECK(layer_tree_frame_sink_);
  if (host_)
    host_->LayerTreeFrameSinkCreated();
}

void SynchronousCompositorProxy::BindChannel(
    mojo::PendingRemote<mojom::blink::SynchronousCompositorControlHost>
        control_host,
    mojo::PendingAssociatedRemote<mojom::blink::SynchronousCompositorHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::SynchronousCompositor>
        compositor_request) {
  // Reset bound mojo channels before rebinding new variants as the
  // associated RenderWidgetHost may be reused.
  control_host_.reset();
  host_.reset();
  receiver_.reset();
  control_host_.Bind(std::move(control_host));
  host_.Bind(std::move(host));
  receiver_.Bind(std::move(compositor_request));
  receiver_.set_disconnect_handler(base::BindOnce(
      &SynchronousCompositorProxy::HostDisconnected, base::Unretained(this)));

  if (layer_tree_frame_sink_)
    LayerTreeFrameSinkCreated();

  if (needs_begin_frames_)
    host_->SetNeedsBeginFrames(true);
  if (!thread_ids_.empty()) {
    host_->SetThreadIds(thread_ids_);
  }
}

void SynchronousCompositorProxy::HostDisconnected() {
  // It is possible due to bugs that the Host is disconnected without pausing
  // begin frames. This causes hard-to-reproduce but catastrophic bug of
  // blocking the renderer main thread forever on a commit. See
  // crbug.com/1010478 for when this happened. This is to prevent a similar
  // bug in the future.
  SetBeginFrameSourcePaused(true);
}

}  // namespace blink
