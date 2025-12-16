// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider_external_bitmap.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/ganesh/GrDriverBugWorkarounds.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

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

ImageOrientationEnum VideoTransformationToImageOrientation(
    media::VideoTransformation transform) {
  if (!transform.mirrored) {
    switch (transform.rotation) {
      case media::VIDEO_ROTATION_0:
        return ImageOrientationEnum::kOriginTopLeft;
      case media::VIDEO_ROTATION_90:
        return ImageOrientationEnum::kOriginRightTop;
      case media::VIDEO_ROTATION_180:
        return ImageOrientationEnum::kOriginBottomRight;
      case media::VIDEO_ROTATION_270:
        return ImageOrientationEnum::kOriginLeftBottom;
    }
  }

  switch (transform.rotation) {
    case media::VIDEO_ROTATION_0:
      return ImageOrientationEnum::kOriginTopRight;
    case media::VIDEO_ROTATION_90:
      return ImageOrientationEnum::kOriginLeftTop;
    case media::VIDEO_ROTATION_180:
      return ImageOrientationEnum::kOriginBottomLeft;
    case media::VIDEO_ROTATION_270:
      return ImageOrientationEnum::kOriginRightBottom;
  }
}

media::VideoTransformation ImageOrientationToVideoTransformation(
    ImageOrientationEnum orientation) {
  switch (orientation) {
    case ImageOrientationEnum::kOriginTopLeft:
      return media::kNoTransformation;
    case ImageOrientationEnum::kOriginTopRight:
      return media::VideoTransformation(media::VIDEO_ROTATION_0,
                                        /*mirrored=*/true);
    case ImageOrientationEnum::kOriginBottomRight:
      return media::VIDEO_ROTATION_180;
    case ImageOrientationEnum::kOriginBottomLeft:
      return media::VideoTransformation(media::VIDEO_ROTATION_180,
                                        /*mirrored=*/true);
    case ImageOrientationEnum::kOriginLeftTop:
      return media::VideoTransformation(media::VIDEO_ROTATION_90,
                                        /*mirrored=*/true);
    case ImageOrientationEnum::kOriginRightTop:
      return media::VIDEO_ROTATION_90;
    case ImageOrientationEnum::kOriginRightBottom:
      return media::VideoTransformation(media::VIDEO_ROTATION_270,
                                        /*mirrored=*/true);
    case ImageOrientationEnum::kOriginLeftBottom:
      return media::VIDEO_ROTATION_270;
  };
}

bool WillCreateAcceleratedImagesFromVideoFrame() {
  return ShouldCreateAcceleratedImages(GetRasterContextProvider().get());
}

scoped_refptr<StaticBitmapImage> CreateImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    CanvasSnapshotProvider* snapshot_provider,
    media::PaintCanvasVideoRenderer* video_renderer,
    bool prefer_tagged_orientation,
    bool reinterpret_video_as_srgb) {
  DCHECK(frame);
  if (!snapshot_provider) {
    DLOG(ERROR) << "An external CanvasSnapshotProvider must be provided";
    return nullptr;
  }

  auto raster_context_provider = GetRasterContextProvider();
  if (snapshot_provider->IsAccelerated()) {
    prefer_tagged_orientation = false;
  }

  const auto transform =
      frame->metadata().transformation.value_or(media::kNoTransformation);

  // If the provider isn't accelerated, avoid GPU round trips to upload frame
  // data from GpuMemoryBuffer backed frames which aren't mappable.
  if (frame->HasMappableSharedImage() && !frame->IsMappable() &&
      !snapshot_provider->IsAccelerated()) {
    frame = media::ConvertToMemoryMappedFrame(std::move(frame));
    if (!frame) {
      DLOG(ERROR) << "Failed to map VideoFrame.";
      return nullptr;
    }
  }

  if (frame->HasSharedImage()) {
    if (!raster_context_provider) {
      DLOG(ERROR) << "Unable to process a texture backed VideoFrame w/o a "
                     "RasterContextProvider.";
      return nullptr;  // Unable to get/create a shared main thread context.
    }
  }

  cc::PaintFlags media_flags;
  media_flags.setAlphaf(1.0f);
  media_flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);
  media_flags.setBlendMode(SkBlendMode::kSrc);

  std::unique_ptr<media::PaintCanvasVideoRenderer> local_video_renderer;
  if (!video_renderer) {
    local_video_renderer = std::make_unique<media::PaintCanvasVideoRenderer>();
    video_renderer = local_video_renderer.get();
  }

  media::PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect = gfx::RectF(snapshot_provider->Size());
  params.transformation =
      prefer_tagged_orientation
          ? media::kNoTransformation
          : frame->metadata().transformation.value_or(media::kNoTransformation);
  params.reinterpret_as_srgb = reinterpret_video_as_srgb;
  return snapshot_provider->DoExternalDrawAndSnapshot(
      [&](MemoryManagedPaintCanvas& canvas) {
        video_renderer->Paint(frame.get(), &canvas, media_flags, params,
                              raster_context_provider.get());
      },
      prefer_tagged_orientation
          ? VideoTransformationToImageOrientation(transform)
          : ImageOrientationEnum::kDefault);
}

void DrawVideoFrameIntoCanvas(scoped_refptr<media::VideoFrame> frame,
                              cc::PaintCanvas* canvas,
                              const cc::PaintFlags& flags,
                              bool ignore_video_transformation) {
  viz::RasterContextProvider* raster_context_provider = nullptr;
  if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
    raster_context_provider =
        wrapper->ContextProvider().RasterContextProvider();
  }

  media::PaintCanvasVideoRenderer video_renderer;
  media::PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect =
      gfx::RectF(frame->natural_size().width(), frame->natural_size().height());
  params.transformation =
      ignore_video_transformation
          ? media::kNoTransformation
          : frame->metadata().transformation.value_or(media::kNoTransformation);
  video_renderer.Paint(frame, canvas, flags, params, raster_context_provider);
}

scoped_refptr<viz::RasterContextProvider> GetRasterContextProvider() {
  auto wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!wrapper)
    return nullptr;

  return base::WrapRefCounted(
      wrapper->ContextProvider().RasterContextProvider());
}

std::unique_ptr<CanvasSnapshotProvider> CreateSnapshotProviderForVideoFrame(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    viz::RasterContextProvider* raster_context_provider) {
  constexpr auto kShouldInitialize =
      CanvasResourceProvider::ShouldInitialize::kNo;
  if (!ShouldCreateAcceleratedImages(raster_context_provider)) {
    return CanvasSnapshotProviderExternalBitmap::Create(
        size, format, alpha_type, color_space);
  }
  return CanvasResourceProvider::CreateSharedImageProvider(
      size, format, alpha_type, color_space, kShouldInitialize,
      SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ);
}

}  // namespace blink
