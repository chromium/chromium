// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_layer_tree_frame_sink.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display/texture_deleter.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/common/view_messages.h"
#include "content/renderer/frame_swap_message_queue.h"
#include "content/renderer/input/synchronous_compositor_registry.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace content {

namespace {

const int64_t kFallbackTickTimeoutInMilliseconds = 100;
const viz::FrameSinkId kRootFrameSinkId(1, 1);
const viz::FrameSinkId kChildFrameSinkId(1, 2);

// Do not limit number of resources, so use an unrealistically high value.
const size_t kNumResourcesLimit = 10 * 1000 * 1000;

class SoftwareDevice : public viz::SoftwareOutputDevice {
 public:
  SoftwareDevice(SkCanvas** canvas) : canvas_(canvas) {}

  void Resize(const gfx::Size& pixel_size, float device_scale_factor) override {
    // Intentional no-op: canvas size is controlled by the embedder.
  }
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override {
    DCHECK(*canvas_) << "BeginPaint with no canvas set";
    return *canvas_;
  }
  void EndPaint() override {}

 private:
  SkCanvas** canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareDevice);
};

// This is used with resourceless software draws.
class SoftwareCompositorFrameSinkClient
    : public viz::mojom::CompositorFrameSinkClient {
 public:
  SoftwareCompositorFrameSinkClient() = default;
  ~SoftwareCompositorFrameSinkClient() override = default;

  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override {
    DCHECK(resources.empty());
  }
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details) override {
  }
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override {
    DCHECK(resources.empty());
  }
  void OnBeginFramePausedChanged(bool paused) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SoftwareCompositorFrameSinkClient);
};

}  // namespace

class SynchronousLayerTreeFrameSink::SoftwareOutputSurface
    : public viz::OutputSurface {
 public:
  SoftwareOutputSurface(std::unique_ptr<SoftwareDevice> software_device)
      : viz::OutputSurface(std::move(software_device)) {}

  // viz::OutputSurface implementation.
  void BindToClient(viz::OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {}
  void SetDrawRectangle(const gfx::Rect& rect) override {}
  void SwapBuffers(viz::OutputSurfaceFrame frame) override {}
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override {}
  uint32_t GetFramebufferCopyTextureFormat() override { return 0; }
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  gfx::BufferFormat GetOverlayBufferFormat() const override {
    return gfx::BufferFormat::RGBX_8888;
  }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}
  unsigned UpdateGpuFence() override { return 0; }
  void SetUpdateVSyncParametersCallback(
      viz::UpdateVSyncParametersCallback callback) override {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override {
    return gfx::OVERLAY_TRANSFORM_NONE;
  }
};

base::TimeDelta SynchronousLayerTreeFrameSink::StubDisplayClient::
    GetPreferredFrameIntervalForFrameSinkId(const viz::FrameSinkId& id) {
  return viz::BeginFrameArgs::MinInterval();
}

SynchronousLayerTreeFrameSink::SynchronousLayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    IPC::Sender* sender,
    int routing_id,
    uint32_t layer_tree_frame_sink_id,
    std::unique_ptr<viz::BeginFrameSource> synthetic_begin_frame_source,
    SynchronousCompositorRegistry* registry,
    scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue)
    : cc::LayerTreeFrameSink(std::move(context_provider),
                             std::move(worker_context_provider),
                             std::move(compositor_task_runner),
                             gpu_memory_buffer_manager),
      routing_id_(routing_id),
      layer_tree_frame_sink_id_(layer_tree_frame_sink_id),
      registry_(registry),
      sender_(sender),
      memory_policy_(0u),
      frame_swap_message_queue_(frame_swap_message_queue),
      synthetic_begin_frame_source_(std::move(synthetic_begin_frame_source)) {
  DCHECK(registry_);
  DCHECK(sender_);
  if (!synthetic_begin_frame_source_) {
    external_begin_frame_source_ =
        std::make_unique<viz::ExternalBeginFrameSource>(this);
  }
  thread_checker_.DetachFromThread();
  memory_policy_.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
}

SynchronousLayerTreeFrameSink::~SynchronousLayerTreeFrameSink() = default;

void SynchronousLayerTreeFrameSink::SetSyncClient(
    SynchronousLayerTreeFrameSinkClient* compositor) {
  sync_client_ = compositor;
}

bool SynchronousLayerTreeFrameSink::BindToClient(
    cc::LayerTreeFrameSinkClient* sink_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::LayerTreeFrameSink::BindToClient(sink_client))
    return false;

  // The SharedBitmapManager is null since software compositing is not supported
  // or used on Android.
  frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
      /*shared_bitmap_manager=*/nullptr);

  client_->SetBeginFrameSource(synthetic_begin_frame_source_
                                   ? synthetic_begin_frame_source_.get()
                                   : external_begin_frame_source_.get());
  client_->SetMemoryPolicy(memory_policy_);
  client_->SetTreeActivationCallback(base::BindRepeating(
      &SynchronousLayerTreeFrameSink::DidActivatePendingTree,
      base::Unretained(this)));
  registry_->RegisterLayerTreeFrameSink(routing_id_, this);

  software_frame_sink_client_ =
      std::make_unique<SoftwareCompositorFrameSinkClient>();
  constexpr bool root_support_is_root = true;
  constexpr bool child_support_is_root = false;
  constexpr bool needs_sync_points = true;
  root_support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      software_frame_sink_client_.get(), frame_sink_manager_.get(),
      kRootFrameSinkId, root_support_is_root, needs_sync_points);
  child_support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      software_frame_sink_client_.get(), frame_sink_manager_.get(),
      kChildFrameSinkId, child_support_is_root, needs_sync_points);

  viz::RendererSettings software_renderer_settings;

  auto output_surface = std::make_unique<SoftwareOutputSurface>(
      std::make_unique<SoftwareDevice>(&current_sw_canvas_));
  software_output_surface_ = output_surface.get();

  // The gpu_memory_buffer_manager here is null as the Display is only used for
  // resourcesless software draws, where no resources are included in the frame
  // swapped from the compositor. So there is no need for it.
  // The shared_bitmap_manager_ is provided for the Display to allocate
  // resources.
  // TODO(crbug.com/692814): The Display never sends its resources out of
  // process so there is no reason for it to use a SharedBitmapManager.
  display_ = std::make_unique<viz::Display>(
      &shared_bitmap_manager_, software_renderer_settings, kRootFrameSinkId,
      std::move(output_surface), nullptr /* scheduler */,
      nullptr /* current_task_runner */);
  display_->Initialize(&display_client_,
                       frame_sink_manager_->surface_manager());
  display_->SetVisible(true);
  return true;
}

void SynchronousLayerTreeFrameSink::DetachFromClient() {
  DCHECK(CalledOnValidThread());
  client_->SetBeginFrameSource(nullptr);
  // Destroy the begin frame source on the same thread it was bound on.
  synthetic_begin_frame_source_ = nullptr;
  external_begin_frame_source_ = nullptr;
  if (sync_client_)
    sync_client_->SinkDestroyed();
  registry_->UnregisterLayerTreeFrameSink(routing_id_, this);
  client_->SetTreeActivationCallback(base::RepeatingClosure());
  root_support_.reset();
  child_support_.reset();
  software_frame_sink_client_ = nullptr;
  software_output_surface_ = nullptr;
  display_ = nullptr;
  frame_sink_manager_ = nullptr;
  cc::LayerTreeFrameSink::DetachFromClient();
  CancelFallbackTick();
}

void SynchronousLayerTreeFrameSink::SubmitCompositorFrame(
    viz::CompositorFrame frame,
    bool hit_test_data_changed,
    bool show_hit_test_borders) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_client_);

  if (fallback_tick_running_) {
    DCHECK(frame.resource_list.empty());
    did_submit_frame_ = true;
    return;
  }

  viz::CompositorFrame submit_frame;
  gfx::Size child_size = in_software_draw_
                             ? sw_viewport_for_current_draw_.size()
                             : frame.size_in_pixels();
  if (!child_local_surface_id_allocation_.IsValid() ||
      child_size_ != child_size ||
      device_scale_factor_ != frame.metadata.device_scale_factor) {
    child_local_surface_id_allocator_.GenerateId();
    child_local_surface_id_allocation_ =
        child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
    child_size_ = child_size;
    device_scale_factor_ = frame.metadata.device_scale_factor;
  }

  if (in_software_draw_) {
    // The frame we send to the client is actually just the metadata. Preserve
    // the |frame| for the software path below.
    submit_frame.metadata = frame.metadata.Clone();

    // The layer compositor should be giving a frame that covers the
    // |sw_viewport_for_current_draw_| but at 0,0.
    DCHECK(gfx::Rect(child_size) == frame.render_pass_list.back()->output_rect);

    // Make a size that covers from 0,0 and includes the area coming from the
    // layer compositor.
    gfx::Size display_size(sw_viewport_for_current_draw_.right(),
                           sw_viewport_for_current_draw_.bottom());
    display_->Resize(display_size);

    if (!root_local_surface_id_allocation_.IsValid() ||
        display_size_ != display_size ||
        device_scale_factor_ != frame.metadata.device_scale_factor) {
      root_local_surface_id_allocator_.GenerateId();
      root_local_surface_id_allocation_ =
          root_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
      display_size_ = display_size;
      device_scale_factor_ = frame.metadata.device_scale_factor;
    }

    display_->SetLocalSurfaceId(
        root_local_surface_id_allocation_.local_surface_id(),
        frame.metadata.device_scale_factor);

    // The offset for the child frame relative to the origin of the canvas being
    // drawn into.
    gfx::Transform child_transform;
    child_transform.Translate(
        gfx::Vector2dF(sw_viewport_for_current_draw_.OffsetFromOrigin()));

    // Make a root frame that embeds the frame coming from the layer compositor
    // and positions it based on the provided viewport.
    // TODO(danakj): We could apply the transform here instead of passing it to
    // the LayerTreeFrameSink client too? (We'd have to do the same for
    // hardware frames in SurfacesInstance?)
    viz::CompositorFrame embed_frame;
    embed_frame.metadata.begin_frame_ack = frame.metadata.begin_frame_ack;
    embed_frame.metadata.device_scale_factor =
        frame.metadata.device_scale_factor;
    embed_frame.render_pass_list.push_back(viz::RenderPass::Create());

    // The embedding RenderPass covers the entire Display's area.
    const auto& embed_render_pass = embed_frame.render_pass_list.back();
    embed_render_pass->SetNew(1, gfx::Rect(display_size),
                              gfx::Rect(display_size), gfx::Transform());
    embed_render_pass->has_transparent_background = false;

    // The RenderPass has a single SurfaceDrawQuad (and SharedQuadState for it).
    bool are_contents_opaque =
        !frame.render_pass_list.back()->has_transparent_background;
    auto* shared_quad_state =
        embed_render_pass->CreateAndAppendSharedQuadState();
    auto* surface_quad =
        embed_render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
    shared_quad_state->SetAll(
        child_transform, gfx::Rect(child_size), gfx::Rect(child_size),
        gfx::RRectF() /* rounded_corner_bounds */, gfx::Rect() /* clip_rect */,
        false /* is_clipped */, are_contents_opaque /* are_contents_opaque */,
        1.f /* opacity */, SkBlendMode::kSrcOver, 0 /* sorting_context_id */);
    surface_quad->SetNew(
        shared_quad_state, gfx::Rect(child_size), gfx::Rect(child_size),
        viz::SurfaceRange(
            base::nullopt,
            viz::SurfaceId(
                kChildFrameSinkId,
                child_local_surface_id_allocation_.local_surface_id())),
        SK_ColorWHITE, false /* stretch_content_to_fill_bounds */,
        false /* ignores_input_event */);

    child_support_->SubmitCompositorFrame(
        child_local_surface_id_allocation_.local_surface_id(),
        std::move(frame));
    root_support_->SubmitCompositorFrame(
        root_local_surface_id_allocation_.local_surface_id(),
        std::move(embed_frame));
    display_->DrawAndSwap();

    // We don't track metrics for frames submitted to |display_| but it still
    // expects that every frame will receive a swap ack and presentation
    // feedback so we send null signals here.
    display_->DidReceiveSwapBuffersAck(gfx::SwapTimings());
    display_->DidReceivePresentationFeedback(
        gfx::PresentationFeedback::Failure());
  } else {
    // For hardware draws we send the whole frame to the client so it can draw
    // the content in it.
    submit_frame = std::move(frame);
  }
  submit_frame.metadata.local_surface_id_allocation_time =
      child_local_surface_id_allocation_.allocation_time();

  sync_client_->SubmitCompositorFrame(layer_tree_frame_sink_id_,
                                      std::move(submit_frame));
  did_submit_frame_ = true;
}

void SynchronousLayerTreeFrameSink::DidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
}

void SynchronousLayerTreeFrameSink::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const viz::SharedBitmapId& id) {
  // Webview does not use software compositing (other than resourceless draws,
  // but this is called for software /resources/).
  NOTREACHED();
}

void SynchronousLayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id) {
  // Webview does not use software compositing (other than resourceless draws,
  // but this is called for software /resources/).
  NOTREACHED();
}

void SynchronousLayerTreeFrameSink::CancelFallbackTick() {
  fallback_tick_.Cancel();
  fallback_tick_pending_ = false;
}

void SynchronousLayerTreeFrameSink::FallbackTickFired() {
  DCHECK(CalledOnValidThread());
  TRACE_EVENT0("renderer", "SynchronousLayerTreeFrameSink::FallbackTickFired");
  base::AutoReset<bool> in_fallback_tick(&fallback_tick_running_, true);
  frame_swap_message_queue_->NotifyFramesAreDiscarded(true);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(0);
  SkCanvas canvas(bitmap);
  fallback_tick_pending_ = false;
  DemandDrawSw(&canvas);
  frame_swap_message_queue_->NotifyFramesAreDiscarded(false);
}

void SynchronousLayerTreeFrameSink::Invalidate(bool needs_draw) {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->Invalidate(needs_draw);

  if (!fallback_tick_pending_) {
    fallback_tick_.Reset(
        base::BindOnce(&SynchronousLayerTreeFrameSink::FallbackTickFired,
                       base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, fallback_tick_.callback(),
        base::TimeDelta::FromMilliseconds(kFallbackTickTimeoutInMilliseconds));
    fallback_tick_pending_ = true;
  }
}

void SynchronousLayerTreeFrameSink::DemandDrawHw(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(context_provider_.get());
  CancelFallbackTick();

  client_->SetExternalTilePriorityConstraints(viewport_rect_for_tile_priority,
                                              transform_for_tile_priority);
  InvokeComposite(gfx::Transform(), gfx::Rect(viewport_size));
}

void SynchronousLayerTreeFrameSink::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);
  CancelFallbackTick();

  base::AutoReset<SkCanvas*> canvas_resetter(&current_sw_canvas_, canvas);

  SkIRect canvas_clip = canvas->getDeviceClipBounds();
  gfx::Rect viewport = gfx::SkIRectToRect(canvas_clip);

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = canvas->getTotalMatrix();  // Converts 3x3 matrix to 4x4.

  // We will resize the Display to ensure it covers the entire |viewport|, so
  // save it for later.
  sw_viewport_for_current_draw_ = viewport;

  base::AutoReset<bool> set_in_software_draw(&in_software_draw_, true);
  InvokeComposite(transform, viewport);
}

void SynchronousLayerTreeFrameSink::WillSkipDraw() {
  CancelFallbackTick();
  client_->OnDraw(gfx::Transform(), gfx::Rect(), in_software_draw_,
                  true /*skip_draw*/);
}

void SynchronousLayerTreeFrameSink::InvokeComposite(
    const gfx::Transform& transform,
    const gfx::Rect& viewport) {
  did_submit_frame_ = false;
  // Adjust transform so that the layer compositor draws the |viewport| rect
  // at its origin. The offset of the |viewport| we pass to the layer compositor
  // must also be zero, since the rect needs to be in the coordinates of the
  // layer compositor.
  gfx::Transform adjusted_transform = transform;
  adjusted_transform.matrix().postTranslate(-viewport.x(), -viewport.y(), 0);
  // Don't propagate the viewport origin, as it will affect the clip rect.
  client_->OnDraw(adjusted_transform, gfx::Rect(viewport.size()),
                  in_software_draw_, false /*skip_draw*/);

  if (did_submit_frame_) {
    // This must happen after unwinding the stack and leaving the compositor.
    // Usually it is a separate task but we just defer it until OnDraw completes
    // instead.
    client_->DidReceiveCompositorFrameAck();
  }
}

void SynchronousLayerTreeFrameSink::ReclaimResources(
    uint32_t layer_tree_frame_sink_id,
    const std::vector<viz::ReturnedResource>& resources) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (layer_tree_frame_sink_id != layer_tree_frame_sink_id_)
    return;
  client_->ReclaimResources(resources);
}

void SynchronousLayerTreeFrameSink::SetMemoryPolicy(size_t bytes_limit) {
  DCHECK(CalledOnValidThread());
  bool became_zero = memory_policy_.bytes_limit_when_visible && !bytes_limit;
  bool became_non_zero =
      !memory_policy_.bytes_limit_when_visible && bytes_limit;
  memory_policy_.bytes_limit_when_visible = bytes_limit;
  memory_policy_.num_resources_limit = kNumResourcesLimit;

  if (client_)
    client_->SetMemoryPolicy(memory_policy_);

  if (became_zero) {
    // This is small hack to drop context resources without destroying it
    // when this compositor is put into the background.
    context_provider()->ContextSupport()->SetAggressivelyFreeResources(
        true /* aggressively_free_resources */);
  } else if (became_non_zero) {
    context_provider()->ContextSupport()->SetAggressivelyFreeResources(
        false /* aggressively_free_resources */);
  }
}

void SynchronousLayerTreeFrameSink::DidActivatePendingTree() {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->DidActivatePendingTree();
  DeliverMessages();
}

void SynchronousLayerTreeFrameSink::DeliverMessages() {
  std::vector<std::unique_ptr<IPC::Message>> messages;
  std::unique_ptr<FrameSwapMessageQueue::SendMessageScope> send_message_scope =
      frame_swap_message_queue_->AcquireSendMessageScope();
  frame_swap_message_queue_->DrainMessages(&messages);
  for (auto& msg : messages) {
    Send(msg.release());
  }
}

bool SynchronousLayerTreeFrameSink::Send(IPC::Message* message) {
  DCHECK(CalledOnValidThread());
  return sender_->Send(message);
}

bool SynchronousLayerTreeFrameSink::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

void SynchronousLayerTreeFrameSink::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  if (sync_client_) {
    sync_client_->SetNeedsBeginFrames(needs_begin_frames);
  }
}

void SynchronousLayerTreeFrameSink::DidPresentCompositorFrame(
    const viz::FrameTimingDetailsMap& timing_details) {
  if (!client_)
    return;
  for (const auto& pair : timing_details)
    client_->DidPresentCompositorFrame(pair.first, pair.second);
}

void SynchronousLayerTreeFrameSink::BeginFrame(
    const viz::BeginFrameArgs& args) {
  if (external_begin_frame_source_)
    external_begin_frame_source_->OnBeginFrame(args);
}

void SynchronousLayerTreeFrameSink::SetBeginFrameSourcePaused(bool paused) {
  if (external_begin_frame_source_)
    external_begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

}  // namespace content
