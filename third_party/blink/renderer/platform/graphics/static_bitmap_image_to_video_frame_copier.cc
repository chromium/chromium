// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_to_video_frame_copier.h"

#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/color_space.h"

namespace blink {

StaticBitmapImageToVideoFrameCopier::StaticBitmapImageToVideoFrameCopier(
    bool accelerated_frame_pool_enabled)
    : accelerated_frame_pool_enabled_(accelerated_frame_pool_enabled),
      weak_ptr_factory_(this) {}

StaticBitmapImageToVideoFrameCopier::~StaticBitmapImageToVideoFrameCopier() =
    default;

WebGraphicsContext3DVideoFramePool*
StaticBitmapImageToVideoFrameCopier::GetAcceleratedVideoFramePool(
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        context_provider) {
  if (accelerated_frame_pool_enabled_ && !accelerated_frame_pool_) {
    accelerated_frame_pool_ =
        std::make_unique<WebGraphicsContext3DVideoFramePool>(context_provider);
  }
  return accelerated_frame_pool_.get();
}

void StaticBitmapImageToVideoFrameCopier::Convert(
    scoped_refptr<StaticBitmapImage> image,
    bool can_discard_alpha,
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper,
    FrameReadyCallback callback) {
  can_discard_alpha_ = can_discard_alpha;
  if (!image)
    return;

  const auto size = image->Size();
  if (!media::VideoFrame::IsValidSize(size, gfx::Rect(size), size)) {
    DVLOG(1) << __func__ << " received frame with invalid size "
             << size.ToString();
    return;
  }

  // We might need to convert the frame into I420 pixel format, and 1x1 frame
  // can't be read back into I420.
  const bool too_small_for_i420 = image->width() == 1 || image->height() == 1;
  if (!image->IsTextureBacked()) {
    // Initially try accessing pixels directly if they are in memory.
    sk_sp<SkImage> sk_image = image->PaintImageForCurrentFrame().GetSwSkImage();
    if (sk_image->alphaType() != kPremul_SkAlphaType) {
      const gfx::Size sk_image_size(sk_image->width(), sk_image->height());
      auto sk_image_video_frame = media::CreateFromSkImage(
          std::move(sk_image), gfx::Rect(sk_image_size), sk_image_size,
          base::TimeDelta());
      if (sk_image_video_frame) {
        std::move(callback).Run(std::move(sk_image_video_frame));
        return;
      }
    }

    // Copy the pixels into memory synchronously. This call may block the main
    // render thread.
    ReadARGBPixelsSync(image, std::move(callback));
    return;
  }

  if (!context_provider_wrapper) {
    DLOG(ERROR) << "Context lost, skipping frame";
    return;
  }

  auto* context_provider = context_provider_wrapper->ContextProvider();
  if (!context_provider) {
    DLOG(ERROR) << "Context lost, skipping frame";
    return;
  }

  // Readback to YUV is only used when result is opaque.
  const bool result_is_opaque =
      image->CurrentFrameKnownToBeOpaque() || can_discard_alpha_;

  const bool supports_yuv_readback =
      context_provider->GetCapabilities().supports_yuv_readback;
  // If supports_rgb_to_yuv_conversion is true, supports_yuv_readback must also
  // be.
  CHECK(!context_provider->GetCapabilities().supports_rgb_to_yuv_conversion ||
        supports_yuv_readback);

  // Try async reading if image is texture backed.
  if (!too_small_for_i420 && result_is_opaque && supports_yuv_readback) {
    // Split the callback so it can be used for both the GMB frame pool copy and
    // ReadYUVPixelsAsync fallback paths.
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    if (accelerated_frame_pool_enabled_) {
      if (!accelerated_frame_pool_) {
        accelerated_frame_pool_ =
            std::make_unique<WebGraphicsContext3DVideoFramePool>(
                context_provider_wrapper);
      }
      // TODO(https://crbug.com/1224279): This assumes that all
      // StaticBitmapImages are 8-bit sRGB. Expose the color space and pixel
      // format that is backing `image->GetMailboxHolder()`, or, alternatively,
      // expose an accelerated SkImage.
      if (accelerated_frame_pool_->CopyRGBATextureToVideoFrame(
              viz::SkColorTypeToSinglePlaneSharedImageFormat(
                  kRGBA_8888_SkColorType),
              gfx::Size(image->width(), image->height()),
              gfx::ColorSpace::CreateSRGB(),
              image->IsOriginTopLeft() ? kTopLeft_GrSurfaceOrigin
                                       : kBottomLeft_GrSurfaceOrigin,
              image->GetMailboxHolder(), gfx::ColorSpace::CreateREC709(),
              std::move(split_callback.first))) {
        TRACE_EVENT1("blink", "StaticBitmapImageToVideoFrameCopier::Convert",
                     "accelerated_frame_pool_copy", true);
        // Early out on success, otherwise fallback to ReadYUVPixelsAsync path.
        return;
      }
    }
    ReadYUVPixelsAsync(image, context_provider,
                       std::move(split_callback.second));
  } else {
    ReadARGBPixelsAsync(image, context_provider, std::move(callback));
  }

  TRACE_EVENT1("blink", "StaticBitmapImageToVideoFrameCopier::Convert",
               "accelerated_frame_pool_copy", false);
}

void StaticBitmapImageToVideoFrameCopier::ReadARGBPixelsSync(
    scoped_refptr<StaticBitmapImage> image,
    FrameReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  PaintImage paint_image = image->PaintImageForCurrentFrame();
  const gfx::Size image_size(paint_image.width(), paint_image.height());
  const bool is_opaque = paint_image.IsOpaque();
  const media::VideoPixelFormat temp_argb_pixel_format =
      media::VideoPixelFormatFromSkColorType(kN32_SkColorType, is_opaque);
  scoped_refptr<media::VideoFrame> temp_argb_frame = frame_pool_.CreateFrame(
      temp_argb_pixel_format, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!temp_argb_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }
  SkImageInfo image_info = SkImageInfo::MakeN32(
      image_size.width(), image_size.height(),
      is_opaque ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  if (!paint_image.readPixels(
          image_info,
          temp_argb_frame->GetWritableVisibleData(
              media::VideoFrame::Plane::kARGB),
          temp_argb_frame->stride(media::VideoFrame::Plane::kARGB), 0 /*srcX*/,
          0 /*srcY*/)) {
    DLOG(ERROR) << "Couldn't read pixels from PaintImage";
    return;
  }
  temp_argb_frame->set_color_space(gfx::ColorSpace::CreateSRGB());
  std::move(callback).Run(std::move(temp_argb_frame));
}

void StaticBitmapImageToVideoFrameCopier::ReadARGBPixelsAsync(
    scoped_refptr<StaticBitmapImage> image,
    blink::WebGraphicsContext3DProvider* context_provider,
    FrameReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(context_provider);

  const media::VideoPixelFormat temp_argb_pixel_format =
      media::VideoPixelFormatFromSkColorType(kN32_SkColorType,
                                             /*is_opaque = */ false);
  const gfx::Size image_size(image->width(), image->height());
  scoped_refptr<media::VideoFrame> temp_argb_frame = frame_pool_.CreateFrame(
      temp_argb_pixel_format, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!temp_argb_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }

  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "CanvasCaptureHandler::ReadARGBPixelsAsync supports only "
                "kRGBA_8888_SkColorType and kBGRA_8888_SkColorType.");
  SkImageInfo info = SkImageInfo::MakeN32(
      image_size.width(), image_size.height(), kUnpremul_SkAlphaType);
  GrSurfaceOrigin image_origin = image->IsOriginTopLeft()
                                     ? kTopLeft_GrSurfaceOrigin
                                     : kBottomLeft_GrSurfaceOrigin;

  gfx::Point src_point;
  gpu::MailboxHolder mailbox_holder = image->GetMailboxHolder();
  DCHECK(context_provider->RasterInterface());
  context_provider->RasterInterface()->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());
  context_provider->RasterInterface()->ReadbackARGBPixelsAsync(
      mailbox_holder.mailbox, mailbox_holder.texture_target, image_origin,
      image_size, src_point, info,
      temp_argb_frame->stride(media::VideoFrame::Plane::kARGB),
      temp_argb_frame->GetWritableVisibleData(media::VideoFrame::Plane::kARGB),
      WTF::BindOnce(&StaticBitmapImageToVideoFrameCopier::OnARGBPixelsReadAsync,
                    weak_ptr_factory_.GetWeakPtr(), image, temp_argb_frame,
                    std::move(callback)));
}

void StaticBitmapImageToVideoFrameCopier::ReadYUVPixelsAsync(
    scoped_refptr<StaticBitmapImage> image,
    blink::WebGraphicsContext3DProvider* context_provider,
    FrameReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(context_provider);

  // Our ReadbackYUVPixelsAsync() implementations either cut off odd pixels or
  // simply fail. So, there is no point even trying reading odd sized images
  // into I420.
  const gfx::Size image_size(image->width() & ~1u, image->height() & ~1u);
  scoped_refptr<media::VideoFrame> output_frame = frame_pool_.CreateFrame(
      media::PIXEL_FORMAT_I420, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!output_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }

  gpu::MailboxHolder mailbox_holder = image->GetMailboxHolder();
  context_provider->RasterInterface()->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());
  context_provider->RasterInterface()->ReadbackYUVPixelsAsync(
      mailbox_holder.mailbox, mailbox_holder.texture_target, image_size,
      gfx::Rect(image_size), !image->IsOriginTopLeft(),
      output_frame->stride(media::VideoFrame::Plane::kY),
      output_frame->GetWritableVisibleData(media::VideoFrame::Plane::kY),
      output_frame->stride(media::VideoFrame::Plane::kU),
      output_frame->GetWritableVisibleData(media::VideoFrame::Plane::kU),
      output_frame->stride(media::VideoFrame::Plane::kV),
      output_frame->GetWritableVisibleData(media::VideoFrame::Plane::kV),
      gfx::Point(0, 0),
      WTF::BindOnce(&StaticBitmapImageToVideoFrameCopier::OnReleaseMailbox,
                    weak_ptr_factory_.GetWeakPtr(), image),
      WTF::BindOnce(&StaticBitmapImageToVideoFrameCopier::OnYUVPixelsReadAsync,
                    weak_ptr_factory_.GetWeakPtr(), output_frame,
                    std::move(callback)));
}

void StaticBitmapImageToVideoFrameCopier::OnARGBPixelsReadAsync(
    scoped_refptr<StaticBitmapImage> image,
    scoped_refptr<media::VideoFrame> argb_frame,
    FrameReadyCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!success) {
    DLOG(ERROR) << "Couldn't read SkImage using async callback";
    // Async reading is not supported on some platforms, see
    // http://crbug.com/788386.
    ReadARGBPixelsSync(image, std::move(callback));
    return;
  }
  argb_frame->set_color_space(gfx::ColorSpace::CreateSRGB());
  std::move(callback).Run(std::move(argb_frame));
}

void StaticBitmapImageToVideoFrameCopier::OnYUVPixelsReadAsync(
    scoped_refptr<media::VideoFrame> yuv_frame,
    FrameReadyCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  if (!success) {
    DLOG(ERROR) << "Couldn't read SkImage using async callback";
    return;
  }
  yuv_frame->set_color_space(gfx::ColorSpace::CreateREC601());
  std::move(callback).Run(yuv_frame);
}

void StaticBitmapImageToVideoFrameCopier::OnReleaseMailbox(
    scoped_refptr<StaticBitmapImage> image) {
  // All shared image operations have been completed, stop holding the ref.
  image = nullptr;
}

}  // namespace blink
