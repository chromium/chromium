// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/raster/playback_image_provider.h"
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
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
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

// Used for any images that clients pass to cc::PaintCanvas::DrawImage() in
// the invocation of the `draw_callback` that clients provide to
// `DrawAndSnapshotToImage()`.
class ImageProviderImpl : public cc::ImageProvider {
 public:
  ImageProviderImpl(bool is_f16, const gfx::ColorSpace& color_space)
      : is_f16_(is_f16), color_space_(color_space) {}
  ~ImageProviderImpl() override = default;

  // cc::ImageProvider:
  cc::ImageProvider::ScopedResult GetRasterContent(
      const cc::DrawImage& draw_image) override {
    cc::PaintImage paint_image = draw_image.paint_image();
    if (paint_image.IsDeferredPaintRecord()) {
      CHECK(!paint_image.IsPaintWorklet());
      scoped_refptr<CanvasDeferredPaintRecord> canvas_deferred_paint_record(
          static_cast<CanvasDeferredPaintRecord*>(
              paint_image.deferred_paint_record().get()));
      return cc::ImageProvider::ScopedResult(
          canvas_deferred_paint_record->GetPaintRecord());
    }

    // To decode high bit depth image source to half float backed image, we need
    // to sniff the image bit depth here to avoid double decoding.
    auto target_color_type =
        (is_f16_ && draw_image.paint_image().is_high_bit_depth())
            ? kRGBA_F16_SkColorType
            : kN32_SkColorType;
    cc::TargetColorParams target_color_params;
    target_color_params.color_space = color_space_;

    return cc::PlaybackImageProvider(
               &Image::SharedCCDecodeCache(target_color_type),
               target_color_params, cc::PlaybackImageProvider::Settings())
        .GetRasterContent(draw_image);
  }

 private:
  bool is_f16_;
  gfx::ColorSpace color_space_;
};

sk_sp<SkSurface> CreateSoftwareSurface(const CanvasSnapshotInfo& info) {
  const bool can_use_lcd_text = info.alpha_type == kOpaque_SkAlphaType;
  const auto props =
      skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
  return SkSurfaces::Raster(
      SkImageInfo::Make(info.size.width(), info.size.height(),
                        viz::ToClosestSkColorType(info.format),
                        kPremul_SkAlphaType, info.color_space.ToSkColorSpace()),
      &props);
}

scoped_refptr<StaticBitmapImage> CreateImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    CanvasNon2DResourceProviderSharedImage* snapshot_provider,
    std::optional<CanvasSnapshotInfo> sw_draw_info,
    media::PaintCanvasVideoRenderer* video_renderer,
    bool prefer_tagged_orientation,
    bool reinterpret_video_as_srgb,
    std::optional<media::VideoTransformation> transformation_override) {
  CHECK(sw_draw_info || snapshot_provider);
  CHECK(!sw_draw_info || !snapshot_provider);

  auto raster_context_provider = GetRasterContextProvider();
  bool is_accelerated = snapshot_provider && !snapshot_provider->IsSoftware();
  if (is_accelerated) {
    prefer_tagged_orientation = false;
  }

  const auto transform = transformation_override.value_or(
      frame->metadata().transformation.value_or(media::kNoTransformation));

  // If not doing an accelerated draw, avoid GPU round trips to upload frame
  // data from MappableSI-backed frames.
  if (frame->HasMappableSharedImage() && !is_accelerated) {
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
  media_flags.setTargetedHdrHeadroom(
      cc::PaintFlags::TargetedHdrHeadroom::kDisableEverything);

  std::unique_ptr<media::PaintCanvasVideoRenderer> local_video_renderer;
  if (!video_renderer) {
    local_video_renderer = std::make_unique<media::PaintCanvasVideoRenderer>();
    video_renderer = local_video_renderer.get();
  }

  media::PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect = gfx::RectF(snapshot_provider ? snapshot_provider->Size()
                                                  : sw_draw_info->size);
  params.transformation =
      prefer_tagged_orientation
          ? media::kNoTransformation
          : frame->metadata().transformation.value_or(media::kNoTransformation);
  params.reinterpret_as_srgb = reinterpret_video_as_srgb;
  auto draw_callback = [&](cc::PaintCanvas& canvas) {
    video_renderer->Paint(frame.get(), &canvas, media_flags, params,
                          raster_context_provider.get());
  };
  auto orientation = prefer_tagged_orientation
                         ? VideoTransformationToImageOrientation(transform)
                         : ImageOrientationEnum::kDefault;
  if (sw_draw_info) {
    return DrawAndSnapshotToImage(sw_draw_info.value(), draw_callback,
                                  orientation);
  }

  return static_cast<CanvasNon2DResourceProviderSharedImage*>(snapshot_provider)
      ->DoExternalOverdrawAndSnapshot(draw_callback, orientation);
}

}  // namespace

scoped_refptr<StaticBitmapImage> DrawAndSnapshotToImage(
    const CanvasSnapshotInfo& info,
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback,
    ImageOrientation orientation) {
  auto surface = CreateSoftwareSurface(info);
  if (!surface) {
    return nullptr;
  }

  ImageProviderImpl image_provider(
      info.format == viz::SinglePlaneFormat::kRGBA_F16, info.color_space);
  cc::SkiaPaintCanvas canvas(surface->getCanvas(), &image_provider);
  draw_callback(canvas);

  cc::PaintImage paint_image;

  auto sk_image = surface->makeImageSnapshot();
  if (sk_image) {
    paint_image =
        PaintImageBuilder::WithDefault()
            .set_id(cc::PaintImage::GetNextId())
            .set_image(std::move(sk_image), PaintImage::GetNextContentId())
            .set_hdr_metadata(info.hdr_metadata)
            .TakePaintImage();
  }

  DCHECK(!paint_image.IsTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(paint_image),
                                                orientation);
}

bool ShouldCreateAcceleratedImages(
    viz::RasterContextProvider* raster_context_provider) {
  if (!raster_context_provider) {
    return false;
  }

  if (!SharedGpuContext::IsGpuCompositingEnabled()) {
    return false;
  }

  if (raster_context_provider->GetGpuFeatureInfo().IsWorkaroundEnabled(
          DISABLE_IMAGEBITMAP_FROM_VIDEO_USING_GPU)) {
    return false;
  }

  return true;
}

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

scoped_refptr<StaticBitmapImage> CreateAcceleratedImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    CanvasNon2DResourceProviderSharedImage* snapshot_provider,
    media::PaintCanvasVideoRenderer* video_renderer,
    bool prefer_tagged_orientation,
    bool reinterpret_video_as_srgb,
    std::optional<media::VideoTransformation> transformation_override) {
  CHECK(snapshot_provider);
  return CreateImageFromVideoFrame(
      std::move(frame), snapshot_provider, /*sw_draw_info=*/std::nullopt,
      video_renderer, prefer_tagged_orientation, reinterpret_video_as_srgb,
      transformation_override);
}

scoped_refptr<StaticBitmapImage> CreateUnacceleratedImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    const CanvasSnapshotInfo& draw_info,
    media::PaintCanvasVideoRenderer* video_renderer,
    bool prefer_tagged_orientation,
    bool reinterpret_video_as_srgb,
    std::optional<media::VideoTransformation> transformation_override) {
  return CreateImageFromVideoFrame(
      std::move(frame), /*snapshot_provider=*/nullptr, draw_info,
      video_renderer, prefer_tagged_orientation, reinterpret_video_as_srgb,
      transformation_override);
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

CanvasSnapshotInfo CreateSnapshotProviderInfoForVideoFrame(
    const media::VideoFrame& frame,
    std::optional<gfx::Size> scaled_size,
    bool reinterpret_video_as_srgb) {
  return {
      .alpha_type = media::IsOpaque(frame.format()) ? kOpaque_SkAlphaType
                                                    : kPremul_SkAlphaType,
      .color_space = reinterpret_video_as_srgb ? gfx::ColorSpace::CreateSRGB()
                                               : frame.CompatRGBColorSpace(),
      .hdr_metadata = frame.hdr_metadata(),
      // TODO(https://crbug.com/40230609): N32 may be incorrect when drawing
      // high bit depth frames destined for a high bit depth canvas.
      .format = GetN32FormatForCanvas(),
      .size = scaled_size.value_or(frame.natural_size()),
  };
}

}  // namespace blink
