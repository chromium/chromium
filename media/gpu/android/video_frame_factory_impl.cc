// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/config/gpu_finch_features.h"
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
  switch (overlay_mode) {
    case VideoFrameFactory::OverlayMode::kDontRequestPromotionHints:
    case VideoFrameFactory::OverlayMode::kRequestPromotionHints:
      return base::android::EnableAndroidImageReader()
                 ? gpu::TextureOwner::Mode::kAImageReaderInsecure
                 : gpu::TextureOwner::Mode::kSurfaceTextureInsecure;
    case VideoFrameFactory::OverlayMode::kSurfaceControlSecure:
      CHECK(base::android::EnableAndroidImageReader());
      return gpu::TextureOwner::Mode::kAImageReaderSecureSurfaceControl;
    case VideoFrameFactory::OverlayMode::kSurfaceControlInsecure:
      CHECK(base::android::EnableAndroidImageReader());
      return gpu::TextureOwner::Mode::kAImageReaderInsecureSurfaceControl;
  }

  NOTREACHED();
}

// Run on the GPU main thread to allocate the texture owner, and return it
// via |init_cb|.
static void AllocateTextureOwnerOnGpuThread(
    VideoFrameFactory::InitCB init_cb,
    VideoFrameFactory::OverlayMode overlay_mode,
    scoped_refptr<gpu::RefCountedLock> drdc_lock,
    scoped_refptr<gpu::SharedContextState> shared_context_state) {
  if (!shared_context_state) {
    std::move(init_cb).Run(nullptr);
    return;
  }

  std::move(init_cb).Run(gpu::TextureOwner::Create(
      GetTextureOwnerMode(overlay_mode), shared_context_state,
      std::move(drdc_lock), gpu::TextureOwnerCodecType::kMediaCodec));
}

}  // namespace

VideoFrameFactoryImpl::VideoFrameFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    const gpu::GpuPreferences& gpu_preferences,
    std::unique_ptr<SharedImageVideoProvider> image_provider,
    std::unique_ptr<MaybeRenderEarlyManager> mre_manager,
    std::unique_ptr<FrameInfoHelper> frame_info_helper,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
      image_provider_(std::move(image_provider)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      video_frame_copy_required_(
          gpu_preferences.enable_threaded_texture_mailboxes &&
          !features::NeedThreadSafeAndroidMedia()),
      mre_manager_(std::move(mre_manager)),
      frame_info_helper_(std::move(frame_info_helper)) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoFrameFactoryImpl::Initialize(OverlayMode overlay_mode,
                                       InitCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  overlay_mode_ = overlay_mode;

  // On init success, create the TextureOwner and hop it back to this thread to
  // call |init_cb|.
  auto gpu_init_cb =
      base::BindOnce(&AllocateTextureOwnerOnGpuThread,
                     base::BindPostTaskToCurrentDefault(std::move(init_cb)),
                     overlay_mode, GetDrDcLock());
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
    OnceOutputCB output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gfx::Size coded_size = output_buffer->size();
  gfx::Rect visible_rect(coded_size);

  auto output_buffer_renderer = std::make_unique<CodecOutputBufferRenderer>(
      std::move(output_buffer), codec_buffer_wait_coordinator_, GetDrDcLock());

  // The pixel format doesn't matter here as long as it's valid for texture
  // frames. But SkiaRenderer wants to ensure that the format of the resource
  // used here which will eventually create a promise image must match the
  // format of the resource(AndroidVideoImageBacking) used to create fulfill
  // image. crbug.com/1028746. Since we create all the textures/abstract
  // textures as well as shared images for video to be of format RGBA, we need
  // to use the pixel format as ABGR here(which corresponds to 32bpp RGBA).
  VideoPixelFormat pixel_format = PIXEL_FORMAT_ABGR;

  // Check that we can create a VideoFrame for this config before trying to
  // create the textures for it.
  if (!VideoFrame::IsValidConfig(pixel_format, VideoFrame::STORAGE_OPAQUE,
                                 coded_size, visible_rect, natural_size)) {
    LOG(ERROR) << __func__ << " unsupported video frame format";
    std::move(output_cb).Run(nullptr);
    return;
  }

  auto image_ready_cb =
      base::BindOnce(&VideoFrameFactoryImpl::CreateVideoFrame_OnImageReady,
                     weak_factory_.GetWeakPtr(), std::move(output_cb),
                     timestamp, natural_size, !!codec_buffer_wait_coordinator_,
                     std::move(promotion_hint_cb), pixel_format, overlay_mode_,
                     video_frame_copy_required_, gpu_task_runner_);

  RequestImage(std::move(output_buffer_renderer), std::move(image_ready_cb));
}

void VideoFrameFactoryImpl::RequestImage(
    std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
    ImageWithInfoReadyCB image_ready_cb) {
  auto info_cb =
      base::BindOnce(&VideoFrameFactoryImpl::CreateVideoFrame_OnFrameInfoReady,
                     weak_factory_.GetWeakPtr(), std::move(image_ready_cb));

  frame_info_helper_->GetFrameInfo(std::move(buffer_renderer),
                                   std::move(info_cb));
}

void VideoFrameFactoryImpl::CreateVideoFrame_OnFrameInfoReady(
    ImageWithInfoReadyCB image_ready_cb,
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
    FrameInfoHelper::FrameInfo frame_info) {
  // If we don't have output buffer here we can't rely on reply from
  // FrameInfoHelper as there might be not cached value and we can't render
  // nothing. But in this case call comes from RunAfterPendingVideoFrames and we
  // just want to ask for the same image spec as before to order callback after
  // all RequestImage, so skip updating image_spec_ in this case.
  if (output_buffer_renderer) {
    image_spec_.coded_size = frame_info.coded_size;
    image_spec_.color_space = output_buffer_renderer->color_space();
  } else {
    // It is possible that we come here from RunAfterPendingVideoFrames before
    // CreateVideoFrame was called. In this case we don't have coded_size, but
    // it also means that there was no `image_provider_->RequestImage` calls so
    // we can just run callback instantly.
    if (image_spec_.coded_size.IsEmpty()) {
      std::move(image_ready_cb)
          .Run(nullptr, FrameInfoHelper::FrameInfo(),
               SharedImageVideoProvider::ImageRecord());
      return;
    }
  }
  DCHECK(!image_spec_.coded_size.IsEmpty());

  auto cb = base::BindOnce(std::move(image_ready_cb),
                           std::move(output_buffer_renderer), frame_info);
  image_provider_->RequestImage(std::move(cb), image_spec_);
}

// static
void VideoFrameFactoryImpl::CreateVideoFrame_OnImageReady(
    base::WeakPtr<VideoFrameFactoryImpl> thiz,
    OnceOutputCB output_cb,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    bool is_texture_owner_backed,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    VideoPixelFormat pixel_format,
    OverlayMode overlay_mode,
    bool video_frame_copy_required,
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
    FrameInfoHelper::FrameInfo frame_info,
    SharedImageVideoProvider::ImageRecord record) {
  TRACE_EVENT0("media", "VideoVideoFrameFactoryImpl::OnVideoFrameImageReady");

  if (!thiz)
    return;

  gfx::ColorSpace color_space = output_buffer_renderer->color_space();

  // Initialize the CodecImage to use this output buffer.  Note that we're not
  // on the gpu main thread here, but it's okay since CodecImage is not being
  // used at this point.  Alternatively, we could post it, or hand it off to the
  // MaybeRenderEarlyManager to save a post.
  //
  // When we remove the output buffer management from CodecImage, then that's
  // what we'd have a reference to here rather than CodecImage.
  record.codec_image_holder->codec_image_raw()->Initialize(
      std::move(output_buffer_renderer), is_texture_owner_backed,
      std::move(promotion_hint_cb));

  // Send the CodecImage (via holder, since we can't touch the refcount here) to
  // the MaybeRenderEarlyManager.
  thiz->mre_manager()->AddCodecImage(record.codec_image_holder);

  // In case we need to get the YCbCr info, take the image holder out of the
  // record before we move it into |completion_cb|.
  auto codec_image_holder = std::move(record.codec_image_holder);

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
      pixel_format, std::move(record.shared_image), gpu::SyncToken(),
      VideoFrame::ReleaseMailboxCB(), frame_info.coded_size,
      frame_info.visible_rect, natural_size, timestamp);

  // If, for some reason, we failed to create a frame, then fail.  Note that we
  // don't need to call |release_cb|; dropping it is okay since the api says so.
  if (!frame) {
    LOG(ERROR) << __func__ << " failed to create video frame";
    std::move(output_cb).Run(nullptr);
    return;
  }

  // For Vulkan.
  frame->set_ycbcr_info(frame_info.ycbcr_info);

  frame->set_color_space(color_space);

  frame->metadata().copy_required = video_frame_copy_required;

  const bool is_surface_control =
      overlay_mode == OverlayMode::kSurfaceControlSecure ||
      overlay_mode == OverlayMode::kSurfaceControlInsecure;
  const bool wants_promotion_hints =
      overlay_mode == OverlayMode::kRequestPromotionHints;

  bool allow_overlay = false;
  if (is_surface_control) {
    DCHECK(is_texture_owner_backed);
    allow_overlay = true;
  } else {
    // We unconditionally mark the picture as overlayable, even if
    // |!is_texture_owner_backed|, if we want to get hints.  It's
    // required, else we won't get hints.
    allow_overlay = !is_texture_owner_backed || wants_promotion_hints;
  }

  frame->metadata().allow_overlay = allow_overlay;
  frame->metadata().wants_promotion_hint = wants_promotion_hints;
  frame->metadata().texture_owner = is_texture_owner_backed;

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
         std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
         FrameInfoHelper::FrameInfo frame_info,
         SharedImageVideoProvider::ImageRecord record) {
        // Ignore |record| since we don't actually need an image.
        std::move(closure).Run();
      },
      std::move(closure));

  RequestImage(nullptr, std::move(image_ready_cb));
}

bool VideoFrameFactoryImpl::IsStalled() const {
  return frame_info_helper_->IsStalled();
}

}  // namespace media
