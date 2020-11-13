// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <utility>

#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/renderers/video_frame_yuv_converter.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace blink {

namespace {

bool IsValidSkColorSpace(sk_sp<SkColorSpace> sk_color_space) {
  // Refer to CanvasColorSpaceToGfxColorSpace in CanvasColorParams.
  sk_sp<SkColorSpace> valid_sk_color_spaces[] = {
      gfx::ColorSpace::CreateSRGB().ToSkColorSpace(),
      gfx::ColorSpace::CreateDisplayP3D65().ToSkColorSpace(),
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::GAMMA24)
          .ToSkColorSpace()};
  for (auto& valid_sk_color_space : valid_sk_color_spaces) {
    if (SkColorSpace::Equals(sk_color_space.get(),
                             valid_sk_color_space.get())) {
      return true;
    }
  }
  return false;
}

bool IsValidSkColorType(SkColorType sk_color_type) {
  SkColorType valid_sk_color_types[] = {
      kBGRA_8888_SkColorType, kRGBA_8888_SkColorType,
      // TODO(jie.a.chen@intel.com): Add F16 support.
      // kRGBA_F16_SkColorType
  };
  for (auto& valid_sk_color_type : valid_sk_color_types) {
    if (sk_color_type == valid_sk_color_type) {
      return true;
    }
  }
  return false;
}

void OnYUVReadbackDone(
    void* raw_frame_ptr,
    std::unique_ptr<const SkImage::AsyncReadResult> async_result) {
  scoped_refptr<media::VideoFrame> frame(
      static_cast<media::VideoFrame*>(raw_frame_ptr));
  if (!async_result) {
    LOG(ERROR) << "Failed to read yuv420 back!";
    return;
  }
  auto* data0 = static_cast<const uint8_t*>(async_result->data(0));
  DCHECK(data0);
  auto* data1 = static_cast<const uint8_t*>(async_result->data(1));
  DCHECK(data1);
  auto* data2 = static_cast<const uint8_t*>(async_result->data(2));
  DCHECK(data2);
  gfx::Size size = frame->coded_size();
  libyuv::CopyPlane(data0, static_cast<int>(async_result->rowBytes(0)),
                    frame->visible_data(media::VideoFrame::kYPlane),
                    frame->stride(media::VideoFrame::kYPlane), size.width(),
                    size.height());
  libyuv::CopyPlane(data1, static_cast<int>(async_result->rowBytes(1)),
                    frame->visible_data(media::VideoFrame::kUPlane),
                    frame->stride(media::VideoFrame::kUPlane), size.width() / 2,
                    size.height() / 2);
  libyuv::CopyPlane(data2, static_cast<int>(async_result->rowBytes(2)),
                    frame->visible_data(media::VideoFrame::kVPlane),
                    frame->stride(media::VideoFrame::kVPlane), size.width() / 2,
                    size.height() / 2);
}

}  // namespace

VideoFrame::VideoFrame(scoped_refptr<media::VideoFrame> frame)
    : handle_(base::MakeRefCounted<VideoFrameHandle>(std::move(frame))) {
  DCHECK(handle_->frame());
}

VideoFrame::VideoFrame(scoped_refptr<VideoFrameHandle> handle)
    : handle_(std::move(handle)) {
  DCHECK(handle_);
}

// static
VideoFrame* VideoFrame::Create(ImageBitmap* source,
                               VideoFrameInit* init,
                               ExceptionState& exception_state) {
  if (!source) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "No source was provided");
    return nullptr;
  }

  if (!source->BitmapImage()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid source state");
    return nullptr;
  }

  gfx::Size size(source->width(), source->height());
  gfx::Rect rect(size);
  base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(init->timestamp());

  auto sk_image_info =
      source->BitmapImage()->PaintImageForCurrentFrame().GetSkImageInfo();
  auto sk_color_space = sk_image_info.refColorSpace();
  if (!sk_color_space) {
    sk_color_space = SkColorSpace::MakeSRGB();
  }
  if (!IsValidSkColorSpace(sk_color_space)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid color space");
    return nullptr;
  }

  auto frame = media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_I420, size,
                                              rect, size, timestamp);
  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Frame creation failed");
    return nullptr;
  }

  bool is_texture =
      source->BitmapImage()->PaintImageForCurrentFrame().IsTextureBacked();
  // Now only SkImage_Gpu implemented the readbackYUV420 method, so for
  // non-texture image, still use libyuv do the csc until SkImage_Base
  // implement asyncRescaleAndReadPixelsYUV420.
  if (is_texture) {
    auto sk_image =
        source->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
    SkIRect src_rect = SkIRect::MakeWH(source->width(), source->height());
    sk_image->asyncRescaleAndReadPixelsYUV420(
        kRec709_SkYUVColorSpace, sk_color_space, src_rect,
        {source->width(), source->height()}, SkImage::RescaleGamma::kSrc,
        kHigh_SkFilterQuality, &OnYUVReadbackDone, frame.get());
    GrDirectContext* gr_context =
        source->BitmapImage()->ContextProvider()->GetGrContext();
    DCHECK(gr_context);
    gr_context->flushAndSubmit(/*syncCpu=*/true);
  } else {
    auto sk_color_type = sk_image_info.colorType();
    if (!IsValidSkColorType(sk_color_type)) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid pixel format");
      return nullptr;
    }

    // TODO(jie.a.chen@intel.com): Handle data of float type.
    // Full copy #1
    WTF::Vector<uint8_t> pixel_data = source->CopyBitmapData();
    if (pixel_data.size() <
        media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_ARGB, size)) {
      exception_state.ThrowDOMException(DOMExceptionCode::kBufferOverrunError,
                                        "Image buffer is too small.");
      return nullptr;
    }

    DCHECK(sk_color_type == kRGBA_8888_SkColorType ||
           sk_color_type == kBGRA_8888_SkColorType);
    auto libyuv_convert_to_i420 = (sk_color_type == kRGBA_8888_SkColorType)
                                      ? libyuv::ABGRToI420
                                      : libyuv::ARGBToI420;

    // TODO(jie.a.chen@intel.com): Use GPU to do the conversion.
    // Full copy #2
    int error =
        libyuv_convert_to_i420(pixel_data.data(), source->width() * 4,
                               frame->visible_data(media::VideoFrame::kYPlane),
                               frame->stride(media::VideoFrame::kYPlane),
                               frame->visible_data(media::VideoFrame::kUPlane),
                               frame->stride(media::VideoFrame::kUPlane),
                               frame->visible_data(media::VideoFrame::kVPlane),
                               frame->stride(media::VideoFrame::kVPlane),
                               source->width(), source->height());
    if (error) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "ARGB to YUV420 conversion error");
      return nullptr;
    }
    gfx::ColorSpace gfx_color_space(*sk_color_space);
    // 'libyuv_convert_to_i420' assumes SMPTE170M.
    // Refer to the func below to check the actual conversion:
    // third_party/libyuv/source/row_common.cc -- RGBToY(...)
    gfx_color_space = gfx_color_space.GetWithMatrixAndRange(
        gfx::ColorSpace::MatrixID::SMPTE170M,
        gfx::ColorSpace::RangeID::LIMITED);
    frame->set_color_space(gfx_color_space);
  }
  auto* result = MakeGarbageCollected<VideoFrame>(std::move(frame));
  return result;
}

// static
bool VideoFrame::IsSupportedPlanarFormat(media::VideoFrame* frame) {
  // For now only I420 or NV12 in CPU or GPU memory is supported.
  return frame && (frame->IsMappable() || frame->HasGpuMemoryBuffer()) &&
         ((frame->format() == media::PIXEL_FORMAT_I420 &&
           frame->layout().num_planes() == 3) ||
          (frame->format() == media::PIXEL_FORMAT_NV12 &&
           frame->layout().num_planes() == 2));
}

String VideoFrame::format() const {
  auto local_frame = handle_->frame();
  if (!local_frame || !IsSupportedPlanarFormat(local_frame.get()))
    return String();

  switch (local_frame->format()) {
    case media::PIXEL_FORMAT_I420:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI420);
    case media::PIXEL_FORMAT_NV12:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kNV12);

    default:
      NOTREACHED();
      return String();
  }
}

base::Optional<HeapVector<Member<Plane>>> VideoFrame::planes() {
  // Verify that |this| has not been invalidated, and that the format is
  // supported.
  auto local_frame = handle_->frame();
  if (!local_frame || !IsSupportedPlanarFormat(local_frame.get()))
    return base::nullopt;

  // Create a Plane for each VideoFrame plane, but only the first time.
  if (planes_.IsEmpty()) {
    for (size_t i = 0; i < local_frame->layout().num_planes(); i++) {
      // Note: |handle_| may have been invalidated since |local_frame| was read.
      planes_.push_back(MakeGarbageCollected<Plane>(handle_, i));
    }
  }

  return planes_;
}

uint32_t VideoFrame::codedWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->coded_size().width();
}

uint32_t VideoFrame::codedHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->coded_size().height();
}

uint32_t VideoFrame::cropLeft() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().x();
}

uint32_t VideoFrame::cropTop() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().y();
}

uint32_t VideoFrame::cropWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().width();
}

uint32_t VideoFrame::cropHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->visible_rect().height();
}

uint32_t VideoFrame::displayWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->natural_size().width();
}

uint32_t VideoFrame::displayHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->natural_size().height();
}

base::Optional<uint64_t> VideoFrame::timestamp() const {
  auto local_frame = handle_->frame();
  if (!local_frame || local_frame->timestamp() == media::kNoTimestamp)
    return base::nullopt;
  return local_frame->timestamp().InMicroseconds();
}

base::Optional<uint64_t> VideoFrame::duration() const {
  auto local_frame = handle_->frame();
  // TODO(sandersd): Can a duration be kNoTimestamp?
  if (!local_frame || !local_frame->metadata()->frame_duration.has_value())
    return base::nullopt;
  return local_frame->metadata()->frame_duration->InMicroseconds();
}

void VideoFrame::destroy() {
  // TODO(tguilbert): Add a warning when destroying already destroyed frames?
  handle_->Invalidate();
}

VideoFrame* VideoFrame::clone(ExceptionState& exception_state) {
  auto frame = handle_->frame();

  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot clone destroyed VideoFrame.");
    return nullptr;
  }

  return MakeGarbageCollected<VideoFrame>(std::move(frame));
}

scoped_refptr<VideoFrameHandle> VideoFrame::handle() {
  return handle_;
}

scoped_refptr<media::VideoFrame> VideoFrame::frame() {
  return handle_->frame();
}

scoped_refptr<const media::VideoFrame> VideoFrame::frame() const {
  return handle_->frame();
}

ScriptPromise VideoFrame::createImageBitmap(ScriptState* script_state,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  base::Optional<IntRect> crop_rect;

  if (auto local_frame = handle_->frame())
    crop_rect = IntRect(local_frame->visible_rect());

  return ImageBitmapFactories::CreateImageBitmap(script_state, this, crop_rect,
                                                 options, exception_state);
}

IntSize VideoFrame::BitmapSourceSize() const {
  // TODO(crbug.com/1096724): Should be scaled to display size.
  return IntSize(cropWidth(), cropHeight());
}

bool VideoFrame::preferAcceleratedImageBitmap() const {
  auto local_frame = frame();
  return BitmapSourceSize().Area() > kCpuEfficientFrameSize ||
         (local_frame && local_frame->HasTextures());
}

ScriptPromise VideoFrame::CreateImageBitmap(ScriptState* script_state,
                                            base::Optional<IntRect> crop_rect,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  auto local_frame = frame();

  if (!local_frame) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot create ImageBitmap from destroyed VideoFrame.");
    return ScriptPromise();
  }

  if ((local_frame->IsMappable() &&
       (local_frame->format() == media::PIXEL_FORMAT_I420)) ||
      (local_frame->HasTextures() &&
       (local_frame->format() == media::PIXEL_FORMAT_I420 ||
        local_frame->format() == media::PIXEL_FORMAT_NV12 ||
        local_frame->format() == media::PIXEL_FORMAT_ABGR ||
        local_frame->format() == media::PIXEL_FORMAT_XRGB))) {
    scoped_refptr<StaticBitmapImage> image;
    gfx::ColorSpace gfx_color_space = local_frame->ColorSpace();
    gfx_color_space = gfx_color_space.GetWithMatrixAndRange(
        gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
    auto sk_color_space = gfx_color_space.ToSkColorSpace();
    if (!sk_color_space) {
      sk_color_space = SkColorSpace::MakeSRGB();
    }

    if (!preferAcceleratedImageBitmap()) {
      size_t bytes_per_row = sizeof(SkColor) * cropWidth();
      size_t image_pixels_size = bytes_per_row * cropHeight();

      sk_sp<SkData> image_pixels = TryAllocateSkData(image_pixels_size);
      if (!image_pixels) {
        exception_state.ThrowDOMException(DOMExceptionCode::kBufferOverrunError,
                                          "Out of memory.");
        return ScriptPromise();
      }
      media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
          local_frame.get(), image_pixels->writable_data(), bytes_per_row);

      SkImageInfo info =
          SkImageInfo::Make(cropWidth(), cropHeight(), kN32_SkColorType,
                            kUnpremul_SkAlphaType, std::move(sk_color_space));
      sk_sp<SkImage> skImage =
          SkImage::MakeRasterData(info, image_pixels, bytes_per_row);
      image = UnacceleratedStaticBitmapImage::Create(std::move(skImage));
    } else {
      scoped_refptr<viz::RasterContextProvider> raster_context_provider =
          Platform::Current()->SharedMainThreadContextProvider();
      auto* ri = raster_context_provider->RasterInterface();

      gpu::SharedImageInterface* shared_image_interface =
          raster_context_provider->SharedImageInterface();
      uint32_t usage = gpu::SHARED_IMAGE_USAGE_GLES2;
      if (raster_context_provider->ContextCapabilities().supports_oop_raster) {
        usage |= gpu::SHARED_IMAGE_USAGE_RASTER |
                 gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
      }

      gpu::MailboxHolder dest_holder;
      // Use coded_size() to comply with media::ConvertFromVideoFrameYUV.
      dest_holder.mailbox = shared_image_interface->CreateSharedImage(
          viz::ResourceFormat::RGBA_8888, local_frame->coded_size(),
          gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
          usage, gpu::kNullSurfaceHandle);
      dest_holder.sync_token = shared_image_interface->GenUnverifiedSyncToken();
      dest_holder.texture_target = GL_TEXTURE_2D;

      if (local_frame->NumTextures() == 1) {
        ri->WaitSyncTokenCHROMIUM(dest_holder.sync_token.GetConstData());
        ri->CopySubTexture(
            local_frame->mailbox_holder(0).mailbox, dest_holder.mailbox,
            GL_TEXTURE_2D, 0, 0, 0, 0, local_frame->coded_size().width(),
            local_frame->coded_size().height(), GL_FALSE, GL_FALSE);
      } else {
        media::VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
            local_frame.get(), raster_context_provider.get(), dest_holder);
      }

      gpu::SyncToken sync_token;
      ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

      auto release_callback = viz::SingleReleaseCallback::Create(base::BindOnce(
          [](scoped_refptr<viz::RasterContextProvider> provider,
             gpu::Mailbox mailbox, const gpu::SyncToken& sync_token,
             bool is_lost) {
            provider->SharedImageInterface()->DestroySharedImage(sync_token,
                                                                 mailbox);
          },
          raster_context_provider, dest_holder.mailbox));

      const SkImageInfo sk_image_info =
          SkImageInfo::Make(codedWidth(), codedHeight(), kN32_SkColorType,
                            kUnpremul_SkAlphaType, std::move(sk_color_space));

      image = AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
          dest_holder.mailbox, sync_token, 0u, sk_image_info,
          dest_holder.texture_target, true,
          SharedGpuContext::ContextProviderWrapper(),
          base::PlatformThread::CurrentRef(),
          Thread::Current()->GetTaskRunner(), std::move(release_callback));

      if (local_frame->HasTextures()) {
        // Attach a new sync token to |local_frame|, so it's not destroyed
        // before |image| is fully created.
        media::WaitAndReplaceSyncTokenClient client(ri);
        local_frame->UpdateReleaseSyncToken(&client);
      }
    }

    ImageBitmap* image_bitmap =
        MakeGarbageCollected<ImageBitmap>(image, crop_rect, options);
    return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                                 exception_state);
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Unsupported VideoFrame.");
  return ScriptPromise();
}

void VideoFrame::Trace(Visitor* visitor) const {
  visitor->Trace(planes_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
