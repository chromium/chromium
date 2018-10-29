// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_image_group.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/texture_pool.h"
#include "media/gpu/command_buffer_helper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

}  // namespace

using gpu::gles2::AbstractTexture;

VideoFrameFactoryImpl::VideoFrameFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCb get_stub_cb)
    : gpu_task_runner_(std::move(gpu_task_runner)),
      get_stub_cb_(std::move(get_stub_cb)) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gpu_video_frame_factory_)
    gpu_task_runner_->DeleteSoon(FROM_HERE, gpu_video_frame_factory_.release());
}

void VideoFrameFactoryImpl::Initialize(bool wants_promotion_hint,
                                       bool use_texture_owner_as_overlays,
                                       InitCb init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!gpu_video_frame_factory_);
  gpu_video_frame_factory_ = std::make_unique<GpuVideoFrameFactory>();
  base::PostTaskAndReplyWithResult(
      gpu_task_runner_.get(), FROM_HERE,
      base::Bind(&GpuVideoFrameFactory::Initialize,
                 base::Unretained(gpu_video_frame_factory_.get()),
                 wants_promotion_hint, use_texture_owner_as_overlays,
                 get_stub_cb_),
      std::move(init_cb));
}

void VideoFrameFactoryImpl::SetSurfaceBundle(
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  scoped_refptr<CodecImageGroup> image_group;
  if (!surface_bundle) {
    // Clear everything, just so we're not holding a reference.
    texture_owner_ = nullptr;
  } else {
    // If |surface_bundle| is using a TextureOwner, then get it.
    texture_owner_ =
        surface_bundle->overlay ? nullptr : surface_bundle->texture_owner_;

    // Start a new image group.  Note that there's no reason that we can't have
    // more than one group per surface bundle; it's okay if we're called
    // mulitiple times with the same surface bundle.  It just helps to combine
    // the callbacks if we don't, especially since AndroidOverlay doesn't know
    // how to remove destruction callbacks.  That's one reason why we don't just
    // make the CodecImage register itself.  The other is that the threading is
    // easier if we do it this way, since the image group is constructed on the
    // proper thread to talk to the overlay.
    image_group =
        base::MakeRefCounted<CodecImageGroup>(gpu_task_runner_, surface_bundle);
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoFrameFactory::SetImageGroup,
                     base::Unretained(gpu_video_frame_factory_.get()),
                     std::move(image_group)));
}

void VideoFrameFactoryImpl::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    OnceOutputCb output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoFrameFactory::CreateVideoFrame,
                     base::Unretained(gpu_video_frame_factory_.get()),
                     base::Passed(&output_buffer), texture_owner_, timestamp,
                     natural_size, std::move(promotion_hint_cb),
                     std::move(output_cb),
                     base::ThreadTaskRunnerHandle::Get()));
}

void VideoFrameFactoryImpl::RunAfterPendingVideoFrames(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Hop through |gpu_task_runner_| to ensure it comes after pending frames.
  gpu_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                     std::move(closure));
}

GpuVideoFrameFactory::GpuVideoFrameFactory() : weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

GpuVideoFrameFactory::~GpuVideoFrameFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_)
    stub_->RemoveDestructionObserver(this);
}

scoped_refptr<TextureOwner> GpuVideoFrameFactory::Initialize(
    bool wants_promotion_hint,
    bool use_texture_owner_as_overlays,
    VideoFrameFactoryImpl::GetStubCb get_stub_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  wants_promotion_hint_ = wants_promotion_hint;
  use_texture_owner_as_overlays_ = use_texture_owner_as_overlays;
  stub_ = get_stub_cb.Run();
  if (!MakeContextCurrent(stub_))
    return nullptr;
  stub_->AddDestructionObserver(this);

  texture_pool_ = new TexturePool(CommandBufferHelper::Create(stub_));

  decoder_helper_ = GLES2DecoderHelper::Create(stub_->decoder_context());
  return TextureOwner::Create();
}

void GpuVideoFrameFactory::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<TextureOwner> texture_owner_,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    VideoFrameFactory::OnceOutputCb output_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scoped_refptr<VideoFrame> frame;
  std::unique_ptr<AbstractTexture> texture;
  CodecImage* codec_image = nullptr;
  CreateVideoFrameInternal(std::move(output_buffer), std::move(texture_owner_),
                           timestamp, natural_size,
                           std::move(promotion_hint_cb), &frame, &texture,
                           &codec_image);
  TRACE_EVENT0("media", "GpuVideoFrameFactory::CreateVideoFrame");
  if (!frame || !texture)
    return;

  // Try to render this frame if possible.
  internal::MaybeRenderEarly(&images_);

  // Callback to notify us when |texture| is going to drop its ref to the
  // underlying texture.  This happens when we (a) are notified that |frame|
  // has been released by the renderer and the sync token has cleared, or (b)
  // when the stub is destroyed.  In the former case, we want to release any
  // codec resources as quickly as possible so that we can re-use them.  In
  // the latter case, decoding has stopped and we want to release any buffers
  // so that the MediaCodec instance can clean up.  Note that the texture will
  // remain renderable, but it won't necessarily refer to the frame it was
  // supposed to; it'll be the most recently rendered frame.
  auto cleanup_cb = base::BindOnce([](AbstractTexture* texture) {
    gl::GLImage* image = texture->GetImage();
    if (image)
      static_cast<CodecImage*>(image)->ReleaseCodecBuffer();
  });
  texture->SetCleanupCallback(std::move(cleanup_cb));

  // Note that this keeps the pool around while any texture is.
  auto drop_texture_ref = base::BindOnce(
      [](scoped_refptr<TexturePool> texture_pool, AbstractTexture* texture,
         const gpu::SyncToken& sync_token) {
        texture_pool->ReleaseTexture(texture, sync_token);
      },
      texture_pool_, base::Unretained(texture.get()));
  texture_pool_->AddTexture(std::move(texture));

  // Guarantee that the AbstractTexture is released even if the VideoFrame is
  // dropped. Otherwise we could keep TextureRefs we don't need alive.
  auto release_cb = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      BindToCurrentLoop(std::move(drop_texture_ref)), gpu::SyncToken());
  frame->SetReleaseMailboxCB(std::move(release_cb));
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(output_cb), std::move(frame)));
}

void GpuVideoFrameFactory::CreateVideoFrameInternal(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<TextureOwner> texture_owner_,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    scoped_refptr<VideoFrame>* video_frame_out,
    std::unique_ptr<AbstractTexture>* texture_out,
    CodecImage** codec_image_out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_))
    return;

  gpu::gles2::ContextGroup* group = stub_->decoder_context()->GetContextGroup();
  if (!group)
    return;
  gpu::gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager)
    return;

  gfx::Size size = output_buffer->size();
  gfx::Rect visible_rect(size);
  // The pixel format doesn't matter as long as it's valid for texture frames.
  VideoPixelFormat pixel_format = PIXEL_FORMAT_ARGB;

  // Check that we can create a VideoFrame for this config before creating the
  // TextureRef so that we don't have to clean up the TextureRef if creating the
  // frame fails.
  if (!VideoFrame::IsValidConfig(pixel_format, VideoFrame::STORAGE_OPAQUE, size,
                                 visible_rect, natural_size)) {
    return;
  }

  // Create a Texture and a CodecImage to back it.
  std::unique_ptr<AbstractTexture> texture = decoder_helper_->CreateTexture(
      GL_TEXTURE_EXTERNAL_OES, GL_RGBA, size.width(), size.height(), GL_RGBA,
      GL_UNSIGNED_BYTE);
  auto image = base::MakeRefCounted<CodecImage>(
      std::move(output_buffer), texture_owner_, std::move(promotion_hint_cb));
  images_.push_back(image.get());
  *codec_image_out = image.get();

  // Add |image| to our current image group.  This makes sure that any overlay
  // lasts as long as the images.  For TextureOwner, it doesn't do much.
  image_group_->AddCodecImage(image.get());

  // Attach the image to the texture.
  // Either way, we expect this to be UNBOUND (i.e., decoder-managed).  For
  // overlays, BindTexImage will return true, causing it to transition to the
  // BOUND state, and thus receive ScheduleOverlayPlane calls.  For TextureOwner
  // backed images, BindTexImage will return false, and CopyTexImage will be
  // tried next.
  // TODO(liberato): consider not binding this as a StreamTextureImage if we're
  // using an overlay.  There's no advantage.  We'd likely want to create (and
  // initialize to a 1x1 texture) a 2D texture above in that case, in case
  // somebody tries to sample from it.  Be sure that promotion hints still
  // work properly, though -- they might require a stream texture image.
  GLuint texture_owner_service_id =
      texture_owner_ ? texture_owner_->GetTextureId() : 0;
  texture->BindStreamTextureImage(image.get(), texture_owner_service_id);

  gpu::Mailbox mailbox = decoder_helper_->CreateMailbox(texture.get());
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] =
      gpu::MailboxHolder(mailbox, gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);

  auto frame = VideoFrame::WrapNativeTextures(
      pixel_format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), size,
      visible_rect, natural_size, timestamp);

  // The frames must be copied when threaded texture mailboxes are in use
  // (http://crbug.com/582170).
  if (group->gpu_preferences().enable_threaded_texture_mailboxes)
    frame->metadata()->SetBoolean(VideoFrameMetadata::COPY_REQUIRED, true);

  bool allow_overlay = false;
  if (use_texture_owner_as_overlays_) {
    DCHECK(texture_owner_);
    allow_overlay = true;
  } else {
    // We unconditionally mark the picture as overlayable, even if
    // |!texture_owner_|, if we want to get hints.  It's required, else we won't
    // get hints.
    allow_overlay = !texture_owner_ || wants_promotion_hint_;
  }

  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY,
                                allow_overlay);
  frame->metadata()->SetBoolean(VideoFrameMetadata::WANTS_PROMOTION_HINT,
                                wants_promotion_hint_);
  frame->metadata()->SetBoolean(VideoFrameMetadata::TEXTURE_OWNER,
                                !!texture_owner_);

  *video_frame_out = std::move(frame);
  *texture_out = std::move(texture);
}

void GpuVideoFrameFactory::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
  decoder_helper_ = nullptr;
}

void GpuVideoFrameFactory::OnImageDestructed(CodecImage* image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::Erase(images_, image);
  internal::MaybeRenderEarly(&images_);
}

void GpuVideoFrameFactory::SetImageGroup(
    scoped_refptr<CodecImageGroup> image_group) {
  image_group_ = std::move(image_group);

  if (!image_group_)
    return;

  image_group_->SetDestructionCb(base::BindRepeating(
      &GpuVideoFrameFactory::OnImageDestructed, weak_factory_.GetWeakPtr()));
}

}  // namespace media
