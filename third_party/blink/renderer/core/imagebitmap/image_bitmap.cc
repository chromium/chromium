// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"

#include <memory>
#include <utility>
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/config/gpu_feature_info.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/common/features.h"
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
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
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

ImageBitmap::ParsedOptions ParseOptions(const ImageBitmapOptions* options,
                                        absl::optional<gfx::Rect> crop_rect,
                                        gfx::Size source_size) {
  ImageBitmap::ParsedOptions parsed_options;
  if (options->imageOrientation() == kImageOrientationFlipY) {
    parsed_options.flip_y = true;
    parsed_options.orientation_from_image = true;
  } else {
    DCHECK(options->imageOrientation() == kImageOrientationFromImage ||
           options->imageOrientation() == kImageBitmapOptionNone);
    parsed_options.flip_y = false;
    parsed_options.orientation_from_image = true;

    if (base::FeatureList::IsEnabled(
            features::kCreateImageBitmapOrientationNone) &&
        options->imageOrientation() == kImageBitmapOptionNone) {
      parsed_options.orientation_from_image = false;
    }
  }
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
    NOTREACHED()
        << "Invalid ImageBitmap creation attribute colorSpaceConversion: "
        << IDLEnumAsString(options->colorSpaceConversion());
  }

  int source_width = source_size.width();
  int source_height = source_size.height();
  if (!crop_rect) {
    parsed_options.crop_rect = gfx::Rect(0, 0, source_width, source_height);
  } else {
    parsed_options.crop_rect = *crop_rect;
  }
  if (!options->hasResizeWidth() && !options->hasResizeHeight()) {
    parsed_options.resize_width = parsed_options.crop_rect.width();
    parsed_options.resize_height = parsed_options.crop_rect.height();
  } else if (options->hasResizeWidth() && options->hasResizeHeight()) {
    parsed_options.resize_width = options->resizeWidth();
    parsed_options.resize_height = options->resizeHeight();
  } else if (options->hasResizeWidth() && !options->hasResizeHeight()) {
    parsed_options.resize_width = options->resizeWidth();
    parsed_options.resize_height = ClampTo<unsigned>(ceil(
        static_cast<float>(options->resizeWidth()) /
        parsed_options.crop_rect.width() * parsed_options.crop_rect.height()));
  } else {
    parsed_options.resize_height = options->resizeHeight();
    parsed_options.resize_width = ClampTo<unsigned>(ceil(
        static_cast<float>(options->resizeHeight()) /
        parsed_options.crop_rect.height() * parsed_options.crop_rect.width()));
  }
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
  return parsed_options;
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

static inline bool ShouldAvoidPremul(
    const ImageBitmap::ParsedOptions& options) {
  return options.source_is_unpremul && !options.premultiply_alpha;
}

std::unique_ptr<CanvasResourceProvider> CreateProvider(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    const SkImageInfo& info,
    const scoped_refptr<StaticBitmapImage>& source_image,
    bool fallback_to_software) {
  const cc::PaintFlags::FilterQuality filter_quality =
      cc::PaintFlags::FilterQuality::kLow;
  if (context_provider) {
    uint32_t usage_flags =
        context_provider->ContextProvider()
            ->SharedImageInterface()
            ->UsageForMailbox(source_image->GetMailboxHolder().mailbox);
    auto resource_provider = CanvasResourceProvider::CreateSharedImageProvider(
        info, filter_quality, CanvasResourceProvider::ShouldInitialize::kNo,
        context_provider, RasterMode::kGPU, source_image->IsOriginTopLeft(),
        usage_flags);
    if (resource_provider)
      return resource_provider;

    if (!fallback_to_software)
      return nullptr;
  }

  return CanvasResourceProvider::CreateBitmapProvider(
      info, filter_quality, CanvasResourceProvider::ShouldInitialize::kNo);
}

scoped_refptr<StaticBitmapImage> FlipImageVertically(
    scoped_refptr<StaticBitmapImage> input,
    const ImageBitmap::ParsedOptions& parsed_options) {
  SkImageInfo info = GetSkImageInfo(input);
  if (info.isEmpty())
    return nullptr;

  PaintImage paint_image = input->PaintImageForCurrentFrame();

  if (ShouldAvoidPremul(parsed_options)) {
    // Unpremul code path results in a GPU readback if |input| is texture
    // backed since CopyImageData() uses SkImage::readPixels() to extract the
    // pixels from SkImage.
    sk_sp<SkData> image_pixels = TryAllocateSkData(info.computeMinByteSize());
    if (!image_pixels)
      return nullptr;

    uint8_t* writable_pixels =
        static_cast<uint8_t*>(image_pixels->writable_data());
    size_t image_row_bytes = static_cast<size_t>(info.minRowBytes64());
    bool read_successful =
        paint_image.readPixels(info, writable_pixels, image_row_bytes, 0, 0);
    DCHECK(read_successful);

    // Since rotation is applied after flip, vertical flip becomes horizontal
    // flips after rotation for oritation 5-8. So we swap pixels left to right
    // to flip the image horizontally instead.
    if (input->CurrentFrameOrientation().UsesWidthAsHeight()) {
      for (int i = 0; i < info.height() - 1; i++) {
        for (int j = 0; j < info.width() / 2; j++) {
          size_t first_element = i * image_row_bytes + j * info.bytesPerPixel();
          size_t last_element =
              i * image_row_bytes + (j + 1) * info.bytesPerPixel();
          size_t bottom_element =
              (i + 1) * image_row_bytes - (j + 1) * info.bytesPerPixel();
          std::swap_ranges(&writable_pixels[first_element],
                           &writable_pixels[last_element],
                           &writable_pixels[bottom_element]);
        }
      }
    } else {
      for (int i = 0; i < info.height() / 2; i++) {
        size_t top_first_element = i * image_row_bytes;
        size_t top_last_element = (i + 1) * image_row_bytes;
        size_t bottom_first_element = (info.height() - 1 - i) * image_row_bytes;
        std::swap_ranges(&writable_pixels[top_first_element],
                         &writable_pixels[top_last_element],
                         &writable_pixels[bottom_first_element]);
      }
    }
    return StaticBitmapImage::Create(std::move(image_pixels), info,
                                     input->CurrentFrameOrientation());
  }

  // Since we are allowed to premul the input image if needed, we can use Skia
  // to flip the image by drawing it on a surface. If the image is premul, we
  // can use both accelerated and software surfaces. If the image is unpremul,
  // we have to use software surfaces.
  bool use_accelerated =
      paint_image.IsTextureBacked() && info.alphaType() == kPremul_SkAlphaType;
  auto resource_provider = CreateProvider(
      use_accelerated ? input->ContextProviderWrapper() : nullptr, info, input,
      true /* fallback_to_software */);
  if (!resource_provider)
    return nullptr;

  auto* canvas = resource_provider->Canvas();
  // Since rotation is applied after flip, vertical flips becomes horizontal
  // flips for oritation 5-8. So we flip the images horizontally instead.
  if (input->CurrentFrameOrientation().UsesWidthAsHeight()) {
    canvas->scale(-1, 1);
    canvas->translate(-input->width(), 0);
  } else {
    canvas->scale(1, -1);
    canvas->translate(0, -input->height());
  }
  cc::PaintFlags paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  canvas->drawImage(input->PaintImageForCurrentFrame(), 0, 0,
                    SkSamplingOptions(), &paint);
  return resource_provider->Snapshot(
      CanvasResourceProvider::FlushReason::kNon2DCanvas,
      input->CurrentFrameOrientation());
}

scoped_refptr<StaticBitmapImage> ScaleImage(
    scoped_refptr<StaticBitmapImage>&& image,
    const ImageBitmap::ParsedOptions& parsed_options) {
  auto src_image_info = image->PaintImageForCurrentFrame().GetSkImageInfo();
  auto image_info = GetSkImageInfo(image).makeWH(parsed_options.resize_width,
                                                 parsed_options.resize_height);

  // Try to avoid GPU read back by drawing accelerated premul image on an
  // accelerated surface.
  if (!ShouldAvoidPremul(parsed_options) && image->IsTextureBacked() &&
      src_image_info.alphaType() == kPremul_SkAlphaType) {
    auto resource_provider =
        CreateProvider(image->ContextProviderWrapper(), image_info, image,
                       false /* fallback_to_software */);
    if (resource_provider) {
      SkSamplingOptions sampling =
          cc::PaintFlags::FilterQualityToSkSamplingOptions(
              parsed_options.resize_quality);
      cc::PaintFlags paint;
      paint.setBlendMode(SkBlendMode::kSrc);
      resource_provider->Canvas()->drawImageRect(
          image->PaintImageForCurrentFrame(),
          SkRect::MakeWH(src_image_info.width(), src_image_info.height()),
          SkRect::MakeWH(parsed_options.resize_width,
                         parsed_options.resize_height),
          sampling, &paint, SkCanvas::kStrict_SrcRectConstraint);
      return resource_provider->Snapshot(
          CanvasResourceProvider::FlushReason::kNon2DCanvas,
          image->CurrentFrameOrientation());
    }
  }

  // Avoid sRGB transfer function by setting the color space to nullptr.
  if (image_info.colorSpace()->isSRGB())
    image_info = image_info.makeColorSpace(nullptr);

  sk_sp<SkData> image_pixels =
      TryAllocateSkData(image_info.computeMinByteSize());
  if (!image_pixels) {
    return nullptr;
  }

  SkPixmap resized_pixmap(image_info, image_pixels->data(),
                          image_info.minRowBytes());
  auto sk_image = image->PaintImageForCurrentFrame().GetSwSkImage();
  if (!sk_image)
    return nullptr;
  sk_image->scalePixels(resized_pixmap,
                        cc::PaintFlags::FilterQualityToSkSamplingOptions(
                            parsed_options.resize_quality));
  // Tag the resized Pixmap with the correct color space.
  resized_pixmap.setColorSpace(GetSkImageInfo(image).refColorSpace());

  auto resized_sk_image =
      SkImages::RasterFromData(resized_pixmap.info(), std::move(image_pixels),
                               resized_pixmap.rowBytes());
  if (!resized_sk_image)
    return nullptr;
  return UnacceleratedStaticBitmapImage::Create(
      resized_sk_image, image->CurrentFrameOrientation());
}

scoped_refptr<StaticBitmapImage> ApplyColorSpaceConversion(
    scoped_refptr<StaticBitmapImage>&& image,
    ImageBitmap::ParsedOptions& options) {
  sk_sp<SkColorSpace> color_space = SkColorSpace::MakeSRGB();
  SkColorType color_type =
      image->IsTextureBacked() ? kRGBA_8888_SkColorType : kN32_SkColorType;
  SkImageInfo src_image_info =
      image->PaintImageForCurrentFrame().GetSkImageInfo();
  if (src_image_info.isEmpty())
    return nullptr;

  // This will always convert to 8-bit sRGB.
  return image->ConvertToColorSpace(color_space, color_type);
}

scoped_refptr<StaticBitmapImage> BakeOrientation(
    scoped_refptr<StaticBitmapImage> input,
    ImageBitmap::ParsedOptions& options,
    gfx::Rect src_rect) {
  SkImageInfo info = GetSkImageInfo(input);
  if (info.isEmpty()) {
    return nullptr;
  }

  PaintImage paint_image = input->PaintImageForCurrentFrame();

  // For the premul code path, rotate and resize the paint image directly.
  if (ShouldAvoidPremul(options)) {
    PaintImage new_paint_image = Image::ResizeAndOrientImage(
        paint_image, input->CurrentFrameOrientation());
    return StaticBitmapImage::Create(std::move(new_paint_image),
                                     ImageOrientationEnum::kDefault);
  }

  bool use_accelerated =
      paint_image.IsTextureBacked() && info.alphaType() == kPremul_SkAlphaType;
  auto resource_provider = CreateProvider(
      use_accelerated ? input->ContextProviderWrapper() : nullptr, info, input,
      true /* fallback_to_software */);
  if (!resource_provider) {
    return nullptr;
  }

  auto* canvas = resource_provider->Canvas();
  ImageOrientation orientation = input->CurrentFrameOrientation();
  auto affineTransform =
      orientation.TransformToDefault(gfx::SizeF(src_rect.size()));
  canvas->concat(AffineTransformToSkM44(affineTransform));

  gfx::Rect dst_rect = src_rect;
  // The destination rect will have its width and height already reversed
  // for the orientation of the image, as it was needed for page layout, so
  // we need to reverse it back here.
  if (orientation.UsesWidthAsHeight()) {
    dst_rect.set_size(gfx::TransposeSize(dst_rect.size()));
  }

  cc::PaintFlags paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  canvas->drawImageRect(
      std::move(paint_image), gfx::RectFToSkRect(gfx::RectF(src_rect)),
      gfx::RectFToSkRect(gfx::RectF(dst_rect)), SkSamplingOptions(), &paint,
      WebCoreClampingModeToSkiaRectConstraint(
          Image::kDoNotClampImageToSourceRect));
  return resource_provider->Snapshot(
      CanvasResourceProvider::FlushReason::kNon2DCanvas,
      input->CurrentFrameOrientation());
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
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
  if (!surface)
    return nullptr;
  return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

static scoped_refptr<StaticBitmapImage> CropImageAndApplyColorSpaceConversion(
    scoped_refptr<StaticBitmapImage>&& image,
    ImageBitmap::ParsedOptions& parsed_options) {
  DCHECK(image);
  DCHECK(!image->HasData());

  gfx::Rect img_rect(image->width(), image->height());
  const gfx::Rect& src_rect = parsed_options.crop_rect;
  const gfx::Rect intersect_rect = IntersectRects(img_rect, src_rect);

  // If cropRect doesn't intersect the source image, return a transparent black
  // image.
  if (intersect_rect.IsEmpty())
    return MakeBlankImage(parsed_options);

  scoped_refptr<StaticBitmapImage> result = image;
  if (src_rect != img_rect) {
    auto paint_image = result->PaintImageForCurrentFrame();
    auto image_info = paint_image.GetSkImageInfo().makeWH(src_rect.width(),
                                                          src_rect.height());
    auto resource_provider =
        CreateProvider(image->ContextProviderWrapper(), image_info, result,
                       true /* fallback_to_software*/);
    if (!resource_provider)
      return nullptr;
    cc::PaintCanvas* canvas = resource_provider->Canvas();
    cc::PaintFlags paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    canvas->drawImageRect(paint_image,
                          SkRect::MakeXYWH(src_rect.x(), src_rect.y(),
                                           src_rect.width(), src_rect.height()),
                          SkRect::MakeWH(src_rect.width(), src_rect.height()),
                          SkSamplingOptions(), &paint,
                          SkCanvas::kStrict_SrcRectConstraint);
    result = resource_provider->Snapshot(
        CanvasResourceProvider::FlushReason::kNon2DCanvas,
        image->CurrentFrameOrientation());
  }

  // down-scaling has higher priority than other tasks, up-scaling has lower.
  bool down_scaling = parsed_options.should_scale_input &&
                      (static_cast<uint64_t>(parsed_options.resize_width) *
                           parsed_options.resize_height <
                       result->Size().Area64());
  bool up_scaling = parsed_options.should_scale_input && !down_scaling;

  // resize if down-scaling
  if (down_scaling) {
    result = ScaleImage(std::move(result), parsed_options);
    if (!result)
      return nullptr;
  }

  // flip if needed
  if (parsed_options.flip_y) {
    result = FlipImageVertically(std::move(result), parsed_options);
    if (!result)
      return nullptr;
  }

  // color convert if needed
  if (parsed_options.has_color_space_conversion) {
    result = ApplyColorSpaceConversion(std::move(result), parsed_options);
    if (!result)
      return nullptr;
  }

  // apply the orientation from EXIF metadata if needed.
  if (!parsed_options.orientation_from_image &&
      result->CurrentFrameOrientation() !=
          ImageOrientationEnum::kOriginTopLeft) {
    result = BakeOrientation(std::move(result), parsed_options, intersect_rect);
    if (!result) {
      return nullptr;
    }
  }

  // premultiply / unpremultiply if needed
  result = GetImageWithAlphaDisposition(
      CanvasResourceProvider::FlushReason::kCreateImageBitmap,
      std::move(result),
      parsed_options.premultiply_alpha ? kPremultiplyAlpha
                                       : kUnpremultiplyAlpha);
  if (!result)
    return nullptr;

  // resize if up-scaling
  if (up_scaling) {
    result = ScaleImage(std::move(result), parsed_options);
    if (!result)
      return nullptr;
  }

  return result;
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
                         absl::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  scoped_refptr<Image> input = image->CachedImage()->GetImage();
  DCHECK(!input->IsTextureBacked());

  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->BitmapSourceSize());
  parsed_options.source_is_unpremul =
      (input->PaintImageForCurrentFrame().GetAlphaType() ==
       kUnpremul_SkAlphaType);
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
        ImageDecoder::kDefaultBitDepth,
        parsed_options.has_color_space_conversion ? ColorBehavior::Tag()
                                                  : ColorBehavior::Ignore()));
    auto skia_image = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));
    if (!skia_image)
      return;

    paint_image = PaintImageBuilder::WithDefault()
                      .set_id(paint_image.stable_id())
                      .set_image(std::move(skia_image),
                                 paint_image.GetContentIdForFrame(0u))
                      .TakePaintImage();
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
  image_ = CropImageAndApplyColorSpaceConversion(std::move(static_input),
                                                 parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(!image->WouldTaintOrigin());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(HTMLVideoElement* video,
                         absl::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, video->BitmapSourceSize());
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  // TODO(crbug.com/1181329): ImageBitmap resize test case failed when
  // quality equals to "low" and "medium". Need further investigate to
  // enable gpu backed imageBitmap with resize options.
  const bool allow_accelerated_images =
      !options->hasResizeWidth() && !options->hasResizeHeight();
  auto input = video->CreateStaticBitmapImage(allow_accelerated_images);
  if (!input)
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(!video->WouldTaintOrigin());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(HTMLCanvasElement* canvas,
                         absl::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  SourceImageStatus status;
  scoped_refptr<Image> image_input = canvas->GetSourceImageForCanvas(
      CanvasResourceProvider::FlushReason::kCreateImageBitmap, &status,
      gfx::SizeF());
  if (status != kNormalSourceImageStatus)
    return;
  DCHECK(IsA<StaticBitmapImage>(image_input.get()));
  scoped_refptr<StaticBitmapImage> input =
      static_cast<StaticBitmapImage*>(image_input.get());

  ParsedOptions parsed_options = ParseOptions(
      options, crop_rect, gfx::Size(input->width(), input->height()));
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(canvas->OriginClean());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(OffscreenCanvas* offscreen_canvas,
                         absl::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  SourceImageStatus status;
  scoped_refptr<Image> raw_input = offscreen_canvas->GetSourceImageForCanvas(
      CanvasResourceProvider::FlushReason::kCreateImageBitmap, &status,
      gfx::SizeF(offscreen_canvas->Size()));
  DCHECK(IsA<StaticBitmapImage>(raw_input.get()));
  scoped_refptr<StaticBitmapImage> input =
      static_cast<StaticBitmapImage*>(raw_input.get());
  raw_input = nullptr;

  if (status != kNormalSourceImageStatus)
    return;

  ParsedOptions parsed_options = ParseOptions(
      options, crop_rect, gfx::Size(input->width(), input->height()));
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
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
                         absl::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, data->BitmapSourceSize());
  // ImageData is always unpremul.
  parsed_options.source_is_unpremul = true;
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  const gfx::Rect& src_rect = parsed_options.crop_rect;
  const gfx::Rect data_src_rect(data->Size());
  const gfx::Rect intersect_rect =
      crop_rect ? IntersectRects(src_rect, data_src_rect) : data_src_rect;

  // If cropRect doesn't intersect the source image, return a transparent black
  // image.
  if (intersect_rect.IsEmpty()) {
    image_ = MakeBlankImage(parsed_options);
    return;
  }

  // Copy / color convert the pixels
  SkImageInfo info = SkImageInfo::Make(
      src_rect.width(), src_rect.height(), kN32_SkColorType,
      parsed_options.premultiply_alpha ? kPremul_SkAlphaType
                                       : kUnpremul_SkAlphaType,
      SkColorSpace::MakeSRGB());
  size_t image_pixels_size = info.computeMinByteSize();
  if (SkImageInfo::ByteSizeOverflowed(image_pixels_size))
    return;
  sk_sp<SkData> image_pixels = TryAllocateSkData(image_pixels_size);
  if (!image_pixels)
    return;
  if (!data->GetSkPixmap().readPixels(info, image_pixels->writable_data(),
                                      info.minRowBytes(), src_rect.x(),
                                      src_rect.y())) {
    return;
  }

  // Create Image object
  image_ = StaticBitmapImage::Create(std::move(image_pixels), info);
  if (!image_)
    return;

  // down-scaling has higher priority than other tasks, up-scaling has lower.
  bool down_scaling = parsed_options.should_scale_input &&
                      (static_cast<uint64_t>(parsed_options.resize_width) *
                           parsed_options.resize_height <
                       image_->Size().Area64());
  bool up_scaling = parsed_options.should_scale_input && !down_scaling;

  // resize if down-scaling
  if (down_scaling)
    image_ = ScaleImage(std::move(image_), parsed_options);
  if (!image_)
    return;

  // flip if needed
  if (parsed_options.flip_y) {
    if (!image_)
      return;

    image_ = FlipImageVertically(std::move(image_), parsed_options);
  }

  // apply the orientation from EXIF metadata if needed.
  if (!parsed_options.orientation_from_image &&
      image_->CurrentFrameOrientation() !=
          ImageOrientationEnum::kOriginTopLeft) {
    if (!image_) {
      return;
    }
    image_ = BakeOrientation(std::move(image_), parsed_options, intersect_rect);
  }

  // resize if up-scaling
  if (up_scaling)
    image_ = ScaleImage(std::move(image_), parsed_options);

  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(ImageBitmap* bitmap,
                         absl::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  scoped_refptr<StaticBitmapImage> input = bitmap->BitmapImage();
  if (!input)
    return;
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, input->Size());
  parsed_options.source_is_unpremul =
      (input->PaintImageForCurrentFrame().GetAlphaType() ==
       kUnpremul_SkAlphaType);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(input), parsed_options);
  if (!image_)
    return;

  image_->SetOriginClean(bitmap->OriginClean());
  UpdateImageBitmapMemoryUsage();
}

ImageBitmap::ImageBitmap(scoped_refptr<StaticBitmapImage> image,
                         absl::optional<gfx::Rect> crop_rect,
                         const ImageBitmapOptions* options) {
  bool origin_clean = image->OriginClean();
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->Size());
  parsed_options.source_is_unpremul =
      (image->PaintImageForCurrentFrame().GetAlphaType() ==
       kUnpremul_SkAlphaType);
  if (DstBufferSizeHasOverflow(parsed_options))
    return;

  image_ =
      CropImageAndApplyColorSpaceConversion(std::move(image), parsed_options);
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
      SkImageInfo info = GetSkImageInfo(image_);
      if (info.isEmpty())
        return nullptr;
      PaintImage paint_image = image_->PaintImageForCurrentFrame();
      bool use_accelerated = paint_image.IsTextureBacked() &&
                             info.alphaType() == kPremul_SkAlphaType;
      auto resource_provider = CreateProvider(
          use_accelerated ? image_->ContextProviderWrapper() : nullptr, info,
          image_, true /* fallback_to_software */);
      if (!resource_provider)
        return nullptr;

      auto* canvas = resource_provider->Canvas();
      cc::PaintFlags paint;
      paint.setBlendMode(SkBlendMode::kSrc);
      canvas->drawImage(image_->PaintImageForCurrentFrame(), 0, 0,
                        SkSamplingOptions(), &paint);
      image_ = resource_provider->Snapshot(
          CanvasResourceProvider::FlushReason::kNon2DCanvas,
          image_->CurrentFrameOrientation());
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

  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      new_memory_usage - memory_usage_);
  memory_usage_ = new_memory_usage;
}

ImageBitmap::~ImageBitmap() {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -memory_usage_);
}

void ImageBitmap::ResolvePromiseOnOriginalThread(
    ScriptPromiseResolver* resolver,
    bool origin_clean,
    std::unique_ptr<ParsedOptions> parsed_options,
    sk_sp<SkImage> skia_image,
    const ImageOrientationEnum orientation) {
  if (!skia_image) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState()->GetIsolate(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
    return;
  }
  scoped_refptr<StaticBitmapImage> image =
      UnacceleratedStaticBitmapImage::Create(std::move(skia_image),
                                             orientation);
  DCHECK(IsMainThread());
  if (!parsed_options->premultiply_alpha) {
    image = GetImageWithAlphaDisposition(
        CanvasResourceProvider::FlushReason::kCreateImageBitmap,
        std::move(image), kUnpremultiplyAlpha);
  }
  if (!image) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState()->GetIsolate(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
    return;
  }
  image = ApplyColorSpaceConversion(std::move(image), *(parsed_options.get()));
  if (!image) {
    resolver->Reject(
        ScriptValue(resolver->GetScriptState()->GetIsolate(),
                    v8::Null(resolver->GetScriptState()->GetIsolate())));
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
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info, &props);
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

ScriptPromise ImageBitmap::CreateAsync(
    ImageElementBase* image,
    absl::optional<gfx::Rect> crop_rect,
    ScriptState* script_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojom::blink::PreferredColorScheme preferred_color_scheme,
    ExceptionState& exception_state,
    const ImageBitmapOptions* options) {
  ParsedOptions parsed_options =
      ParseOptions(options, crop_rect, image->BitmapSourceSize());
  if (DstBufferSizeHasOverflow(parsed_options)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The ImageBitmap could not be allocated.");
    return ScriptPromise();
  }

  scoped_refptr<Image> input = image->CachedImage()->GetImage();
  DCHECK(input->IsSVGImage());
  gfx::Rect input_rect(input->Size());

  // In the case when |crop_rect| doesn't intersect the source image, we return
  // a transparent black image, respecting the color_params but ignoring
  // premultiply_alpha.
  if (!parsed_options.crop_rect.Intersects(input_rect)) {
    ImageBitmap* bitmap =
        MakeGarbageCollected<ImageBitmap>(MakeBlankImage(parsed_options));
    if (bitmap->BitmapImage()) {
      bitmap->BitmapImage()->SetOriginClean(!image->WouldTaintOrigin());
      return ScriptPromise::Cast(
          script_state,
          ToV8Traits<ImageBitmap>::ToV8(script_state, bitmap).ToLocalChecked());
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The ImageBitmap could not be allocated.");
      return ScriptPromise();
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

  SVGImageForContainer::Create(To<SVGImage>(input.get()),
                               gfx::SizeF(input_rect.size()), 1, NullURL(),
                               preferred_color_scheme)
      ->Draw(canvas, cc::PaintFlags(), gfx::RectF(draw_dst_rect),
             gfx::RectF(draw_src_rect), ImageDrawOptions());
  PaintRecord paint_record = recorder.finishRecordingAsPicture();

  std::unique_ptr<ParsedOptions> passed_parsed_options =
      std::make_unique<ParsedOptions>(parsed_options);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  worker_pool::PostTask(
      FROM_HERE, CrossThreadBindOnce(
                     &RasterizeImageOnBackgroundThread, std::move(paint_record),
                     draw_dst_rect, std::move(task_runner),
                     CrossThreadBindOnce(&ResolvePromiseOnOriginalThread,
                                         WrapCrossThreadPersistent(resolver),
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
ImageBitmap* ImageBitmap::Take(ScriptPromiseResolver*, sk_sp<SkImage> image) {
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

ScriptPromise ImageBitmap::CreateImageBitmap(
    ScriptState* script_state,
    absl::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, MakeGarbageCollected<ImageBitmap>(this, crop_rect, options),
      options, exception_state);
}

scoped_refptr<Image> ImageBitmap::GetSourceImageForCanvas(
    CanvasResourceProvider::FlushReason reason,
    SourceImageStatus* status,
    const gfx::SizeF&,
    const AlphaDisposition alpha_disposition) {
  *status = kNormalSourceImageStatus;
  if (!image_)
    return nullptr;

  scoped_refptr<StaticBitmapImage> image = image_;

  // If the alpha_disposition is already correct, or the image is opaque, this
  // is a no-op.
  return GetImageWithAlphaDisposition(reason, std::move(image),
                                      alpha_disposition);
}

gfx::SizeF ImageBitmap::ElementSize(
    const gfx::SizeF&,
    const RespectImageOrientationEnum respect_orientation) const {
  return gfx::SizeF(image_->Size(respect_orientation));
}

}  // namespace blink
