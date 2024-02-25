/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/image.h"

#include <math.h>

#include <tuple>

#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_cache.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

Image::Image(ImageObserver* observer, bool is_multipart)
    : image_observer_disabled_(false),
      image_observer_(observer),
      stable_image_id_(PaintImage::GetNextId()),
      is_multipart_(is_multipart) {}

Image::~Image() = default;

Image* Image::NullImage() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_REF(Image, null_image, (BitmapImage::Create()));
  return null_image;
}

// static
cc::ImageDecodeCache& Image::SharedCCDecodeCache(SkColorType color_type) {
  // This denotes the allocated locked memory budget for the cache used for
  // book-keeping. The cache indicates when the total memory locked exceeds this
  // budget in cc::DecodedDrawImage.
  DCHECK(color_type == kN32_SkColorType || color_type == kRGBA_F16_SkColorType);
  static const size_t kLockedMemoryLimitBytes = 64 * 1024 * 1024;
  if (color_type == kRGBA_F16_SkColorType) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        cc::SoftwareImageDecodeCache, image_decode_cache,
        (kRGBA_F16_SkColorType, kLockedMemoryLimitBytes));
    return image_decode_cache;
  }
  DEFINE_THREAD_SAFE_STATIC_LOCAL(cc::SoftwareImageDecodeCache,
                                  image_decode_cache,
                                  (kN32_SkColorType, kLockedMemoryLimitBytes));
  return image_decode_cache;
}

scoped_refptr<Image> Image::LoadPlatformResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  const WebData& resource =
      Platform::Current()->GetDataResource(resource_id, scale_factor);
  if (resource.IsEmpty())
    return Image::NullImage();

  scoped_refptr<Image> image = BitmapImage::Create();
  image->SetData(resource, true);
  return image;
}

PaintImage Image::ResizeAndOrientImage(
    const PaintImage& image,
    ImageOrientation orientation,
    gfx::Vector2dF image_scale,
    float opacity,
    InterpolationQuality interpolation_quality) {
  return ResizeAndOrientImage(image, orientation, image_scale, opacity,
                              interpolation_quality, nullptr);
}

// static
PaintImage Image::ResizeAndOrientImage(
    const PaintImage& image,
    ImageOrientation orientation,
    gfx::Vector2dF image_scale,
    float opacity,
    InterpolationQuality interpolation_quality,
    sk_sp<SkColorSpace> color_space) {
  gfx::Size size(image.width(), image.height());
  size = gfx::ScaleToFlooredSize(size, image_scale.x(), image_scale.y());
  AffineTransform transform;
  if (orientation != ImageOrientationEnum::kDefault) {
    if (orientation.UsesWidthAsHeight())
      size.Transpose();
    transform *= orientation.TransformFromDefault(gfx::SizeF(size));
  }
  transform.ScaleNonUniform(image_scale.x(), image_scale.y());

  if (size.IsEmpty())
    return PaintImage();

  const auto image_color_space = image.GetSkImageInfo().colorSpace()
                                     ? image.GetSkImageInfo().refColorSpace()
                                     : SkColorSpace::MakeSRGB();
  const auto surface_color_space =
      color_space ? color_space : image_color_space;
  const bool needs_color_conversion =
      !SkColorSpace::Equals(image_color_space.get(), surface_color_space.get());

  if (transform.IsIdentity() && opacity == 1 && !needs_color_conversion) {
    // Nothing to adjust, just use the original.
    DCHECK_EQ(image.width(), size.width());
    DCHECK_EQ(image.height(), size.height());
    return image;
  }

  const SkImageInfo surface_info = SkImageInfo::MakeN32(
      size.width(), size.height(), image.GetSkImageInfo().alphaType(),
      surface_color_space);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(surface_info);
  if (!surface)
    return PaintImage();

  SkPaint paint;
  DCHECK_GE(opacity, 0);
  DCHECK_LE(opacity, 1);
  paint.setAlpha(opacity * 255);
  SkSamplingOptions sampling;
  if (interpolation_quality != kInterpolationNone)
    sampling = SkSamplingOptions(SkCubicResampler::CatmullRom());

  SkCanvas* canvas = surface->getCanvas();
  canvas->concat(AffineTransformToSkMatrix(transform));
  canvas->drawImage(image.GetSwSkImage(), 0, 0, sampling, &paint);

  return PaintImageBuilder::WithProperties(std::move(image))
      .set_image(surface->makeImageSnapshot(), PaintImage::GetNextContentId())
      .TakePaintImage();
}

Image::SizeAvailability Image::SetData(scoped_refptr<SharedBuffer> data,
                                       bool all_data_received) {
  encoded_image_data_ = std::move(data);
  if (!encoded_image_data_.get())
    return kSizeAvailable;

  size_t length = encoded_image_data_->size();
  if (!length)
    return kSizeAvailable;

  return DataChanged(all_data_received);
}

String Image::FilenameExtension() const {
  return String();
}

const AtomicString& Image::MimeType() const {
  return g_empty_atom;
}

namespace {

sk_sp<PaintShader> CreatePatternShader(const PaintImage& image,
                                       const SkMatrix& shader_matrix,
                                       const SkSamplingOptions& sampling,
                                       bool should_antialias,
                                       const gfx::SizeF& spacing,
                                       SkTileMode tmx,
                                       SkTileMode tmy,
                                       const gfx::Rect& subset_rect) {
  if (spacing.IsZero() &&
      subset_rect == gfx::Rect(image.width(), image.height())) {
    return PaintShader::MakeImage(image, tmx, tmy, &shader_matrix);
  }

  // Arbitrary tiling is currently only supported for SkPictureShader, so we use
  // that instead of a plain bitmap shader to implement spacing.
  const SkRect tile_rect =
      SkRect::MakeWH(subset_rect.width() + spacing.width(),
                     subset_rect.height() + spacing.height());

  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();
  cc::PaintFlags flags;
  flags.setAntiAlias(should_antialias);
  canvas->drawImageRect(
      image, gfx::RectToSkRect(subset_rect),
      SkRect::MakeWH(subset_rect.width(), subset_rect.height()), sampling,
      &flags, SkCanvas::kStrict_SrcRectConstraint);

  return PaintShader::MakePaintRecord(recorder.finishRecordingAsPicture(),
                                      tile_rect, tmx, tmy, &shader_matrix);
}

SkTileMode ComputeTileMode(float left, float right, float min, float max) {
  DCHECK(left < right);
  return left >= min && right <= max ? SkTileMode::kClamp : SkTileMode::kRepeat;
}

}  // anonymous namespace

void Image::DrawPattern(GraphicsContext& context,
                        const cc::PaintFlags& base_flags,
                        const gfx::RectF& dest_rect,
                        const ImageTilingInfo& tiling_info,
                        const ImageDrawOptions& draw_options) {
  TRACE_EVENT0("skia", "Image::drawPattern");

  if (dest_rect.IsEmpty())
    return;  // nothing to draw

  PaintImage image = PaintImageForCurrentFrame();
  if (!image)
    return;  // nothing to draw

  // Fetch orientation data if needed.
  ImageOrientation orientation = ImageOrientationEnum::kDefault;
  if (draw_options.respect_orientation)
    orientation = CurrentFrameOrientation();

  // |tiling_info.image_rect| is in source image space, unscaled but oriented.
  // image-resolution information is baked into |tiling_info.scale|,
  // so we do not want to use it in computing the subset. That requires
  // explicitly applying orientation here.
  gfx::Rect subset_rect = gfx::ToEnclosingRect(tiling_info.image_rect);
  gfx::Size oriented_image_size(image.width(), image.height());
  if (orientation.UsesWidthAsHeight())
    oriented_image_size.Transpose();
  subset_rect.Intersect(gfx::Rect(oriented_image_size));
  if (subset_rect.IsEmpty())
    return;  // nothing to draw

  // Apply image orientation, if necessary
  if (orientation != ImageOrientationEnum::kDefault)
    image = ResizeAndOrientImage(image, orientation);

  // We also need to translate it such that the origin of the pattern is the
  // origin of the destination rect, which is what Blink expects. Skia uses
  // the coordinate system origin as the base for the pattern. If Blink wants
  // a shifted image, it will shift it from there using the localMatrix.
  gfx::RectF tile_rect(subset_rect);
  tile_rect.Scale(tiling_info.scale.x(), tiling_info.scale.y());
  tile_rect.Offset(tiling_info.phase.OffsetFromOrigin());
  tile_rect.set_size(tile_rect.size() + tiling_info.spacing);

  SkMatrix local_matrix;
  local_matrix.setTranslate(tile_rect.x(), tile_rect.y());
  // Apply the scale to have the subset correctly fill the destination.
  local_matrix.preScale(tiling_info.scale.x(), tiling_info.scale.y());

  const auto tmx = ComputeTileMode(dest_rect.x(), dest_rect.right(),
                                   tile_rect.x(), tile_rect.right());
  const auto tmy = ComputeTileMode(dest_rect.y(), dest_rect.bottom(),
                                   tile_rect.y(), tile_rect.bottom());

  // Fetch this now as subsetting may swap the image.
  auto image_id = image.stable_id();

  SkSamplingOptions sampling_to_use =
      context.ComputeSamplingOptions(*this, dest_rect, gfx::RectF(subset_rect));
  sk_sp<PaintShader> tile_shader = CreatePatternShader(
      image, local_matrix, sampling_to_use, context.ShouldAntialias(),
      gfx::SizeF(tiling_info.spacing.width() / tiling_info.scale.x(),
                 tiling_info.spacing.height() / tiling_info.scale.y()),
      tmx, tmy, subset_rect);

  // If the shader could not be instantiated (e.g. non-invertible matrix),
  // draw transparent.
  // Note: we can't simply bail, because of arbitrary blend mode.
  cc::PaintFlags flags(base_flags);
  flags.setColor(tile_shader ? SK_ColorBLACK : SK_ColorTRANSPARENT);
  flags.setShader(std::move(tile_shader));
  if (draw_options.dark_mode_filter) {
    draw_options.dark_mode_filter->ApplyFilterToImage(
        this, &flags, gfx::RectToSkRect(subset_rect));
  }

  context.DrawRect(gfx::RectFToSkRect(dest_rect), flags,
                   AutoDarkMode::Disabled());

  StartAnimation();

  if (CurrentFrameIsLazyDecoded()) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "Draw LazyPixelRef", TRACE_EVENT_SCOPE_THREAD,
                         "LazyPixelRef", image_id);
  }
}

mojom::blink::ImageAnimationPolicy Image::AnimationPolicy() {
  return mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyAllowed;
}

scoped_refptr<Image> Image::ImageForDefaultFrame() {
  scoped_refptr<Image> image(this);

  return image;
}

PaintImageBuilder Image::CreatePaintImageBuilder() {
  auto animation_type = MaybeAnimated() ? PaintImage::AnimationType::kAnimated
                                        : PaintImage::AnimationType::kStatic;
  return PaintImageBuilder::WithDefault()
      .set_id(stable_image_id_)
      .set_animation_type(animation_type)
      .set_is_multipart(is_multipart_);
}

bool Image::ApplyShader(cc::PaintFlags& flags,
                        const SkMatrix& local_matrix,
                        const gfx::RectF& src_rect,
                        const ImageDrawOptions& draw_options) {
  // Default shader impl: attempt to build a shader based on the current frame
  // SkImage.
  PaintImage image = PaintImageForCurrentFrame();
  if (!image)
    return false;

  if (draw_options.dark_mode_filter) {
    draw_options.dark_mode_filter->ApplyFilterToImage(
        this, &flags, gfx::RectFToSkRect(src_rect));
  }
  flags.setShader(PaintShader::MakeImage(image, SkTileMode::kClamp,
                                         SkTileMode::kClamp, &local_matrix));
  if (!flags.HasShader())
    return false;

  // Animation is normally refreshed in draw() impls, which we don't call when
  // painting via shaders.
  StartAnimation();

  return true;
}

SkBitmap Image::AsSkBitmapForCurrentFrame(
    RespectImageOrientationEnum respect_image_orientation) {
  PaintImage paint_image = PaintImageForCurrentFrame();
  if (!paint_image)
    return {};

  if (auto* bitmap_image = DynamicTo<BitmapImage>(this)) {
    const gfx::Size paint_image_size(paint_image.width(), paint_image.height());
    const gfx::Size density_corrected_size =
        bitmap_image->DensityCorrectedSize();

    ImageOrientation orientation = ImageOrientationEnum::kDefault;
    if (respect_image_orientation == kRespectImageOrientation)
      orientation = bitmap_image->CurrentFrameOrientation();

    gfx::Vector2dF image_scale(1, 1);
    if (density_corrected_size != paint_image_size) {
      image_scale = gfx::Vector2dF(
          density_corrected_size.width() / paint_image_size.width(),
          density_corrected_size.height() / paint_image_size.height());
    }

    paint_image = ResizeAndOrientImage(paint_image, orientation, image_scale);
    if (!paint_image)
      return {};
  }

  sk_sp<SkImage> sk_image = paint_image.GetSwSkImage();
  if (!sk_image)
    return {};

  SkBitmap bitmap;
  sk_image->asLegacyBitmap(&bitmap);
  return bitmap;
}

DarkModeImageCache* Image::GetDarkModeImageCache() {
  if (!dark_mode_image_cache_)
    dark_mode_image_cache_ = std::make_unique<DarkModeImageCache>();

  return dark_mode_image_cache_.get();
}

gfx::RectF Image::CorrectSrcRectForImageOrientation(gfx::SizeF image_size,
                                                    gfx::RectF src_rect) const {
  ImageOrientation orientation = CurrentFrameOrientation();
  DCHECK(orientation != ImageOrientationEnum::kDefault);
  AffineTransform forward_map = orientation.TransformFromDefault(image_size);
  AffineTransform inverse_map = forward_map.Inverse();
  return inverse_map.MapRect(src_rect);
}

}  // namespace blink
