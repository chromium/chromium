// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_submitter.h"

#include <vector>

#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/filter_operations.h"
#include "cc/scheduler/video_frame_controller.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "media/base/video_frame.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

namespace blink {

namespace {

// Delay to retry getting the context_provider.
constexpr base::TimeDelta kGetContextProviderRetryTimeout =
    base::TimeDelta::FromMilliseconds(150);

}  // namespace

VideoFrameSubmitter::VideoFrameSubmitter(
    WebContextProviderCallback context_provider_callback,
    std::unique_ptr<VideoFrameResourceProvider> resource_provider)
    : binding_(this),
      context_provider_callback_(context_provider_callback),
      resource_provider_(std::move(resource_provider)),
      is_rendering_(false),
      weak_ptr_factory_(this) {
  DETACH_FROM_THREAD(media_thread_checker_);
}

VideoFrameSubmitter::~VideoFrameSubmitter() {
  if (context_provider_)
    context_provider_->RemoveObserver(this);
}

void VideoFrameSubmitter::SetRotation(media::VideoRotation rotation) {
  rotation_ = rotation;
}

void VideoFrameSubmitter::SetIsOpaque(bool is_opaque) {
  if (is_opaque_ == is_opaque)
    return;

  is_opaque_ = is_opaque;
  UpdateSubmissionStateInternal();
}

void VideoFrameSubmitter::EnableSubmission(
    viz::SurfaceId surface_id,
    base::TimeTicks local_surface_id_allocation_time,
    WebFrameSinkDestroyedCallback frame_sink_destroyed_callback) {
  // TODO(lethalantidote): Set these fields earlier in the constructor. Will
  // need to construct VideoFrameSubmitter later in order to do this.
  frame_sink_id_ = surface_id.frame_sink_id();
  frame_sink_destroyed_callback_ = frame_sink_destroyed_callback;
  child_local_surface_id_allocator_.UpdateFromParent(
      surface_id.local_surface_id(), local_surface_id_allocation_time);
  if (resource_provider_->IsInitialized())
    StartSubmitting();
}

void VideoFrameSubmitter::UpdateSubmissionState(bool should_submit) {
  should_submit_internal_ = should_submit;
  UpdateSubmissionStateInternal();
}

void VideoFrameSubmitter::SetForceSubmit(bool force_submit) {
  force_submit_ = force_submit;
  UpdateSubmissionStateInternal();
}

void VideoFrameSubmitter::UpdateSubmissionStateInternal() {
  if (compositor_frame_sink_) {
    compositor_frame_sink_->SetNeedsBeginFrame(IsDrivingFrameUpdates());
    if (ShouldSubmit())
      SubmitSingleFrame();
    else if (!frame_size_.IsEmpty())
      SubmitEmptyFrame();
  }
}

void VideoFrameSubmitter::StopUsingProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (is_rendering_)
    StopRendering();
  provider_ = nullptr;
}

void VideoFrameSubmitter::StopRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(is_rendering_);
  DCHECK(provider_);

  is_rendering_ = false;
  UpdateSubmissionStateInternal();
}

void VideoFrameSubmitter::SubmitSingleFrame() {
  // If we haven't gotten a valid result yet from |context_provider_callback_|
  // |resource_provider_| will remain uninitalized.
  if (!resource_provider_->IsInitialized())
    return;

  viz::BeginFrameAck current_begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  scoped_refptr<media::VideoFrame> video_frame = provider_->GetCurrentFrame();
  if (video_frame) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&VideoFrameSubmitter::SubmitFrame),
                       weak_ptr_factory_.GetWeakPtr(), current_begin_frame_ack,
                       video_frame));
    provider_->PutCurrentFrame();
  }
}

bool VideoFrameSubmitter::ShouldSubmit() const {
  return should_submit_internal_ || force_submit_;
}

bool VideoFrameSubmitter::IsDrivingFrameUpdates() const {
  // We drive frame updates only when we believe that something is consuming
  // them.  This is different than VideoLayer, which drives updates any time
  // they're in the layer tree.
  return is_rendering_ && ShouldSubmit();
}

void VideoFrameSubmitter::DidReceiveFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(provider_);

  // DidReceiveFrame is called before renderering has started, as a part of
  // PaintSingleFrame.
  if (!is_rendering_) {
    SubmitSingleFrame();
  }
}

void VideoFrameSubmitter::StartRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(!is_rendering_);
  is_rendering_ = true;

  if (compositor_frame_sink_)
    compositor_frame_sink_->SetNeedsBeginFrame(is_rendering_ && ShouldSubmit());
}

void VideoFrameSubmitter::Initialize(cc::VideoFrameProvider* provider) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (provider) {
    DCHECK(!provider_);
    provider_ = provider;
    context_provider_callback_.Run(
        nullptr, base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

void VideoFrameSubmitter::OnReceivedContextProvider(
    bool use_gpu_compositing,
    scoped_refptr<viz::ContextProvider> context_provider) {
  if (!use_gpu_compositing) {
    resource_provider_->Initialize(nullptr, this);
    return;
  }

  bool has_good_context = false;
  while (!has_good_context) {
    if (!context_provider) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              context_provider_callback_, context_provider_,
              base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                             weak_ptr_factory_.GetWeakPtr())),
          kGetContextProviderRetryTimeout);
      return;
    }

    // Note that |context_provider| is now null after the move, such that if we
    // end up having !|has_good_context|, we will retry to obtain the
    // context_provider.
    context_provider_ = std::move(context_provider);
    auto result = context_provider_->BindToCurrentThread();

    has_good_context =
        result == gpu::ContextResult::kSuccess &&
        context_provider_->ContextGL()->GetGraphicsResetStatusKHR() ==
            GL_NO_ERROR;
  }
  context_provider_->AddObserver(this);
  resource_provider_->Initialize(context_provider_.get(), nullptr);

  if (frame_sink_id_.is_valid())
    StartSubmitting();
}

void VideoFrameSubmitter::StartSubmitting() {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(frame_sink_id_.is_valid());

  mojom::blink::EmbeddedFrameSinkProviderPtr provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&provider));

  viz::mojom::blink::CompositorFrameSinkClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  provider->CreateCompositorFrameSink(
      frame_sink_id_, std::move(client),
      mojo::MakeRequest(&compositor_frame_sink_));

  compositor_frame_sink_.set_connection_error_handler(base::BindOnce(
      &VideoFrameSubmitter::OnContextLost, base::Unretained(this)));

  UpdateSubmissionStateInternal();
}

bool VideoFrameSubmitter::SubmitFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    scoped_refptr<media::VideoFrame> video_frame) {
  TRACE_EVENT0("media", "VideoFrameSubmitter::SubmitFrame");
  DCHECK(video_frame);
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  if (!compositor_frame_sink_ || !ShouldSubmit())
    return false;

  if (frame_size_ != gfx::Rect(video_frame->coded_size())) {
    if (!frame_size_.IsEmpty())
      child_local_surface_id_allocator_.GenerateId();
    frame_size_ = gfx::Rect(video_frame->coded_size());
  }

  viz::CompositorFrame compositor_frame;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();

  render_pass->SetNew(1, frame_size_, frame_size_, gfx::Transform());
  render_pass->filters = cc::FilterOperations();
  resource_provider_->AppendQuads(render_pass.get(), video_frame, rotation_,
                                  is_opaque_);
  compositor_frame.metadata.begin_frame_ack = begin_frame_ack;
  // We don't assume that the ack is marked as having damage.  However, we're
  // definitely emitting a CompositorFrame that damages the entire surface.
  compositor_frame.metadata.begin_frame_ack.has_damage = true;
  compositor_frame.metadata.device_scale_factor = 1;
  compositor_frame.metadata.may_contain_video = true;

  std::vector<viz::ResourceId> resources;
  DCHECK_LE(render_pass->quad_list.size(), 1u);
  if (!render_pass->quad_list.empty()) {
    for (viz::ResourceId resource_id :
         render_pass->quad_list.front()->resources) {
      resources.push_back(resource_id);
    }
  }
  resource_provider_->PrepareSendToParent(resources,
                                          &compositor_frame.resource_list);
  compositor_frame.render_pass_list.push_back(std::move(render_pass));
  compositor_frame.metadata.local_surface_id_allocation_time =
      child_local_surface_id_allocator_.allocation_time();

  // TODO(lethalantidote): Address third/fourth arg in SubmitCompositorFrame.
  compositor_frame_sink_->SubmitCompositorFrame(
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
      std::move(compositor_frame), nullptr, 0);
  resource_provider_->ReleaseFrameResources();

  waiting_for_compositor_ack_ = true;
  return true;
}

void VideoFrameSubmitter::SubmitEmptyFrame() {
  TRACE_EVENT0("media", "VideoFrameSubmitter::SubmitEmptyFrame");
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  DCHECK(compositor_frame_sink_ && !ShouldSubmit());
  DCHECK(!frame_size_.IsEmpty());

  viz::CompositorFrame compositor_frame;

  compositor_frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  compositor_frame.metadata.device_scale_factor = 1;
  compositor_frame.metadata.may_contain_video = true;

  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  render_pass->SetNew(1, frame_size_, frame_size_, gfx::Transform());
  compositor_frame.render_pass_list.push_back(std::move(render_pass));

  compositor_frame_sink_->SubmitCompositorFrame(
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
      std::move(compositor_frame), nullptr, 0);
  waiting_for_compositor_ack_ = true;
}

void VideoFrameSubmitter::OnBeginFrame(const viz::BeginFrameArgs& args) {
  TRACE_EVENT0("media", "VideoFrameSubmitter::OnBeginFrame");
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  viz::BeginFrameAck current_begin_frame_ack(args, false);
  if (args.type == viz::BeginFrameArgs::MISSED) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    return;
  }

  // Update the current frame, even if we haven't gotten an ack for a previous
  // frame yet.  That probably signals a dropped frame, and this will let the
  // provider know that it happened, since we won't PutCurrentFrame this one.
  // Note that we should DidNotProduceFrame with or without the ack.
  if (!provider_ ||
      !provider_->UpdateCurrentFrame(args.frame_time + args.interval,
                                     args.frame_time + 2 * args.interval)) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    return;
  }

  scoped_refptr<media::VideoFrame> video_frame = provider_->GetCurrentFrame();

  // We do have a new frame that we could display.  See if we're supposed to
  // actually submit a frame or not, and try to submit one.
  if (!is_rendering_ || waiting_for_compositor_ack_ ||
      !SubmitFrame(current_begin_frame_ack, std::move(video_frame))) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    return;
  }

  // We submitted a frame!

  // We still signal PutCurrentFrame here, rather than on the ack, so that it
  // lines up with the correct frame.  Otherwise, any intervening calls to
  // OnBeginFrame => UpdateCurrentFrame will cause the put to signal that the
  // later frame was displayed.
  provider_->PutCurrentFrame();
}

void VideoFrameSubmitter::OnContextLost() {
  // TODO(lethalantidote): This check will be obsolete once other TODO to move
  // field initialization earlier is fulfilled.
  if (frame_sink_destroyed_callback_)
    frame_sink_destroyed_callback_.Run();

  if (binding_.is_bound())
    binding_.Unbind();

  if (context_provider_) {
    context_provider_->RemoveObserver(this);
  }
  waiting_for_compositor_ack_ = false;

  resource_provider_->OnContextLost();

  // |compositor_frame_sink_| should be reset last.
  compositor_frame_sink_.reset();

  context_provider_callback_.Run(
      context_provider_,
      base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                     weak_ptr_factory_.GetWeakPtr()));

  // We need to trigger another submit so that surface_id's get propagated
  // correctly. If we don't, we don't get any more signals to update the
  // submission state.
  should_submit_internal_ = true;
}

void VideoFrameSubmitter::DidReceiveCompositorFrameAck(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  ReclaimResources(resources);

  waiting_for_compositor_ack_ = false;
}

void VideoFrameSubmitter::ReclaimResources(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(media_thread_checker_);
  WebVector<viz::ReturnedResource> temp_resources = resources;
  std::vector<viz::ReturnedResource> std_resources =
      temp_resources.ReleaseVector();
  resource_provider_->ReceiveReturnsFromParent(std_resources);
}

void VideoFrameSubmitter::DidPresentCompositorFrame(
    uint32_t presentation_token,
    ::gfx::mojom::blink::PresentationFeedbackPtr feedback) {}

void VideoFrameSubmitter::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const viz::SharedBitmapId& id) {
  DCHECK(compositor_frame_sink_);
  compositor_frame_sink_->DidAllocateSharedBitmap(
      std::move(buffer), SharedBitmapIdToGpuMailboxPtr(id));
}

void VideoFrameSubmitter::DidDeleteSharedBitmap(const viz::SharedBitmapId& id) {
  DCHECK(compositor_frame_sink_);
  compositor_frame_sink_->DidDeleteSharedBitmap(
      SharedBitmapIdToGpuMailboxPtr(id));
}

void VideoFrameSubmitter::SetSurfaceIdForTesting(
    const viz::SurfaceId& surface_id,
    base::TimeTicks allocation_time) {
  frame_sink_id_ = surface_id.frame_sink_id();
  child_local_surface_id_allocator_.UpdateFromParent(
      surface_id.local_surface_id(), allocation_time);
}

}  // namespace blink
