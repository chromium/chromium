// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/release_callback.h"
#include "services/viz/public/mojom/compositing/frame_timing_details.mojom-blink.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom-blink.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_placeholder.h"
#include "third_party/blink/renderer/platform/graphics/resource_id_traits.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/mojom/presentation_feedback.mojom-blink.h"

namespace {
// Frame delay for synthetic frame timing.
// TODO(crbug.com/325532633): match this to the requested capture rate.
constexpr base::TimeDelta kSyntheticFrameDelay = base::Hertz(60);
}  // namespace

namespace blink {

// Holds the ref and release callback for a CanvasResource that has been
// exported to the compositor, to be released when either (a) the compositor
// notifies CanvasResourceDispatcher that it no longer requires this resource,
// or (b) the CanvasResourceDispatcher is torn down (e.g., because its owning
// thread was torn down).
struct CanvasResourceDispatcher::ExportedResource {
 public:
  ExportedResource(scoped_refptr<CanvasResource> resource,
                   CanvasResource::ReleaseCallback callback)
      : resource_(std::move(resource)), release_callback_(std::move(callback)) {
    CHECK(resource_);
  }

  void ReleaseResource(gpu::SharedImageExportResult shared_image_export_result,
                       bool is_lost) {
    auto sync_token = resource_->GetClientSharedImage()->EndExport(
        std::move(shared_image_export_result));
    ReleaseResource(sync_token, is_lost);
  }

  ~ExportedResource() { ReleaseResource(gpu::SyncToken(), /*is_lost=*/false); }

 private:
  void ReleaseResource(const gpu::SyncToken& sync_token, bool is_lost) {
    auto resource = std::move(resource_);
    if (release_callback_) {
      std::move(release_callback_)
          .Run(std::move(resource), sync_token, is_lost);
    }
  }

  scoped_refptr<CanvasResource> resource_;
  CanvasResource::ReleaseCallback release_callback_;
};

CanvasResourceDispatcher::CanvasResourceDispatcher(
    CanvasResourceDispatcherClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner>
        agent_group_scheduler_compositor_task_runner,
    uint32_t client_id,
    uint32_t sink_id,
    int canvas_id,
    const gfx::Size& size)
    : frame_sink_id_(viz::FrameSinkId(client_id, sink_id)),
      size_(size),
      change_size_for_next_commit_(false),
      placeholder_canvas_id_(canvas_id),
      num_pending_placeholder_resources_(0),
      client_(client),
      task_runner_(std::move(task_runner)),
      agent_group_scheduler_compositor_task_runner_(
          std::move(agent_group_scheduler_compositor_task_runner)),
      fake_frame_timer_(task_runner_,
                        this,
                        &CanvasResourceDispatcher::OnFakeFrameTimer) {
  // Frameless canvas pass an invalid |frame_sink_id_|; don't create mojo
  // channel for this special case.
  if (!frame_sink_id_.is_valid())
    return;

  DCHECK(!sink_.is_bound());
  mojo::Remote<mojom::blink::EmbeddedFrameSinkProvider> provider;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      provider.BindNewPipeAndPassReceiver());

  DCHECK(provider);
  provider->CreateCompositorFrameSink(frame_sink_id_,
                                      receiver_.BindNewPipeAndPassRemote(),
                                      sink_.BindNewPipeAndPassReceiver());
  provider->ConnectToEmbedder(frame_sink_id_,
                              surface_embedder_.BindNewPipeAndPassReceiver());
}

CanvasResourceDispatcher::~CanvasResourceDispatcher() = default;

namespace {

void UpdatePlaceholderImage(
    base::WeakPtr<CanvasResourceDispatcher> dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int placeholder_canvas_id,
    scoped_refptr<blink::CanvasResource>&& canvas_resource,
    viz::ResourceId resource_id) {
  DCHECK(IsMainThread());
  OffscreenCanvasPlaceholder* placeholder_canvas =
      OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
          placeholder_canvas_id);
  if (placeholder_canvas) {
    placeholder_canvas->SetOffscreenCanvasResource(std::move(canvas_resource),
                                                   resource_id);
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&CanvasResourceDispatcher::OnMainThreadReceivedImage,
                       dispatcher));
  }
}

void UpdatePlaceholderDispatcher(
    base::WeakPtr<CanvasResourceDispatcher> dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int placeholder_canvas_id) {
  OffscreenCanvasPlaceholder* placeholder_canvas =
      OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
          placeholder_canvas_id);
  // Note that the placeholder canvas may be destroyed when this post task get
  // to executed.
  if (placeholder_canvas)
    placeholder_canvas->SetOffscreenCanvasDispatcher(dispatcher, task_runner);
}

}  // namespace

void CanvasResourceDispatcher::PostImageToPlaceholderIfNotBlocked(
    scoped_refptr<CanvasResource>&& canvas_resource,
    viz::ResourceId resource_id) {
  if (placeholder_canvas_id_ == kInvalidPlaceholderCanvasId ||
      // `agent_group_scheduler_compositor_task_runner_` may be null if this
      // was created from a SharedWorker.
      !agent_group_scheduler_compositor_task_runner_) {
    // Inform the resource that the placeholder ref was released so it can do
    // any appropriate cleanup/recycling.
    CanvasResource::OnPlaceholderReleasedResource(std::move(canvas_resource));
    return;
  }

  // Determines whether the main thread may be blocked. If unblocked, post
  // |canvas_resource|. Otherwise, save it but do not post it.
  if (num_pending_placeholder_resources_ < kMaxPendingPlaceholderResources) {
    PostImageToPlaceholder(std::move(canvas_resource), resource_id);
    num_pending_placeholder_resources_++;
  } else {
    DCHECK(num_pending_placeholder_resources_ ==
           kMaxPendingPlaceholderResources);

    // The previous unposted resource becomes obsolete now.
    // Inform the resource that the placeholder ref was released so it can do
    // any appropriate cleanup/recycling.
    CanvasResource::OnPlaceholderReleasedResource(
        std::move(latest_unposted_resource_));

    latest_unposted_resource_ = std::move(canvas_resource);
    latest_unposted_resource_id_ = resource_id;
  }
}

void CanvasResourceDispatcher::PostImageToPlaceholder(
    scoped_refptr<CanvasResource>&& canvas_resource,
    viz::ResourceId resource_id) {
  // After this point, |canvas_resource| can only be used on the main thread,
  // until it is returned.
  canvas_resource->Transfer();

  CHECK(agent_group_scheduler_compositor_task_runner_);
  PostCrossThreadTask(
      *agent_group_scheduler_compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(UpdatePlaceholderImage, GetWeakPtr(), task_runner_,
                          placeholder_canvas_id_, std::move(canvas_resource),
                          resource_id));
}

void CanvasResourceDispatcher::DispatchFrame(
    scoped_refptr<CanvasResource>&& canvas_resource,
    const SkIRect& damage_rect,
    bool is_opaque) {
  TRACE_EVENT0("blink", "CanvasResourceDispatcher::DispatchFrame");
  viz::CompositorFrame frame;
  if (!PrepareFrame(std::move(canvas_resource), damage_rect, is_opaque,
                    &frame)) {
    return;
  }

  pending_compositor_frames_++;
  sink_->SubmitCompositorFrame(
      parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
      std::move(frame), std::nullopt, 0);
}

bool CanvasResourceDispatcher::PrepareFrame(
    scoped_refptr<CanvasResource>&& canvas_resource,
    const SkIRect& damage_rect,
    bool is_opaque,
    viz::CompositorFrame* frame) {
  TRACE_EVENT0("blink", "CanvasResourceDispatcher::PrepareFrame");
  if (!canvas_resource || !VerifyImageSize(canvas_resource->Size())) {
    return false;
  }

  auto next_resource_id = id_generator_.GenerateNextId();

  // For frameless canvas, we don't get a valid frame_sink_id and should drop.
  if (!frame_sink_id_.is_valid()) {
    PostImageToPlaceholderIfNotBlocked(std::move(canvas_resource),
                                       next_resource_id);
    return false;
  }

  // TODO(crbug.com/652931): update the device_scale_factor
  frame->metadata.device_scale_factor = 1.0f;
  if (!current_begin_frame_ack_.frame_id.IsSequenceValid()) {
    // TODO(eseckler): This shouldn't be necessary when OffscreenCanvas no
    // longer submits CompositorFrames without prior BeginFrame.
    current_begin_frame_ack_ = viz::BeginFrameAck::CreateManualAckWithDamage();
  } else {
    current_begin_frame_ack_.has_damage = true;
  }
  frame->metadata.begin_frame_ack = current_begin_frame_ack_;

  frame->metadata.frame_token = ++next_frame_token_;

  // Ask viz not to throttle us if we've not voluntarily suspended animation.
  // Typically, we'll suspend if we're hidden, unless we're hidden-but-painting.
  // In that case, we can still submit frames that will contribute, possibly
  // indirectly, to picture-in-picture content even if those frames are not
  // consumed by a viz frame sink directly.  In those cases, it might choose to
  // throttle us, incorrectly if we don't request otherwise.
  frame->metadata.may_throttle_if_undrawn_frames = IsAnimationSuspended();

  const gfx::Rect bounds(size_.width(), size_.height());
  constexpr viz::CompositorRenderPassId kRenderPassId{1};
  auto pass =
      viz::CompositorRenderPass::Create(/*shared_quad_state_list_size=*/1u,
                                        /*quad_list_size=*/1u);
  pass->SetNew(kRenderPassId, bounds,
               gfx::Rect(damage_rect.x(), damage_rect.y(), damage_rect.width(),
                         damage_rect.height()),
               gfx::Transform());

  viz::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds, bounds, gfx::MaskFilterInfo(),
              /*clip=*/std::nullopt, is_opaque, /*opacity_f=*/1.f,
              SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
              /*fast_rounded_corner=*/false);

  viz::TransferableResource resource;

  // This property will be overridden by the embedding SurfaceLayer, so this
  // value will have no effect.
  const bool nearest_neighbor = false;

  CanvasResource::ReleaseCallback release_callback;
  canvas_resource->PrepareTransferableResource(
      &resource, &release_callback,
      /*needs_verified_synctoken=*/true);

  const viz::ResourceId resource_id = next_resource_id;
  resource.id = resource_id;

  const gfx::Size resource_size = resource.GetSize();

  // Create a new ref on `canvas_resource` to pass to the placeholder, which
  // will manage the lifetime of this ref.
  auto resource_ref_for_placeholder = canvas_resource;
  PostImageToPlaceholderIfNotBlocked(std::move(resource_ref_for_placeholder),
                                     resource_id);

  // Now store our ref to ensure that the resource remains valid for the
  // duration of the compositor's usage (we'll drop our ref when the compositor
  // notifies us that it is no longer using the resource via
  // `ReclaimResources()`).
  exported_resources_.insert(resource_id, std::make_unique<ExportedResource>(
                                              std::move(canvas_resource),
                                              std::move(release_callback)));

  frame->resource_list.push_back(std::move(resource));

  viz::TextureDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();

  const bool needs_blending = !is_opaque;
  constexpr gfx::PointF uv_top_left(0.f, 0.f);
  quad->SetAll(sqs, bounds, bounds, needs_blending, resource_id, uv_top_left,
               gfx::PointF(resource_size.width(), resource_size.height()),
               SkColors::kTransparent, nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear,
               /*is_tex_coords_normalized=*/false);

  frame->render_pass_list.push_back(std::move(pass));

  if (change_size_for_next_commit_ ||
      !parent_local_surface_id_allocator_.HasValidLocalSurfaceId()) {
    parent_local_surface_id_allocator_.GenerateId();
    surface_embedder_->SetLocalSurfaceId(
        parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
    change_size_for_next_commit_ = false;
  }

  return true;
}

void CanvasResourceDispatcher::DidReceiveCompositorFrameAck(
    Vector<viz::ReturnedResource> resources) {
  ReclaimResources(std::move(resources));
  pending_compositor_frames_--;
  DCHECK_GE(pending_compositor_frames_, 0u);
}

void CanvasResourceDispatcher::SetNeedsBeginFrame(bool needs_begin_frame) {
  if (needs_begin_frame_ == needs_begin_frame) {
    // If the offscreencanvas is in the same tread as the canvas, and we are
    // trying for a second time to request the being frame, and we are in a
    // capture_stream scenario, we will call a BeginFrame right away. So
    // Offscreen Canvas can behave in a more synchronous way when it's on the
    // main thread.
    if (needs_begin_frame_ && IsMainThread()) {
      OffscreenCanvasPlaceholder* placeholder_canvas =
          OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
              placeholder_canvas_id_);
      if (placeholder_canvas &&
          placeholder_canvas->IsOffscreenCanvasRegistered() &&
          placeholder_canvas->HasCanvasCapture() && Client()) {
        Client()->BeginFrame();
      }
    }
    return;
  }
  needs_begin_frame_ = needs_begin_frame;
  UpdateBeginFrameSource();
}

void CanvasResourceDispatcher::SetAnimationState(
    AnimationState animation_state) {
  if (animation_state_ == animation_state) {
    return;
  }
  animation_state_ = animation_state;
  UpdateBeginFrameSource();
}

void CanvasResourceDispatcher::UpdateBeginFrameSource() {
  if (!sink_) {
    fake_frame_timer_.Stop();
    return;
  }

  bool needs_begin_frame = needs_begin_frame_ && !IsAnimationSuspended();
  if (needs_begin_frame &&
      animation_state_ == AnimationState::kActiveWithSyntheticTiming) {
    // Generate a synthetic OBF instead of asking viz, if we aren't already.
    sink_->SetNeedsBeginFrame(false);
    if (!fake_frame_timer_.IsActive()) {
      fake_frame_timer_.StartRepeating(kSyntheticFrameDelay, FROM_HERE);
    }
  } else {
    sink_->SetNeedsBeginFrame(needs_begin_frame);
    fake_frame_timer_.Stop();
  }
}

bool CanvasResourceDispatcher::HasTooManyPendingFrames() const {
  return pending_compositor_frames_ >= kMaxPendingCompositorFrames;
}

void CanvasResourceDispatcher::OnBeginFrame(
    const viz::BeginFrameArgs& begin_frame_args,
    const HashMap<uint32_t, viz::FrameTimingDetails>&,
    Vector<viz::ReturnedResource> resources) {
  if (!resources.empty()) {
    ReclaimResources(std::move(resources));
  }
  current_begin_frame_ack_ = viz::BeginFrameAck(begin_frame_args, false);
  if (HasTooManyPendingFrames() ||
      (begin_frame_args.type == viz::BeginFrameArgs::MISSED &&
       base::TimeTicks::Now() > begin_frame_args.deadline)) {
    sink_->DidNotProduceFrame(current_begin_frame_ack_);
    return;
  }

  // TODO(fserb): should EnqueueMicrotask BeginFrame().
  // We usually never get to BeginFrame if we are on RAF mode. But it could
  // still happen that begin frame gets requested and we don't have a frame
  // anymore, so we shouldn't let the compositor wait.
  const bool submitted_frame = Client() && Client()->BeginFrame();

  if (!submitted_frame) {
    sink_->DidNotProduceFrame(current_begin_frame_ack_);
  }

  // TODO(fserb): Update this with the correct value if we are on RAF submit.
  current_begin_frame_ack_.frame_id.sequence_number =
      viz::BeginFrameArgs::kInvalidFrameNumber;
}

void CanvasResourceDispatcher::OnFakeFrameTimer(TimerBase* timer) {
  viz::BeginFrameArgs begin_frame_args;
  if (HasTooManyPendingFrames() || !Client()) {
    return;
  }

  // Since this is a synthetic OBF, create a manual ack to go with it.
  current_begin_frame_ack_ = viz::BeginFrameAck::CreateManualAckWithDamage();
  // It doesn't matter if this succeeds or fails, because viz didn't ask for a
  // frame from us.
  Client()->BeginFrame();
  current_begin_frame_ack_.frame_id.sequence_number =
      viz::BeginFrameArgs::kInvalidFrameNumber;
}

void CanvasResourceDispatcher::ReclaimResources(
    Vector<viz::ReturnedResource> resources) {
  for (auto& resource : resources) {
    auto it = exported_resources_.find(resource.id);

    CHECK(it != exported_resources_.end());
    if (it == exported_resources_.end()) {
      continue;
    }

    it->value->ReleaseResource(resource.shared_image_export_result,
                               resource.lost);
    exported_resources_.erase(it);
  }
}

void CanvasResourceDispatcher::OnMainThreadReceivedImage() {
  num_pending_placeholder_resources_--;

  // The main thread has become unblocked recently and we have a resource that
  // has not been posted yet.
  if (latest_unposted_resource_) {
    DCHECK(num_pending_placeholder_resources_ ==
           kMaxPendingPlaceholderResources - 1);
    PostImageToPlaceholderIfNotBlocked(std::move(latest_unposted_resource_),
                                       latest_unposted_resource_id_);
    // To make it safe to use/check latest_unposted_resource_ after using
    // std::move on it, we need to force a reset because the move above is
    // elide-able.
    latest_unposted_resource_.reset();
    latest_unposted_resource_id_ = viz::kInvalidResourceId;
  }
}

bool CanvasResourceDispatcher::VerifyImageSize(const gfx::Size& image_size) {
  return image_size == size_;
}

void CanvasResourceDispatcher::Reshape(const gfx::Size& size) {
  if (size_ != size) {
    size_ = size;
    change_size_for_next_commit_ = true;
  }
}

void CanvasResourceDispatcher::SetPlaceholderCanvasDispatcher(
    int placeholder_canvas_id) {
  // `agent_group_scheduler_compositor_task_runner_` may be null if this
  // was created from a SharedWorker.
  if (!agent_group_scheduler_compositor_task_runner_)
    return;

  // If the offscreencanvas is in the same thread as the canvas, we will update
  // the canvas resource dispatcher directly. So Offscreen Canvas can behave in
  // a more synchronous way when it's on the main thread.
  if (IsMainThread()) {
    UpdatePlaceholderDispatcher(GetWeakPtr(), task_runner_,
                                placeholder_canvas_id);
  } else {
    PostCrossThreadTask(
        *agent_group_scheduler_compositor_task_runner_, FROM_HERE,
        CrossThreadBindOnce(UpdatePlaceholderDispatcher, GetWeakPtr(),
                            task_runner_, placeholder_canvas_id));
  }
}

}  // namespace blink
