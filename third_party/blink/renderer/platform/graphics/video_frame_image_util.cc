// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/raster/playback_image_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/gfx/paint_canvas_video_renderer.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/flush_for_image_listener.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
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
    CHECK(!draw_image.paint_image().IsDeferredPaintRecord());

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
    VideoFrameAcceleratedSnapshotPool* snapshot_pool,
    std::optional<CanvasSnapshotInfo> sw_draw_info,
    media::PaintCanvasVideoRenderer* video_renderer,
    VideoOrientationBehavior orientation_behavior,
    VideoColorSpaceInterpretation color_space_interpretation) {
  CHECK(sw_draw_info || snapshot_pool);
  CHECK(!sw_draw_info || !snapshot_pool);

  auto raster_context_provider = GetRasterContextProvider();
  bool is_accelerated = snapshot_pool != nullptr;
  if (is_accelerated) {
    orientation_behavior = VideoOrientationBehavior::kHardFlip;
  }

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

  const auto transform =
      frame->metadata().transformation.value_or(media::kNoTransformation);

  media::PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect =
      gfx::RectF(snapshot_pool ? snapshot_pool->Size() : sw_draw_info->size);
  params.transformation =
      orientation_behavior == VideoOrientationBehavior::kTagOrientation
          ? media::kNoTransformation
          : transform;
  params.reinterpret_as_srgb =
      color_space_interpretation ==
      VideoColorSpaceInterpretation::kReinterpretAsSRGB;
  auto draw_callback = [&](cc::PaintCanvas& canvas) {
    video_renderer->Paint(frame.get(), &canvas, media_flags, params,
                          raster_context_provider.get());
  };
  if (sw_draw_info) {
    auto orientation =
        orientation_behavior == VideoOrientationBehavior::kTagOrientation
            ? VideoTransformationToImageOrientation(transform)
            : ImageOrientationEnum::kDefault;
    return DrawAndSnapshotToImage(sw_draw_info.value(), draw_callback,
                                  orientation);
  }

  return snapshot_pool->DoExternalOverdrawAndSnapshot(draw_callback);
}

}  // namespace

// static
std::unique_ptr<VideoFrameAcceleratedSnapshotPool>
VideoFrameAcceleratedSnapshotPool::Create(const CanvasSnapshotInfo& info) {
  // IsGpuCompositingEnabled can re-create the context if it has been lost, do
  // this up front so that we can fail early and not expose ourselves to
  // use after free bugs (crbug.com/1126424)
  SharedGpuContext::IsGpuCompositingEnabled();

  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();

  // If the context is lost we don't want to re-create it here, the resulting
  // resource provider would be invalid anyway
  if (!context_provider_wrapper ||
      !context_provider_wrapper->ContextProvider().RasterInterface() ||
      context_provider_wrapper->ContextProvider().IsContextLost()) {
    return nullptr;
  }

  const auto& capabilities =
      context_provider_wrapper->ContextProvider().GetCapabilities();
  if ((info.size.width() < 1 || info.size.height() < 1 ||
       info.size.width() > capabilities.max_texture_size ||
       info.size.height() > capabilities.max_texture_size)) {
    return nullptr;
  }

  // TODO(crbug.com/40767377): Pass in info as is for all cases.
  // Overriding the info to use RGBA instead of N32 is needed because code
  // elsewhere assumes RGBA. OTOH the software path seems to be assuming N32
  // somewhere in the later pipeline but for offscreen canvas only.
  auto format = info.format;
  if (format != viz::SinglePlaneFormat::kRGBA_F16) {
    format = viz::SinglePlaneFormat::kRGBA_8888;
  }

  CanvasSnapshotInfo pool_info = info;
  pool_info.format = format;
  auto provider = base::WrapUnique(new CanvasNon2DResourceProvider(
      pool_info.size, pool_info.format, pool_info.alpha_type,
      pool_info.color_space, pool_info.hdr_metadata, context_provider_wrapper,
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, base::DoNothing()));
  auto pool = base::WrapUnique(new VideoFrameAcceleratedSnapshotPool(
      pool_info, std::move(context_provider_wrapper), std::move(provider)));
  return pool->IsValid() ? std::move(pool) : nullptr;
}

VideoFrameAcceleratedSnapshotPool::VideoFrameAcceleratedSnapshotPool(
    const CanvasSnapshotInfo& info,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    std::unique_ptr<CanvasNon2DResourceProvider> provider)
    : info_(info),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      provider_(std::move(provider)) {
  CHECK(provider_);
}

VideoFrameAcceleratedSnapshotPool::~VideoFrameAcceleratedSnapshotPool() =
    default;

gpu::raster::RasterInterface*
VideoFrameAcceleratedSnapshotPool::RasterInterface() const {
  if (!context_provider_wrapper_) {
    return nullptr;
  }
  return context_provider_wrapper_->ContextProvider().RasterInterface();
}

bool VideoFrameAcceleratedSnapshotPool::IsValid() const {
  auto* raster_interface = RasterInterface();
  return raster_interface &&
         raster_interface->GetGraphicsResetStatusKHR() == GL_NO_ERROR;
}

scoped_refptr<CanvasResourceSharedImage>
VideoFrameAcceleratedSnapshotPool::NewOrRecycledResource() {
  if (!provider_->image_pool_) {
    return nullptr;
  }

  auto resource = provider_->image_pool_->GetImage();
  if (!resource) {
    return nullptr;
  }

  if (!resource->IsInitialized()) {
    resource->Initialize(provider_->CreateWeakPtr(), context_provider_wrapper_,
                         info_.hdr_metadata, /*is_accelerated=*/true);
    ++provider_->num_inflight_resources_;
    if (provider_->num_inflight_resources_ > provider_->max_inflight_resources_) {
      provider_->max_inflight_resources_ = provider_->num_inflight_resources_;
    }
  }
  DCHECK(resource->HasOneRef());
  return resource;
}

bool VideoFrameAcceleratedSnapshotPool::ShouldReplaceTargetBuffer(
    PaintImage::ContentId content_id) {
  // If the resource was lost, we can not use it for writes again.
  if (provider_->resource()->IsLost()) {
    return true;
  }

  // We have the only ref to the resource which implies there are no active
  // readers.
  if (provider_->resource_->HasOneRef()) {
    return false;
  }

  // Its possible to have deferred work in skia which uses this resource. Try
  // flushing once to see if that releases the read refs. We can avoid a copy
  // by queuing this work before writing to this resource.
  FlushForImageListener::Get()->NotifyFlushForImage(content_id);

  return !provider_->resource_->HasOneRef();
}

void VideoFrameAcceleratedSnapshotPool::EnsureResourceReadyForDraw() {
  DCHECK(provider_->resource_);

  // Determine if a new resource is needed.
  if (ShouldReplaceTargetBuffer(provider_->cached_content_id_)) {
    provider_->cached_content_id_ = PaintImage::kInvalidContentId;

    provider_->resource_ = NewOrRecycledResource();

    // As the image might have just been created, we need to ensure that it is
    // cleared on the next BeginRasterCHROMIUM to satisfy service-side security
    // requirements (note: as an optimization we could avoid doing this if the
    // resource was recycled as in that case there are no security implications).
    provider_->is_cleared_ = false;
  }
}

void VideoFrameAcceleratedSnapshotPool::FlushRecording(
    cc::PaintRecord last_recording) {
  if (IsValid()) {
    EnsureResourceReadyForDraw();

    const bool needs_clear = !provider_->is_cleared_;
    provider_->is_cleared_ = true;

    // Create an ImageDecodeCache for half float images only if the canvas is
    // using half float back storage.
    cc::ImageDecodeCache* cache_f16 = nullptr;
    if (info_.format == viz::SinglePlaneFormat::kRGBA_F16) {
      cache_f16 = context_provider_wrapper_->ContextProvider().ImageDecodeCache(
          kRGBA_F16_SkColorType);
    }

    cc::ImageDecodeCache* cache_rgba8 =
        context_provider_wrapper_->ContextProvider().ImageDecodeCache(
            kN32_SkColorType);

    CanvasImageProvider image_provider(
        cache_rgba8, cache_f16, info_.color_space, info_.format,
        cc::PlaybackImageProvider::RasterMode::kGpu, context_provider_wrapper_);

    gpu::SyncToken sync_token = RasterInterface()->RasterSharedImage(
        provider_->resource()->GetSharedImage(),
        provider_->resource()->acquire_sync_token(), std::move(last_recording),
        &image_provider, needs_clear);
    provider_->resource()->SetReleaseSyncToken(sync_token);

    // Images are locked for the duration of the rasterization, in case they get
    // used multiple times. We can unlock them once the rasterization is
    // complete.
    image_provider.ReleaseLockedImages();
    image_provider.UnbindTextureBackedImages();
  }
}

scoped_refptr<StaticBitmapImage>
VideoFrameAcceleratedSnapshotPool::DoExternalOverdrawAndSnapshot(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback) {
  if (!IsValid()) {
    return nullptr;
  }

  MemoryManagedPaintRecorder recorder(Size(), /*client=*/nullptr);
  draw_callback(recorder.getRecordingCanvas());
  if (recorder.HasReleasableDrawOps()) {
    FlushRecording(recorder.ReleaseMainRecording());
  }

  if (!IsValid()) {
    return nullptr;
  }

  scoped_refptr<StaticBitmapImage> snapshot = provider_->resource_->Bitmap();
  if (snapshot) {
    provider_->cached_content_id_ =
        snapshot->PaintImageForCurrentFrame().GetContentIdForFrame(0u);
  }
  DCHECK(snapshot);
  return snapshot;
}

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
    VideoFrameAcceleratedSnapshotPool* snapshot_pool,
    media::PaintCanvasVideoRenderer* video_renderer,
    VideoOrientationBehavior orientation_behavior,
    VideoColorSpaceInterpretation color_space_interpretation) {
  CHECK(snapshot_pool);
  return CreateImageFromVideoFrame(
      std::move(frame), snapshot_pool, /*sw_draw_info=*/std::nullopt,
      video_renderer, orientation_behavior, color_space_interpretation);
}

scoped_refptr<StaticBitmapImage> CreateUnacceleratedImageFromVideoFrame(
    scoped_refptr<media::VideoFrame> frame,
    const CanvasSnapshotInfo& draw_info,
    media::PaintCanvasVideoRenderer* video_renderer,
    VideoOrientationBehavior orientation_behavior,
    VideoColorSpaceInterpretation color_space_interpretation) {
  return CreateImageFromVideoFrame(
      std::move(frame), /*snapshot_provider=*/nullptr, draw_info,
      video_renderer, orientation_behavior, color_space_interpretation);
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
  gfx::SizeF dest_size(frame->natural_size());
  if (!ignore_video_transformation &&
      frame->metadata()
          .transformation.value_or(media::kNoTransformation)
          .IsOrthogonal()) {
    dest_size.Transpose();
  }
  params.dest_rect = gfx::RectF(dest_size);
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
    VideoColorSpaceInterpretation color_space_interpretation,
    VideoOrientationBehavior orientation_behavior) {
  const auto transform =
      frame.metadata().transformation.value_or(media::kNoTransformation);
  gfx::Size size = scaled_size.value_or(frame.natural_size());
  if (orientation_behavior == VideoOrientationBehavior::kHardFlip &&
      !scaled_size && transform.IsOrthogonal()) {
    size.Transpose();
  }
  return {
      .alpha_type = media::IsOpaque(frame.format()) ? kOpaque_SkAlphaType
                                                    : kPremul_SkAlphaType,
      .color_space = color_space_interpretation ==
                             VideoColorSpaceInterpretation::kReinterpretAsSRGB
                         ? gfx::ColorSpace::CreateSRGB()
                         : frame.CompatRGBColorSpace(),
      .hdr_metadata = frame.hdr_metadata(),
      // TODO(https://crbug.com/40230609): N32 may be incorrect when drawing
      // high bit depth frames destined for a high bit depth canvas.
      .format = GetN32FormatForCanvas(),
      .size = size,
  };
}

}  // namespace blink
