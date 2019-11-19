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
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/graphics/scoped_interpolation_quality.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

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
        (kRGBA_F16_SkColorType, kLockedMemoryLimitBytes,
         PaintImage::kDefaultGeneratorClientId));
    return image_decode_cache;
  }
  DEFINE_THREAD_SAFE_STATIC_LOCAL(cc::SoftwareImageDecodeCache,
                                  image_decode_cache,
                                  (kN32_SkColorType, kLockedMemoryLimitBytes,
                                   PaintImage::kDefaultGeneratorClientId));
  return image_decode_cache;
}

scoped_refptr<Image> Image::LoadPlatformResource(int resource_id,
                                                 ui::ScaleFactor scale_factor) {
  const WebData& resource =
      Platform::Current()->GetDataResource(resource_id, scale_factor);
  if (resource.IsEmpty())
    return Image::NullImage();

  scoped_refptr<Image> image = BitmapImage::Create();
  image->SetData(resource, true);
  return image;
}

// static
PaintImage Image::ResizeAndOrientImage(
    const PaintImage& image,
    ImageOrientation orientation,
    FloatSize image_scale,
    float opacity,
    InterpolationQuality interpolation_quality) {
  IntSize size(image.width(), image.height());
  size.Scale(image_scale.Width(), image_scale.Height());
  AffineTransform transform;
  if (orientation != kDefaultImageOrientation) {
    if (orientation.UsesWidthAsHeight())
      size = size.TransposedSize();
    transform *= orientation.TransformFromDefault(FloatSize(size));
  }
  transform.ScaleNonUniform(image_scale.Width(), image_scale.Height());

  if (size.IsEmpty())
    return PaintImage();

  if (transform.IsIdentity() && opacity == 1) {
    // Nothing to adjust, just use the original.
    DCHECK_EQ(image.width(), size.Width());
    DCHECK_EQ(image.height(), size.Height());
    return image;
  }

  const SkImageInfo info =
      SkImageInfo::MakeN32(size.Width(), size.Height(), kPremul_SkAlphaType,
                           SkColorSpace::MakeSRGB());
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
  if (!surface)
    return PaintImage();

  SkPaint paint;
  DCHECK_GE(opacity, 0);
  DCHECK_LE(opacity, 1);
  paint.setAlpha(opacity * 255);
  paint.setFilterQuality(interpolation_quality == kInterpolationNone
                             ? kNone_SkFilterQuality
                             : kHigh_SkFilterQuality);

  SkCanvas* canvas = surface->getCanvas();
  canvas->concat(AffineTransformToSkMatrix(transform));
  canvas->drawImage(image.GetSkImage(), 0, 0, &paint);

  return PaintImageBuilder::WithProperties(std::move(image))
      .set_image(surface->makeImageSnapshot(), PaintImage::GetNextContentId())
      .TakePaintImage();
}

Image::SizeAvailability Image::SetData(scoped_refptr<SharedBuffer> data,
                                       bool all_data_received) {
  encoded_image_data_ = std::move(data);
  if (!encoded_image_data_.get())
    return kSizeAvailable;

  int length = encoded_image_data_->size();
  if (!length)
    return kSizeAvailable;

  return DataChanged(all_data_received);
}

String Image::FilenameExtension() const {
  return String();
}

namespace {

sk_sp<PaintShader> CreatePatternShader(const PaintImage& image,
                                       const SkMatrix& shader_matrix,
                                       SkFilterQuality quality_to_use,
                                       bool should_antialias,
                                       const FloatSize& spacing,
                                       SkTileMode tmx,
                                       SkTileMode tmy) {
  if (spacing.IsZero()) {
    return PaintShader::MakeImage(image, tmx, tmy, &shader_matrix);
  }

  // Arbitrary tiling is currently only supported for SkPictureShader, so we use
  // that instead of a plain bitmap shader to implement spacing.
  const SkRect tile_rect = SkRect::MakeWH(image.width() + spacing.Width(),
                                          image.height() + spacing.Height());

  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording(tile_rect);
  PaintFlags flags;
  flags.setAntiAlias(should_antialias);
  flags.setFilterQuality(quality_to_use);
  canvas->drawImage(image, 0, 0, &flags);

  return PaintShader::MakePaintRecord(recorder.finishRecordingAsPicture(),
                                      tile_rect, tmx, tmy, &shader_matrix);
}

SkTileMode ComputeTileMode(float left, float right, float min, float max) {
  DCHECK(left < right);
  return left >= min && right <= max ? SkTileMode::kClamp : SkTileMode::kRepeat;
}

}  // anonymous namespace

void Image::DrawPattern(GraphicsContext& context,
                        const FloatRect& float_src_rect,
                        const FloatSize& scale_src_to_dest,
                        const FloatPoint& phase,
                        SkBlendMode composite_op,
                        const FloatRect& dest_rect,
                        const FloatSize& repeat_spacing) {
  TRACE_EVENT0("skia", "Image::drawPattern");

  if (dest_rect.IsEmpty())
    return;  // nothing to draw

  PaintImage image = PaintImageForCurrentFrame();
  if (!image)
    return;  // nothing to draw

  // The subset_rect is in source image space, unscaled.
  IntRect subset_rect = EnclosingIntRect(float_src_rect);
  subset_rect.Intersect(IntRect(0, 0, image.width(), image.height()));
  if (subset_rect.IsEmpty())
    return;  // nothing to draw

  SkMatrix local_matrix;
  // We also need to translate it such that the origin of the pattern is the
  // origin of the destination rect, which is what Blink expects. Skia uses
  // the coordinate system origin as the base for the pattern. If Blink wants
  // a shifted image, it will shift it from there using the localMatrix.
  const float adjusted_x =
      phase.X() + subset_rect.X() * scale_src_to_dest.Width();
  const float adjusted_y =
      phase.Y() + subset_rect.Y() * scale_src_to_dest.Height();
  local_matrix.setTranslate(SkFloatToScalar(adjusted_x),
                            SkFloatToScalar(adjusted_y));

  // Apply the scale to have the subset correctly fill the destination.
  local_matrix.preScale(scale_src_to_dest.Width(), scale_src_to_dest.Height());

  // Fetch this now as subsetting may swap the image.
  auto image_id = image.GetSkImage()->uniqueID();

  image = PaintImageBuilder::WithCopy(std::move(image))
              .make_subset(subset_rect)
              .TakePaintImage();
  if (!image)
    return;

  const FloatSize tile_size(
      image.width() * scale_src_to_dest.Width() + repeat_spacing.Width(),
      image.height() * scale_src_to_dest.Height() + repeat_spacing.Height());
  const auto tmx = ComputeTileMode(dest_rect.X(), dest_rect.MaxX(), adjusted_x,
                                   adjusted_x + tile_size.Width());
  const auto tmy = ComputeTileMode(dest_rect.Y(), dest_rect.MaxY(), adjusted_y,
                                   adjusted_y + tile_size.Height());

  SkFilterQuality quality_to_use =
      context.ComputeFilterQuality(this, dest_rect, FloatRect(subset_rect));
  sk_sp<PaintShader> tile_shader = CreatePatternShader(
      image, local_matrix, quality_to_use, context.ShouldAntialias(),
      FloatSize(repeat_spacing.Width() / scale_src_to_dest.Width(),
                repeat_spacing.Height() / scale_src_to_dest.Height()),
      tmx, tmy);

  PaintFlags flags = context.FillFlags();
  // If the shader could not be instantiated (e.g. non-invertible matrix),
  // draw transparent.
  // Note: we can't simply bail, because of arbitrary blend mode.
  flags.setColor(tile_shader ? SK_ColorBLACK : SK_ColorTRANSPARENT);
  flags.setBlendMode(composite_op);
  flags.setFilterQuality(quality_to_use);
  flags.setShader(std::move(tile_shader));

  context.DrawRect(dest_rect, flags);

  StartAnimation();

  if (CurrentFrameIsLazyDecoded()) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "Draw LazyPixelRef", TRACE_EVENT_SCOPE_THREAD,
                         "LazyPixelRef", image_id);
  }
}

scoped_refptr<Image> Image::ImageForDefaultFrame() {
  scoped_refptr<Image> image(this);

  return image;
}

PaintImageBuilder Image::CreatePaintImageBuilder() {
  auto animation_type = MaybeAnimated() ? PaintImage::AnimationType::ANIMATED
                                        : PaintImage::AnimationType::STATIC;
  return PaintImageBuilder::WithDefault()
      .set_id(stable_image_id_)
      .set_animation_type(animation_type)
      .set_is_multipart(is_multipart_);
}

bool Image::ApplyShader(PaintFlags& flags, const SkMatrix& local_matrix) {
  // Default shader impl: attempt to build a shader based on the current frame
  // SkImage.
  PaintImage image = PaintImageForCurrentFrame();
  if (!image)
    return false;

  flags.setShader(PaintShader::MakeImage(image, SkTileMode::kRepeat,
                                         SkTileMode::kRepeat, &local_matrix));
  if (!flags.HasShader())
    return false;

  // Animation is normally refreshed in draw() impls, which we don't call when
  // painting via shaders.
  StartAnimation();

  return true;
}

SkBitmap Image::AsSkBitmapForCurrentFrame(
    RespectImageOrientationEnum should_respect_image_orientation) {
  PaintImage paint_image = PaintImageForCurrentFrame();
  if (!paint_image)
    return {};

  if (should_respect_image_orientation == kRespectImageOrientation &&
      IsBitmapImage()) {
    ImageOrientation orientation =
        ToBitmapImage(this)->CurrentFrameOrientation();
    paint_image = ResizeAndOrientImage(paint_image, orientation);
    if (!paint_image)
      return {};
  }

  sk_sp<SkImage> sk_image = paint_image.GetSkImage();
  if (!sk_image)
    return {};

  SkBitmap bitmap;
  sk_image->asLegacyBitmap(&bitmap);
  return bitmap;
}

bool Image::GetBitmap(const FloatRect& src_rect, SkBitmap* bitmap) {
  if (!src_rect.Width() || !src_rect.Height())
    return false;

  SkScalar sx = SkFloatToScalar(src_rect.X());
  SkScalar sy = SkFloatToScalar(src_rect.Y());
  SkScalar sw = SkFloatToScalar(src_rect.Width());
  SkScalar sh = SkFloatToScalar(src_rect.Height());
  SkRect src = {sx, sy, sx + sw, sy + sh};
  SkRect dest = {0, 0, sw, sh};

  if (!bitmap || !bitmap->tryAllocPixels(SkImageInfo::MakeN32(
                     static_cast<int>(src_rect.Width()),
                     static_cast<int>(src_rect.Height()), kPremul_SkAlphaType)))
    return false;

  SkCanvas canvas(*bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.drawImageRect(PaintImageForCurrentFrame().GetSkImage(), src, dest,
                       nullptr);
  return true;
}

DarkModeClassification Image::GetDarkModeClassification(
    const FloatRect& src_rect) {
  // Assuming that multiple uses of the same sprite region all have the same
  // size, only the top left corner coordinates of the src_rect are used to
  // generate the key for caching and retrieving the classification.
  ClassificationKey key(src_rect.X(), src_rect.Y());
  auto result = dark_mode_classifications_.find(key);
  if (result == dark_mode_classifications_.end())
    return DarkModeClassification::kNotClassified;

  return result->value;
}

void Image::AddDarkModeClassification(
    const FloatRect& src_rect,
    DarkModeClassification dark_mode_classification) {
  // Add the classification in the map only if the image is not classified yet.
  DCHECK(GetDarkModeClassification(src_rect) ==
         DarkModeClassification::kNotClassified);
  ClassificationKey key(src_rect.X(), src_rect.Y());
  dark_mode_classifications_.insert(key, dark_mode_classification);
}

}  // namespace blink
