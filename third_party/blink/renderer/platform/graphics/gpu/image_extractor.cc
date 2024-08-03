// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/image_extractor.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {
namespace {
bool FrameIsValid(const SkBitmap& frame_bitmap) {
  if (frame_bitmap.isNull()) {
    return false;
  }
  if (frame_bitmap.empty()) {
    return false;
  }
  if (frame_bitmap.colorType() != kN32_SkColorType &&
      frame_bitmap.colorType() != kRGBA_F16_SkColorType) {
    return false;
  }
  return true;
}
}  // anonymous namespace

ImageExtractor::ImageExtractor(Image* image,
                               bool premultiply_alpha,
                               sk_sp<SkColorSpace> target_color_space) {
  if (!image) {
    return;
  }

  const auto& paint_image = image->PaintImageForCurrentFrame();
  sk_sp<SkImage> skia_image = paint_image.GetSwSkImage();
  if (skia_image && !skia_image->colorSpace()) {
    skia_image = skia_image->reinterpretColorSpace(SkColorSpace::MakeSRGB());
  }

  if (image->HasData()) {
    bool paint_image_is_f16 =
        paint_image.GetColorType() == kRGBA_F16_SkColorType;

    // If there already exists a decoded image in `skia_image`, determine if we
    // can re-use that image. If we can't, then we need to re-decode the image
    // here.
    bool needs_redecode = false;
    if (skia_image) {
      // The `target_color_space` is set to nullptr iff
      // UNPACK_COLORSPACE_CONVERSION is NONE, which means that the color
      // profile of the image should be ignored. In this case, always re-decode,
      // because we can't reliably know that `skia_image` ignored the image's
      // color profile when it was created.
      if (!target_color_space) {
        needs_redecode = true;
      }

      // If there is a target color space, but the SkImage that was decoded is
      // not already in this color space, then re-decode the image. The reason
      // for this is that repeated color converisons may accumulate clamping and
      // rounding errors.
      if (target_color_space &&
          !SkColorSpace::Equals(skia_image->colorSpace(),
                                target_color_space.get())) {
        needs_redecode = true;
      }

      // If the image was decoded with premultipled alpha and unpremultipled
      // alpha was requested, then re-decode without premultiplying alpha. Don't
      // bother re-decoding if premultiply alpha was requested, because we will
      // do that lossy conversion later.
      if (skia_image->alphaType() == kPremul_SkAlphaType &&
          !premultiply_alpha) {
        needs_redecode = true;
      }

      // If the image is high bit depth, but was not decoded as high bit depth,
      // then re-decode the image.
      if (paint_image_is_f16 &&
          skia_image->colorType() != kRGBA_F16_SkColorType) {
        needs_redecode = true;
      }
    } else {
      // If the image has not been decoded yet, then it needs to be decoded.
      needs_redecode = true;
    }

    if (needs_redecode) {
      const bool data_complete = true;

      // Always decode as unpremultiplied. If premultiplication is desired, it
      // will be applied later.
      const auto alpha_option = ImageDecoder::kAlphaNotPremultiplied;

      // Decode to the paint image's bit depth. If conversion is needed, it will
      // be applied later.
      const auto bit_depth = paint_image_is_f16
                                 ? ImageDecoder::kHighBitDepthToHalfFloat
                                 : ImageDecoder::kDefaultBitDepth;

      // If we are not ignoring the color space, then tag the image with the
      // target color space. It will be converted later on.
      const auto color_behavior =
          target_color_space ? ColorBehavior::kTag : ColorBehavior::kIgnore;

      // Decode the image here on the main thread.
      std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
          image->Data(), data_complete, alpha_option, bit_depth, color_behavior,
          cc::AuxImage::kDefault, Platform::GetMaxDecodedImageBytes()));
      if (!decoder || !decoder->FrameCount()) {
        return;
      }
      ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
      if (!frame || frame->GetStatus() != ImageFrame::kFrameComplete) {
        return;
      }
      SkBitmap bitmap = frame->Bitmap();
      if (!FrameIsValid(bitmap)) {
        return;
      }

      // TODO(fmalita): Partial frames are not supported currently: only fully
      // decoded frames make it through.  We could potentially relax this and
      // use SkImages::RasterFromBitmap(bitmap) to make a copy.
      skia_image = frame->FinalizePixelsAndGetImage();
    }
  }

  if (!skia_image) {
    return;
  }

  DCHECK(skia_image->width());
  DCHECK(skia_image->height());

  // Fail if the image was downsampled because of memory limits.
  if (skia_image->width() != image->width() ||
      skia_image->height() != image->height()) {
    return;
  }

  sk_image_ = std::move(skia_image);
}

}  // namespace blink
