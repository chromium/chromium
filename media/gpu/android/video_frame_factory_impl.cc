// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_image_group.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/maybe_render_early_manager.h"
#include "media/gpu/command_buffer_helper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/scoped_make_current.h"

namespace media {
namespace {

gpu::TextureOwner::Mode GetTextureOwnerMode(
    VideoFrameFactory::OverlayMode overlay_mode) {
  const bool a_image_reader_supported =
      base::android::AndroidImageReader::GetInstance().IsSupported();

  switch (overlay_mode) {
    case VideoFrameFactory::OverlayMode::kDontRequestPromotionHints:
    case VideoFrameFactory::OverlayMode::kRequestPromotionHints:
      return a_image_reader_supported && base::FeatureList::IsEnabled(
                                             media::kAImageReaderVideoOutput)
                 ? gpu::TextureOwner::Mode::kAImageReaderInsecure
                 : gpu::TextureOwner::Mode::kSurfaceTextureInsecure;
    case VideoFrameFactory::OverlayMode::kSurfaceControlSecure:
      DCHECK(a_image_reader_supported);
      return gpu::TextureOwner::Mode::kAImageReaderSecureSurfaceControl;
    case VideoFrameFactory::OverlayMode::kSurfaceControlInsecure:
      DCHECK(a_image_reader_supported);
      return gpu::TextureOwner::Mode::kAImageReaderInsecureSurfaceControl;
  }

  NOTREACHED();
  return gpu::TextureOwner::Mode::kSurfaceTextureInsecure;
}

// Run on the GPU main thread to allocate the texture owner, and return it
// via |init_cb|.
static void AllocateTextureOwnerOnGpuThread(
    VideoFrameFactory::InitCb init_cb,
    VideoFrameFactory::OverlayMode overlay_mode,
    scoped_refptr<gpu::SharedContextState> shared_context_state) {
  if (!shared_context_state) {
    std::move(init_cb).Run(nullptr);
    return;
  }

  std::move(init_cb).Run(gpu::TextureOwner::Create(
      gpu::TextureOwner::CreateTexture(shared_context_state),
      GetTextureOwnerMode(overlay_mode)));
}

}  // namespace

using gpu::gles2::AbstractTexture;

VideoFrameFactoryImpl::VideoFrameFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    const gpu::GpuPreferences& gpu_preferences,
    std::unique_ptr<SharedImageVideoProvider> image_provider,
    std::unique_ptr<MaybeRenderEarlyManager> mre_manager,
    base::SequenceBound<YCbCrHelper> ycbcr_helper)
    : image_provider_(std::move(image_provider)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      enable_threaded_texture_mailboxes_(
          gpu_preferences.enable_threaded_texture_mailboxes),
      mre_manager_(std::move(mre_manager)),
      ycbcr_helper_(std::move(ycbcr_helper)) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoFrameFactoryImpl::Initialize(OverlayMode overlay_mode,
                                       InitCb init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  overlay_mode_ = overlay_mode;
  // On init success, create the TextureOwner and hop it back to this thread to
  // call |init_cb|.
  auto gpu_init_cb =
      base::BindOnce(&AllocateTextureOwnerOnGpuThread,
                     BindToCurrentLoop(std::move(init_cb)), overlay_mode);
  image_provider_->Initialize(std::move(gpu_init_cb));
}

void VideoFrameFactoryImpl::SetSurfaceBundle(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  scoped_refptr<CodecImageGroup> image_group;

  // Increase the generation ID used by the shared image provider, since we're
  // changing the TextureOwner.  This is temporary.  See ImageSpec.
  image_spec_.generation_id++;

  if (!surface_bundle) {
    // Clear everything, just so we're not holding a reference.
    codec_buffer_wait_coordinator_ = nullptr;
  } else {
    // If |surface_bundle| is using a CodecBufferWaitCoordinator, then get it.
    // Note that the only reason we need this is for legacy mailbox support; we
    // send it to the SharedImageVideoProvider so that (eventually) it can get
    // the service id from the owner for the legacy mailbox texture.  Otherwise,
    // this would be a lot simpler.
    codec_buffer_wait_coordinator_ =
        surface_bundle->overlay()
            ? nullptr
            : surface_bundle->codec_buffer_wait_coordinator();

    // TODO(liberato): When we enable pooling, do we need to clear the pool
    // here because the CodecImageGroup has changed?  It's unclear, since the
    // CodecImage shouldn't be in any group once we re-use it, so maybe it's
    // fine to take no action.

    mre_manager_->SetSurfaceBundle(std::move(surface_bundle));
  }
}

void VideoFrameFactoryImpl::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    OnceOutputCb output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gfx::Size coded_size = output_buffer->size();
  gfx::Rect visible_rect(coded_size);
  // The pixel format doesn't matter as long as it's valid for texture frames.
  VideoPixelFormat pixel_format = PIXEL_FORMAT_ARGB;

  // Check that we can create a VideoFrame for this config before trying to
  // create the textures for it.
  if (!VideoFrame::IsValidConfig(pixel_format, VideoFrame::STORAGE_OPAQUE,
                                 coded_size, visible_rect, natural_size)) {
    LOG(ERROR) << __func__ << " unsupported video frame format";
    std::move(output_cb).Run(nullptr);
    return;
  }

  // Update the current spec to match the size.
  image_spec_.size = coded_size;

  auto image_ready_cb = base::BindOnce(
      &VideoFrameFactoryImpl::CreateVideoFrame_OnImageReady,
      weak_factory_.GetWeakPtr(), std::move(output_cb), timestamp, coded_size,
      natural_size, std::move(output_buffer), codec_buffer_wait_coordinator_,
      std::move(promotion_hint_cb), pixel_format, overlay_mode_,
      enable_threaded_texture_mailboxes_, gpu_task_runner_);

  image_provider_->RequestImage(
      std::move(image_ready_cb), image_spec_,
      codec_buffer_wait_coordinator_
          ? codec_buffer_wait_coordinator_->texture_owner()
          : nullptr);
}

// static
void VideoFrameFactoryImpl::CreateVideoFrame_OnImageReady(
    base::WeakPtr<VideoFrameFactoryImpl> thiz,
    OnceOutputCb output_cb,
    base::TimeDelta timestamp,
    gfx::Size coded_size,
    gfx::Size natural_size,
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    VideoPixelFormat pixel_format,
    OverlayMode overlay_mode,
    bool enable_threaded_texture_mailboxes,
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    SharedImageVideoProvider::ImageRecord record) {
  TRACE_EVENT0("media", "VideoVideoFrameFactoryImpl::OnVideoFrameImageReady");

  if (!thiz)
    return;

  // Initialize the CodecImage to use this output buffer.  Note that we're not
  // on the gpu main thread here, but it's okay since CodecImage is not being
  // used at this point.  Alternatively, we could post it, or hand it off to the
  // MaybeRenderEarlyManager to save a post.
  //
  // When we remove the output buffer management from CodecImage, then that's
  // what we'd have a reference to here rather than CodecImage.
  record.codec_image_holder->codec_image_raw()->Initialize(
      std::move(output_buffer), codec_buffer_wait_coordinator,
      std::move(promotion_hint_cb));

  // Send the CodecImage (via holder, since we can't touch the refcount here) to
  // the MaybeRenderEarlyManager.
  thiz->mre_manager()->AddCodecImage(record.codec_image_holder);

  // In case we need to get the YCbCr info, take the image holder out of the
  // record before we move it into |completion_cb|.
  auto codec_image_holder = std::move(record.codec_image_holder);

  // Doesn't need to be weak-ptr'd, since we're either calling it inline, or
  // calling it from the YCbCr callback which is, itself weak-ptr'd.
  auto completion_cb = base::BindOnce(
      &VideoFrameFactoryImpl::CreateVideoFrame_Finish, thiz,
      std::move(output_cb), timestamp, coded_size, natural_size,
      std::move(codec_buffer_wait_coordinator), pixel_format, overlay_mode,
      enable_threaded_texture_mailboxes, std::move(record));

  // TODO(liberato): Use |ycbcr_helper_| as a signal about whether we're
  // supposed to get YCbCr info or not, rather than requiring the provider to
  // tell us.  Note that right now, we do have the helper even if we don't
  // need it.  See GpuMojoMediaClient.
  if (!thiz->ycbcr_info_ && record.is_vulkan) {
    // We need YCbCr info to create the frame.  Post back to the gpu thread to
    // do it.  Note that we might post multiple times before succeeding once,
    // both because of failures and because we might get multiple requests to
    // create frames on the mcvd thread, before the gpu thread returns one ycbcr
    // info to us.  Either way, it's fine, since the helper also caches the
    // info locally.  It won't render more frames than needed.
    auto ycbcr_cb = BindToCurrentLoop(base::BindOnce(
        &VideoFrameFactoryImpl::CreateVideoFrame_OnYCbCrInfo,
        thiz->weak_factory_.GetWeakPtr(), std::move(completion_cb)));
    thiz->ycbcr_helper_.Post(FROM_HERE, &YCbCrHelper::GetYCbCrInfo,
                             std::move(codec_image_holder),
                             std::move(ycbcr_cb));
    return;
  }

  std::move(completion_cb).Run();
}

void VideoFrameFactoryImpl::CreateVideoFrame_OnYCbCrInfo(
    base::OnceClosure completion_cb,
    YCbCrHelper::OptionalInfo ycbcr_info) {
  ycbcr_info_ = std::move(ycbcr_info);
  if (ycbcr_info_) {
    // Clear the helper just to free it up, though we might continue to get
    // callbacks from it if we've posted multiple requests.
    //
    // We only do this if we actually get the info; we should continue to ask
    // if we don't.  This can happen if, for example, the frame failed to render
    // due to a timeout.
    ycbcr_helper_.Reset();
  }
  std::move(completion_cb).Run();
}

void VideoFrameFactoryImpl::CreateVideoFrame_Finish(
    OnceOutputCb output_cb,
    base::TimeDelta timestamp,
    gfx::Size coded_size,
    gfx::Size natural_size,
    scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator,
    VideoPixelFormat pixel_format,
    OverlayMode overlay_mode,
    bool enable_threaded_texture_mailboxes,
    SharedImageVideoProvider::ImageRecord record) {
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] = gpu::MailboxHolder(record.mailbox, gpu::SyncToken(),
                                          GL_TEXTURE_EXTERNAL_OES);

  gfx::Rect visible_rect(coded_size);

  auto frame = VideoFrame::WrapNativeTextures(
      pixel_format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), coded_size,
      visible_rect, natural_size, timestamp);

  // For Vulkan.
  frame->set_ycbcr_info(ycbcr_info_);

  // If, for some reason, we failed to create a frame, then fail.  Note that we
  // don't need to call |release_cb|; dropping it is okay since the api says so.
  if (!frame) {
    LOG(ERROR) << __func__ << " failed to create video frame";
    std::move(output_cb).Run(nullptr);
    return;
  }

  // The frames must be copied when threaded texture mailboxes are in use
  // (http://crbug.com/582170).
  if (enable_threaded_texture_mailboxes)
    frame->metadata()->SetBoolean(VideoFrameMetadata::COPY_REQUIRED, true);

  const bool is_surface_control =
      overlay_mode == OverlayMode::kSurfaceControlSecure ||
      overlay_mode == OverlayMode::kSurfaceControlInsecure;
  const bool wants_promotion_hints =
      overlay_mode == OverlayMode::kRequestPromotionHints;

  // Remember that we can't access |codec_buffer_wait_coordinator|, but we can
  // check if we have one here.
  bool allow_overlay = false;
  if (is_surface_control) {
    DCHECK(codec_buffer_wait_coordinator);
    allow_overlay = true;
  } else {
    // We unconditionally mark the picture as overlayable, even if
    // |!codec_buffer_wait_coordinator|, if we want to get hints.  It's
    // required, else we won't get hints.
    allow_overlay = !codec_buffer_wait_coordinator || wants_promotion_hints;
  }

  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY,
                                allow_overlay);
  frame->metadata()->SetBoolean(VideoFrameMetadata::WANTS_PROMOTION_HINT,
                                wants_promotion_hints);
  frame->metadata()->SetBoolean(VideoFrameMetadata::TEXTURE_OWNER,
                                !!codec_buffer_wait_coordinator);

  // TODO(liberato): if this is run via being dropped, then it would be nice
  // to find that out rather than treating the image as unused.  If the renderer
  // is torn down, then this will be dropped rather than run.  While |provider_|
  // allows this, it doesn't have enough information to understand if the image
  // is free or not.  The problem only really affects the pool, since the
  // direct provider destroys the SharedImage which works in either case.  Any
  // use of the image (e.g., if viz is still using it after the renderer has
  // been torn down unexpectedly), will just not draw anything.  That's fine.
  //
  // However, the pool will try to re-use the image, so the SharedImage remains
  // valid.  However, it's not a good idea to draw with it until the CodecImage
  // is re-initialized with a new frame.  If the renderer is torn down without
  // getting returns from viz, then the pool does the wrong thing.  However,
  // the pool really doesn't know anything about VideoFrames, and dropping the
  // callback does, in fact, signal that it's unused now (as described in the
  // api).  So, we probably should wrap the release cb in a default invoke, and
  // if the default invoke happens, do something.  Unclear what, though.  Can't
  // move it into the CodecImage (might hold a ref to the CodecImage in the cb),
  // so it's unclear.  As it is, CodecImage just handles the case where it's
  // used after release.
  frame->SetReleaseMailboxCB(std::move(record.release_cb));

  // Note that we don't want to handle the CodecImageGroup here.  It needs to be
  // accessed on the gpu thread.  Once we move to pooling, only the initial
  // create / destroy operations will affect it anyway, so it might as well stay
  // on the gpu thread.

  std::move(output_cb).Run(std::move(frame));
}

void VideoFrameFactoryImpl::RunAfterPendingVideoFrames(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Run |closure| after we receive an image from |image_provider_|.  We don't
  // need the image, but it guarantees that it's ordered after all previous
  // requests have been fulfilled.

  auto image_ready_cb = base::BindOnce(
      [](base::OnceClosure closure,
         SharedImageVideoProvider::ImageRecord record) {
        // Ignore |record| since we don't actually need an image.
        std::move(closure).Run();
      },
      std::move(closure));

  image_provider_->RequestImage(
      std::move(image_ready_cb), image_spec_,
      codec_buffer_wait_coordinator_
          ? codec_buffer_wait_coordinator_->texture_owner()
          : nullptr);
}

}  // namespace media
