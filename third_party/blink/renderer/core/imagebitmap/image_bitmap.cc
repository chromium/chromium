// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/image_element_base.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_transform.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gfx.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_skia.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSwizzle.h"

namespace blink {

constexpr const char* kImageOrientationFlipY = "flipY";
constexpr const char* kImageOrientationFromImage = "from-image";
constexpr const char* kImageBitmapOptionNone = "none";
constexpr const char* kImageBitmapOptionDefault = "default";
constexpr const char* kImageBitmapOptionPremultiply = "premultiply";
constexpr const char* kImageBitmapOptionResizeQualityHigh = "high";
constexpr const char* kImageBitmapOptionResizeQualityMedium = "medium";
constexpr const char* kImageBitmapOptionResizeQualityPixelated = "pixelated";

namespace {

gfx::Size ParseDstSize(const ImageBitmapOptions* options,
                       const gfx::Rect& src_rect) {
  int resize_width = 0;
  int resize_height = 0;
  if (!options->hasResizeWidth() && !options->hasResizeHeight()) {
    resize_width = src_rect.width();
    resize_height = src_rect.height();
  } else if (options->hasResizeWidth() && options->hasResizeHeight()) {
    resize_width = options->resizeWidth();
    resize_height = options->resizeHeight();
  } else if (options->hasResizeWidth() && !options->hasResizeHeight()) {
    resize_width = options->resizeWidth();
    resize_height =
        ClampTo<unsigned>(ceil(static_cast<float>(options->resizeWidth()) /
                               src_rect.width() * src_rect.height()));
  } else {
    resize_height = options->resizeHeight();
    resize_width =
        ClampTo<unsigned>(ceil(static_cast<float>(options->resizeHeight()) /
                               src_rect.height() * src_rect.width()));
  }
  return gfx::Size(resize_width, resize_height);
}

ImageBitmap::ParsedOptions ParseOptions(const ImageBitmapOptions* options,
                                        std::optional<gfx::Rect> crop_rect,
                                        gfx::Size source_size,
                                        ImageOrientation source_orientation,
                                        bool source_is_unpremul) {
  ImageBitmap::ParsedOptions parsed_options;
  if (options->imageOrientation() == kImageOrientationFlipY) {
    parsed_options.flip_y = true;
    parsed_options.orientation_from_image = true;
    parsed_options.source_orientation = source_orientation;
  } else {
    DCHECK(options->imageOrientation() == kImageOrientationFromImage ||
           options->imageOrientation() == kImageBitmapOptionNone);
    parsed_options.flip_y = false;
    parsed_options.orientation_from_image = true;
    parsed_options.source_orientation = source_orientation;
    if (base::FeatureList::IsEnabled(
            features::kCreateImageBitmapOrientationNone) &&
        options->imageOrientation() == kImageBitmapOptionNone) {
      parsed_options.orientation_from_image = false;
      parsed_options.source_orientation = ImageOrientation();
    }
  }

  parsed_options.source_is_unpremul = source_is_unpremul;
  if (options->premultiplyAlpha() == kImageBitmapOptionNone) {
    parsed_options.premultiply_alpha = false;
  } else {
    parsed_options.premultiply_alpha = true;
    DCHECK(options->premultiplyAlpha() == kImageBitmapOptionDefault ||
           options->premultiplyAlpha() == kImageBitmapOptionPremultiply);
  }

  parsed_options.has_color_space_conversion =
      (options->colorSpaceConversion() != kImageBitmapOptionNone);
  if (options->colorSpaceConversion() != kImageBitmapOptionNone &&
      options->colorSpaceConversion() != kImageBitmapOptionDefault) {
    NOTREACHED_IN_MIGRATION()
        << "Invalid ImageBitmap creation attribute colorSpaceConversion: "
        << IDLEnumAsString(options->colorSpaceConversion());
  }

  parsed_options.source_size =
      parsed_options.source_orientation.UsesWidthAsHeight()
          ? gfx::TransposeSize(source_size)
          : source_size;
  if (!crop_rect) {
    // TODO(crbug.com/40773069): This should use `parsed_options.source_size`,
    // because it should be in the same (post-orientation) space. The are
    // other bugs that depend on this bug, so keep this present, adding
    // `source_rect` as the future replacement.
    parsed_options.crop_rect = gfx::Rect(source_size);
    parsed_options.source_rect = gfx::Rect(parsed_options.source_size);
  } else {
    parsed_options.crop_rect = *crop_rect;
    parsed_options.source_rect = *crop_rect;
  }
  // TODO(crbug.com/40773069): The above error propagates into `resize_width`
  // and `resize_height`. Add `dest_size` as the future replacement.
  gfx::Size resize = ParseDstSize(options, parsed_options.crop_rect);
  parsed_options.resize_width = resize.width();
  parsed_options.resize_height = resize.height();
  parsed_options.dest_size = ParseDstSize(options, parsed_options.source_rect);

  if (static_cast<int>(parsed_options.resize_width) ==
          parsed_options.crop_rect.width() &&
      static_cast<int>(parsed_options.resize_height) ==
          parsed_options.crop_rect.height()) {
    parsed_options.should_scale_input = false;
    return parsed_options;
  }
  parsed_options.should_scale_input = true;

  if (options->resizeQuality() == kImageBitmapOptionResizeQualityHigh)
    parsed_options.resize_quality = cc::PaintFlags::FilterQuality::kHigh;
  else if (options->resizeQuality() == kImageBitmapOptionResizeQualityMedium)
    parsed_options.resize_quality = cc::PaintFlags::FilterQuality::kMedium;
  else if (options->resizeQuality() == kImageBitmapOptionResizeQualityPixelated)
    parsed_options.resize_quality = cc::PaintFlags::FilterQuality::kNone;
  else
    parsed_options.resize_quality = cc::PaintFlags::FilterQuality::kLow;

  parsed_options.sampling = cc::PaintFlags::FilterQualityToSkSamplingOptions(
      parsed_options.resize_quality);
  return parsed_options;
}

ImageBitmap::ParsedOptions ParseOptions(const ImageBitmapOptions* options,
                                        std::optional<gfx::Rect> crop_rect,
                                        scoped_refptr<Image> input) {
  const auto info = input->PaintImageForCurrentFrame().GetSkImageInfo();
  return ParseOptions(options, crop_rect,
                      gfx::Size(info.width(), info.height()),
                      input->CurrentFrameOrientation(),
                      info.alphaType() == kUnpremul_SkAlphaType);
}

ImageBitmap::ParsedOptions ParseOptions(
    const ImageBitmapOptions* options,
    std::optional<gfx::Rect> crop_rect,
    scoped_refptr<StaticBitmapImage> input) {
  auto info = input->GetSkImageInfo();
  return ParseOptions(options, crop_rect,
                      gfx::Size(info.width(), info.height()),
                      input->CurrentFrameOrientation(),
                      info.alphaType() == kUnpremul_SkAlphaType);
}

// The function dstBufferSizeHasOverflow() is being called at the beginning of
// each ImageBitmap() constructor, which makes sure that doing
// width * height * bytesPerPixel will never overflow unsigned.
// This function assumes that the pixel format is N32.
bool DstBufferSizeHasOverflow(const ImageBitmap::ParsedOptions& options) {
  base::CheckedNumeric<unsigned> total_bytes = options.crop_rect.width();
  total_bytes *= options.crop_rect.height();
  total_bytes *= SkColorTypeBytesPerPixel(kN32_SkColorType);
  if (!total_bytes.IsValid())
    return true;

  if (!options.should_scale_input)
    return false;
  total_bytes = options.resize_width;
  total_bytes *= options.resize_height;
  total_bytes *= SkColorTypeBytesPerPixel(kN32_SkColorType);
  if (!total_bytes.IsValid())
    return true;

  return false;
}

SkImageInfo GetSkImageInfo(const scoped_refptr<Image>& input) {
  return input->PaintImageForCurrentFrame().GetSkImageInfo();
}

scoped_refptr<StaticBitmapImage> ApplyTransformsFromOptions(
    scoped_refptr<StaticBitmapImage> source,
    const ImageBitmap::ParsedOptions& options,
    bool force_copy = false) {
  // Early-out for empty transformations.
  if (options.source_rect.IsEmpty() || options.dest_size.IsEmpty()) {
    return nullptr;
  }

  StaticBitmapImageTransform::Params params;
  params.force_copy = force_copy;
  params.flip_y = options.flip_y;
  params.premultiply_alpha = options.premultiply_alpha;
  params.has_color_space_conversion = options.has_color_space_conversion;
  params.orientation_from_image = options.orientation_from_image;
  params.sampling = options.sampling;
  params.source_rect = options.source_rect;
  params.dest_size = options.dest_size;
  return StaticBitmapImageTransform::Apply(FlushReason::kCreateImageBitmap,
                                           source, params);
}

scoped_refptr<StaticBitmapImage> MakeBlankImage(
    const ImageBitmap::ParsedOptions& parsed_options) {
  SkImageInfo info = SkImageInfo::Make(
      parsed_options.crop_rect.width(), parsed_options.crop_rect.height(),
      kN32_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  if (parsed_options.should_scale_input) {
    info =
        info.makeWH(parsed_options.resize_width, parsed_options.resize_height);
  }
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  if (!surface)
    return nullptr;
  return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

}  // namespace

sk_sp<SkImage> ImageBitmap::GetSkImageFromDecoder(
    std::unique_ptr<ImageDecoder> decoder) {
  if (!decoder->FrameCount())
    return nullptr;
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  if (!frame || frame->GetStatus() != ImageFrame::kFrameComplete)
    return nullptr;
  DCHECK(!frame->Bitmap().isNull() && !frame->Bitmap().empty());
  return frame->FinalizePixelsAndGetImage();
}

ImageBitmap::ImageBitmap(ImageElementBase* image,
                         std::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  auto* cached = image->CachedImage();
  scoped_refptr<Image> input = cached ? cached->GetImage() : Image::NullImage();
  DCHECK(!input->IsTextureBacked());

  ParsedOptions parsed_options = ParseOptions(options, crop_rect, input);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  cc::PaintImage paint_image = input->PaintImageForCurrentFrame();
  if (!paint_image)
    return;

  DCHECK(!paint_image.IsTextureBacked());
  if (input->IsBitmapImage()) {
    // A BitmapImage indicates that this is a coded backed image.
    if (!input->HasData())
      return;

    DCHECK(paint_image.IsLazyGenerated());
    const bool data_complete = true;
    std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
        input->Data(), data_complete,
        parsed_options.premultiply_alpha ? ImageDecoder::kAlphaPremultiplied
                                         : ImageDecoder::kAlphaNotPremultiplied,
        paint_image.GetColorType() == kRGBA_F16_SkColorType
            ? ImageDecoder::kHighBitDepthToHalfFloat
            : ImageDecoder::kDefaultBitDepth,
        parsed_options.has_color_space_conversion ? ColorBehavior::kTag
                                                  : ColorBehavior::kIgnore,
        cc::AuxImage::kDefault, Platform::GetMaxDecodedImageBytes()));
    auto skia_image = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));
    if (!skia_image)
      return;

    paint_image = PaintImageBuilder::WithDefault()
                      .set_id(paint_image.stable_id())
                      .set_image(std::move(skia_image),
                                 paint_image.GetContentIdForFrame(0u))
                      .TakePaintImage();

    // Update source alpha states after redecoding.
    parsed_options.source_is_unpremul =
        paint_image.GetAlphaType() == kUnpremul_SkAlphaType;

  } else if (paint_image.IsLazyGenerated()) {
    // Other Image types can still produce lazy generated images (for example
    // SVGs).
    SkBitmap bitmap;
    SkImageInfo image_info = GetSkImageInfo(input);
    bitmap.allocPixels(image_info, image_info.minRowBytes());
    if (!paint_image.GetSwSkImage()->readPixels(bitmap.pixmap(), 0, 0))
      return;

    paint_image = PaintImageBuilder::WithDefault()
                      .set_id(paint_image.stable_id())
                      .set_image(SkImages::RasterFromBitmap(bitmap),
                                 paint_image.GetContentIdForFrame(0u))
                      .TakePaintImage();
  }

  auto static_input = UnacceleratedStaticBitmapImage::Create(
      std::move(paint_image), input->CurrentFrameOrientation());

  image_ = ApplyTransformsFromOptions(static_input, parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(!image->WouldTaintOrigin());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video,
                         std::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  // TODO(crbug.com/1181329): ImageBitmap resize test case failed when
  // quality equals to "low" and "medium". Need further investigate to
  // enable gpu backed imageBitmap with resize options.
  const bool allow_accelerated_images =
      !options->hasResizeWidth() && !options->hasResizeHeight();
  const bool reinterpret_as_srgb =
      (options->colorSpaceConversion() == kImageBitmapOptionNone);
  auto input = video->CreateStaticBitmapImage(
      allow_accelerated_images, /*size=*/std::nullopt, reinterpret_as_srgb);
  if (!input)
    return;

  ParsedOptions parsed_options = ParseOptions(options, crop_rect, input);
  if (DstBufferSizeHasOverflow(parsed_options)) {
    return;
  }

  image_ = ApplyTransformsFromOptions(input, parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(!video->WouldTaintOrigin());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas,
                         std::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  SourceImageStatus status;
  scoped_refptr<Image> image_input =
      canvas->GetSourceImageForCanvas(FlushReason::kCreateImageBitmap, &status,
                                      gfx::SizeF(), kPremultiplyAlpha);
  if (status != kNormalSourceImageStatus)
    return;
  DCHECK(IsA<StaticBitmapImage>(image_input.get()));
  scoped_refptr<StaticBitmapImage> input =
      static_cast<StaticBitmapImage*>(image_input.get());

  const ParsedOptions parsed_options = ParseOptions(options, crop_rect, input);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ = ApplyTransformsFromOptions(input, parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(canvas->OriginClean());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(OffscreenCanvas* offscreen_canvas,
                         std::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  SourceImageStatus status;
  scoped_refptr<Image> raw_input = offscreen_canvas->GetSourceImageForCanvas(
      FlushReason::kCreateImageBitmap, &status,
      gfx::SizeF(offscreen_canvas->Size()));
  DCHECK(IsA<StaticBitmapImage>(raw_input.get()));
  scoped_refptr<StaticBitmapImage> input =
      static_cast<StaticBitmapImage*>(raw_input.get());
  raw_input = nullptr;

  if (status != kNormalSourceImageStatus)
    return;

  const ParsedOptions parsed_options = ParseOptions(options, crop_rect, input);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ = ApplyTransformsFromOptions(input, parsed_options);
  if (!image_)
    return;
  image_->SetOriginClean(offscreen_canvas->OriginClean());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(const SkPixmap& pixmap,
                         bool is_image_bitmap_origin_clean,
                         ImageOrientationEnum image_orientation) {
  sk_sp<SkImage> raster_copy = SkImages::RasterFromPixmapCopy(pixmap);
  if (!raster_copy)
    return;
  image_ = UnacceleratedStaticBitmapImage::Create(std::move(raster_copy));
  if (!image_)
    return;
  image_->SetOriginClean(is_image_bitmap_origin_clean);
  image_->SetOrientation(image_orientation);
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(ImageData* data,
                         std::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  const ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, data->BitmapSourceSize(),
                   ImageOrientationEnum::kOriginTopLeft,
                   /*source_is_unpremul=*/true);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  // Create a StaticBitmapImage that directly references the ImageData pixels.
  SkPixmap pm = data->GetSkPixmap();
  auto sk_data = SkData::MakeWithoutCopy(pm.addr(), pm.computeByteSize());
  auto image = StaticBitmapImage::Create(sk_data, pm.info(),
                                         ImageOrientationEnum::kOriginTopLeft);

  // Force a copy of the data during the transformation (so that we do not
  // reference ImageData's mutable data).
  image_ =
      ApplyTransformsFromOptions(image, parsed_options, /*force_copy=*/true);
  if (!image_)
    return;

  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap,
                         std::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  scoped_refptr<StaticBitmapImage> input = bitmap->BitmapImage();
  if (!input)
    return;
  const ParsedOptions parsed_options = ParseOptions(options, crop_rect, input);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ = ApplyTransformsFromOptions(input, parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(bitmap->OriginClean());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(scoped_refptr<StaticBitmapImage> image,
                         std::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  bool origin_clean = image->OriginClean();
  const ParsedOptions parsed_options = ParseOptions(options, crop_rect, image);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ = ApplyTransformsFromOptions(image, parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(origin_clean);
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(scoped_refptr<StaticBitmapImage> image) {
  image_ = std::move(image);
  UpdateImageBitmapMemoryUsage();
}

scoped_refptr<StaticBitmapImage> ImageBitmap::Transfer() {
  DCHECK(!IsNeutered());
  if (!image_->HasOneRef()) {
    // For it to be safe to transfer a StaticBitmapImage it must not be
    // referenced by any other object on this thread.
    // The first step is to attempt to release other references via
    // NotifyWillTransfer
    const auto content_id =
        image_->PaintImageForCurrentFrame().GetContentIdForFrame(0);
    CanvasResourceProvider::NotifyWillTransfer(content_id);

    // If will still have other references, the last resort is to make a copy
    // of the bitmap.  This could happen, for example, if another ImageBitmap
    // or a CanvasPattern object points to the same StaticBitmapImage.
    // This approach is slow and wateful but it is only to handle extremely
    // rare edge cases.
    if (!image_->HasOneRef()) {
      auto copy = StaticBitmapImageTransform::Clone(
          FlushReason::kCreateImageBitmap, image_);
      if (!copy) {
        return nullptr;
      }
      image_ = std::move(copy);
    }
  }

  DCHECK(image_->HasOneRef());
  is_neutered_ = true;
  image_->Transfer();
  UpdateImageBitmapMemoryUsage();
  return std::move(image_);
}

void ImageBitmap::UpdateImageBitmapMemoryUsage() {
  // TODO(fserb): We should be calling GetCanvasColorParams().BytesPerPixel()
  // but this is breaking some tests due to the repaint of the image.
  int bytes_per_pixel = 4;

  int32_t new_memory_usage = 0;

  if (!is_neutered_ && image_) {
    base::CheckedNumeric<int32_t> memory_usage_checked = bytes_per_pixel;
    memory_usage_checked *= image_->width();
    memory_usage_checked *= image_->height();
    new_memory_usage = memory_usage_checked.ValueOrDefault(
        std::numeric_limits<int32_t>::max());
  }

  external_memory_accounter_.Update(v8::Isolate::GetCurrent(),
                                    new_memory_usage - memory_usage_);
  memory_usage_ = new_memory_usage;
}

ImageBitmap::~ImageBitmap() {
  external_memory_accounter_.Decrease(v8::Isolate::GetCurrent(), memory_usage_);
}

void ImageBitmap::ResolvePromiseOnOriginalThread(
    ScriptPromiseResolver<ImageBitmap>* resolver,
    bool origin_clean,
    std::unique_ptr<ParsedOptions> parsed_options,
    sk_sp<SkImage> skia_image,
    const ImageOrientationEnum orientation) {
  if (!skia_image) {
    resolver->Reject(v8::Null(resolver->GetScriptState()->GetIsolate()));
    return;
  }
  scoped_refptr<StaticBitmapImage> image =
      UnacceleratedStaticBitmapImage::Create(std::move(skia_image),
                                             orientation);
  DCHECK(IsMainThread());
  if (!image) {
    resolver->Reject(v8::Null(resolver->GetScriptState()->GetIsolate()));
    return;
  }
  ImageBitmap* bitmap = MakeGarbageCollected<ImageBitmap>(image);
  bitmap->BitmapImage()->SetOriginClean(origin_clean);
  resolver->Resolve(bitmap);
}

void ImageBitmap::RasterizeImageOnBackgroundThread(
    PaintRecord paint_record,
    const gfx::Rect& dst_rect,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    WTF::CrossThreadOnceFunction<void(sk_sp<SkImage>,
                                      const ImageOrientationEnum)> callback) {
  DCHECK(!IsMainThread());
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(dst_rect.width(), dst_rect.height());
  SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info, &props);
  sk_sp<SkImage> skia_image;
  if (surface) {
    paint_record.Playback(surface->getCanvas());
    skia_image = surface->makeImageSnapshot();
  }
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(std::move(callback), std::move(skia_image),
                          ImageOrientationEnum::kDefault));
}

ScriptPromise<ImageBitmap> ImageBitmap::CreateAsync(
    ImageElementBase* image,
    std::optional<gfx::Rect> crop_rect,
    ScriptState* script_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojom::blink::PreferredColorScheme preferred_color_scheme,
    ExceptionState& exception_state,
    const ImageBitmapOptions* options) {
  scoped_refptr<Image> input = image->CachedImage()->GetImage();
  DCHECK(input->IsSVGImage());

  const ParsedOptions parsed_options = ParseOptions(options, crop_rect, input);
  if (DstBufferSizeHasOverflow(parsed_options)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The ImageBitmap could not be allocated.");
    return EmptyPromise();
  }
  gfx::Rect input_rect(input->Size());

  // In the case when |crop_rect| doesn't intersect the source image, we return
  // a transparent black image, respecting the color_params but ignoring
  // premultiply_alpha.
  if (!parsed_options.crop_rect.Intersects(input_rect)) {
    ImageBitmap* bitmap =
        MakeGarbageCollected<ImageBitmap>(MakeBlankImage(parsed_options));
    if (bitmap->BitmapImage()) {
      bitmap->BitmapImage()->SetOriginClean(!image->WouldTaintOrigin());
      return ToResolvedPromise<ImageBitmap>(script_state, bitmap);
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The ImageBitmap could not be allocated.");
      return EmptyPromise();
    }
  }

  gfx::Rect draw_src_rect = parsed_options.crop_rect;
  gfx::Rect draw_dst_rect(0, 0, parsed_options.resize_width,
                          parsed_options.resize_height);
  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();
  if (parsed_options.flip_y) {
    canvas->translate(0, draw_dst_rect.height());
    canvas->scale(1, -1);
  }

  // apply the orientation from EXIF metadata if needed.
  if (!parsed_options.orientation_from_image &&
      input->CurrentFrameOrientation() !=
          ImageOrientationEnum::kOriginTopLeft) {
    auto affineTransform =
        input->CurrentFrameOrientation().TransformFromDefault(
            gfx::SizeF(draw_dst_rect.size()));
    canvas->concat(AffineTransformToSkM44(affineTransform));
    if (input->CurrentFrameOrientation().UsesWidthAsHeight()) {
      draw_dst_rect.set_size(gfx::TransposeSize(draw_dst_rect.size()));
    }
  }

  SVGImageForContainer::Create(To<SVGImage>(*input),
                               gfx::SizeF(input_rect.size()), 1, nullptr,
                               preferred_color_scheme)
      ->Draw(canvas, cc::PaintFlags(), gfx::RectF(draw_dst_rect),
             gfx::RectF(draw_src_rect), ImageDrawOptions());
  PaintRecord paint_record = recorder.finishRecordingAsPicture();

  std::unique_ptr<ParsedOptions> passed_parsed_options =
      std::make_unique<ParsedOptions>(parsed_options);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<ImageBitmap>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  worker_pool::PostTask(
      FROM_HERE,
      CrossThreadBindOnce(
          &RasterizeImageOnBackgroundThread, std::move(paint_record),
          draw_dst_rect, std::move(task_runner),
          CrossThreadBindOnce(&ResolvePromiseOnOriginalThread,
                              MakeUnwrappingCrossThreadHandle(resolver),
                              !image->WouldTaintOrigin(),
                              std::move(passed_parsed_options))));
  return promise;
}

void ImageBitmap::close() {
  if (!image_ || is_neutered_)
    return;
  image_ = nullptr;
  is_neutered_ = true;
  UpdateImageBitmapMemoryUsage();
}

// static
ImageBitmap* ImageBitmap::Take(ScriptPromiseResolverBase*,
                               sk_sp<SkImage> image) {
  return MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(std::move(image)));
}

SkImageInfo ImageBitmap::GetBitmapSkImageInfo() const {
  return GetSkImageInfo(image_);
}

Vector<uint8_t> ImageBitmap::CopyBitmapData(const SkImageInfo& info,
                                            bool apply_orientation) {
  return image_->CopyImageData(info, apply_orientation);
}

unsigned ImageBitmap::width() const {
  if (!image_)
    return 0;
  gfx::Size size = image_->PreferredDisplaySize();
  DCHECK_GT(size.width(), 0);
  return size.width();
}

unsigned ImageBitmap::height() const {
  if (!image_)
    return 0;
  gfx::Size size = image_->PreferredDisplaySize();
  DCHECK_GT(size.height(), 0);
  return size.height();
}

bool ImageBitmap::IsAccelerated() const {
  return image_ && image_->IsTextureBacked();
}

gfx::Size ImageBitmap::Size() const {
  if (!image_)
    return gfx::Size();
  DCHECK_GT(image_->width(), 0);
  DCHECK_GT(image_->height(), 0);
  return image_->PreferredDisplaySize();
}

ScriptPromise<ImageBitmap> ImageBitmap::CreateImageBitmap(
    ScriptState* script_state,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, MakeGarbageCollected<ImageBitmap>(this, crop_rect, options),
      options, exception_state);
}

scoped_refptr<Image> ImageBitmap::GetSourceImageForCanvas(
    FlushReason reason,
    SourceImageStatus* status,
    const gfx::SizeF&,
    const AlphaDisposition alpha_disposition) {
  *status = kNormalSourceImageStatus;
  if (!image_)
    return nullptr;

  scoped_refptr<StaticBitmapImage> image = image_;

  // If the alpha_disposition is already correct, or the image is opaque, this
  // is a no-op.
  return StaticBitmapImageTransform::GetWithAlphaDisposition(
      reason, std::move(image), alpha_disposition);
}

gfx::SizeF ImageBitmap::ElementSize(
    const gfx::SizeF&,
    const RespectImageOrientationEnum respect_orientation) const {
  return gfx::SizeF(image_->Size(respect_orientation));
}

}  // namespace blink
