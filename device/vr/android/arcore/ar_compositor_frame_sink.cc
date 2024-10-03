// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/ar_compositor_frame_sink.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "ui/android/window_android.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_bindings.h"

namespace {
class ArCoreHostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit ArCoreHostDisplayClient(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner,
      ui::WindowAndroid* root_window)
      : HostDisplayClient(gfx::kNullAcceleratedWidget),
        main_thread_task_runner_(main_thread_task_runner),
        root_window_(root_window) {
    // TODO(crbug.com/40758616): Ideally, we'd DCHECK here, but the UTs
    // don't create a root_window.
  }

  ~ArCoreHostDisplayClient() override = default;

  void DidCompleteSwapWithSize(const gfx::Size& pixel_size) override {}

  void OnContextCreationResult(gpu::ContextResult context_result) override {}

  void SetWideColorEnabled(bool enabled) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ArCoreHostDisplayClient::DoSetWideColorEnabled,
                       weak_ptr_factory_.GetWeakPtr(), enabled));
  }

  void SetPreferredRefreshRate(float refresh_rate) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ArCoreHostDisplayClient::DoSetPreferredRefreshRate,
                       weak_ptr_factory_.GetWeakPtr(), refresh_rate));
  }

 private:
  void DoSetWideColorEnabled(bool enabled) {
    if (root_window_) {
      root_window_->SetWideColorEnabled(enabled);
    }
  }

  void DoSetPreferredRefreshRate(float refresh_rate) {
    if (root_window_) {
      root_window_->SetPreferredRefreshRate(refresh_rate);
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  raw_ptr<ui::WindowAndroid> root_window_;

  // Must be the last member so that it will be destructed first.
  base::WeakPtrFactory<ArCoreHostDisplayClient> weak_ptr_factory_{this};
};
}  // namespace

namespace device {
ArCompositorFrameSink::ArCompositorFrameSink(
    scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner,
    BeginFrameCallback on_begin_frame,
    CompositorReceivedFrameCallback on_compositor_received_frame,
    RenderingFinishedCallback on_rendering_finished,
    CanIssueNewFrameCallback on_can_issue_new_frame)
    : gl_thread_task_runner_(gl_thread_task_runner),
      on_begin_frame_(on_begin_frame),
      on_compositor_received_frame_(on_compositor_received_frame),
      on_rendering_finished_(on_rendering_finished),
      on_can_issue_new_frame_(on_can_issue_new_frame) {
  DCHECK(ArImageTransport::UseSharedBuffer())
      << "ArCompositorFrameSink only works with Shared Buffers";
  DCHECK(gl_thread_task_runner_);
}

ArCompositorFrameSink::~ArCompositorFrameSink() {
  DCHECK(IsOnGlThread());
  CloseBindingsIfOpen();
}

bool ArCompositorFrameSink::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

viz::FrameSinkId ArCompositorFrameSink::FrameSinkId() {
  if (!is_initialized_)
    return {};
  return xr_frame_sink_client_->FrameSinkId();
}

void ArCompositorFrameSink::Initialize(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    gpu::SurfaceHandle surface_handle,
    ui::WindowAndroid* root_window,
    const gfx::Size& frame_size,
    device::XrFrameSinkClient* xr_frame_sink_client,
    DomOverlaySetup dom_setup,
    base::OnceCallback<void(bool)> on_initialized,
    base::OnceClosure on_bindings_disconnect) {
  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);
  DCHECK(xr_frame_sink_client);
  DVLOG(1) << __func__;

  // Store the passed in values.
  display_client_ = std::make_unique<ArCoreHostDisplayClient>(
      main_thread_task_runner, root_window);
  frame_size_ = frame_size;
  xr_frame_sink_client_ = xr_frame_sink_client;
  on_initialized_ = std::move(on_initialized);
  on_bindings_disconnect_ = std::move(on_bindings_disconnect);

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();

  // Create interfaces for a root CompositorFrameSink.
  // sink_remote_ and client_receiver are the bits to talk to/from the FrameSink
  root_params->compositor_frame_sink =
      sink_remote_.BindNewEndpointAndPassReceiver();
  sink_receiver_.Bind(root_params->compositor_frame_sink_client
                          .InitWithNewPipeAndPassReceiver());
  root_params->display_private =
      display_private_.BindNewEndpointAndPassReceiver();
  root_params->display_client =
      display_client_->GetBoundRemote(gl_thread_task_runner_);

  root_params->widget = surface_handle;
  root_params->gpu_compositing = true;
  root_params->renderer_settings = viz::CreateRendererSettings();

  // We can still get Frame callbacks if we don't specify this, but ideally we
  // want to tie our framerate to that of ARCore as much as we can, so by
  // setting ourselves up as an ExternalBeginFrame Controller, we can do that.
  root_params->external_begin_frame_controller =
      frame_controller_remote_.BindNewEndpointAndPassReceiver();

  // Ensure that we'll have a valid LocalSurfaceId.
  allocator_.GenerateId();

  xr_frame_sink_client_->InitializeRootCompositorFrameSink(
      std::move(root_params), dom_setup,
      base::BindPostTask(
          gl_thread_task_runner_,
          base::BindOnce(&ArCompositorFrameSink::OnRootCompositorFrameSinkReady,
                         weak_ptr_factory_.GetWeakPtr(), dom_setup)));

  // Note that since we own these remotes, Unretained(this) is okay, since they
  // will be destroyed (and thus unable to call the disconnect handler), before
  // we are.
  display_private_.set_disconnect_handler(base::BindOnce(
      &ArCompositorFrameSink::OnBindingsDisconnect, base::Unretained(this)));
  sink_remote_.set_disconnect_handler(base::BindOnce(
      &ArCompositorFrameSink::OnBindingsDisconnect, base::Unretained(this)));
  frame_controller_remote_.set_disconnect_handler(base::BindOnce(
      &ArCompositorFrameSink::OnBindingsDisconnect, base::Unretained(this)));
  sink_receiver_.set_disconnect_handler(base::BindOnce(
      &ArCompositorFrameSink::OnBindingsDisconnect, base::Unretained(this)));
}

void ArCompositorFrameSink::OnRootCompositorFrameSinkReady(
    DomOverlaySetup dom_setup) {
  DVLOG(1) << __func__;
  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);

  if (dom_setup != DomOverlaySetup::kNone) {
    // If the frame sink client doesn't have a valid SurfaceId at this point,
    // then the compositor hierarchy did not get set up, which means that we'll
    // be unable to composite DOM content.
    std::optional<viz::SurfaceId> dom_surface =
        xr_frame_sink_client_->GetDOMSurface();
    if (dom_surface && dom_surface->is_valid()) {
      should_composite_dom_overlay_ = true;
    } else if (dom_setup == DomOverlaySetup::kRequired) {
      std::move(on_initialized_).Run(false);
      return;
    }
  }

  display_private_->Resize(frame_size_);
  display_private_->SetDisplayVisible(true);
  sink_remote_->SetNeedsBeginFrame(true);

  is_initialized_ = true;
  std::move(on_initialized_).Run(true);
}

void ArCompositorFrameSink::RequestBeginFrame(base::TimeDelta interval,
                                              base::TimeTicks deadline) {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  // Note that this is enforced on the other side of the mojom call, so this
  // DCHECK just helps us fail a little bit earlier.
  DCHECK(can_issue_new_begin_frame_)
      << "Previous frame must finish being submitted before a new frame can be "
         "requested.";
  DVLOG(3) << __func__;

  // Note that the Unretained(this) below is okay, since if this is destroyed,
  // the remote will be closed and the callback is guaranteed to not be run.
  frame_controller_remote_->IssueExternalBeginFrame(
      viz::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, UINT64_MAX,
                                  next_begin_frame_id_++,
                                  base::TimeTicks::Now(), deadline, interval,
                                  viz::BeginFrameArgs::NORMAL),
      true,
      base::BindOnce(&ArCompositorFrameSink::OnFrameSubmitAck,
                     base::Unretained(this)));
  can_issue_new_begin_frame_ = false;
}

void ArCompositorFrameSink::SubmitFrame(WebXrFrame* xr_frame,
                                        FrameType frame_type) {
  DVLOG(3) << __func__;
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  DCHECK(xr_frame);
  sink_remote_->SubmitCompositorFrame(allocator_.GetCurrentLocalSurfaceId(),
                                      CreateFrame(xr_frame, frame_type),
                                      std::optional<viz::HitTestRegionList>(),
                                      /*trace_time=*/0);
}

void ArCompositorFrameSink::DidNotProduceFrame(WebXrFrame* xr_frame) {
  DVLOG(3) << __func__;
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  DCHECK(xr_frame);
  DCHECK(xr_frame->begin_frame_args);
  sink_remote_->DidNotProduceFrame(
      viz::BeginFrameAck(*xr_frame->begin_frame_args, /*has_damage=*/false));
}

void ArCompositorFrameSink::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  DVLOG(3) << __func__;
  // Notify that we've received the Ack for this frame first. It may be that the
  // most recently submitted frame is also dropped, so updating the parent that
  // we acknowledged the frame can help to keep their logic more consistent.
  on_compositor_received_frame_.Run();

  // Resources can come back either via this method or ReclaimResources.
  // This method indicates that the most recently submitted frame has been
  // received; while ReclaimResources will generally be called to free up the
  // resources. However, it's all timing dependent as to which one gets called
  // with the actual freed resources.
  DVLOG(3) << __func__ << " Reclaiming Resources";
  ReclaimResources(std::move(resources));
}

void ArCompositorFrameSink::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  DVLOG(3) << __func__ << " resources.size()=" << resources.size();
  for (const auto& resource : resources) {
    DVLOG(3) << __func__ << " Reclaimed: " << resource.id;
    if (resource.id == viz::kInvalidResourceId)
      continue;

    auto it = id_to_frame_map_.find(resource.id);
    CHECK(it != id_to_frame_map_.end(), base::NotFatalUntil::M130);
    auto* rendering_frame = it->second.get();

    // While we now know that this resource is associated with this frame, we
    // don't know which buffer it is associated with, and we need to ensure that
    // we've got all of the resources associated with a frame cleared before we
    // actually clear the frame. First determine which buffer this ResourceId
    // was associated with and then clear it.
    if (resource.id == rendering_frame->shared_buffer->id) {
      rendering_frame->shared_buffer->id = viz::kInvalidResourceId;
    }
    if (resource.id == rendering_frame->camera_image_shared_buffer->id) {
      rendering_frame->camera_image_shared_buffer->id = viz::kInvalidResourceId;
    }

    // In order to keep our map size small we can remove this association as it
    // is no longer needed.
    id_to_frame_map_.erase(it);

    // Even though we've gotten the ReturnedResource, that just means that all
    // of the needed work has been queued up, we need to actually track the sync
    // token to determine when the frame is *actually* done. Given that each
    // frame can have multiple buffers associated with it, we'll store the token
    // until we get all of the buffers associated with the frame returned.
    rendering_frame->reclaimed_sync_tokens.push_back(resource.sync_token);

    // Once we've cleared all of the buffers on the frame that were passed to
    // viz, we can tell our parent that the frame is ready to be reclaimed
    // (though they will need to wait for the tokens if there are any associated
    // with the frame). Note that we don't need to check elsewhere as if a frame
    // was submitted, it has at least the shared buffer for the camera.
    if (rendering_frame->shared_buffer->id == viz::kInvalidResourceId &&
        rendering_frame->camera_image_shared_buffer->id ==
            viz::kInvalidResourceId) {
      on_rendering_finished_.Run(rendering_frame);
    }
  }
}

void ArCompositorFrameSink::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details,
    bool frame_ack,
    std::vector<viz::ReturnedResource> resources) {
  // TODO(crbug.com/40250552): Determine why the timing of this Ack leads to
  // frame production stopping in tests.
  if (features::IsOnBeginFrameAcksEnabled()) {
    if (frame_ack) {
      DidReceiveCompositorFrameAck(std::move(resources));
    } else if (!resources.empty()) {
      ReclaimResources(std::move(resources));
    }
  }
  on_begin_frame_.Run(args, timing_details);
}

void ArCompositorFrameSink::OnFrameSubmitAck(const viz::BeginFrameAck& ack) {
  DVLOG(3) << __func__;
  can_issue_new_begin_frame_ = true;
  on_can_issue_new_frame_.Run();
}

void ArCompositorFrameSink::CloseBindingsIfOpen() {
  DVLOG(1) << __func__;
  display_private_.reset();
  sink_remote_.reset();
  frame_controller_remote_.reset();
  sink_receiver_.reset();
  display_client_.reset();
}

void ArCompositorFrameSink::OnBindingsDisconnect() {
  DVLOG(1) << __func__;

  is_initialized_ = false;
  CloseBindingsIfOpen();
  if (on_bindings_disconnect_) {
    std::move(on_bindings_disconnect_).Run();
  }
}

viz::CompositorFrame ArCompositorFrameSink::CreateFrame(WebXrFrame* xr_frame,
                                                        FrameType frame_type) {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  DCHECK(xr_frame);
  DCHECK(xr_frame->begin_frame_args)
      << "Never received a BeginFrame for this frame";
  DVLOG(3) << __func__ << " frame_id=" << xr_frame->index
           << " frame_type=" << frame_type;

  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck(*xr_frame->begin_frame_args, true);
  frame.metadata.device_scale_factor = 1.f;
  frame.metadata.frame_token = ++next_frame_token_;

  // As the root compositor, we're always the first render pass.
  const viz::CompositorRenderPassId kRenderPassId{1};

  // Because the Camera Image is the fullscreen, and the AR Content is also
  // rendered fullscreen, we're outputting and saying that the "damage" (e.g.
  // changed region), is the full screen, or in other words, the size of the
  // camera image.
  gfx::Rect output_rect(frame_size_);
  gfx::Rect damage_rect = output_rect;

  auto render_pass = viz::CompositorRenderPass::Create();
  render_pass->SetNew(kRenderPassId, output_rect, damage_rect,
                      gfx::Transform());

  // The order that these are added corresponds to their Z-order, so add things
  // that should be in the front first.
  // This means that we want to add:
  // 1) The DOM content
  // 2) The GL(WebXr) content from the renderer
  // 3) The camera image

  // First the DOM, if it's enabled
  if (should_composite_dom_overlay_) {
    auto dom_surface_id = xr_frame_sink_client_->GetDOMSurface();
    bool can_composite_dom_overlay =
        dom_surface_id && dom_surface_id->is_valid();
    DVLOG(3)
        << __func__
        << " Attempting to composite DOMOverlay, can_composite_dom_overlay="
        << can_composite_dom_overlay;
    if (can_composite_dom_overlay) {
      viz::SharedQuadState* dom_quad_state =
          render_pass->CreateAndAppendSharedQuadState();
      dom_quad_state->SetAll(
          gfx::Transform(),
          /*quad_layer_rect=*/output_rect,
          /*visible_layer_rect=*/output_rect, gfx::MaskFilterInfo(),
          /*clip_rect=*/std::nullopt, /*are_contents_opaque=*/false,
          /*opacity=*/1.f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0,
          /*layer_id=*/0u, /*fast_rounded_corner=*/false);

      viz::SurfaceDrawQuad* dom_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
      dom_quad->SetNew(dom_quad_state, gfx::Rect(output_rect.size()),
                       gfx::Rect(output_rect.size()),
                       viz::SurfaceRange(*dom_surface_id),
                       SkColors::kTransparent,
                       /*stretch_content_to_fill_bounds=*/true);
    }
  }

  // Setup some variables for the SharedQuadState that are the same for the
  // Camera/Renderer
  // Next add the Renderer Content
  if (frame_type == FrameType::kHasWebXrContent) {
    WebXrSharedBuffer* renderer_buffer = xr_frame->shared_buffer.get();
    renderer_buffer->id = resource_id_generator_.GenerateNextId();
    id_to_frame_map_[renderer_buffer->id] = xr_frame;

    viz::SharedQuadState* xr_content_quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    xr_content_quad_state->SetAll(
        gfx::Transform(),
        /*quad_layer_rect=*/output_rect,
        /*visible_layer_rect=*/output_rect, gfx::MaskFilterInfo(),
        /*clip_rect=*/std::nullopt, /*are_contents_opaque=*/false,
        /*opacity=*/1.f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    viz::TextureDrawQuad* xr_content_quad =
        render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
    xr_content_quad->SetNew(
        xr_content_quad_state,
        /*rect=*/output_rect,
        /*visible_rect=*/output_rect,
        /*needs_blending=*/true, renderer_buffer->id,
        /*premultiplied_alpha=*/true,
        /*uv_top_left=*/xr_frame->bounds_left.origin(),
        /*uv_bottom_right=*/xr_frame->bounds_left.bottom_right(),
        /*background_color=*/SkColors::kTransparent,
        /*y_flipped=*/true,
        /*nearest_neighbor=*/false,
        /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);

    auto renderer_resource = viz::TransferableResource::MakeGpu(
        renderer_buffer->shared_image,
        renderer_buffer->shared_image->GetTextureTarget(),
        renderer_buffer->sync_token, renderer_buffer->size,
        viz::SinglePlaneFormat::kRGBA_8888,
        /*is_overlay_candidate=*/false,
        viz::TransferableResource::ResourceSource::kAR);

    renderer_resource.id = renderer_buffer->id;
    id_to_frame_map_[renderer_buffer->id] = xr_frame;

    frame.resource_list.push_back(renderer_resource);
  }

  WebXrSharedBuffer* camera_buffer = xr_frame->camera_image_shared_buffer.get();
  camera_buffer->id = resource_id_generator_.GenerateNextId();

  viz::SharedQuadState* camera_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  camera_quad_state->SetAll(
      gfx::Transform(),
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect, gfx::MaskFilterInfo(),
      /*clip_rect=*/std::nullopt, /*are_contents_opaque=*/true,
      /*opacity=*/1.f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0,
      /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  viz::TextureDrawQuad* camera_quad =
      render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  // UV from 0,0 to 1,1 because the camera texture is fullscreen.
  camera_quad->SetNew(camera_quad_state,
                      /*rect=*/output_rect,
                      /*visible_rect=*/output_rect,
                      /*needs_blending=*/true, camera_buffer->id,
                      /*premultiplied_alpha=*/true,
                      /*uv_top_left=*/gfx::PointF(0.f, 0.f),
                      /*uv_bottom_right=*/gfx::PointF(1.f, 1.f),
                      /*background_color=*/SkColors::kTransparent,
                      /*y_flipped=*/true,
                      /*nearest_neighbor=*/false,
                      /*secure_output_only=*/false,
                      gfx::ProtectedVideoType::kClear);

  // Additionally append to the resource_list
  auto camera_resource = viz::TransferableResource::MakeGpu(
      camera_buffer->shared_image,
      camera_buffer->shared_image->GetTextureTarget(),
      camera_buffer->sync_token, camera_buffer->size,
      viz::SinglePlaneFormat::kRGBA_8888,
      /*is_overlay_candidate=*/false,
      viz::TransferableResource::ResourceSource::kAR);

  camera_resource.id = camera_buffer->id;
  id_to_frame_map_[camera_buffer->id] = xr_frame;

  frame.resource_list.push_back(camera_resource);

  frame.render_pass_list.push_back(std::move(render_pass));

  return frame;
}

}  // namespace device
