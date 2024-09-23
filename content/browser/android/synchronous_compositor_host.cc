// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_host.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/traced_value.h"
#include "content/browser/android/synchronous_compositor_sync_call_bridge.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/compositor_dependencies_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/sync_compositor_statics.h"
#include "content/common/features.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace content {

namespace {

bool g_viz_established = false;

// TODO(vasilyt): Create BrowserCompositor for webview and move it there?
void EstablishVizConnection(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  content::CompositorDependenciesAndroid::Get()
      .TryEstablishVizConnectionIfNeeded();
  g_viz_established = true;
}

void EstablishGpuChannelToEstablishVizConnection() {
  if (g_viz_established)
    return;

  content::BrowserMainLoop::GetInstance()
      ->gpu_channel_establish_factory()
      ->EstablishGpuChannel(base::BindOnce(&EstablishVizConnection));
}

}  // namespace

// This class runs on the IO thread and is destroyed when the renderer
// side closes the mojo channel.
class SynchronousCompositorControlHost
    : public blink::mojom::SynchronousCompositorControlHost {
 public:
  SynchronousCompositorControlHost(
      scoped_refptr<SynchronousCompositorSyncCallBridge> bridge,
      int process_id)
      : bridge_(std::move(bridge)), process_id_(process_id) {}

  ~SynchronousCompositorControlHost() override {
    bridge_->RemoteClosedOnIOThread();
  }

  static void Create(
      mojo::PendingReceiver<blink::mojom::SynchronousCompositorControlHost>
          receiver,
      scoped_refptr<SynchronousCompositorSyncCallBridge> bridge,
      int process_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CreateOnIOThread, std::move(receiver),
                                  std::move(bridge), process_id));
  }

  static void CreateOnIOThread(
      mojo::PendingReceiver<blink::mojom::SynchronousCompositorControlHost>
          receiver,
      scoped_refptr<SynchronousCompositorSyncCallBridge> bridge,
      int process_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    auto host_control_receiver = mojo::MakeSelfOwnedReceiver(
        std::make_unique<SynchronousCompositorControlHost>(bridge, process_id),
        std::move(receiver));
    bridge->SetHostControlReceiverOnIOThread(host_control_receiver);
  }

  // SynchronousCompositorControlHost overrides.
  void ReturnFrame(
      uint32_t layer_tree_frame_sink_id,
      uint32_t metadata_version,
      const std::optional<viz::LocalSurfaceId>& local_surface_id,
      std::optional<viz::CompositorFrame> frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list) override {
    if (frame && (!local_surface_id || !local_surface_id->is_valid())) {
      bad_message::ReceivedBadMessage(
          process_id_, bad_message::SYNC_COMPOSITOR_NO_LOCAL_SURFACE_ID);
      return;
    }
    if (!bridge_->ReceiveFrameOnIOThread(
            layer_tree_frame_sink_id, metadata_version, local_surface_id,
            std::move(frame), std::move(hit_test_region_list))) {
      bad_message::ReceivedBadMessage(
          process_id_, bad_message::SYNC_COMPOSITOR_NO_FUTURE_FRAME);
    }
  }

  void BeginFrameResponse(
      blink::mojom::SyncCompositorCommonRendererParamsPtr params) override {
    if (!bridge_->BeginFrameResponseOnIOThread(std::move(params))) {
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
    const viz::FrameSinkId& frame_sink_id,
    viz::HostFrameSinkManager* host_frame_sink_manager) {
  if (!rwhva->synchronous_compositor_client())
    return nullptr;  // Not using sync compositing.

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool use_in_proc_software_draw =
      command_line->HasSwitch(switches::kSingleProcess);
  return base::WrapUnique(new SynchronousCompositorHost(
      rwhva, frame_sink_id, host_frame_sink_manager,
      use_in_proc_software_draw));
}

SynchronousCompositorHost::SynchronousCompositorHost(
    RenderWidgetHostViewAndroid* rwhva,
    const viz::FrameSinkId& frame_sink_id,
    viz::HostFrameSinkManager* host_frame_sink_manager,
    bool use_in_proc_software_draw)
    : rwhva_(rwhva),
      client_(rwhva->synchronous_compositor_client()),
      frame_sink_id_(frame_sink_id),
      host_frame_sink_manager_(host_frame_sink_manager),
      use_in_process_zero_copy_software_draw_(use_in_proc_software_draw),
      bytes_limit_(0u),
      renderer_param_version_(0u),
      need_invalidate_count_(0u),
      invalidate_needs_draw_(false),
      did_activate_pending_tree_count_(0u) {
  EstablishGpuChannelToEstablishVizConnection();
  client_->DidInitializeCompositor(this, frame_sink_id_);
  host_frame_sink_manager_->CreateHitTestQueryForSynchronousCompositor(
      frame_sink_id_);
  bridge_ = new SynchronousCompositorSyncCallBridge(this);
}

SynchronousCompositorHost::~SynchronousCompositorHost() {
  if (outstanding_begin_frame_requests_ && begin_frame_source_)
    begin_frame_source_->RemoveObserver(this);
  host_frame_sink_manager_->EraseHitTestQueryForSynchronousCompositor(
      frame_sink_id_);
  client_->DidDestroyCompositor(this, frame_sink_id_);
  bridge_->HostDestroyedOnUIThread();
}

void SynchronousCompositorHost::InitMojo() {
  mojo::PendingRemote<blink::mojom::SynchronousCompositorControlHost>
      host_control;

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

void SynchronousCompositorHost::OnCompositorVisible() {
  CompositorDependenciesAndroid::Get().OnSynchronousCompositorVisible();
}

void SynchronousCompositorHost::OnCompositorHidden() {
  CompositorDependenciesAndroid::Get().OnSynchronousCompositorHidden();
}

scoped_refptr<SynchronousCompositor::FrameFuture>
SynchronousCompositorHost::DemandDrawHwAsync(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  velocity_in_pixels_per_second_ = 0.f;
  draw_hw_called_ = true;
  invalidate_needs_draw_ = false;
  num_invalidates_since_last_draw_ = 0u;
  scoped_refptr<FrameFuture> frame_future = new FrameFuture();
  if (!allow_async_draw_) {
    allow_async_draw_ = allow_async_draw_ || IsReadyForSynchronousCall();
    auto frame_ptr = std::make_unique<Frame>();
    *frame_ptr = DemandDrawHw(viewport_size, viewport_rect_for_tile_priority,
                              transform_for_tile_priority);
    frame_future->SetFrame(std::move(frame_ptr));
    return frame_future;
  }

  blink::mojom::SyncCompositorDemandDrawHwParamsPtr params =
      blink::mojom::SyncCompositorDemandDrawHwParams::New(
          viewport_size, viewport_rect_for_tile_priority,
          transform_for_tile_priority,
          /*need_new_local_surface_id=*/was_evicted_);

  was_evicted_ = false;

  blink::mojom::SynchronousCompositor* compositor = GetSynchronousCompositor();
  if (!bridge_->SetFrameFutureOnUIThread(frame_future)) {
    frame_future->SetFrame(nullptr);
  } else {
    DCHECK(compositor);
    compositor->DemandDrawHwAsync(std::move(params));
  }
  return frame_future;
}

SynchronousCompositor::Frame SynchronousCompositorHost::DemandDrawHw(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  blink::mojom::SyncCompositorDemandDrawHwParamsPtr params =
      blink::mojom::SyncCompositorDemandDrawHwParams::New(
          viewport_size, viewport_rect_for_tile_priority,
          transform_for_tile_priority,
          /*need_new_local_surface_id=*/was_evicted_);

  was_evicted_ = false;

  uint32_t layer_tree_frame_sink_id;
  uint32_t metadata_version = 0u;
  std::optional<viz::LocalSurfaceId> local_surface_id;
  std::optional<viz::CompositorFrame> compositor_frame;
  std::optional<viz::HitTestRegionList> hit_test_region_list;
  blink::mojom::SyncCompositorCommonRendererParamsPtr common_renderer_params;

  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
        allow_base_sync_primitives;
    if (!IsReadyForSynchronousCall() ||
        !GetSynchronousCompositor()->DemandDrawHw(
            std::move(params), &common_renderer_params,
            &layer_tree_frame_sink_id, &metadata_version, &local_surface_id,
            &compositor_frame, &hit_test_region_list)) {
      return SynchronousCompositor::Frame();
    }
  }

  UpdateState(std::move(common_renderer_params));

  if (compositor_frame) {
    if (!local_surface_id || !local_surface_id->is_valid()) {
      bad_message::ReceivedBadMessage(
          rwhva_->GetRenderWidgetHost()->GetProcess()->GetID(),
          bad_message::SYNC_COMPOSITOR_NO_LOCAL_SURFACE_ID);
      return SynchronousCompositor::Frame();
    }
  } else {
    return SynchronousCompositor::Frame();
  }

  SynchronousCompositor::Frame frame;
  frame.frame = std::make_unique<viz::CompositorFrame>();
  frame.layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  if (local_surface_id)
    frame.local_surface_id = local_surface_id.value();
  *frame.frame = std::move(*compositor_frame);
  frame.hit_test_region_list = std::move(hit_test_region_list);
  UpdateFrameMetaData(metadata_version, frame.frame->metadata.Clone(),
                      std::move(local_surface_id));
  return frame;
}

void SynchronousCompositorHost::UpdateFrameMetaData(
    uint32_t version,
    viz::CompositorFrameMetadata frame_metadata,
    std::optional<viz::LocalSurfaceId> new_local_surface_id) {
  // Ignore if |frame_metadata_version_| is newer than |version|. This
  // comparison takes into account when the unsigned int wraps.
  if ((frame_metadata_version_ - version) < 0x80000000) {
    return;
  }
  frame_metadata_version_ = version;
  if (new_local_surface_id)
    local_surface_id_ = new_local_surface_id.value();
}

namespace {

class ScopedSetSkCanvas {
 public:
  explicit ScopedSetSkCanvas(SkCanvas* canvas) {
    SynchronousCompositorSetSkCanvas(canvas);
  }

  ScopedSetSkCanvas(const ScopedSetSkCanvas&) = delete;
  ScopedSetSkCanvas& operator=(const ScopedSetSkCanvas&) = delete;

  ~ScopedSetSkCanvas() {
    SynchronousCompositorSetSkCanvas(nullptr);
  }
};

}

bool SynchronousCompositorHost::DemandDrawSwInProc(SkCanvas* canvas) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
      allow_base_sync_primitives;
  blink::mojom::SyncCompositorCommonRendererParamsPtr common_renderer_params;
  std::optional<viz::CompositorFrameMetadata> metadata;
  ScopedSetSkCanvas set_sk_canvas(canvas);
  blink::mojom::SyncCompositorDemandDrawSwParamsPtr params =
      blink::mojom::SyncCompositorDemandDrawSwParams::New();  // Unused.
  uint32_t metadata_version = 0u;
  invalidate_needs_draw_ = false;
  if (!IsReadyForSynchronousCall() ||
      !GetSynchronousCompositor()->DemandDrawSw(std::move(params),
                                                &common_renderer_params,
                                                &metadata_version, &metadata))
    return false;
  if (!metadata)
    return false;
  UpdateState(std::move(common_renderer_params));
  UpdateFrameMetaData(metadata_version, std::move(*metadata), std::nullopt);
  return true;
}

class SynchronousCompositorHost::ScopedSendZeroMemory {
 public:
  ScopedSendZeroMemory(SynchronousCompositorHost* host) : host_(host) {}

  ScopedSendZeroMemory(const ScopedSendZeroMemory&) = delete;
  ScopedSendZeroMemory& operator=(const ScopedSendZeroMemory&) = delete;

  ~ScopedSendZeroMemory() { host_->SendZeroMemory(); }

 private:
  const raw_ptr<SynchronousCompositorHost> host_;
};

struct SynchronousCompositorHost::SharedMemoryWithSize {
  base::WritableSharedMemoryMapping shared_memory;
  const size_t stride;
  const size_t buffer_size;

  SharedMemoryWithSize(size_t stride, size_t buffer_size)
      : stride(stride), buffer_size(buffer_size) {}

  SharedMemoryWithSize(const SharedMemoryWithSize&) = delete;
  SharedMemoryWithSize& operator=(const SharedMemoryWithSize&) = delete;
};

bool SynchronousCompositorHost::DemandDrawSw(SkCanvas* canvas,
                                             bool software_canvas) {
  velocity_in_pixels_per_second_ = 0.f;
  num_invalidates_since_last_draw_ = 0u;
  if (use_in_process_zero_copy_software_draw_)
    return DemandDrawSwInProc(canvas);

  blink::mojom::SyncCompositorDemandDrawSwParamsPtr params =
      blink::mojom::SyncCompositorDemandDrawSwParams::New();
  params->size = gfx::Size(canvas->getBaseLayerSize().width(),
                           canvas->getBaseLayerSize().height());
  SkIRect canvas_clip = canvas->getDeviceClipBounds();
  params->clip = gfx::SkIRectToRect(canvas_clip);
  params->transform = gfx::SkMatrixToTransform(canvas->getTotalMatrix());
  if (params->size.IsEmpty())
    return true;

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params->size.width(), params->size.height());
  DCHECK_EQ(kRGBA_8888_SkColorType, info.colorType());
  size_t stride = info.minRowBytes();
  size_t buffer_size = info.computeByteSize(stride);
  if (SkImageInfo::ByteSizeOverflowed(buffer_size))
    return false;

  SetSoftwareDrawSharedMemoryIfNeeded(stride, buffer_size);
  if (!software_draw_shm_)
    return false;

  std::optional<viz::CompositorFrameMetadata> metadata;
  uint32_t metadata_version = 0u;
  blink::mojom::SyncCompositorCommonRendererParamsPtr common_renderer_params;
  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
        allow_base_sync_primitives;
    if (!IsReadyForSynchronousCall() ||
        !GetSynchronousCompositor()->DemandDrawSw(
            std::move(params), &common_renderer_params, &metadata_version,
            &metadata)) {
      return false;
    }
  }
  ScopedSendZeroMemory send_zero_memory(this);
  if (!metadata)
    return false;

  UpdateState(std::move(common_renderer_params));
  UpdateFrameMetaData(metadata_version, std::move(*metadata), std::nullopt);

  SkBitmap bitmap;
  base::span<uint8_t> mem(software_draw_shm_->shared_memory);
  CHECK_GE(mem.size(), info.computeByteSize(stride));
  SkPixmap pixmap(info, mem.data(), stride);

  bool pixels_released = false;
  {
    TRACE_EVENT0("browser", "DrawBitmap");
    canvas->save();
    canvas->resetMatrix();
    sk_sp<SkImage> image;
    // Software canvas will draw immediately, so it's safe to avoid this copy.
    if (software_canvas) {
      auto mark_bool = [](const void* pixels, void* context) {
        *static_cast<bool*>(context) = true;
      };
      image = SkImages::RasterFromPixmap(pixmap, mark_bool, &pixels_released);
    } else {
      image = SkImages::RasterFromPixmapCopy(pixmap);
    }
    canvas->drawImage(image, 0, 0);
    canvas->restore();
  }

  if (software_canvas) {
    // It could lead to UAF if this CHECK fails, hence it's a release CHECK.
    CHECK(pixels_released);
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
  blink::mojom::SyncCompositorCommonRendererParamsPtr common_renderer_params;
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
  UpdateState(std::move(common_renderer_params));
}

void SynchronousCompositorHost::SendZeroMemory() {
  // No need to check return value.
  if (blink::mojom::SynchronousCompositor* compositor =
          GetSynchronousCompositor())
    compositor->ZeroSharedMemory();
}

void SynchronousCompositorHost::ReturnResources(
    uint32_t layer_tree_frame_sink_id,
    std::vector<viz::ReturnedResource> resources) {
  DCHECK(!resources.empty());
  if (blink::mojom::SynchronousCompositor* compositor =
          GetSynchronousCompositor())
    compositor->ReclaimResources(layer_tree_frame_sink_id,
                                 std::move(resources));
}

void SynchronousCompositorHost::OnCompositorFrameTransitionDirectiveProcessed(
    uint32_t layer_tree_frame_sink_id,
    uint32_t sequence_id) {
  if (blink::mojom::SynchronousCompositor* compositor =
          GetSynchronousCompositor()) {
    compositor->OnCompositorFrameTransitionDirectiveProcessed(
        layer_tree_frame_sink_id, sequence_id);
  }
}

void SynchronousCompositorHost::DidPresentCompositorFrames(
    viz::FrameTimingDetailsMap timing_details,
    uint32_t frame_token) {
  timing_details_.insert(timing_details.begin(), timing_details.end());
  if (!timing_details_.empty())
    AddBeginFrameRequest(BEGIN_FRAME);
}

void SynchronousCompositorHost::SetMemoryPolicy(size_t bytes_limit) {
  if (bytes_limit_ == bytes_limit)
    return;

  bytes_limit_ = bytes_limit;
  if (blink::mojom::SynchronousCompositor* compositor =
          GetSynchronousCompositor())
    compositor->SetMemoryPolicy(bytes_limit_);
}

float SynchronousCompositorHost::GetVelocityInPixelsPerSecond() {
  return velocity_in_pixels_per_second_;
}

void SynchronousCompositorHost::DidChangeRootLayerScrollOffset(
    const gfx::PointF& root_offset) {
  if (root_scroll_offset_ == root_offset)
    return;
  root_scroll_offset_ = root_offset;
  if (blink::mojom::SynchronousCompositor* compositor =
          GetSynchronousCompositor())
    compositor->SetScroll(root_scroll_offset_);
}

void SynchronousCompositorHost::SynchronouslyZoomBy(float zoom_delta,
                                                    const gfx::Point& anchor) {
  blink::mojom::SyncCompositorCommonRendererParamsPtr common_renderer_params;
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
  UpdateState(std::move(common_renderer_params));
}

void SynchronousCompositorHost::OnComputeScroll(
    base::TimeTicks animation_time) {
  on_compute_scroll_called_ = true;
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

void SynchronousCompositorHost::SetNeedsBeginFrames(bool needs_begin_frames) {
  TRACE_EVENT1("cc", "SynchronousCompositorHost::SetNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  if (needs_begin_frames)
    AddBeginFrameRequest(PERSISTENT_BEGIN_FRAME);
  else
    ClearBeginFrameRequest(PERSISTENT_BEGIN_FRAME);
}

void SynchronousCompositorHost::SetThreadIds(
    const std::vector<int32_t>& thread_ids) {
  client_->SetThreadIds(thread_ids);
}

void SynchronousCompositorHost::LayerTreeFrameSinkCreated() {
  bridge_->RemoteReady();

  // New LayerTreeFrameSink is not aware of state from Browser side. So need to
  // re-send all browser side state here.
  blink::mojom::SynchronousCompositor* compositor = GetSynchronousCompositor();
  DCHECK(compositor);
  compositor->SetMemoryPolicy(bytes_limit_);

  SendBeginFramePaused();
}

void SynchronousCompositorHost::UpdateState(
    blink::mojom::SyncCompositorCommonRendererParamsPtr params) {
  // Ignore if |renderer_param_version_| is newer than |params.version|. This
  // comparison takes into account when the unsigned int wraps.
  if ((renderer_param_version_ - params->version) < 0x80000000) {
    return;
  }
  renderer_param_version_ = params->version;
  root_scroll_offset_ = params->total_scroll_offset;
  max_scroll_offset_ = params->max_scroll_offset;
  scrollable_size_ = params->scrollable_size;
  page_scale_factor_ = params->page_scale_factor;
  min_page_scale_factor_ = params->min_page_scale_factor;
  max_page_scale_factor_ = params->max_page_scale_factor;
  invalidate_needs_draw_ |= params->invalidate_needs_draw;

  if (need_invalidate_count_ != params->need_invalidate_count) {
    need_invalidate_count_ = params->need_invalidate_count;
    if (invalidate_needs_draw_) {
      num_invalidates_since_last_draw_++;
      client_->PostInvalidate(this);
    } else {
      GetSynchronousCompositor()->WillSkipDraw();
    }
  }

  if (did_activate_pending_tree_count_ !=
      params->did_activate_pending_tree_count) {
    did_activate_pending_tree_count_ = params->did_activate_pending_tree_count;
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
        this, root_scroll_offset_, max_scroll_offset_, scrollable_size_,
        page_scale_factor_, min_page_scale_factor_, max_page_scale_factor_);
  }
}

RenderProcessHost* SynchronousCompositorHost::GetRenderProcessHost() {
  return rwhva_->GetRenderWidgetHost()->GetProcess();
}

blink::mojom::SynchronousCompositor*
SynchronousCompositorHost::GetSynchronousCompositor() {
  if (!sync_compositor_)
    return nullptr;
  return sync_compositor_.get();
}

void SynchronousCompositorHost::OnBeginFrame(const viz::BeginFrameArgs& args) {
  TRACE_EVENT0("cc,benchmark", "SynchronousCompositorHost::OnBeginFrame");

  // In sync mode, we disregard missed frame args to ensure that
  // SynchronousCompositorBrowserFilter::SyncStateAfterVSync will be called
  // during WindowAndroid::WindowBeginFrameSource::OnVSync() observer iteration.
  if (args.type == viz::BeginFrameArgs::MISSED)
    return;

  // We need to check this before |outstanding_begin_frame_requests_| will be
  // changed by ClearBeginFrameRequest below.
  bool needs_begin_frame =
      (outstanding_begin_frame_requests_ & BEGIN_FRAME) ||
      (outstanding_begin_frame_requests_ & PERSISTENT_BEGIN_FRAME);

  last_begin_frame_time_delta_ =
      args.frame_time - last_begin_frame_args_.frame_time;

  // Update |last_begin_frame_args_| before handling
  // |outstanding_begin_frame_requests_| to prevent the BeginFrameSource from
  // sending the same MISSED args in infinite recursion.
  last_begin_frame_args_ = args;

  ClearBeginFrameRequest(BEGIN_FRAME);

  if (on_compute_scroll_called_ ||
      !rwhva_->GetViewRenderInputRouter()->is_currently_scrolling_viewport()) {
    rwhva_->host()->ProgressFlingIfNeeded(args.frame_time);
  } else if (base::FeatureList::IsEnabled(
                 features::kWebViewSuppressTapDuringFling)) {
    // Normally, `OnComputeScroll` is called after `OnBeginFrame`, but before
    // `DemandDrawHwAsync`. So `OnBeginFrame` calls before the first draw will
    // end up here regardless of whether `OnComputeScroll` will be called. If
    // these frames contain fling, then don't cancel fling prematurely. Note
    // normally fling cannot happen from user interaction this way because touch
    // scroll happens before fling.
    if (draw_hw_called_) {
      // If we are not ticking flings ourselves, also reset the tracking state
      // for fling so the first tap during / after fling is not suppressed.
      rwhva_->host()->StopFling();
    }
  }

  if (needs_begin_frame) {
    SendBeginFrame(args);
  }
}

const viz::BeginFrameArgs& SynchronousCompositorHost::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void SynchronousCompositorHost::OnBeginFrameSourcePausedChanged(bool paused) {
  if (paused != begin_frame_paused_) {
    begin_frame_paused_ = paused;
    SendBeginFramePaused();
  }
}

bool SynchronousCompositorHost::WantsAnimateOnlyBeginFrames() const {
  return false;
}

void SynchronousCompositorHost::SendBeginFramePaused() {
  if (blink::mojom::SynchronousCompositor* compositor =
          GetSynchronousCompositor())
    compositor->SetBeginFrameSourcePaused(begin_frame_paused_);
}

void SynchronousCompositorHost::SendBeginFrame(viz::BeginFrameArgs args) {
  if (num_invalidates_since_last_draw_ > 5u) {
    // Throttle begin frames if there has been no draws in response to
    // invalidates. This can happen if webview is detached or offscreen. There
    // are cases where renderer is still expected to make progress. In this
    // case renderer receives no back pressure so reduce the frequency of begin
    // frames to avoid unnecessary work.
    if (num_begin_frames_to_skip_) {
      TRACE_EVENT_INSTANT0("cc",
                           "SynchronousCompositorHost::SendBeginFrame_skipped",
                           TRACE_EVENT_SCOPE_THREAD);
      num_begin_frames_to_skip_--;
      return;
    } else {
      num_begin_frames_to_skip_ =
          std::min(num_invalidates_since_last_draw_ / 5, 250u);
    }
  } else {
    num_begin_frames_to_skip_ = 0u;
  }

  TRACE_EVENT2("cc", "SynchronousCompositorHost::SendBeginFrame",
               "frame_number", args.frame_id.sequence_number, "frame_time_us",
               args.frame_time);

  if (!bridge_->WaitAfterVSyncOnUIThread())
    return;

  blink::mojom::SynchronousCompositor* compositor = GetSynchronousCompositor();
  DCHECK(compositor);
  compositor->BeginFrame(args, timing_details_);
  timing_details_.clear();
}

void SynchronousCompositorHost::BeginFrameComplete(
    blink::mojom::SyncCompositorCommonRendererParamsPtr params) {
  velocity_in_pixels_per_second_ = 0.f;
  gfx::PointF offset = root_scroll_offset_;
  if (params) {
    UpdateState(std::move(params));
  }
  // Sanity check frame time delta.
  if (last_begin_frame_time_delta_.InMicroseconds() < 100 ||
      last_begin_frame_time_delta_.InMicroseconds() > 1000000) {
    return;
  }
  gfx::Vector2dF scroll = root_scroll_offset_ - offset;
  float major_scroll_in_last_begin_frame =
      std::abs(scroll.x()) > std::abs(scroll.y()) ? scroll.x() : scroll.y();
  velocity_in_pixels_per_second_ = major_scroll_in_last_begin_frame /
                                   last_begin_frame_time_delta_.InSecondsF();
  TRACE_EVENT_INSTANT("cc", "SynchronousCompositorHost::BeginFrameComplete",
                      "scroll", major_scroll_in_last_begin_frame, "delta",
                      last_begin_frame_time_delta_.InMicroseconds());
}

void SynchronousCompositorHost::SetBeginFrameSource(
    viz::BeginFrameSource* begin_frame_source) {
  DCHECK(!begin_frame_source_);
  DCHECK(!outstanding_begin_frame_requests_);
  begin_frame_source_ = begin_frame_source;
}

void SynchronousCompositorHost::AddBeginFrameRequest(
    BeginFrameRequestType request) {
  uint32_t prior_requests = outstanding_begin_frame_requests_;
  outstanding_begin_frame_requests_ = prior_requests | request;

  // Note that if we don't currently have a BeginFrameSource, outstanding begin
  // frame requests will be pushed if/when we get one during
  // |StartObservingRootWindow()| or when the DelegatedFrameHostAndroid sets it.
  viz::BeginFrameSource* source = begin_frame_source_;
  if (source && outstanding_begin_frame_requests_ && !prior_requests)
    source->AddObserver(this);
}

void SynchronousCompositorHost::ClearBeginFrameRequest(
    BeginFrameRequestType request) {
  uint32_t prior_requests = outstanding_begin_frame_requests_;
  outstanding_begin_frame_requests_ = prior_requests & ~request;

  viz::BeginFrameSource* source = begin_frame_source_;
  if (source && !outstanding_begin_frame_requests_ && prior_requests)
    source->RemoveObserver(this);
}

void SynchronousCompositorHost::RequestOneBeginFrame() {
  AddBeginFrameRequest(BEGIN_FRAME);
}

void SynchronousCompositorHost::AddBeginFrameCompletionCallback(
    base::OnceClosure callback) {
  client_->AddBeginFrameCompletionCallback(std::move(callback));
}

viz::SurfaceId SynchronousCompositorHost::GetSurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, local_surface_id_);
}

void SynchronousCompositorHost::DidInvalidate() {
  invalidate_needs_draw_ = true;
}

void SynchronousCompositorHost::WasEvicted() {
  was_evicted_ = true;
}

}  // namespace content
