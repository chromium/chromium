// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_to_video_frame_copier.h"

#include "base/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
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
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper> context_provider,
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

  if (!image->IsTextureBacked()) {
    // Initially try accessing pixels directly if they are in memory.
    sk_sp<SkImage> sk_image = image->PaintImageForCurrentFrame().GetSwSkImage();
    if (sk_image->alphaType() != kPremul_SkAlphaType) {
      const gfx::Size sk_image_size(sk_image->width(), sk_image->height());
      auto sk_image_video_frame = media::CreateFromSkImage(
          std::move(sk_image), gfx::Rect(sk_image_size), sk_image_size,
          base::TimeDelta());
      if (sk_image_video_frame) {
        std::move(callback).Run(
            ConvertToYUVFrame(std::move(sk_image_video_frame),
                              /* flip = */ false));
        return;
      }
    }

    // Copy the pixels into memory synchronously. This call may block the main
    // render thread.
    ReadARGBPixelsSync(image, std::move(callback));
    return;
  }

  if (!context_provider) {
    DLOG(ERROR) << "Context lost, skipping frame";
    return;
  }

  // Try async reading if image is texture backed.
  if (image->CurrentFrameKnownToBeOpaque() || can_discard_alpha_) {
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    if (accelerated_frame_pool_enabled_) {
      if (!accelerated_frame_pool_) {
        accelerated_frame_pool_ =
            std::make_unique<WebGraphicsContext3DVideoFramePool>(
                context_provider);
      }
      auto blit_done_lambda =
          [](base::WeakPtr<StaticBitmapImageToVideoFrameCopier> converter,
             base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
                 context_provider,
             scoped_refptr<StaticBitmapImage> image,
             FrameReadyCallback callback,
             scoped_refptr<media::VideoFrame> video_frame) {
            if (!converter)
              return;
            if (video_frame) {
              converter->OnYUVPixelsReadAsync(video_frame, std::move(callback),
                                              true);
            } else if (context_provider) {
              converter->ReadYUVPixelsAsync(image,
                                            context_provider->ContextProvider(),
                                            std::move(callback));
            }
          };
      auto blit_done_callback =
          WTF::Bind(blit_done_lambda, weak_ptr_factory_.GetWeakPtr(),
                    context_provider, image, std::move(split_callback.first));

      // TODO(https://crbug.com/1224279): This assumes that all
      // StaticBitmapImages are 8-bit sRGB. Expose the color space and pixel
      // format that is backing `image->GetMailboxHolder()`, or, alternatively,
      // expose an accelerated SkImage.
      accelerated_frame_pool_->CopyRGBATextureToVideoFrame(
          viz::SkColorTypeToResourceFormat(kRGBA_8888_SkColorType),
          gfx::Size(image->width(), image->height()),
          gfx::ColorSpace::CreateSRGB(),
          image->IsOriginTopLeft() ? kTopLeft_GrSurfaceOrigin
                                   : kBottomLeft_GrSurfaceOrigin,
          image->GetMailboxHolder(), gfx::ColorSpace::CreateREC709(),
          std::move(blit_done_callback));
      // Early out even if the above fails since it would've already invoked the
      // FrameReadyCallback with a null VideoFrame to indicate failure, and that
      // will cause us to the take the fallback path in |blit_done_lambda|.
      return;
    }
    ReadYUVPixelsAsync(image, context_provider->ContextProvider(),
                       std::move(split_callback.second));
  } else {
    ReadARGBPixelsAsync(image, context_provider->ContextProvider(),
                        std::move(callback));
  }
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
          temp_argb_frame->visible_data(media::VideoFrame::kARGBPlane),
          temp_argb_frame->stride(media::VideoFrame::kARGBPlane), 0 /*srcX*/,
          0 /*srcY*/)) {
    DLOG(ERROR) << "Couldn't read pixels from PaintImage";
    return;
  }
  std::move(callback).Run(
      ConvertToYUVFrame(std::move(temp_argb_frame), /* flip = */ false));
}

void StaticBitmapImageToVideoFrameCopier::ReadARGBPixelsAsync(
    scoped_refptr<StaticBitmapImage> image,
    blink::WebGraphicsContext3DProvider* context_provider,
    FrameReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(context_provider);
  DCHECK(!image->CurrentFrameKnownToBeOpaque());

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
  GLuint row_bytes;
  if (!base::CheckedNumeric<size_t>(info.minRowBytes())
           .AssignIfValid(&row_bytes)) {
    DLOG(ERROR) << "Row stride must fit in GLuint (32 bits), given stride: "
                << info.minRowBytes();
    return;
  }

  GrSurfaceOrigin image_origin = image->IsOriginTopLeft()
                                     ? kTopLeft_GrSurfaceOrigin
                                     : kBottomLeft_GrSurfaceOrigin;

  gpu::MailboxHolder mailbox_holder = image->GetMailboxHolder();
  DCHECK(context_provider->RasterInterface());
  context_provider->RasterInterface()->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());
  context_provider->RasterInterface()->ReadbackARGBPixelsAsync(
      mailbox_holder.mailbox, mailbox_holder.texture_target, image_origin, info,
      row_bytes, temp_argb_frame->visible_data(media::VideoFrame::kARGBPlane),
      WTF::Bind(&StaticBitmapImageToVideoFrameCopier::OnARGBPixelsReadAsync,
                weak_ptr_factory_.GetWeakPtr(), image, temp_argb_frame,
                std::move(callback)));
}

void StaticBitmapImageToVideoFrameCopier::ReadYUVPixelsAsync(
    scoped_refptr<StaticBitmapImage> image,
    blink::WebGraphicsContext3DProvider* context_provider,
    FrameReadyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(context_provider);

  const gfx::Size image_size(image->width(), image->height());
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
      output_frame->stride(media::VideoFrame::kYPlane),
      output_frame->visible_data(media::VideoFrame::kYPlane),
      output_frame->stride(media::VideoFrame::kUPlane),
      output_frame->visible_data(media::VideoFrame::kUPlane),
      output_frame->stride(media::VideoFrame::kVPlane),
      output_frame->visible_data(media::VideoFrame::kVPlane), gfx::Point(0, 0),
      WTF::Bind(&StaticBitmapImageToVideoFrameCopier::OnReleaseMailbox,
                weak_ptr_factory_.GetWeakPtr(), image),
      WTF::Bind(&StaticBitmapImageToVideoFrameCopier::OnYUVPixelsReadAsync,
                weak_ptr_factory_.GetWeakPtr(), output_frame,
                std::move(callback)));
}

void StaticBitmapImageToVideoFrameCopier::OnARGBPixelsReadAsync(
    scoped_refptr<StaticBitmapImage> image,
    scoped_refptr<media::VideoFrame> temp_argb_frame,
    FrameReadyCallback callback,
    GrSurfaceOrigin result_origin,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!success) {
    DLOG(ERROR) << "Couldn't read SkImage using async callback";
    // Async reading is not supported on some platforms, see
    // http://crbug.com/788386.
    ReadARGBPixelsSync(image, std::move(callback));
    return;
  }

  bool flip = result_origin == kBottomLeft_GrSurfaceOrigin;
  std::move(callback).Run(ConvertToYUVFrame(std::move(temp_argb_frame), flip));
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
  std::move(callback).Run(yuv_frame);
}

void StaticBitmapImageToVideoFrameCopier::OnReleaseMailbox(
    scoped_refptr<StaticBitmapImage> image) {
  // All shared image operations have been completed, stop holding the ref.
  image = nullptr;
}

scoped_refptr<media::VideoFrame>
StaticBitmapImageToVideoFrameCopier::ConvertToYUVFrame(
    scoped_refptr<media::VideoFrame> temp_argb_frame,
    bool flip) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  TRACE_EVENT0("webrtc", "CanvasCaptureHandler::ConvertToYUVFrame");

  const bool skip_alpha =
      media::IsOpaque(temp_argb_frame->format()) || can_discard_alpha_;
  const uint8_t* source_ptr =
      temp_argb_frame->visible_data(media::VideoFrame::kARGBPlane);
  const gfx::Size image_size = temp_argb_frame->coded_size();
  const int stride = temp_argb_frame->stride(media::VideoFrame::kARGBPlane);

  scoped_refptr<media::VideoFrame> video_frame = frame_pool_.CreateFrame(
      skip_alpha ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      image_size, gfx::Rect(image_size), image_size, base::TimeDelta());
  if (!video_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return nullptr;
  }

  int (*ConvertToI420)(const uint8_t* src_argb, int src_stride_argb,
                       uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
                       int dst_stride_u, uint8_t* dst_v, int dst_stride_v,
                       int width, int height) = nullptr;
  switch (temp_argb_frame->format()) {
    case media::PIXEL_FORMAT_XBGR:
    case media::PIXEL_FORMAT_ABGR:
      ConvertToI420 = libyuv::ABGRToI420;
      break;
    case media::PIXEL_FORMAT_XRGB:
    case media::PIXEL_FORMAT_ARGB:
      ConvertToI420 = libyuv::ARGBToI420;
      break;
    default:
      NOTIMPLEMENTED() << "Unexpected pixel format.";
      return nullptr;
  }

  if (ConvertToI420(source_ptr, stride,
                    video_frame->visible_data(media::VideoFrame::kYPlane),
                    video_frame->stride(media::VideoFrame::kYPlane),
                    video_frame->visible_data(media::VideoFrame::kUPlane),
                    video_frame->stride(media::VideoFrame::kUPlane),
                    video_frame->visible_data(media::VideoFrame::kVPlane),
                    video_frame->stride(media::VideoFrame::kVPlane),
                    image_size.width(),
                    (flip ? -1 : 1) * image_size.height()) != 0) {
    DLOG(ERROR) << "Couldn't convert to I420";
    return nullptr;
  }
  if (!skip_alpha) {
    // It is ok to use ARGB function because alpha has the same alignment for
    // both ABGR and ARGB.
    libyuv::ARGBExtractAlpha(
        source_ptr, stride,
        video_frame->visible_data(media::VideoFrame::kAPlane),
        video_frame->stride(media::VideoFrame::kAPlane), image_size.width(),
        (flip ? -1 : 1) * image_size.height());
  }

  return video_frame;
}

}  // namespace blink
