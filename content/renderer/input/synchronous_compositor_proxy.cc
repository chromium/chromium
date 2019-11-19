// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/synchronous_compositor_proxy.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/shared_memory.h"
#include "content/common/android/sync_compositor_statics.h"
#include "content/common/input/sync_compositor_messages.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/skia_util.h"

namespace content {

SynchronousCompositorProxy::SynchronousCompositorProxy(
    ui::SynchronousInputHandlerProxy* input_handler_proxy)
    : input_handler_proxy_(input_handler_proxy),
      use_in_process_zero_copy_software_draw_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kSingleProcess)),
      page_scale_factor_(0.f),
      min_page_scale_factor_(0.f),
      max_page_scale_factor_(0.f),
      need_animate_scroll_(false),
      need_invalidate_count_(0u),
      invalidate_needs_draw_(false),
      did_activate_pending_tree_count_(0u) {
  DCHECK(input_handler_proxy_);
}

SynchronousCompositorProxy::~SynchronousCompositorProxy() {
  // The LayerTreeFrameSink is destroyed/removed by the compositor before
  // shutting down everything.
  DCHECK_EQ(layer_tree_frame_sink_, nullptr);
  input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(nullptr);
}

void SynchronousCompositorProxy::Init() {
  input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(this);
}

void SynchronousCompositorProxy::SetLayerTreeFrameSink(
    SynchronousLayerTreeFrameSink* layer_tree_frame_sink) {
  DCHECK_NE(layer_tree_frame_sink_, layer_tree_frame_sink);
  DCHECK(layer_tree_frame_sink);
  if (layer_tree_frame_sink_) {
    layer_tree_frame_sink_->SetSyncClient(nullptr);
  }
  layer_tree_frame_sink_ = layer_tree_frame_sink;
  layer_tree_frame_sink_->SetSyncClient(this);
  LayerTreeFrameSinkCreated();
  if (begin_frame_paused_)
    layer_tree_frame_sink_->SetBeginFrameSourcePaused(true);
}

void SynchronousCompositorProxy::SetNeedsSynchronousAnimateInput() {
  if (compute_scroll_called_via_ipc_) {
    needs_begin_frame_for_animate_input_ = true;
    SendSetNeedsBeginFramesIfNeeded();
  } else {
    need_animate_scroll_ = true;
    Invalidate(true);
  }
}

void SynchronousCompositorProxy::UpdateRootLayerState(
    const gfx::ScrollOffset& total_scroll_offset,
    const gfx::ScrollOffset& max_scroll_offset,
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

void SynchronousCompositorProxy::PopulateCommonParams(
    SyncCompositorCommonRendererParams* params) {
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
  if (!compute_scroll_called_via_ipc_)
    params->need_animate_scroll = need_animate_scroll_;
}

void SynchronousCompositorProxy::DemandDrawHwAsync(
    const SyncCompositorDemandDrawHwParams& params) {
  DemandDrawHw(
      params,
      base::BindOnce(&SynchronousCompositorProxy::SendDemandDrawHwAsyncReply,
                     base::Unretained(this)));
}

void SynchronousCompositorProxy::DemandDrawHw(
    const SyncCompositorDemandDrawHwParams& params,
    DemandDrawHwCallback callback) {
  invalidate_needs_draw_ = false;
  hardware_draw_reply_ = std::move(callback);

  if (layer_tree_frame_sink_) {
    layer_tree_frame_sink_->DemandDrawHw(params.viewport_size,
                                         params.viewport_rect_for_tile_priority,
                                         params.transform_for_tile_priority);
  }

  // Ensure that a response is always sent even if the reply hasn't
  // generated a compostior frame.
  if (hardware_draw_reply_) {
    SyncCompositorCommonRendererParams common_renderer_params;
    PopulateCommonParams(&common_renderer_params);
    // Did not swap.
    std::move(hardware_draw_reply_)
        .Run(common_renderer_params, 0u, 0u, base::nullopt);
  }
}

void SynchronousCompositorProxy::WillSkipDraw() {
  layer_tree_frame_sink_->WillSkipDraw();
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

  memset(software_draw_shm_->shared_memory.memory(), 0,
         software_draw_shm_->buffer_size);
  software_draw_shm_->zeroed = true;
}

void SynchronousCompositorProxy::DemandDrawSw(
    const SyncCompositorDemandDrawSwParams& params,
    DemandDrawSwCallback callback) {
  invalidate_needs_draw_ = false;
  software_draw_reply_ = std::move(callback);
  if (layer_tree_frame_sink_) {
    SkCanvas* sk_canvas_for_draw = SynchronousCompositorGetSkCanvas();
    if (use_in_process_zero_copy_software_draw_) {
      DCHECK(sk_canvas_for_draw);
      layer_tree_frame_sink_->DemandDrawSw(sk_canvas_for_draw);
    } else {
      DCHECK(!sk_canvas_for_draw);
      DoDemandDrawSw(params);
    }
  }

  // Ensure that a response is always sent even if the reply hasn't
  // generated a compostior frame.
  if (software_draw_reply_) {
    SyncCompositorCommonRendererParams common_renderer_params;
    PopulateCommonParams(&common_renderer_params);
    // Did not swap.
    std::move(software_draw_reply_)
        .Run(common_renderer_params, 0u, base::nullopt);
  }
}

void SynchronousCompositorProxy::DoDemandDrawSw(
    const SyncCompositorDemandDrawSwParams& params) {
  DCHECK(layer_tree_frame_sink_);
  DCHECK(software_draw_shm_->zeroed);
  software_draw_shm_->zeroed = false;

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params.size.width(), params.size.height());
  size_t stride = info.minRowBytes();
  size_t buffer_size = info.computeByteSize(stride);
  DCHECK_EQ(software_draw_shm_->buffer_size, buffer_size);

  SkBitmap bitmap;
  if (!bitmap.installPixels(info, software_draw_shm_->shared_memory.memory(),
                            stride)) {
    return;
  }
  SkCanvas canvas(bitmap);
  canvas.clipRect(gfx::RectToSkRect(params.clip));
  canvas.concat(params.transform.matrix());

  layer_tree_frame_sink_->DemandDrawSw(&canvas);
}

void SynchronousCompositorProxy::SubmitCompositorFrame(
    uint32_t layer_tree_frame_sink_id,
    viz::CompositorFrame frame) {
  // Verify that exactly one of these is true.
  DCHECK(hardware_draw_reply_.is_null() ^ software_draw_reply_.is_null());
  SyncCompositorCommonRendererParams common_renderer_params;
  PopulateCommonParams(&common_renderer_params);

  if (hardware_draw_reply_) {
    std::move(hardware_draw_reply_)
        .Run(common_renderer_params, layer_tree_frame_sink_id,
             NextMetadataVersion(), std::move(frame));
  } else if (software_draw_reply_) {
    std::move(software_draw_reply_)
        .Run(common_renderer_params, NextMetadataVersion(),
             std::move(frame.metadata));
  } else {
    NOTREACHED();
  }
}

void SynchronousCompositorProxy::SendSetNeedsBeginFramesIfNeeded() {
  bool needs_begin_frames =
      needs_begin_frame_for_frame_sink_ || needs_begin_frame_for_animate_input_;
  if (browser_needs_begin_frame_state_ != needs_begin_frames)
    SendSetNeedsBeginFrames(needs_begin_frames);
  browser_needs_begin_frame_state_ = needs_begin_frames;
}

void SynchronousCompositorProxy::SetNeedsBeginFrames(bool needs_begin_frames) {
  needs_begin_frame_for_frame_sink_ = needs_begin_frames;
  SendSetNeedsBeginFramesIfNeeded();
}

void SynchronousCompositorProxy::SinkDestroyed() {
  layer_tree_frame_sink_ = nullptr;
}

void SynchronousCompositorProxy::ComputeScroll(base::TimeTicks animation_time) {
  compute_scroll_called_via_ipc_ = true;

  if (need_animate_scroll_) {
    need_animate_scroll_ = false;
    input_handler_proxy_->SynchronouslyAnimate(animation_time);
  }
}

void SynchronousCompositorProxy::SetBeginFrameSourcePaused(bool paused) {
  begin_frame_paused_ = paused;
  if (layer_tree_frame_sink_)
    layer_tree_frame_sink_->SetBeginFrameSourcePaused(paused);
}

void SynchronousCompositorProxy::BeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details) {
  if (needs_begin_frame_for_animate_input_) {
    needs_begin_frame_for_animate_input_ = false;
    input_handler_proxy_->SynchronouslyAnimate(args.frame_time);
  }
  if (layer_tree_frame_sink_) {
    layer_tree_frame_sink_->DidPresentCompositorFrame(timing_details);
    if (needs_begin_frame_for_frame_sink_)
      layer_tree_frame_sink_->BeginFrame(args);
  }

  SyncCompositorCommonRendererParams param;
  PopulateCommonParams(&param);
  SendBeginFrameResponse(param);
}

void SynchronousCompositorProxy::SetScroll(
    const gfx::ScrollOffset& new_total_scroll_offset) {
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
    const std::vector<viz::ReturnedResource>& resources) {
  if (!layer_tree_frame_sink_)
    return;
  layer_tree_frame_sink_->ReclaimResources(layer_tree_frame_sink_id, resources);
}

void SynchronousCompositorProxy::SetSharedMemory(
    base::WritableSharedMemoryRegion shm_region,
    SetSharedMemoryCallback callback) {
  bool success = false;
  SyncCompositorCommonRendererParams common_renderer_params;
  if (shm_region.IsValid()) {
    base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
    if (shm_mapping.IsValid()) {
      software_draw_shm_ = std::make_unique<SharedMemoryWithSize>(
          std::move(shm_mapping), shm_mapping.size());
      PopulateCommonParams(&common_renderer_params);
      success = true;
    }
  }
  std::move(callback).Run(success, common_renderer_params);
}

void SynchronousCompositorProxy::ZoomBy(float zoom_delta,
                                        const gfx::Point& anchor,
                                        ZoomByCallback callback) {
  zoom_by_reply_ = std::move(callback);
  input_handler_proxy_->SynchronouslyZoomBy(zoom_delta, anchor);
  SyncCompositorCommonRendererParams common_renderer_params;
  PopulateCommonParams(&common_renderer_params);
  std::move(zoom_by_reply_).Run(common_renderer_params);
}

uint32_t SynchronousCompositorProxy::NextMetadataVersion() {
  return ++metadata_version_;
}

void SynchronousCompositorProxy::SendDemandDrawHwAsyncReply(
    const content::SyncCompositorCommonRendererParams&,
    uint32_t layer_tree_frame_sink_id,
    uint32_t metadata_version,
    base::Optional<viz::CompositorFrame> frame) {
  control_host_->ReturnFrame(layer_tree_frame_sink_id, metadata_version,
                             std::move(frame));
}

void SynchronousCompositorProxy::SendBeginFrameResponse(
    const content::SyncCompositorCommonRendererParams& param) {
  control_host_->BeginFrameResponse(param);
}

void SynchronousCompositorProxy::SendAsyncRendererStateIfNeeded() {
  if (hardware_draw_reply_ || software_draw_reply_ || zoom_by_reply_ || !host_)
    return;

  SyncCompositorCommonRendererParams params;
  PopulateCommonParams(&params);
  host_->UpdateState(params);
}

void SynchronousCompositorProxy::SendSetNeedsBeginFrames(
    bool needs_begin_frames) {
  needs_begin_frame_ = needs_begin_frames;
  if (host_)
    host_->SetNeedsBeginFrames(needs_begin_frames);
}

void SynchronousCompositorProxy::LayerTreeFrameSinkCreated() {
  DCHECK(layer_tree_frame_sink_);
  if (host_)
    host_->LayerTreeFrameSinkCreated();
}

void SynchronousCompositorProxy::BindChannel(
    mojo::PendingRemote<mojom::SynchronousCompositorControlHost> control_host,
    mojo::PendingAssociatedRemote<mojom::SynchronousCompositorHost> host,
    mojo::PendingAssociatedReceiver<mojom::SynchronousCompositor>
        compositor_request) {
  control_host_.Bind(std::move(control_host));
  host_.Bind(std::move(host));
  receiver_.Bind(std::move(compositor_request));
  receiver_.set_disconnect_handler(base::BindOnce(
      &SynchronousCompositorProxy::HostDisconnected, base::Unretained(this)));

  if (layer_tree_frame_sink_)
    LayerTreeFrameSinkCreated();

  if (needs_begin_frame_)
    host_->SetNeedsBeginFrames(true);
}

void SynchronousCompositorProxy::HostDisconnected() {
  // It is possible due to bugs that the Host is disconnected without pausing
  // begin frames. This causes hard-to-reproduce but catastrophic bug of
  // blocking the renderer main thread forever on a commit. See
  // crbug.com/1010478 for when this happened. This is to prevent a similar
  // bug in the future.
  SetBeginFrameSourcePaused(true);
}

}  // namespace content
