// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/android_webview/synchronous_layer_tree_frame_sink.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

namespace {

const viz::FrameSinkId kRootFrameSinkId(1, 1);
const viz::FrameSinkId kChildFrameSinkId(1, 2);

// Do not limit number of resources, so use an unrealistically high value.
const size_t kNumResourcesLimit = 10 * 1000 * 1000;

class SoftwareDevice : public viz::SoftwareOutputDevice {
 public:
  explicit SoftwareDevice(raw_ptr<SkCanvas>* canvas) : canvas_(canvas) {}
  SoftwareDevice(const SoftwareDevice&) = delete;
  SoftwareDevice& operator=(const SoftwareDevice&) = delete;

  void Resize(const gfx::Size& pixel_size, float device_scale_factor) override {
    // Intentional no-op: canvas size is controlled by the embedder.
  }
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override {
    DCHECK(*canvas_) << "BeginPaint with no canvas set";
    return *canvas_;
  }
  void EndPaint() override {}

 private:
  raw_ptr<raw_ptr<SkCanvas>> canvas_;
};

// This is used with resourceless software draws.
class SoftwareCompositorFrameSinkClient
    : public viz::mojom::CompositorFrameSinkClient {
 public:
  SoftwareCompositorFrameSinkClient() = default;
  SoftwareCompositorFrameSinkClient(const SoftwareCompositorFrameSinkClient&) =
      delete;
  SoftwareCompositorFrameSinkClient& operator=(
      const SoftwareCompositorFrameSinkClient&) = delete;
  ~SoftwareCompositorFrameSinkClient() override = default;

  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override {
    DCHECK(resources.empty());
  }
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resources) override {
    DCHECK(resources.empty());
  }
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override {
    DCHECK(resources.empty());
  }
  void OnBeginFramePausedChanged(bool paused) override {}
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override {}
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
  void SwapBuffers(viz::OutputSurfaceFrame frame) override {}
  void Reshape(const ReshapeParams& params) override {}
  void SetUpdateVSyncParametersCallback(
      viz::UpdateVSyncParametersCallback callback) override {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override {
    return gfx::OVERLAY_TRANSFORM_NONE;
  }
};

base::TimeDelta SynchronousLayerTreeFrameSink::StubDisplayClient::
    GetPreferredFrameIntervalForFrameSinkId(
        const viz::FrameSinkId& id,
        viz::mojom::blink::CompositorFrameSinkType* type) {
  return viz::BeginFrameArgs::MinInterval();
}

SynchronousLayerTreeFrameSink::SynchronousLayerTreeFrameSink(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<cc::RasterContextProviderWrapper>
        worker_context_provider_wrapper,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    uint32_t layer_tree_frame_sink_id,
    std::unique_ptr<viz::BeginFrameSource> synthetic_begin_frame_source,
    SynchronousCompositorRegistry* registry,
    mojo::PendingRemote<viz::mojom::blink::CompositorFrameSink>
        compositor_frame_sink_remote,
    mojo::PendingReceiver<viz::mojom::blink::CompositorFrameSinkClient>
        client_receiver)
    : cc::LayerTreeFrameSink(std::move(context_provider),
                             std::move(worker_context_provider_wrapper),
                             std::move(compositor_task_runner),
                             gpu_memory_buffer_manager,
                             /*shared_image_interface=*/nullptr),
      layer_tree_frame_sink_id_(layer_tree_frame_sink_id),
      registry_(registry),
      memory_policy_(0u),
      unbound_compositor_frame_sink_(std::move(compositor_frame_sink_remote)),
      unbound_client_(std::move(client_receiver)),
      synthetic_begin_frame_source_(std::move(synthetic_begin_frame_source)),
      viz_frame_submission_enabled_(
          ::features::IsUsingVizFrameSubmissionForWebView()),
      use_zero_copy_sw_draw_(
          Platform::Current()
              ->IsZeroCopySynchronousSwDrawEnabledForAndroidWebView()) {
  DCHECK(registry_);
  DETACH_FROM_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!cc::LayerTreeFrameSink::BindToClient(sink_client))
    return false;

  if (viz_frame_submission_enabled_) {
    compositor_frame_sink_.Bind(std::move(unbound_compositor_frame_sink_));
    client_receiver_.Bind(std::move(unbound_client_), compositor_task_runner_);
  }

  // The SharedBitmapManager is null since software compositing is not supported
  // or used on Android.
  frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams(
          /*shared_bitmap_manager=*/nullptr));

  if (synthetic_begin_frame_source_) {
    client_->SetBeginFrameSource(synthetic_begin_frame_source_.get());
  } else {
    external_begin_frame_source_ =
        std::make_unique<viz::ExternalBeginFrameSource>(this);
    external_begin_frame_source_->OnSetBeginFrameSourcePaused(
        begin_frames_paused_);
    client_->SetBeginFrameSource(external_begin_frame_source_.get());
  }

  client_->SetMemoryPolicy(memory_policy_);
  client_->SetTreeActivationCallback(base::BindRepeating(
      &SynchronousLayerTreeFrameSink::DidActivatePendingTree,
      base::Unretained(this)));
  registry_->RegisterLayerTreeFrameSink(this);

  software_frame_sink_client_ =
      std::make_unique<SoftwareCompositorFrameSinkClient>();
  constexpr bool root_support_is_root = true;
  constexpr bool child_support_is_root = false;
  root_support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      software_frame_sink_client_.get(), frame_sink_manager_.get(),
      kRootFrameSinkId, root_support_is_root);
  child_support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      software_frame_sink_client_.get(), frame_sink_manager_.get(),
      kChildFrameSinkId, child_support_is_root);

  viz::RendererSettings software_renderer_settings;

  auto output_surface = std::make_unique<SoftwareOutputSurface>(
      std::make_unique<SoftwareDevice>(&current_sw_canvas_));
  software_output_surface_ = output_surface.get();

  auto overlay_processor = std::make_unique<viz::OverlayProcessorStub>();

  // The gpu_memory_buffer_manager here is null as the Display is only used for
  // resourcesless software draws, where no resources are included in the frame
  // swapped from the compositor. So there is no need for it.
  // The shared_bitmap_manager_ is provided for the Display to allocate
  // resources.
  // TODO(crbug.com/692814): The Display never sends its resources out of
  // process so there is no reason for it to use a SharedBitmapManager.
  // The gpu::GpuTaskSchedulerHelper here is null as the OutputSurface is
  // software only and the overlay processor is a stub.
  display_ = std::make_unique<viz::Display>(
      &shared_bitmap_manager_, /*shared_image_manager=*/nullptr,
      /*sync_point_manager=*/nullptr, /*gpu_scheduler=*/nullptr,
      software_renderer_settings, &debug_settings_, kRootFrameSinkId,
      nullptr /* gpu::GpuTaskSchedulerHelper */, std::move(output_surface),
      std::move(overlay_processor), nullptr /* scheduler */,
      nullptr /* current_task_runner */);
  display_->Initialize(&display_client_,
                       frame_sink_manager_->surface_manager());
  display_->SetVisible(true);
  return true;
}

void SynchronousLayerTreeFrameSink::DetachFromClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetBeginFrameSource(nullptr);
  // Destroy the begin frame source on the same thread it was bound on.
  synthetic_begin_frame_source_ = nullptr;
  external_begin_frame_source_ = nullptr;
  if (sync_client_)
    sync_client_->SinkDestroyed();
  registry_->UnregisterLayerTreeFrameSink(this);
  client_->SetTreeActivationCallback(base::RepeatingClosure());
  root_support_.reset();
  child_support_.reset();
  software_frame_sink_client_ = nullptr;
  software_output_surface_ = nullptr;
  display_ = nullptr;
  frame_sink_manager_ = nullptr;

  client_receiver_.reset();
  compositor_frame_sink_.reset();

  cc::LayerTreeFrameSink::DetachFromClient();
}

void SynchronousLayerTreeFrameSink::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  local_surface_id_ = local_surface_id;
}

void SynchronousLayerTreeFrameSink::SubmitCompositorFrame(
    viz::CompositorFrame frame,
    bool hit_test_data_changed) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(sync_client_);

  std::optional<viz::CompositorFrame> submit_frame;
  gfx::Size child_size = in_software_draw_
                             ? sw_viewport_for_current_draw_.size()
                             : frame.size_in_pixels();
  if (!child_local_surface_id_.is_valid() || child_size_ != child_size ||
      device_scale_factor_ != frame.metadata.device_scale_factor) {
    child_local_surface_id_allocator_.GenerateId();
    child_local_surface_id_ =
        child_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
    child_size_ = child_size;
    device_scale_factor_ = frame.metadata.device_scale_factor;
  }

  if (in_software_draw_) {
    // The frame we send to the client is actually just the metadata. Preserve
    // the |frame| for the software path below.
    submit_frame.emplace();
    submit_frame->metadata = frame.metadata.Clone();

    // The layer compositor should be giving a frame that covers the
    // |sw_viewport_for_current_draw_| but at 0,0.
    DCHECK(gfx::Rect(child_size) == frame.render_pass_list.back()->output_rect);

    // Make a size that covers from 0,0 and includes the area coming from the
    // layer compositor.
    gfx::Size display_size(sw_viewport_for_current_draw_.right(),
                           sw_viewport_for_current_draw_.bottom());
    display_->Resize(display_size);

    if (!root_local_surface_id_.is_valid() || display_size_ != display_size ||
        root_device_scale_factor_ != frame.metadata.device_scale_factor) {
      root_local_surface_id_allocator_.GenerateId();
      root_local_surface_id_ =
          root_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
      display_size_ = display_size;
      root_device_scale_factor_ = frame.metadata.device_scale_factor;
    }

    display_->SetLocalSurfaceId(root_local_surface_id_,
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
    embed_frame.metadata.frame_token = ++root_next_frame_token_;
    embed_frame.metadata.begin_frame_ack = frame.metadata.begin_frame_ack;
    embed_frame.metadata.device_scale_factor =
        frame.metadata.device_scale_factor;
    embed_frame.render_pass_list.push_back(viz::CompositorRenderPass::Create());

    // The embedding RenderPass covers the entire Display's area.
    const auto& embed_render_pass = embed_frame.render_pass_list.back();
    embed_render_pass->SetNew(viz::CompositorRenderPassId{1},
                              gfx::Rect(display_size), gfx::Rect(display_size),
                              gfx::Transform());
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
        gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
        /*contents_opaque=*/are_contents_opaque, /*opacity_f=*/1.f,
        SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    surface_quad->SetNew(
        shared_quad_state, gfx::Rect(child_size), gfx::Rect(child_size),
        viz::SurfaceRange(
            std::nullopt,
            viz::SurfaceId(kChildFrameSinkId, child_local_surface_id_)),
        SkColors::kWhite, false /* stretch_content_to_fill_bounds */);

    child_support_->SubmitCompositorFrame(child_local_surface_id_,
                                          std::move(frame));
    root_support_->SubmitCompositorFrame(root_local_surface_id_,
                                         std::move(embed_frame));
    base::TimeTicks now = base::TimeTicks::Now();
    display_->DrawAndSwap({now, now});

    // We don't track metrics for frames submitted to |display_| but it still
    // expects that every frame will receive a swap ack and presentation
    // feedback so we send null signals here.
    now = base::TimeTicks::Now();
    gpu::SwapBuffersCompleteParams params;
    params.swap_response.timings = {now, now};
    params.swap_response.result = gfx::SwapResult::SWAP_ACK;
    display_->DidReceiveSwapBuffersAck(params,
                                       /*release_fence=*/gfx::GpuFenceHandle());
    display_->DidReceivePresentationFeedback(
        gfx::PresentationFeedback::Failure());

    viz::FrameTimingDetails details;
    details.received_compositor_frame_timestamp = now;
    details.draw_start_timestamp = now;
    details.swap_timings = {now, now, now, now};
    details.presentation_feedback = {now, base::TimeDelta(), 0};
    client_->DidPresentCompositorFrame(submit_frame->metadata.frame_token,
                                       details);
  } else {
    if (viz_frame_submission_enabled_) {
      frame.metadata.begin_frame_ack =
          viz::BeginFrameAck::CreateManualAckWithDamage();

      // For hardware draws with viz we send frame to compositor_frame_sink_
      compositor_frame_sink_->SubmitCompositorFrame(
          local_surface_id_, std::move(frame), client_->BuildHitTestData(), 0);
    } else {
      // For hardware draws without viz we send the whole frame to the client so
      // it can draw the content in it.
      submit_frame = std::move(frame);
    }
  }

  // NOTE: submit_frame will be empty if viz_frame_submission_enabled_ enabled,
  // but it won't be used upstream
  // Because OnDraw can synchronously override the viewport without going
  // through commit and activation, we generate our own LocalSurfaceId by
  // checking the submitted frame instead of using the one set here.
  sync_client_->SubmitCompositorFrame(
      layer_tree_frame_sink_id_,
      viz_frame_submission_enabled_ ? local_surface_id_
                                    : child_local_surface_id_,
      std::move(submit_frame), client_->BuildHitTestData());
  did_submit_frame_ = true;
}

void SynchronousLayerTreeFrameSink::DidNotProduceFrame(
    const viz::BeginFrameAck& ack,
    cc::FrameSkippedReason reason) {
  // We do not call CompositorFrameSink::DidNotProduceFrame here because
  // submission of frame depends on DemandDraw calls.
}

void SynchronousLayerTreeFrameSink::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const viz::SharedBitmapId& id) {
  // Webview does not use software compositing (other than resourceless draws,
  // but this is called for software /resources/).
  NOTREACHED_IN_MIGRATION();
}

void SynchronousLayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id) {
  // Webview does not use software compositing (other than resourceless draws,
  // but this is called for software /resources/).
  NOTREACHED_IN_MIGRATION();
}

void SynchronousLayerTreeFrameSink::Invalidate(bool needs_draw) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (sync_client_)
    sync_client_->Invalidate(needs_draw);
}

void SynchronousLayerTreeFrameSink::DemandDrawHw(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority,
    bool need_new_local_surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(HasClient());
  DCHECK(context_provider_.get());

  if (need_new_local_surface_id) {
    child_local_surface_id_ = viz::LocalSurfaceId();
  }

  client_->SetExternalTilePriorityConstraints(viewport_rect_for_tile_priority,
                                              transform_for_tile_priority);
  InvokeComposite(gfx::Transform(), gfx::Rect(viewport_size));
}

void SynchronousLayerTreeFrameSink::DemandDrawSwZeroCopy() {
  DCHECK(use_zero_copy_sw_draw_);
  DemandDrawSw(
      Platform::Current()->SynchronousCompositorGetSkCanvasForAndroidWebView());
}

void SynchronousLayerTreeFrameSink::DemandDrawSw(SkCanvas* canvas) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);

  base::AutoReset<raw_ptr<SkCanvas>> canvas_resetter(&current_sw_canvas_,
                                                     canvas);

  SkIRect canvas_clip = canvas->getDeviceClipBounds();
  gfx::Rect viewport = gfx::SkIRectToRect(canvas_clip);

  // Converts 3x3 matrix to 4x4.
  gfx::Transform transform = gfx::SkMatrixToTransform(canvas->getTotalMatrix());

  // We will resize the Display to ensure it covers the entire |viewport|, so
  // save it for later.
  sw_viewport_for_current_draw_ = viewport;

  base::AutoReset<bool> set_in_software_draw(&in_software_draw_, true);
  InvokeComposite(transform, viewport);
}

void SynchronousLayerTreeFrameSink::WillSkipDraw() {
  client_->OnDraw(gfx::Transform(), gfx::Rect(), in_software_draw_,
                  true /*skip_draw*/);
}

bool SynchronousLayerTreeFrameSink::UseZeroCopySoftwareDraw() {
  return use_zero_copy_sw_draw_;
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
  adjusted_transform.PostTranslate(-viewport.OffsetFromOrigin());
  // Don't propagate the viewport origin, as it will affect the clip rect.
  client_->OnDraw(adjusted_transform, gfx::Rect(viewport.size()),
                  in_software_draw_, false /*skip_draw*/);

  if (did_submit_frame_) {
    // This must happen after unwinding the stack and leaving the compositor.
    // Usually it is a separate task but we just defer it until OnDraw
    // completes instead.
    client_->DidReceiveCompositorFrameAck();
  }
}

void SynchronousLayerTreeFrameSink::ReclaimResources(
    uint32_t layer_tree_frame_sink_id,
    Vector<viz::ReturnedResource> resources) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (layer_tree_frame_sink_id != layer_tree_frame_sink_id_)
    return;
  client_->ReclaimResources(std::vector<viz::ReturnedResource>(
      std::make_move_iterator(resources.begin()),
      std::make_move_iterator(resources.end())));
}

void SynchronousLayerTreeFrameSink::
    OnCompositorFrameTransitionDirectiveProcessed(
        uint32_t layer_tree_frame_sink_id,
        uint32_t sequence_id) {
  if (layer_tree_frame_sink_id != layer_tree_frame_sink_id_)
    return;
  client_->OnCompositorFrameTransitionDirectiveProcessed(sequence_id);
}

void SynchronousLayerTreeFrameSink::SetMemoryPolicy(size_t bytes_limit) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (sync_client_)
    sync_client_->DidActivatePendingTree();
}

void SynchronousLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    Vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(viz_frame_submission_enabled_);
  client_->ReclaimResources(std::vector<viz::ReturnedResource>(
      std::make_move_iterator(resources.begin()),
      std::make_move_iterator(resources.end())));
  // client_->DidReceiveCompositorFrameAck() is called just after frame
  // submission so cc won't be throttled on actual draw which can happen late
  // (or not happen at all) for WebView.
}

void SynchronousLayerTreeFrameSink::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const HashMap<uint32_t, viz::FrameTimingDetails>& timing_details,
    bool frame_ack,
    Vector<viz::ReturnedResource> resources) {
  DCHECK(viz_frame_submission_enabled_);
  if (::features::IsOnBeginFrameAcksEnabled()) {
    if (frame_ack) {
      DidReceiveCompositorFrameAck(std::move(resources));
    } else if (!resources.empty()) {
      ReclaimResources(std::move(resources));
    }
  }

  // We do not receive BeginFrames via CompositorFrameSink, so we do not forward
  // it to cc. We still might get one with FrameTimingDetailsMap, so we report
  // it here.

  if (client_) {
    for (const auto& pair : timing_details) {
      client_->DidPresentCompositorFrame(pair.key, pair.value);
    }
  }
}

void SynchronousLayerTreeFrameSink::ReclaimResources(
    Vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(viz_frame_submission_enabled_);
  client_->ReclaimResources(std::vector<viz::ReturnedResource>(
      std::make_move_iterator(resources.begin()),
      std::make_move_iterator(resources.end())));
}

void SynchronousLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {
  DCHECK(viz_frame_submission_enabled_);
}

void SynchronousLayerTreeFrameSink::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  if (needs_begin_frames_ != needs_begin_frames) {
    if (needs_begin_frames) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("cc,benchmark", "NeedsBeginFrames",
                                        this);
    } else {
      TRACE_EVENT_NESTABLE_ASYNC_END0("cc,benchmark", "NeedsBeginFrames", this);
    }
  }
  needs_begin_frames_ = needs_begin_frames;
  if (sync_client_) {
    sync_client_->SetNeedsBeginFrames(needs_begin_frames);
  }
}

void SynchronousLayerTreeFrameSink::DidPresentCompositorFrame(
    const viz::FrameTimingDetailsMap& timing_details) {
  DCHECK(!viz_frame_submission_enabled_ || timing_details.empty());

  if (!client_)
    return;
  for (const auto& pair : timing_details)
    client_->DidPresentCompositorFrame(pair.first, pair.second);
}

void SynchronousLayerTreeFrameSink::BeginFrame(
    const viz::BeginFrameArgs& args) {
  if (!external_begin_frame_source_)
    return;
  external_begin_frame_source_->OnBeginFrame(args);
}

void SynchronousLayerTreeFrameSink::SetBeginFrameSourcePaused(bool paused) {
  if (external_begin_frame_source_)
    external_begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

}  // namespace blink
