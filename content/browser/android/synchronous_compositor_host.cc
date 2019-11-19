// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_host.h"

#include <atomic>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/traced_value.h"
#include "content/browser/android/synchronous_compositor_sync_call_bridge.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/sync_compositor_statics.h"
#include "content/common/input/sync_compositor_messages.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/gfx/skia_util.h"

namespace content {

// This class runs on the IO thread and is destroyed when the renderer
// side closes the mojo channel.
class SynchronousCompositorControlHost
    : public mojom::SynchronousCompositorControlHost {
 public:
  SynchronousCompositorControlHost(
      scoped_refptr<SynchronousCompositorSyncCallBridge> bridge,
      int process_id)
      : bridge_(std::move(bridge)), process_id_(process_id) {}

  ~SynchronousCompositorControlHost() override {
    bridge_->RemoteClosedOnIOThread();
  }

  static void Create(
      mojo::PendingReceiver<mojom::SynchronousCompositorControlHost> receiver,
      scoped_refptr<SynchronousCompositorSyncCallBridge> bridge,
      int process_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&CreateOnIOThread, std::move(receiver),
                                  std::move(bridge), process_id));
  }

  static void CreateOnIOThread(
      mojo::PendingReceiver<mojom::SynchronousCompositorControlHost> receiver,
      scoped_refptr<SynchronousCompositorSyncCallBridge> bridge,
      int process_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<SynchronousCompositorControlHost>(std::move(bridge),
                                                           process_id),
        std::move(receiver));
  }

  // SynchronousCompositorControlHost overrides.
  void ReturnFrame(uint32_t layer_tree_frame_sink_id,
                   uint32_t metadata_version,
                   base::Optional<viz::CompositorFrame> frame) override {
    if (!bridge_->ReceiveFrameOnIOThread(layer_tree_frame_sink_id,
                                         metadata_version, std::move(frame))) {
      bad_message::ReceivedBadMessage(
          process_id_, bad_message::SYNC_COMPOSITOR_NO_FUTURE_FRAME);
    }
  }

  void BeginFrameResponse(
      const content::SyncCompositorCommonRendererParams& params) override {
    if (!bridge_->BeginFrameResponseOnIOThread(params)) {
      bad_message::ReceivedBadMessage(
          process_id_, bad_message::SYNC_COMPOSITOR_NO_BEGIN_FRAME);
    }
  }

 private:
  scoped_refptr<SynchronousCompositorSyncCallBridge> bridge_;
  const int process_id_;
};

// static
std::unique_ptr<SynchronousCompositorHost> SynchronousCompositorHost::Create(
    RenderWidgetHostViewAndroid* rwhva,
    const viz::FrameSinkId& frame_sink_id) {
  if (!rwhva->synchronous_compositor_client())
    return nullptr;  // Not using sync compositing.

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool use_in_proc_software_draw =
      command_line->HasSwitch(switches::kSingleProcess);
  return base::WrapUnique(new SynchronousCompositorHost(
      rwhva, frame_sink_id, use_in_proc_software_draw));
}

SynchronousCompositorHost::SynchronousCompositorHost(
    RenderWidgetHostViewAndroid* rwhva,
    const viz::FrameSinkId& frame_sink_id,
    bool use_in_proc_software_draw)
    : rwhva_(rwhva),
      client_(rwhva->synchronous_compositor_client()),
      frame_sink_id_(frame_sink_id),
      use_in_process_zero_copy_software_draw_(use_in_proc_software_draw),
      bytes_limit_(0u),
      renderer_param_version_(0u),
      need_animate_scroll_(false),
      need_invalidate_count_(0u),
      invalidate_needs_draw_(false),
      did_activate_pending_tree_count_(0u) {
  client_->DidInitializeCompositor(this, frame_sink_id_);
  bridge_ = new SynchronousCompositorSyncCallBridge(this);
}

SynchronousCompositorHost::~SynchronousCompositorHost() {
  client_->DidDestroyCompositor(this, frame_sink_id_);
  bridge_->HostDestroyedOnUIThread();
}

void SynchronousCompositorHost::InitMojo() {
  mojo::PendingRemote<mojom::SynchronousCompositorControlHost> host_control;

  SynchronousCompositorControlHost::Create(
      host_control.InitWithNewPipeAndPassReceiver(), bridge_,
      rwhva_->GetRenderWidgetHost()->GetProcess()->GetID());
  rwhva_->host()->GetWidgetInputHandler()->AttachSynchronousCompositor(
      std::move(host_control), host_receiver_.BindNewEndpointAndPassRemote(),
      sync_compositor_.BindNewEndpointAndPassReceiver());
}

bool SynchronousCompositorHost::IsReadyForSynchronousCall() {
  bool res = bridge_->IsRemoteReadyOnUIThread();
  DCHECK(!res || GetSynchronousCompositor());
  return res;
}

scoped_refptr<SynchronousCompositor::FrameFuture>
SynchronousCompositorHost::DemandDrawHwAsync(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  invalidate_needs_draw_ = false;
  scoped_refptr<FrameFuture> frame_future =
      new FrameFuture(rwhva_->GetLocalSurfaceIdAllocation().local_surface_id());
  if (compute_scroll_needs_synchronous_draw_ || !allow_async_draw_) {
    allow_async_draw_ = allow_async_draw_ || IsReadyForSynchronousCall();
    compute_scroll_needs_synchronous_draw_ = false;
    auto frame_ptr = std::make_unique<Frame>();
    *frame_ptr = DemandDrawHw(viewport_size, viewport_rect_for_tile_priority,
                              transform_for_tile_priority);
    frame_future->SetFrame(std::move(frame_ptr));
    return frame_future;
  }

  SyncCompositorDemandDrawHwParams params(viewport_size,
                                          viewport_rect_for_tile_priority,
                                          transform_for_tile_priority);
  mojom::SynchronousCompositor* compositor = GetSynchronousCompositor();
  if (!bridge_->SetFrameFutureOnUIThread(frame_future)) {
    frame_future->SetFrame(nullptr);
  } else {
    DCHECK(compositor);
    compositor->DemandDrawHwAsync(params);
  }
  return frame_future;
}

SynchronousCompositor::Frame SynchronousCompositorHost::DemandDrawHw(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  SyncCompositorDemandDrawHwParams params(viewport_size,
                                          viewport_rect_for_tile_priority,
                                          transform_for_tile_priority);
  uint32_t layer_tree_frame_sink_id;
  uint32_t metadata_version = 0u;
  base::Optional<viz::CompositorFrame> compositor_frame;
  SyncCompositorCommonRendererParams common_renderer_params;

  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
        allow_base_sync_primitives;
    if (!IsReadyForSynchronousCall() ||
        !GetSynchronousCompositor()->DemandDrawHw(
            params, &common_renderer_params, &layer_tree_frame_sink_id,
            &metadata_version, &compositor_frame)) {
      return SynchronousCompositor::Frame();
    }
  }

  UpdateState(common_renderer_params);

  if (!compositor_frame)
    return SynchronousCompositor::Frame();

  SynchronousCompositor::Frame frame;
  frame.frame.reset(new viz::CompositorFrame);
  frame.layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  *frame.frame = std::move(*compositor_frame);
  UpdateFrameMetaData(metadata_version, frame.frame->metadata.Clone());
  return frame;
}

void SynchronousCompositorHost::UpdateFrameMetaData(
    uint32_t version,
    viz::CompositorFrameMetadata frame_metadata) {
  // Ignore if |frame_metadata_version_| is newer than |version|. This
  // comparison takes into account when the unsigned int wraps.
  if ((frame_metadata_version_ - version) < 0x80000000) {
    return;
  }
  frame_metadata_version_ = version;
  UpdatePresentedFrameToken(frame_metadata.frame_token);
}

namespace {

class ScopedSetSkCanvas {
 public:
  explicit ScopedSetSkCanvas(SkCanvas* canvas) {
    SynchronousCompositorSetSkCanvas(canvas);
  }

  ~ScopedSetSkCanvas() {
    SynchronousCompositorSetSkCanvas(nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSetSkCanvas);
};

}

bool SynchronousCompositorHost::DemandDrawSwInProc(SkCanvas* canvas) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
      allow_base_sync_primitives;
  SyncCompositorCommonRendererParams common_renderer_params;
  base::Optional<viz::CompositorFrameMetadata> metadata;
  ScopedSetSkCanvas set_sk_canvas(canvas);
  SyncCompositorDemandDrawSwParams params;  // Unused.
  uint32_t metadata_version = 0u;
  invalidate_needs_draw_ = false;
  if (!IsReadyForSynchronousCall() ||
      !GetSynchronousCompositor()->DemandDrawSw(params, &common_renderer_params,
                                                &metadata_version, &metadata))
    return false;
  if (!metadata)
    return false;
  UpdateState(common_renderer_params);
  UpdateFrameMetaData(metadata_version, std::move(*metadata));
  return true;
}

class SynchronousCompositorHost::ScopedSendZeroMemory {
 public:
  ScopedSendZeroMemory(SynchronousCompositorHost* host) : host_(host) {}
  ~ScopedSendZeroMemory() { host_->SendZeroMemory(); }

 private:
  SynchronousCompositorHost* const host_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSendZeroMemory);
};

struct SynchronousCompositorHost::SharedMemoryWithSize {
  base::WritableSharedMemoryMapping shared_memory;
  const size_t stride;
  const size_t buffer_size;

  SharedMemoryWithSize(size_t stride, size_t buffer_size)
      : stride(stride), buffer_size(buffer_size) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedMemoryWithSize);
};

bool SynchronousCompositorHost::DemandDrawSw(SkCanvas* canvas) {
  if (use_in_process_zero_copy_software_draw_)
    return DemandDrawSwInProc(canvas);

  SyncCompositorDemandDrawSwParams params;
  params.size = gfx::Size(canvas->getBaseLayerSize().width(),
                          canvas->getBaseLayerSize().height());
  SkIRect canvas_clip = canvas->getDeviceClipBounds();
  params.clip = gfx::SkIRectToRect(canvas_clip);
  params.transform.matrix() = canvas->getTotalMatrix();
  if (params.size.IsEmpty())
    return true;

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params.size.width(), params.size.height());
  DCHECK_EQ(kRGBA_8888_SkColorType, info.colorType());
  size_t stride = info.minRowBytes();
  size_t buffer_size = info.computeByteSize(stride);
  if (SkImageInfo::ByteSizeOverflowed(buffer_size))
    return false;

  SetSoftwareDrawSharedMemoryIfNeeded(stride, buffer_size);
  if (!software_draw_shm_)
    return false;

  base::Optional<viz::CompositorFrameMetadata> metadata;
  uint32_t metadata_version = 0u;
  SyncCompositorCommonRendererParams common_renderer_params;
  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
        allow_base_sync_primitives;
    if (!IsReadyForSynchronousCall() ||
        !GetSynchronousCompositor()->DemandDrawSw(
            params, &common_renderer_params, &metadata_version, &metadata)) {
      return false;
    }
  }
  ScopedSendZeroMemory send_zero_memory(this);
  if (!metadata)
    return false;

  UpdateState(common_renderer_params);
  UpdateFrameMetaData(metadata_version, std::move(*metadata));

  SkBitmap bitmap;
  if (!bitmap.installPixels(info, software_draw_shm_->shared_memory.memory(),
                            stride)) {
    return false;
  }

  {
    TRACE_EVENT0("browser", "DrawBitmap");
    canvas->save();
    canvas->resetMatrix();
    canvas->drawBitmap(bitmap, 0, 0);
    canvas->restore();
  }

  return true;
}

void SynchronousCompositorHost::SetSoftwareDrawSharedMemoryIfNeeded(
    size_t stride,
    size_t buffer_size) {
  if (software_draw_shm_ && software_draw_shm_->stride == stride &&
      software_draw_shm_->buffer_size == buffer_size)
    return;
  software_draw_shm_.reset();
  auto software_draw_shm =
      std::make_unique<SharedMemoryWithSize>(stride, buffer_size);
  base::WritableSharedMemoryRegion shm_region;
  {
    TRACE_EVENT1("browser", "AllocateSharedMemory", "buffer_size", buffer_size);
    shm_region = base::WritableSharedMemoryRegion::Create(buffer_size);
    if (!shm_region.IsValid())
      return;

    software_draw_shm->shared_memory = shm_region.Map();
    if (!software_draw_shm->shared_memory.IsValid())
      return;
  }

  bool success = false;
  SyncCompositorCommonRendererParams common_renderer_params;
  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
        allow_base_sync_primitives;
    if (!IsReadyForSynchronousCall() ||
        !GetSynchronousCompositor()->SetSharedMemory(
            std::move(shm_region), &success, &common_renderer_params) ||
        !success) {
      return;
    }
  }
  software_draw_shm_ = std::move(software_draw_shm);
  UpdateState(common_renderer_params);
}

void SynchronousCompositorHost::SendZeroMemory() {
  // No need to check return value.
  if (mojom::SynchronousCompositor* compositor = GetSynchronousCompositor())
    compositor->ZeroSharedMemory();
}

void SynchronousCompositorHost::ReturnResources(
    uint32_t layer_tree_frame_sink_id,
    const std::vector<viz::ReturnedResource>& resources) {
  DCHECK(!resources.empty());
  if (mojom::SynchronousCompositor* compositor = GetSynchronousCompositor())
    compositor->ReclaimResources(layer_tree_frame_sink_id, resources);
}

void SynchronousCompositorHost::DidPresentCompositorFrames(
    viz::FrameTimingDetailsMap timing_details,
    uint32_t frame_token) {
  rwhva_->DidPresentCompositorFrames(timing_details);
  UpdatePresentedFrameToken(frame_token);
}

void SynchronousCompositorHost::UpdatePresentedFrameToken(
    uint32_t frame_token) {
  if (!viz::FrameTokenGT(frame_token, last_frame_token_))
    return;
  last_frame_token_ = frame_token;
  rwhva_->FrameTokenChangedForSynchronousCompositor(frame_token,
                                                    root_scroll_offset_);
}

void SynchronousCompositorHost::SetMemoryPolicy(size_t bytes_limit) {
  if (bytes_limit_ == bytes_limit)
    return;

  bytes_limit_ = bytes_limit;
  if (mojom::SynchronousCompositor* compositor = GetSynchronousCompositor())
    compositor->SetMemoryPolicy(bytes_limit_);
}

void SynchronousCompositorHost::DidChangeRootLayerScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  if (root_scroll_offset_ == root_offset)
    return;
  root_scroll_offset_ = root_offset;
  if (mojom::SynchronousCompositor* compositor = GetSynchronousCompositor())
    compositor->SetScroll(root_scroll_offset_);
}

void SynchronousCompositorHost::SynchronouslyZoomBy(float zoom_delta,
                                                    const gfx::Point& anchor) {
  SyncCompositorCommonRendererParams common_renderer_params;
  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
        allow_base_sync_primitives;
    if (!IsReadyForSynchronousCall() ||
        !GetSynchronousCompositor()->ZoomBy(zoom_delta, anchor,
                                            &common_renderer_params)) {
      return;
    }
  }
  UpdateState(common_renderer_params);
}

void SynchronousCompositorHost::OnComputeScroll(
    base::TimeTicks animation_time) {
  on_compute_scroll_called_ = true;

  if (!need_animate_scroll_)
    return;
  need_animate_scroll_ = false;

  if (mojom::SynchronousCompositor* compositor = GetSynchronousCompositor())
    compositor->ComputeScroll(animation_time);
  compute_scroll_needs_synchronous_draw_ = true;
}

ui::ViewAndroid::CopyViewCallback
SynchronousCompositorHost::GetCopyViewCallback() {
  // Unretained is safe since callback is helped by ViewAndroid which has same
  // lifetime as this, and client outlives this.
  return base::BindRepeating(&SynchronousCompositorClient::CopyOutput,
                             base::Unretained(client_), this);
}

void SynchronousCompositorHost::DidOverscroll(
    const ui::DidOverscrollParams& over_scroll_params) {
  client_->DidOverscroll(this, over_scroll_params.accumulated_overscroll,
                         over_scroll_params.latest_overscroll_delta,
                         over_scroll_params.current_fling_velocity);
}

void SynchronousCompositorHost::BeginFrame(
    ui::WindowAndroid* window_android,
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details) {
  compute_scroll_needs_synchronous_draw_ = false;
  if (!bridge_->WaitAfterVSyncOnUIThread(window_android))
    return;
  mojom::SynchronousCompositor* compositor = GetSynchronousCompositor();
  DCHECK(compositor);
  compositor->BeginFrame(args, timing_details);
}

void SynchronousCompositorHost::SetBeginFramePaused(bool paused) {
  begin_frame_paused_ = paused;
  if (mojom::SynchronousCompositor* compositor = GetSynchronousCompositor())
    compositor->SetBeginFrameSourcePaused(paused);
}

void SynchronousCompositorHost::SetNeedsBeginFrames(bool needs_begin_frames) {
  rwhva_->host()->SetNeedsBeginFrame(needs_begin_frames);
}

void SynchronousCompositorHost::LayerTreeFrameSinkCreated() {
  bridge_->RemoteReady();

  // New LayerTreeFrameSink is not aware of state from Browser side. So need to
  // re-send all browser side state here.
  mojom::SynchronousCompositor* compositor = GetSynchronousCompositor();
  DCHECK(compositor);
  compositor->SetMemoryPolicy(bytes_limit_);

  if (begin_frame_paused_)
    compositor->SetBeginFrameSourcePaused(begin_frame_paused_);
}

void SynchronousCompositorHost::UpdateState(
    const SyncCompositorCommonRendererParams& params) {
  // Ignore if |renderer_param_version_| is newer than |params.version|. This
  // comparison takes into account when the unsigned int wraps.
  if ((renderer_param_version_ - params.version) < 0x80000000) {
    return;
  }
  renderer_param_version_ = params.version;
  need_animate_scroll_ = params.need_animate_scroll;
  root_scroll_offset_ = params.total_scroll_offset;
  max_scroll_offset_ = params.max_scroll_offset;
  scrollable_size_ = params.scrollable_size;
  page_scale_factor_ = params.page_scale_factor;
  min_page_scale_factor_ = params.min_page_scale_factor;
  max_page_scale_factor_ = params.max_page_scale_factor;
  invalidate_needs_draw_ |= params.invalidate_needs_draw;

  if (need_invalidate_count_ != params.need_invalidate_count) {
    need_invalidate_count_ = params.need_invalidate_count;
    if (invalidate_needs_draw_) {
      client_->PostInvalidate(this);
    } else {
      GetSynchronousCompositor()->WillSkipDraw();
    }
  }

  if (did_activate_pending_tree_count_ !=
      params.did_activate_pending_tree_count) {
    did_activate_pending_tree_count_ = params.did_activate_pending_tree_count;
    client_->DidUpdateContent(this);
  }

  UpdateRootLayerStateOnClient();
}

void SynchronousCompositorHost::DidBecomeActive() {
  UpdateRootLayerStateOnClient();
}

void SynchronousCompositorHost::UpdateRootLayerStateOnClient() {
  // Ensure only valid values from compositor are sent to client.
  // Compositor has page_scale_factor set to 0 before initialization, so check
  // for that case here.
  if (page_scale_factor_) {
    client_->UpdateRootLayerState(
        this, gfx::ScrollOffsetToVector2dF(root_scroll_offset_),
        gfx::ScrollOffsetToVector2dF(max_scroll_offset_), scrollable_size_,
        page_scale_factor_, min_page_scale_factor_, max_page_scale_factor_);
  }
}

RenderProcessHost* SynchronousCompositorHost::GetRenderProcessHost() {
  return rwhva_->GetRenderWidgetHost()->GetProcess();
}

mojom::SynchronousCompositor*
SynchronousCompositorHost::GetSynchronousCompositor() {
  if (!sync_compositor_)
    return nullptr;
  return sync_compositor_.get();
}

}  // namespace content
