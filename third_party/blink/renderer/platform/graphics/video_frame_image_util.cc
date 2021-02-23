// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

scoped_refptr<viz::RasterContextProvider> GetRasterContextProvider() {
  auto wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!wrapper)
    return nullptr;

  if (auto* provider = wrapper->ContextProvider())
    return base::WrapRefCounted(provider->RasterContextProvider());

  return nullptr;
}

bool CanUseZeroCopyImages(const media::VideoFrame& frame) {
  // SharedImage optimization: create AcceleratedStaticBitmapImage directly.
  // Disabled on Android because the hardware decode implementation may neuter
  // frames, which would violate ImageBitmap requirements.
  // TODO(sandersd): Handle YUV pixel formats.
  // TODO(sandersd): Handle high bit depth formats.
#if defined(OS_ANDROID)
  return false;
#else
  return frame.NumTextures() == 1 &&
         frame.mailbox_holder(0).mailbox.IsSharedImage() &&
         (frame.format() == media::PIXEL_FORMAT_ARGB ||
          frame.format() == media::PIXEL_FORMAT_XRGB ||
          frame.format() == media::PIXEL_FORMAT_ABGR ||
          frame.format() == media::PIXEL_FORMAT_XBGR ||
          frame.format() == media::PIXEL_FORMAT_BGRA);
#endif
}

bool ShouldCreateAcceleratedImages(
    viz::RasterContextProvider* raster_context_provider) {
  if (!SharedGpuContext::IsGpuCompositingEnabled())
    return false;

  if (!raster_context_provider)
    return false;

  if (raster_context_provider->GetGpuFeatureInfo().IsWorkaroundEnabled(
          DISABLE_IMAGEBITMAP_FROM_VIDEO_USING_GPU)) {
    return false;
  }

  return true;
}

}  // namespace

bool WillCreateAcceleratedImagesFromVideoFrame(const media::VideoFrame* frame) {
  return CanUseZeroCopyImages(*frame) ||
         ShouldCreateAcceleratedImages(GetRasterContextProvider().get());
}

scoped_refptr<StaticBitmapImage> CreateImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    bool allow_zero_copy_images,
    CanvasResourceProvider* resource_provider,
    media::PaintCanvasVideoRenderer* video_renderer) {
  DCHECK(frame);
  if (allow_zero_copy_images && CanUseZeroCopyImages(*frame)) {
    // TODO(sandersd): Do we need to be able to handle limited-range RGB? It
    // may never happen, and SkColorSpace doesn't know about it.
    auto sk_color_space =
        frame->ColorSpace().GetAsFullRangeRGB().ToSkColorSpace();
    if (!sk_color_space)
      sk_color_space = SkColorSpace::MakeSRGB();

    const SkImageInfo sk_image_info = SkImageInfo::Make(
        frame->coded_size().width(), frame->coded_size().height(),
        kN32_SkColorType, kUnpremul_SkAlphaType, std::move(sk_color_space));

    // Hold a ref by storing it in the release callback.
    auto release_callback = viz::SingleReleaseCallback::Create(
        WTF::Bind([](scoped_refptr<media::VideoFrame> frame,
                     const gpu::SyncToken& sync_token, bool is_lost) {},
                  frame));

    return AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
        frame->mailbox_holder(0).mailbox, frame->mailbox_holder(0).sync_token,
        0u, sk_image_info, frame->mailbox_holder(0).texture_target, true,
        // Pass nullptr for |context_provider_wrapper|, because we don't
        // know which context the mailbox came from. It is used only to
        // detect when the mailbox is invalid due to context loss, and is
        // ignored when |is_cross_thread|.
        base::WeakPtr<WebGraphicsContext3DProviderWrapper>(),
        // Pass null |context_thread_ref|, again because we don't know
        // which context the mailbox came from. This should always trigger
        // |is_cross_thread|.
        base::PlatformThreadRef(),
        // The task runner is only used for |release_callback|.
        Thread::Current()->GetTaskRunner(), std::move(release_callback));
  }

  auto raster_context_provider = GetRasterContextProvider();
  if (frame->HasTextures()) {
    if (!raster_context_provider) {
      DLOG(ERROR) << "Unable to process a texture backed VideoFrame w/o a "
                     "RasterContextProvider.";
      return nullptr;  // Unable to get/create a shared main thread context.
    }
    if (!raster_context_provider->GrContext() &&
        !raster_context_provider->ContextCapabilities().supports_oop_raster) {
      DLOG(ERROR) << "Unable to process a texture backed VideoFrame w/o a "
                     "GrContext or OOP raster support.";
      return nullptr;  // The context has been lost.
    }
  }

  std::unique_ptr<CanvasResourceProvider> local_resource_provider;
  if (!resource_provider) {
    local_resource_provider = CreateResourceProviderForVideoFrame(
        IntSize(frame->visible_rect().size()), raster_context_provider.get());
    resource_provider = local_resource_provider.get();
  }

  cc::PaintFlags media_flags;
  media_flags.setAlpha(0xFF);
  media_flags.setFilterQuality(kLow_SkFilterQuality);
  media_flags.setBlendMode(SkBlendMode::kSrc);

  // PaintCanvasVideoRenderer can't handle GpuMemoryBuffer frames.
  if (frame->HasGpuMemoryBuffer() && !frame->IsMappable())
    frame = media::ConvertToMemoryMappedFrame(std::move(frame));

  std::unique_ptr<media::PaintCanvasVideoRenderer> local_video_renderer;
  if (!video_renderer) {
    local_video_renderer = std::make_unique<media::PaintCanvasVideoRenderer>();
    video_renderer = local_video_renderer.get();
  }

  // Since we're copying, the destination is always aligned with the origin.
  const auto& visible_rect = frame->visible_rect();
  const auto dest_rect =
      gfx::RectF(0, 0, visible_rect.width(), visible_rect.height());

  video_renderer->Paint(
      frame.get(), resource_provider->Canvas(), dest_rect, media_flags,
      frame->metadata().transformation.value_or(media::kNoTransformation),
      raster_context_provider.get());
  return resource_provider->Snapshot();
}

std::unique_ptr<CanvasResourceProvider> CreateResourceProviderForVideoFrame(
    IntSize size,
    viz::RasterContextProvider* raster_context_provider) {
  if (!ShouldCreateAcceleratedImages(raster_context_provider)) {
    return CanvasResourceProvider::CreateBitmapProvider(
        size, kLow_SkFilterQuality, CanvasResourceParams(),
        CanvasResourceProvider::ShouldInitialize::kNo);
  }

  return CanvasResourceProvider::CreateSharedImageProvider(
      size, kLow_SkFilterQuality, CanvasResourceParams(),
      CanvasResourceProvider::ShouldInitialize::kNo,
      SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
      false,  // Origin of GL texture is bottom left on screen
      gpu::SHARED_IMAGE_USAGE_DISPLAY);
}

}  // namespace blink
