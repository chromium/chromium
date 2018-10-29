// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"

#include <memory>
#include "base/single_thread_task_runner.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_placeholder.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

enum {
  kMaxPendingCompositorFrames = 2,
  kMaxUnreclaimedPlaceholderFrames = 3,
};

struct CanvasResourceDispatcher::FrameResource {
  FrameResource() = default;
  ~FrameResource() {
    if (release_callback)
      release_callback->Run(sync_token, is_lost);
  }

  // TODO(junov):  What does this do?
  bool spare_lock = true;

  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  gpu::SyncToken sync_token;
  bool is_lost = false;
};

CanvasResourceDispatcher::CanvasResourceDispatcher(
    CanvasResourceDispatcherClient* client,
    uint32_t client_id,
    uint32_t sink_id,
    int canvas_id,
    const IntSize& size)
    : frame_sink_id_(viz::FrameSinkId(client_id, sink_id)),
      size_(size),
      change_size_for_next_commit_(false),
      needs_begin_frame_(false),
      binding_(this),
      placeholder_canvas_id_(canvas_id),
      num_unreclaimed_frames_posted_(0),
      client_(client),
      weak_ptr_factory_(this) {
  // Frameless canvas pass an invalid |frame_sink_id_|; don't create mojo
  // channel for this special case.
  if (!frame_sink_id_.is_valid())
    return;

  DCHECK(!sink_.is_bound());
  mojom::blink::EmbeddedFrameSinkProviderPtr provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&provider));

  DCHECK(provider);
  binding_.Bind(mojo::MakeRequest(&client_ptr_));
  provider->CreateCompositorFrameSink(frame_sink_id_, std::move(client_ptr_),
                                      mojo::MakeRequest(&sink_));
}

CanvasResourceDispatcher::~CanvasResourceDispatcher() = default;

namespace {

void UpdatePlaceholderImage(
    base::WeakPtr<CanvasResourceDispatcher> dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int placeholder_canvas_id,
    scoped_refptr<blink::CanvasResource> canvas_resource,
    viz::ResourceId resource_id) {
  DCHECK(IsMainThread());
  OffscreenCanvasPlaceholder* placeholder_canvas =
      OffscreenCanvasPlaceholder::GetPlaceholderById(placeholder_canvas_id);
  if (placeholder_canvas) {
    placeholder_canvas->SetPlaceholderFrame(
        std::move(canvas_resource), std::move(dispatcher),
        std::move(task_runner), resource_id);
  }
}

}  // namespace

void CanvasResourceDispatcher::PostImageToPlaceholderIfNotBlocked(
    scoped_refptr<CanvasResource> canvas_resource,
    viz::ResourceId resource_id) {
  if (placeholder_canvas_id_ == kInvalidPlaceholderCanvasId) {
    ReclaimResourceInternal(resource_id);
    return;
  }
  // Determines whether the main thread may be blocked. If unblocked, post
  // |canvas_resource|. Otherwise, save it but do not post it.
  if (num_unreclaimed_frames_posted_ < kMaxUnreclaimedPlaceholderFrames) {
    this->PostImageToPlaceholder(std::move(canvas_resource), resource_id);
    num_unreclaimed_frames_posted_++;
  } else {
    DCHECK(num_unreclaimed_frames_posted_ == kMaxUnreclaimedPlaceholderFrames);
    if (latest_unposted_image_) {
      // The previous unposted resource becomes obsolete now.
      ReclaimResourceInternal(latest_unposted_resource_id_);
    }

    latest_unposted_image_ = std::move(canvas_resource);
    latest_unposted_resource_id_ = resource_id;
  }
}

void CanvasResourceDispatcher::PostImageToPlaceholder(
    scoped_refptr<CanvasResource> canvas_resource,
    viz::ResourceId resource_id) {
  scoped_refptr<base::SingleThreadTaskRunner> dispatcher_task_runner =
      Platform::Current()->CurrentThread()->GetTaskRunner();

  // After this point, |canvas_resource| can only be used on the main thread,
  // until it is returned.
  canvas_resource->Transfer();

  PostCrossThreadTask(
      *Platform::Current()->MainThread()->Scheduler()->CompositorTaskRunner(),
      FROM_HERE,
      CrossThreadBind(UpdatePlaceholderImage, this->GetWeakPtr(),
                      WTF::Passed(std::move(dispatcher_task_runner)),
                      placeholder_canvas_id_, std::move(canvas_resource),
                      resource_id));
}

void CanvasResourceDispatcher::DispatchFrameSync(
    scoped_refptr<CanvasResource> canvas_resource,
    base::TimeTicks commit_start_time,
    const SkIRect& damage_rect,
    bool needs_vertical_flip,
    bool is_opaque) {
  TRACE_EVENT0("blink", "CanvasResourceDispatcher::DispatchFrameSync");
  viz::CompositorFrame frame;
  if (!PrepareFrame(std::move(canvas_resource), commit_start_time, damage_rect,
                    needs_vertical_flip, is_opaque, &frame)) {
    return;
  }

  pending_compositor_frames_++;
  WTF::Vector<viz::ReturnedResource> resources;
  sink_->SubmitCompositorFrameSync(
      parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
      std::move(frame), nullptr, 0, &resources);
  DidReceiveCompositorFrameAck(resources);
}

void CanvasResourceDispatcher::DispatchFrame(
    scoped_refptr<CanvasResource> canvas_resource,
    base::TimeTicks commit_start_time,
    const SkIRect& damage_rect,
    bool needs_vertical_flip,
    bool is_opaque) {
  TRACE_EVENT0("blink", "CanvasResourceDispatcher::DispatchFrame");
  viz::CompositorFrame frame;
  if (!PrepareFrame(std::move(canvas_resource), commit_start_time, damage_rect,
                    needs_vertical_flip, is_opaque, &frame)) {
    return;
  }

  pending_compositor_frames_++;
  sink_->SubmitCompositorFrame(
      parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
      std::move(frame), nullptr, 0);
}

bool CanvasResourceDispatcher::PrepareFrame(
    scoped_refptr<CanvasResource> canvas_resource,
    base::TimeTicks commit_start_time,
    const SkIRect& damage_rect,
    bool needs_vertical_flip,
    bool is_opaque,
    viz::CompositorFrame* frame) {
  TRACE_EVENT0("blink", "CanvasResourceDispatcher::PrepareFrame");
  if (!canvas_resource || !VerifyImageSize(canvas_resource->Size())) {
    return false;
  }

  next_resource_id_++;

  // For frameless canvas, we don't get a valid frame_sink_id and should drop.
  if (!frame_sink_id_.is_valid()) {
    PostImageToPlaceholderIfNotBlocked(std::move(canvas_resource),
                                       next_resource_id_);
    return false;
  }

  // TODO(crbug.com/652931): update the device_scale_factor
  frame->metadata.device_scale_factor = 1.0f;
  if (current_begin_frame_ack_.sequence_number ==
      viz::BeginFrameArgs::kInvalidFrameNumber) {
    // TODO(eseckler): This shouldn't be necessary when OffscreenCanvas no
    // longer submits CompositorFrames without prior BeginFrame.
    current_begin_frame_ack_ = viz::BeginFrameAck::CreateManualAckWithDamage();
  } else {
    current_begin_frame_ack_.has_damage = true;
  }
  frame->metadata.begin_frame_ack = current_begin_frame_ack_;

  const gfx::Rect bounds(size_.Width(), size_.Height());
  constexpr int kRenderPassId = 1;
  constexpr bool is_clipped = false;
  std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
  pass->SetNew(kRenderPassId, bounds,
               gfx::Rect(damage_rect.x(), damage_rect.y(), damage_rect.width(),
                         damage_rect.height()),
               gfx::Transform());

  viz::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds, bounds, bounds, is_clipped, is_opaque,
              1.f, SkBlendMode::kSrcOver, 0);

  OffscreenCanvasCommitType commit_type;
  if (canvas_resource->IsAccelerated()) {
    // While |image| is texture backed, it could be generated with "software
    // rendering" aka swiftshader. If the compositor is not also using
    // swiftshader, then we could not give a swiftshader based texture
    // to the compositor. However in that case, IsGpuCompositingEnabled() will
    // also be false, so we will avoid doing so.
    if (SharedGpuContext::IsGpuCompositingEnabled()) {
      // Case 1: both canvas and compositor are gpu accelerated.
      commit_type = kCommitGPUCanvasGPUCompositing;
    } else {
      // Case 2: canvas is accelerated but gpu compositing is disabled.
      commit_type = kCommitGPUCanvasSoftwareCompositing;
    }
  } else {
    if (SharedGpuContext::IsGpuCompositingEnabled()) {
      // Case 3: canvas is not gpu-accelerated, but compositor is.
      commit_type = kCommitSoftwareCanvasGPUCompositing;
    } else {
      // Case 4: both canvas and compositor are not gpu accelerated.
      commit_type = kCommitSoftwareCanvasSoftwareCompositing;
    }
  }

  viz::TransferableResource resource;
  auto frame_resource = std::make_unique<FrameResource>();

  canvas_resource->PrepareTransferableResource(
      &resource, &frame_resource->release_callback, kVerifiedSyncToken);
  resource.id = next_resource_id_;

  resources_.insert(next_resource_id_, std::move(frame_resource));

  // TODO(crbug.com/869913): add unit testing for this.
  const gfx::Size canvas_resource_size(canvas_resource->Size());

  PostImageToPlaceholderIfNotBlocked(std::move(canvas_resource),
                                     next_resource_id_);

  frame->resource_list.push_back(std::move(resource));

  viz::TextureDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();

  const bool needs_blending = !is_opaque;
  // TODO(crbug.com/645993): this should be inherited from WebGL context's
  // creation settings.
  constexpr bool kPremultipliedAlpha = true;
  constexpr gfx::PointF uv_top_left(0.f, 0.f);
  constexpr gfx::PointF uv_bottom_right(1.f, 1.f);
  constexpr float vertex_opacity[4] = {1.f, 1.f, 1.f, 1.f};
  // TODO(crbug.com/645994): this should be true when using style
  // "image-rendering: pixelated".
  // TODO(crbug.com/645590): filter should respect the image-rendering CSS
  // property of associated canvas element.
  constexpr bool kNearestNeighbor = false;
  // Accelerated resources have the origin of coordinates in the upper left
  // corner while canvases have it in the lower left corner. The DrawQuad is
  // marked as vertically flipped unless someone else has done the flip for us.
  const bool yflipped =
      SharedGpuContext::IsGpuCompositingEnabled() && needs_vertical_flip;
  quad->SetAll(sqs, bounds, bounds, needs_blending, resource.id,
               canvas_resource_size, kPremultipliedAlpha, uv_top_left,
               uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity, yflipped,
               kNearestNeighbor, false);

  frame->render_pass_list.push_back(std::move(pass));

  base::TimeDelta elapsed_time = WTF::CurrentTimeTicks() - commit_start_time;

  switch (commit_type) {
    case kCommitGPUCanvasGPUCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, commit_gpu_canvas_gpu_compositing_main_timer,
            ("Blink.Canvas.OffscreenCommit.GPUCanvasGPUCompositingMain", 0,
             10000000, 50));
        commit_gpu_canvas_gpu_compositing_main_timer.CountMicroseconds(
            elapsed_time);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_gpu_canvas_gpu_compositing_worker_timer,
            ("Blink.Canvas.OffscreenCommit.GPUCanvasGPUCompositingWorker", 0,
             10000000, 50));
        commit_gpu_canvas_gpu_compositing_worker_timer.CountMicroseconds(
            elapsed_time);
      }
      break;
    case kCommitGPUCanvasSoftwareCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_gpu_canvas_software_compositing_main_timer,
            ("Blink.Canvas.OffscreenCommit.GPUCanvasSoftwareCompositingMain", 0,
             10000000, 50));
        commit_gpu_canvas_software_compositing_main_timer.CountMicroseconds(
            elapsed_time);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_gpu_canvas_software_compositing_worker_timer,
            ("Blink.Canvas.OffscreenCommit."
             "GPUCanvasSoftwareCompositingWorker",
             0, 10000000, 50));
        commit_gpu_canvas_software_compositing_worker_timer.CountMicroseconds(
            elapsed_time);
      }
      break;
    case kCommitSoftwareCanvasGPUCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_gpu_compositing_main_timer,
            ("Blink.Canvas.OffscreenCommit.SoftwareCanvasGPUCompositingMain", 0,
             10000000, 50));
        commit_software_canvas_gpu_compositing_main_timer.CountMicroseconds(
            elapsed_time);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_gpu_compositing_worker_timer,
            ("Blink.Canvas.OffscreenCommit."
             "SoftwareCanvasGPUCompositingWorker",
             0, 10000000, 50));
        commit_software_canvas_gpu_compositing_worker_timer.CountMicroseconds(
            elapsed_time);
      }
      break;
    case kCommitSoftwareCanvasSoftwareCompositing:
      if (IsMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_software_compositing_main_timer,
            ("Blink.Canvas.OffscreenCommit."
             "SoftwareCanvasSoftwareCompositingMain",
             0, 10000000, 50));
        commit_software_canvas_software_compositing_main_timer
            .CountMicroseconds(elapsed_time);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commit_software_canvas_software_compositing_worker_timer,
            ("Blink.Canvas.OffscreenCommit."
             "SoftwareCanvasSoftwareCompositingWorker",
             0, 10000000, 50));
        commit_software_canvas_software_compositing_worker_timer
            .CountMicroseconds(elapsed_time);
      }
      break;
    case kOffscreenCanvasCommitTypeCount:
      NOTREACHED();
  }

  if (change_size_for_next_commit_) {
    parent_local_surface_id_allocator_.GenerateId();
    change_size_for_next_commit_ = false;
  }

  return true;
}

void CanvasResourceDispatcher::DidReceiveCompositorFrameAck(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  ReclaimResources(resources);
  pending_compositor_frames_--;
  DCHECK_GE(pending_compositor_frames_, 0);
}

void CanvasResourceDispatcher::DidPresentCompositorFrame(
    uint32_t presentation_token,
    ::gfx::mojom::blink::PresentationFeedbackPtr feedback) {
  NOTIMPLEMENTED();
}

void CanvasResourceDispatcher::SetNeedsBeginFrame(bool needs_begin_frame) {
  if (needs_begin_frame_ == needs_begin_frame)
    return;
  needs_begin_frame_ = needs_begin_frame;
  if (!suspend_animation_)
    SetNeedsBeginFrameInternal();
}

void CanvasResourceDispatcher::SetSuspendAnimation(bool suspend_animation) {
  if (suspend_animation_ == suspend_animation)
    return;
  suspend_animation_ = suspend_animation;
  if (needs_begin_frame_)
    SetNeedsBeginFrameInternal();
}

void CanvasResourceDispatcher::SetNeedsBeginFrameInternal() {
  if (sink_)
    sink_->SetNeedsBeginFrame(needs_begin_frame_ && !suspend_animation_);
}

void CanvasResourceDispatcher::OnBeginFrame(
    const viz::BeginFrameArgs& begin_frame_args) {
  current_begin_frame_ack_ = viz::BeginFrameAck(begin_frame_args, false);
  if (pending_compositor_frames_ >= kMaxPendingCompositorFrames ||
      (begin_frame_args.type == viz::BeginFrameArgs::MISSED &&
       base::TimeTicks::Now() > begin_frame_args.deadline)) {
    sink_->DidNotProduceFrame(current_begin_frame_ack_);
    return;
  }

  if (Client())
    Client()->BeginFrame();
  // TODO(eseckler): Tell |sink_| if we did not draw during the BeginFrame.
  current_begin_frame_ack_.sequence_number =
      viz::BeginFrameArgs::kInvalidFrameNumber;
}

void CanvasResourceDispatcher::ReclaimResources(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  for (const auto& resource : resources) {
    auto it = resources_.find(resource.id);

    DCHECK(it != resources_.end());
    if (it == resources_.end())
      continue;

    it->value->sync_token = resource.sync_token;
    it->value->is_lost = resource.lost;
    ReclaimResourceInternal(it);
  }
}

void CanvasResourceDispatcher::ReclaimResource(viz::ResourceId resource_id) {
  ReclaimResourceInternal(resource_id);

  num_unreclaimed_frames_posted_--;

  // The main thread has become unblocked recently and we have an image that
  // have not been posted yet.
  if (latest_unposted_image_) {
    DCHECK(num_unreclaimed_frames_posted_ ==
           kMaxUnreclaimedPlaceholderFrames - 1);
    PostImageToPlaceholderIfNotBlocked(std::move(latest_unposted_image_),
                                       latest_unposted_resource_id_);
    latest_unposted_resource_id_ = 0;
  }
}

bool CanvasResourceDispatcher::VerifyImageSize(const IntSize image_size) {
  return image_size == size_;
}

void CanvasResourceDispatcher::Reshape(const IntSize& size) {
  if (size_ != size) {
    size_ = size;
    change_size_for_next_commit_ = true;
  }
}

void CanvasResourceDispatcher::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    ::gpu::mojom::blink::MailboxPtr id) {
  if (sink_)
    sink_->DidAllocateSharedBitmap(std::move(buffer), std::move(id));
}

void CanvasResourceDispatcher::DidDeleteSharedBitmap(
    ::gpu::mojom::blink::MailboxPtr id) {
  if (sink_)
    sink_->DidDeleteSharedBitmap(std::move(id));
}

void CanvasResourceDispatcher::ReclaimResourceInternal(
    viz::ResourceId resource_id) {
  auto it = resources_.find(resource_id);
  if (it != resources_.end())
    ReclaimResourceInternal(it);
}

void CanvasResourceDispatcher::ReclaimResourceInternal(
    const ResourceMap::iterator& it) {
  if (it->value->spare_lock) {
    it->value->spare_lock = false;
    return;
  }
  resources_.erase(it);
}

}  // namespace blink
