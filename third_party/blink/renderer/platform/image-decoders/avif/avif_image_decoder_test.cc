// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/avif/avif_image_decoder.h"

#include <cmath>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bit_cast.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/libavif/src/include/avif/avif.h"

#define FIXME_SUPPORT_ICC_PROFILE_NO_TRANSFORM 0
#define FIXME_SUPPORT_ICC_PROFILE_TRANSFORM 0
#define FIXME_DISTINGUISH_LOSSY_OR_LOSSLESS 0

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateAVIFDecoderWithOptions(
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_option,
    ColorBehavior color_behavior,
    ImageDecoder::AnimationOption animation_option) {
  return std::make_unique<AVIFImageDecoder>(
      alpha_option, high_bit_depth_option, color_behavior,
      ImageDecoder::kNoDecodedImageByteLimit, animation_option);
}

std::unique_ptr<ImageDecoder> CreateAVIFDecoder() {
  return CreateAVIFDecoderWithOptions(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::Tag(), ImageDecoder::AnimationOption::kUnspecified);
}

struct ExpectedColor {
  gfx::Point point;
  SkColor color;
};

enum class ColorType {
  kRgb,
  kRgbA,
  kMono,
  kMonoA,
};

struct StaticColorCheckParam {
  const char* path;
  int bit_depth;
  ColorType color_type;
  ImageDecoder::CompressionFormat compression_format;
  ImageDecoder::AlphaOption alpha_option;
  ColorBehavior color_behavior;
  ImageOrientation orientation;
  int color_threshold;
  std::vector<ExpectedColor> colors;
};

std::ostream& operator<<(std::ostream& os, const StaticColorCheckParam& param) {
  const char* color_type;
  switch (param.color_type) {
    case ColorType::kRgb:
      color_type = "kRgb";
      break;
    case ColorType::kRgbA:
      color_type = "kRgbA";
      break;
    case ColorType::kMono:
      color_type = "kMono";
      break;
    case ColorType::kMonoA:
      color_type = "kMonoA";
      break;
  }
  const char* alpha_option =
      (param.alpha_option == ImageDecoder::kAlphaPremultiplied
           ? "kAlphaPremultiplied"
           : "kAlphaNotPremultiplied");
  const char* color_behavior;
  if (param.color_behavior.IsIgnore()) {
    color_behavior = "Ignore";
  } else if (param.color_behavior.IsTag()) {
    color_behavior = "Tag";
  } else {
    DCHECK(param.color_behavior.IsTransformToSRGB());
    color_behavior = "TransformToSRGB";
  }
  const char* orientation;
  switch (param.orientation.Orientation()) {
    case ImageOrientationEnum::kOriginTopLeft:
      orientation = "kOriginTopLeft";
      break;
    case ImageOrientationEnum::kOriginTopRight:
      orientation = "kOriginTopRight";
      break;
    case ImageOrientationEnum::kOriginBottomRight:
      orientation = "kOriginBottomRight";
      break;
    case ImageOrientationEnum::kOriginBottomLeft:
      orientation = "kOriginBottomLeft";
      break;
    case ImageOrientationEnum::kOriginLeftTop:
      orientation = "kOriginLeftTop";
      break;
    case ImageOrientationEnum::kOriginRightTop:
      orientation = "kOriginRightTop";
      break;
    case ImageOrientationEnum::kOriginRightBottom:
      orientation = "kOriginRightBottom";
      break;
    case ImageOrientationEnum::kOriginLeftBottom:
      orientation = "kOriginLeftBottom";
      break;
  }
  return os << "\nStaticColorCheckParam {\n  path: \"" << param.path
            << "\",\n  bit_depth: " << param.bit_depth
            << ",\n  color_type: " << color_type
            << ",\n  alpha_option: " << alpha_option
            << ",\n  color_behavior: " << color_behavior
            << ",\n  orientation: " << orientation << "\n}";
}

StaticColorCheckParam kTestParams[] = {
    {
        "/images/resources/avif/red-at-12-oclock-with-color-profile-lossy.avif",
        8,
        ColorType::kRgb,
        ImageDecoder::kLossyFormat,
        ImageDecoder::kAlphaNotPremultiplied,  // q=60(lossy)
        ColorBehavior::Tag(),
        ImageOrientationEnum::kOriginTopLeft,
        0,
        {},  // we just check that this image is lossy.
    },
    {
        "/images/resources/avif/red-at-12-oclock-with-color-profile-lossy.avif",
        8,
        ColorType::kRgb,
        ImageDecoder::kLossyFormat,
        ImageDecoder::kAlphaNotPremultiplied,  // q=60(lossy)
        ColorBehavior::Ignore(),
        ImageOrientationEnum::kOriginTopLeft,
        0,
        {},  // we just check that the decoder won't crash when
             // ColorBehavior::Ignore() is used.
    },
    {"/images/resources/avif/red-with-alpha-8bpc.avif",
     8,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     3,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-range-420-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-range-unspecified-420-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/silver-full-range-srgb-420-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 192, 192, 192)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 192, 192, 192)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 192, 192, 192)},
     }},
    {"/images/resources/avif/alpha-mask-limited-range-8bpc.avif",
     8,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
    {"/images/resources/avif/alpha-mask-full-range-8bpc.avif",
     8,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
    {"/images/resources/avif/red-with-alpha-8bpc.avif",
     8,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaPremultiplied,
     ColorBehavior::TransformToSRGB(),
     ImageOrientationEnum::kOriginTopLeft,
     3,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#if FIXME_SUPPORT_ICC_PROFILE_NO_TRANSFORM
    {"/images/resources/avif/red-with-profile-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Ignore(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 0, 0, 255)},
     }},
#endif
#if FIXME_SUPPORT_ICC_PROFILE_TRANSFORM
    {"/images/resources/avif/red-with-profile-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::TransformToSRGB(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         /*
          * "Color Spin" ICC profile, embedded in this image,
          * changes blue to red.
          */
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
    {"/images/resources/avif/red-with-alpha-10bpc.avif",
     10,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     2,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-with-alpha-10bpc.avif",
     10,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaPremultiplied,
     ColorBehavior::TransformToSRGB(),
     ImageOrientationEnum::kOriginTopLeft,
     2,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-range-420-10bpc.avif",
     10,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/alpha-mask-limited-range-10bpc.avif",
     10,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
    {"/images/resources/avif/alpha-mask-full-range-10bpc.avif",
     10,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
#if FIXME_SUPPORT_ICC_PROFILE_NO_TRANSFORM
    {"/images/resources/avif/red-with-profile-10bpc.avif",
     10,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Ignore(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 0, 0, 255)},
     }},
#endif
#if FIXME_SUPPORT_ICC_PROFILE_TRANSFORM
    {"/images/resources/avif/red-with-profile-10bpc.avif",
     10,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::TransformToSRGB(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         /*
          * "Color Spin" ICC profile, embedded in this image,
          * changes blue to red.
          */
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
    {"/images/resources/avif/red-with-alpha-12bpc.avif",
     12,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-with-alpha-12bpc.avif",
     12,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaPremultiplied,
     ColorBehavior::TransformToSRGB(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-range-420-12bpc.avif",
     12,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/alpha-mask-limited-range-12bpc.avif",
     12,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
    {"/images/resources/avif/alpha-mask-full-range-12bpc.avif",
     12,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
#if FIXME_SUPPORT_ICC_PROFILE_NO_TRANSFORM
    {"/images/resources/avif/red-with-profile-12bpc.avif",
     12,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Ignore(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 0, 0, 255)},
     }},
#endif
#if FIXME_SUPPORT_ICC_PROFILE_TRANSFORM
    {"/images/resources/avif/red-with-profile-12bpc.avif",
     12,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::TransformToSRGB(),
     ImageOrientationEnum::kOriginTopLeft,
     1,
     {
         /*
          * "Color Spin" ICC profile, embedded in this image,
          * changes blue to red.
          */
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
    {"/images/resources/avif/red-full-range-angle-1-420-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginLeftBottom,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-range-mode-0-420-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginBottomLeft,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-range-mode-1-420-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopRight,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-range-angle-2-mode-0-420-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginTopRight,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-range-angle-3-mode-1-420-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     ImageOrientationEnum::kOriginLeftTop,
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    // TODO(ryoh): Add other color profile images, such as BT2020CL,
    //  SMPTE 274M
    // TODO(ryoh): Add images with different combinations of ColorPrimaries,
    //  TransferFunction and MatrixCoefficients,
    //  such as:
    //   sRGB ColorPrimaries, BT.2020 TransferFunction and
    //   BT.709 MatrixCoefficients
    // TODO(ryoh): Add Mono + Alpha Images.
};

enum class ErrorPhase { kParse, kDecode };

// If 'error_phase' is ErrorPhase::kParse, error is expected during parse
// (SetData() call); else error is expected during decode
// (DecodeFrameBufferAtIndex() call).
void TestInvalidStaticImage(const char* avif_file, ErrorPhase error_phase) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();

  scoped_refptr<SharedBuffer> data = ReadFile(avif_file);
  ASSERT_TRUE(data.get());
  decoder->SetData(std::move(data), true);

  if (error_phase == ErrorPhase::kParse) {
    EXPECT_FALSE(decoder->IsSizeAvailable());
    EXPECT_TRUE(decoder->Failed());
    EXPECT_EQ(0u, decoder->FrameCount());
    EXPECT_FALSE(decoder->DecodeFrameBufferAtIndex(0));
  } else {
    EXPECT_TRUE(decoder->IsSizeAvailable());
    EXPECT_FALSE(decoder->Failed());
    EXPECT_GT(decoder->FrameCount(), 0u);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_NE(ImageFrame::kFrameComplete, frame->GetStatus());
    EXPECT_TRUE(decoder->Failed());
  }
}

float HalfFloatToUnorm(uint16_t h) {
  const uint32_t f = ((h & 0x8000) << 16) | (((h & 0x7c00) + 0x1c000) << 13) |
                     ((h & 0x03ff) << 13);
  return base::bit_cast<float>(f);
}

void ReadYUV(const char* file_name,
             const gfx::Size& expected_y_size,
             const gfx::Size& expected_uv_size,
             SkColorType color_type,
             int bit_depth,
             gfx::Point3F* rgb_pixel = nullptr) {
  scoped_refptr<SharedBuffer> data =
      ReadFile("web_tests/images/resources/avif/", file_name);
  ASSERT_TRUE(data);

  auto decoder = CreateAVIFDecoder();
  decoder->SetData(std::move(data), true);

  ASSERT_TRUE(decoder->IsDecodedSizeAvailable());
  ASSERT_TRUE(decoder->CanDecodeToYUV());
  EXPECT_NE(decoder->GetYUVSubsampling(), cc::YUVSubsampling::kUnknown);
  EXPECT_NE(decoder->GetYUVColorSpace(),
            SkYUVColorSpace::kIdentity_SkYUVColorSpace);
  EXPECT_EQ(decoder->GetYUVBitDepth(), bit_depth);

  gfx::Size size = decoder->DecodedSize();
  gfx::Size y_size = decoder->DecodedYUVSize(cc::YUVIndex::kY);
  gfx::Size u_size = decoder->DecodedYUVSize(cc::YUVIndex::kU);
  gfx::Size v_size = decoder->DecodedYUVSize(cc::YUVIndex::kV);

  EXPECT_EQ(size, y_size);
  EXPECT_EQ(u_size, v_size);

  EXPECT_EQ(expected_y_size, y_size);
  EXPECT_EQ(expected_uv_size, u_size);

  wtf_size_t row_bytes[3];
  row_bytes[0] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kY);
  row_bytes[1] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kU);
  row_bytes[2] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kV);

  size_t planes_data_size = row_bytes[0] * y_size.height() +
                            row_bytes[1] * u_size.height() +
                            row_bytes[2] * v_size.height();
  auto planes_data = std::make_unique<char[]>(planes_data_size);

  void* planes[3];
  planes[0] = planes_data.get();
  planes[1] = static_cast<char*>(planes[0]) + row_bytes[0] * y_size.height();
  planes[2] = static_cast<char*>(planes[1]) + row_bytes[1] * u_size.height();

  decoder->SetImagePlanes(
      std::make_unique<ImagePlanes>(planes, row_bytes, color_type));

  decoder->DecodeToYUV();
  EXPECT_FALSE(decoder->Failed());
  EXPECT_TRUE(decoder->HasDisplayableYUVData());

  auto metadata = decoder->MakeMetadataForDecodeAcceleration();
  EXPECT_EQ(cc::ImageType::kAVIF, metadata.image_type);
  EXPECT_EQ(size, metadata.image_size);
  if (expected_y_size == expected_uv_size)
    EXPECT_EQ(cc::YUVSubsampling::k444, metadata.yuv_subsampling);
  else if (expected_y_size.height() == expected_uv_size.height())
    EXPECT_EQ(cc::YUVSubsampling::k422, metadata.yuv_subsampling);
  else
    EXPECT_EQ(cc::YUVSubsampling::k420, metadata.yuv_subsampling);

  if (!rgb_pixel)
    return;

  if (bit_depth > 8) {
    rgb_pixel->set_x(reinterpret_cast<uint16_t*>(planes[0])[0]);
    rgb_pixel->set_y(reinterpret_cast<uint16_t*>(planes[1])[0]);
    rgb_pixel->set_z(reinterpret_cast<uint16_t*>(planes[2])[0]);
  } else {
    rgb_pixel->set_x(reinterpret_cast<uint8_t*>(planes[0])[0]);
    rgb_pixel->set_y(reinterpret_cast<uint8_t*>(planes[1])[0]);
    rgb_pixel->set_z(reinterpret_cast<uint8_t*>(planes[2])[0]);
  }

  if (color_type == kGray_8_SkColorType) {
    const float max_channel = (1 << bit_depth) - 1;
    rgb_pixel->set_x(rgb_pixel->x() / max_channel);
    rgb_pixel->set_y(rgb_pixel->y() / max_channel);
    rgb_pixel->set_z(rgb_pixel->z() / max_channel);
  } else if (color_type == kA16_unorm_SkColorType) {
    constexpr float kR16MaxChannel = 65535.0f;
    rgb_pixel->set_x(rgb_pixel->x() / kR16MaxChannel);
    rgb_pixel->set_y(rgb_pixel->y() / kR16MaxChannel);
    rgb_pixel->set_z(rgb_pixel->z() / kR16MaxChannel);
  } else {
    DCHECK_EQ(color_type, kA16_float_SkColorType);
    rgb_pixel->set_x(HalfFloatToUnorm(rgb_pixel->x()));
    rgb_pixel->set_y(HalfFloatToUnorm(rgb_pixel->y()));
    rgb_pixel->set_z(HalfFloatToUnorm(rgb_pixel->z()));
  }

  // Convert our YUV pixel to RGB to avoid an excessive amounts of test
  // expectations. We otherwise need bit_depth * yuv_sampling * color_type.
  auto* transform = reinterpret_cast<AVIFImageDecoder*>(decoder.get())
                        ->GetColorTransformForTesting();
  transform->Transform(rgb_pixel, 1);
}

void TestYUVRed(const char* file_name,
                const gfx::Size& expected_uv_size,
                SkColorType color_type = kGray_8_SkColorType,
                int bit_depth = 8) {
  SCOPED_TRACE(base::StringPrintf("file_name=%s, color_type=%d", file_name,
                                  int{color_type}));

  constexpr gfx::Size kRedYSize(3, 3);

  gfx::Point3F decoded_pixel;
  ASSERT_NO_FATAL_FAILURE(ReadYUV(file_name, kRedYSize, expected_uv_size,
                                  color_type, bit_depth, &decoded_pixel));

  // Allow the RGB value to be off by one step. 1/max_value is the minimum
  // amount of error possible if error exists for integer sources.
  //
  // For half float values we have additional error from precision limitations,
  // which gets worse at the extents of [-0.5, 1] -- which is the case for our R
  // channel since we're using a pure red source.
  //
  // https://en.wikipedia.org/wiki/Half-precision_floating-point_format#Precision_limitations_on_decimal_values_in_[0,_1]
  const double kMinError = 1.0 / ((1 << bit_depth) - 1);
  const double kError = color_type == kA16_float_SkColorType
                            ? kMinError + std::pow(2, -11)
                            : kMinError;
  EXPECT_NEAR(decoded_pixel.x(), 1, kError);     // R
  EXPECT_NEAR(decoded_pixel.y(), 0, kMinError);  // G
  EXPECT_NEAR(decoded_pixel.z(), 0, kMinError);  // B
}

void DecodeTask(const SharedBuffer* data, base::RepeatingClosure* done) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();

  scoped_refptr<SharedBuffer> data_copy = SharedBuffer::Create();
  data_copy->Append(*data);
  decoder->SetData(std::move(data_copy), true);

  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(decoder->FrameCount(), 1u);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());

  done->Run();
}

}  // namespace

TEST(AnimatedAVIFTests, ValidImages) {
  // star-animated-8bpc.avif, star-animated-10bpc.avif, and
  // star-animated-12bpc.avif contain an EditListBox whose `flags` field is
  // equal to 0, meaning the edit list is not repeated. Therefore their
  // `expected_repetition_count` is 0.
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-animated-8bpc.avif", 5u, 0);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/star-animated-8bpc-with-alpha.avif", 5u,
      kAnimationLoopInfinite);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-animated-10bpc.avif", 5u,
                       0);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/star-animated-10bpc-with-alpha.avif", 5u,
      kAnimationLoopInfinite);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-animated-12bpc.avif", 5u,
                       0);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/star-animated-12bpc-with-alpha.avif", 5u,
      kAnimationLoopInfinite);
  // TODO(ryoh): Add animated avif files with EditListBox.
}

TEST(AnimatedAVIFTests, HasMultipleSubImages) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();
  decoder->SetData(ReadFile("/images/resources/avif/star-animated-8bpc.avif"),
                   true);
  EXPECT_TRUE(decoder->ImageHasBothStillAndAnimatedSubImages());
}

TEST(StaticAVIFTests, DoesNotHaveMultipleSubImages) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();
  decoder->SetData(ReadFile("/images/resources/avif/"
                            "red-at-12-oclock-with-color-profile-8bpc.avif"),
                   true);
  EXPECT_FALSE(decoder->ImageHasBothStillAndAnimatedSubImages());
}

TEST(StaticAVIFTests, HasTimingInformation) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();
  decoder->SetData(ReadFile("/images/resources/avif/"
                            "red-at-12-oclock-with-color-profile-8bpc.avif"),
                   true);
  EXPECT_TRUE(!!decoder->DecodeFrameBufferAtIndex(0));

  // libavif has placeholder values for timestamp and duration on still images,
  // so any duration value is valid, but the timestamp should be zero.
  EXPECT_EQ(base::TimeDelta(), decoder->FrameTimestampAtIndex(0));
}

TEST(AnimatedAVIFTests, HasTimingInformation) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();
  decoder->SetData(ReadFile("/images/resources/avif/star-animated-8bpc.avif"),
                   true);

  constexpr auto kDuration = base::Milliseconds(100);

  EXPECT_TRUE(!!decoder->DecodeFrameBufferAtIndex(0));
  EXPECT_EQ(base::TimeDelta(), decoder->FrameTimestampAtIndex(0));
  EXPECT_EQ(kDuration, decoder->FrameDurationAtIndex(0));

  EXPECT_TRUE(!!decoder->DecodeFrameBufferAtIndex(1));
  EXPECT_EQ(kDuration, decoder->FrameTimestampAtIndex(1));
  EXPECT_EQ(kDuration, decoder->FrameDurationAtIndex(1));
}

TEST(StaticAVIFTests, NoCrashWhenCheckingForMultipleSubImages) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();
  constexpr char kHeader[] = {0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70};
  auto buffer = SharedBuffer::Create();
  buffer->Append(kHeader, std::size(kHeader));
  decoder->SetData(std::move(buffer), false);
  EXPECT_FALSE(decoder->ImageHasBothStillAndAnimatedSubImages());
}

// TODO(ryoh): Add corrupted video tests.

TEST(StaticAVIFTests, invalidImages) {
  // Image data is truncated.
  TestInvalidStaticImage(
      "/images/resources/avif/"
      "red-at-12-oclock-with-color-profile-truncated.avif",
      ErrorPhase::kParse);
  // Chunk size in AV1 frame header doesn't match the file size.
  TestInvalidStaticImage(
      "/images/resources/avif/"
      "red-at-12-oclock-with-color-profile-with-wrong-frame-header.avif",
      ErrorPhase::kDecode);
}

TEST(StaticAVIFTests, ValidImages) {
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/red-at-12-oclock-with-color-profile-lossy.avif",
      1, kAnimationNone);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/red-at-12-oclock-with-color-profile-8bpc.avif", 1,
      kAnimationNone);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/red-at-12-oclock-with-color-profile-10bpc.avif",
      1, kAnimationNone);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/red-at-12-oclock-with-color-profile-12bpc.avif",
      1, kAnimationNone);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/tiger_3layer_1res.avif", 1,
                       kAnimationNone);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/tiger_3layer_3res.avif", 1,
                       kAnimationNone);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/tiger_420_8b_grid1x13.avif", 1,
                       kAnimationNone);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/dice_444_10b_grid4x3.avif", 1,
                       kAnimationNone);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/gracehopper_422_12b_grid2x4.avif", 1,
      kAnimationNone);
}

TEST(StaticAVIFTests, YUV) {
  // 3x3, YUV 4:2:0
  constexpr gfx::Size kUVSize420(2, 2);
  TestYUVRed("red-limited-range-420-8bpc.avif", kUVSize420);
  TestYUVRed("red-full-range-420-8bpc.avif", kUVSize420);

  // 3x3, YUV 4:2:2
  constexpr gfx::Size kUVSize422(2, 3);
  TestYUVRed("red-limited-range-422-8bpc.avif", kUVSize422);

  // 3x3, YUV 4:4:4
  constexpr gfx::Size kUVSize444(3, 3);
  TestYUVRed("red-limited-range-444-8bpc.avif", kUVSize444);

  // Full range BT709 color space is uncommon, but should be supported.
  TestYUVRed("red-full-range-bt709-444-8bpc.avif", kUVSize444);

  for (const auto ct : {kA16_unorm_SkColorType, kA16_float_SkColorType}) {
    // 3x3, YUV 4:2:0, 10bpc
    TestYUVRed("red-limited-range-420-10bpc.avif", kUVSize420, ct, 10);

    // 3x3, YUV 4:2:2, 10bpc
    TestYUVRed("red-limited-range-422-10bpc.avif", kUVSize422, ct, 10);

    // 3x3, YUV 4:4:4, 10bpc
    TestYUVRed("red-limited-range-444-10bpc.avif", kUVSize444, ct, 10);

    // 3x3, YUV 4:2:0, 12bpc
    TestYUVRed("red-limited-range-420-12bpc.avif", kUVSize420, ct, 12);

    // 3x3, YUV 4:2:2, 12bpc
    TestYUVRed("red-limited-range-422-12bpc.avif", kUVSize422, ct, 12);

    // 3x3, YUV 4:4:4, 12bpc
    TestYUVRed("red-limited-range-444-12bpc.avif", kUVSize444, ct, 12);

    // Various common color spaces should be supported.
    TestYUVRed("red-full-range-bt2020-pq-444-10bpc.avif", kUVSize444, ct, 10);
    TestYUVRed("red-full-range-bt2020-pq-444-12bpc.avif", kUVSize444, ct, 12);
    TestYUVRed("red-full-range-bt2020-hlg-444-10bpc.avif", kUVSize444, ct, 10);
    TestYUVRed("red-full-range-bt2020-hlg-444-12bpc.avif", kUVSize444, ct, 12);
  }
}

TEST(StaticAVIFTests, SizeAvailableBeforeAllDataReceived) {
  scoped_refptr<SharedBuffer> stream_buffer = WTF::SharedBuffer::Create();
  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSharedBuffer(stream_buffer);
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::CreateByMimeType(
      "image/avif", segment_reader, /*data_complete=*/false,
      ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::Tag(), SkISize::MakeEmpty(),
      ImageDecoder::AnimationOption::kUnspecified);
  EXPECT_FALSE(decoder->IsSizeAvailable());

  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/avif/red-limited-range-420-8bpc.avif");
  ASSERT_TRUE(data.get());
  stream_buffer->Append(data->Data(), data->size());
  EXPECT_EQ(stream_buffer->size(), 318u);
  decoder->SetData(stream_buffer, /*all_data_received=*/false);
  // All bytes are appended so we should have size, even though we pass
  // all_data_received=false.
  EXPECT_TRUE(decoder->IsSizeAvailable());

  decoder->SetData(stream_buffer, /*all_data_received=*/true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
}

TEST(StaticAVIFTests, ProgressiveDecoding) {
  scoped_refptr<SharedBuffer> stream_buffer = WTF::SharedBuffer::Create();
  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSharedBuffer(stream_buffer);
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::CreateByMimeType(
      "image/avif", segment_reader, /*data_complete=*/false,
      ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::Tag(), SkISize::MakeEmpty(),
      ImageDecoder::AnimationOption::kUnspecified);

  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/avif/tiger_3layer_1res.avif");
  ASSERT_TRUE(data.get());
  ASSERT_EQ(data->size(), 70944u);

  // This image has three layers. The first layer is 8299 bytes. Because of
  // image headers and other overhead, if we pass exactly 8299 bytes to the
  // decoder, the decoder does not have enough data to decode the first layer.
  stream_buffer->Append(data->Data(), 8299u);
  decoder->SetData(stream_buffer, /*all_data_received=*/false);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(decoder->FrameCount(), 1u);
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->GetStatus(), ImageFrame::kFrameEmpty);
  EXPECT_FALSE(decoder->Failed());

  // An additional 301 bytes are enough data for the decoder to decode the first
  // layer. With progressive decoding, the frame buffer status will transition
  // to ImageFrame::kFramePartial.
  stream_buffer->Append(data->Data() + 8299u, 301u);
  decoder->SetData(stream_buffer, /*all_data_received=*/false);
  EXPECT_FALSE(decoder->Failed());
  frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->GetStatus(), ImageFrame::kFramePartial);
  EXPECT_FALSE(decoder->Failed());
}

TEST(StaticAVIFTests, IncrementalDecoding) {
  scoped_refptr<SharedBuffer> stream_buffer = WTF::SharedBuffer::Create();
  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSharedBuffer(stream_buffer);
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::CreateByMimeType(
      "image/avif", segment_reader, /*data_complete=*/false,
      ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::Tag(), SkISize::MakeEmpty(),
      ImageDecoder::AnimationOption::kUnspecified);

  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/avif/tiger_420_8b_grid1x13.avif");
  ASSERT_TRUE(data.get());

  struct Step {
    size_t size;  // In bytes.
    ImageFrame::Status status;
    int num_decoded_rows;  // In pixels.
  };
  // There are 13 tiles. Tiles are as wide as the image and 64 pixels tall.
  // |num_decoded_rows| may be odd due to an output pixel row missing the
  // following upsampled decoded chroma row (belonging to the next tile).
  const Step steps[] = {
      {2000, ImageFrame::kFrameEmpty, 0},
      // Decoding half of the bytes gives 6 tile rows.
      {data->size() / 2, ImageFrame::kFramePartial, 6 * 64 - 1},
      // Decoding all bytes but one gives 12 tile rows.
      {data->size() - 1, ImageFrame::kFramePartial, 12 * 64 - 1},
      // Decoding all bytes gives all 13 tile rows.
      {data->size(), ImageFrame::kFrameComplete, 13 * 64}};
  size_t previous_size = 0;
  for (const Step& step : steps) {
    stream_buffer->Append(data->Data() + previous_size,
                          step.size - previous_size);
    decoder->SetData(stream_buffer, step.status == ImageFrame::kFrameComplete);

    EXPECT_EQ(decoder->FrameCount(), 1u);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    ASSERT_FALSE(decoder->Failed());
    EXPECT_EQ(frame->GetStatus(), step.status);

    const SkBitmap& bitmap = frame->Bitmap();
    for (int y = 0; y < bitmap.height(); ++y) {
      const uint32_t* row = bitmap.getAddr32(0, y);
      const bool is_row_decoded = y < step.num_decoded_rows;
      for (int x = 0; x < bitmap.width(); ++x) {
        // The input image is opaque. Pixels outside the decoded area are fully
        // transparent black pixels, with each channel value being 0.
        const bool is_pixel_decoded = row[x] != 0x00000000u;
        ASSERT_EQ(is_pixel_decoded, is_row_decoded);
      }
    }
    previous_size = step.size;
  }
}

// Reproduces crbug.com/1402841. Decodes a large AVIF image 104 times in
// parallel from base::ThreadPool. Should not cause temporary deadlock of
// base::ThreadPool.
TEST(StaticAVIFTests, ParallelDecoding) {
  // The base::test::TaskEnvironment constructor creates a base::ThreadPool
  // instance with 4 foreground threads. The number 4 comes from the
  // test::TaskEnvironment::kNumForegroundThreadPoolThreads constant.
  base::test::TaskEnvironment task_environment;

  // This test image is fast to decode (all neutral gray pixels) and its
  // allocation size is large enough to cause
  // media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels() to pick
  // n_tasks > 1 if AVIFImageDecoder did not pass disable_threading=true to it.
  scoped_refptr<SharedBuffer> data =
      ReadFile("/images/resources/avif/gray1024x704.avif");
  ASSERT_TRUE(data.get());

  // Task timeout in tests is 30 seconds (see https://crrev.com/c/1949028).
  // Four blocking tasks cause a temporary deadlock (1.2 seconds) of
  // base::ThreadPool, so we need at least 30 / 1.2 * 4 = 100 decodes for the
  // test to time out without the bug fix. We add a margin of 4 decodes, i.e.,
  // (30 / 1.2 + 1) * 4 = 104.
  const size_t n_decodes = 104;
  base::WaitableEvent event;
  base::RepeatingClosure barrier = base::BarrierClosure(
      n_decodes,
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));

  for (size_t i = 0; i < n_decodes; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(DecodeTask, base::Unretained(data.get()), &barrier));
  }

  event.Wait();
}

TEST(StaticAVIFTests, AlphaHasNoIspeProperty) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();
  decoder->SetData(ReadFile("/images/resources/avif/green-no-alpha-ispe.avif"),
                   true);
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

TEST(StaticAVIFTests, UnsupportedTransferFunctionInColrProperty) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();
  decoder->SetData(
      ReadFile("/images/resources/avif/red-unsupported-transfer.avif"), true);
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_TRUE(decoder->Failed());
}

using StaticAVIFColorTests = ::testing::TestWithParam<StaticColorCheckParam>;

INSTANTIATE_TEST_SUITE_P(Parameterized,
                         StaticAVIFColorTests,
                         ::testing::ValuesIn(kTestParams));

TEST_P(StaticAVIFColorTests, InspectImage) {
  const StaticColorCheckParam& param = GetParam();
  // TODO(ryoh): Add tests with ImageDecoder::kHighBitDepthToHalfFloat
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoderWithOptions(
      param.alpha_option, ImageDecoder::kDefaultBitDepth, param.color_behavior,
      ImageDecoder::AnimationOption::kUnspecified);
  scoped_refptr<SharedBuffer> data = ReadFile(param.path);
  ASSERT_TRUE(data.get());
#if FIXME_DISTINGUISH_LOSSY_OR_LOSSLESS
  EXPECT_EQ(param.compression_format,
            ImageDecoder::GetCompressionFormat(data, "image/avif"));
#endif
  decoder->SetData(std::move(data), true);
  EXPECT_EQ(1u, decoder->FrameCount());
  EXPECT_EQ(kAnimationNone, decoder->RepetitionCount());
  EXPECT_EQ(param.bit_depth > 8, decoder->ImageIsHighBitDepth());
  auto metadata = decoder->MakeMetadataForDecodeAcceleration();
  EXPECT_EQ(cc::ImageType::kAVIF, metadata.image_type);
  // TODO(wtc): Check metadata.yuv_subsampling.
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(param.orientation, decoder->Orientation());
  EXPECT_EQ(param.color_type == ColorType::kRgbA ||
                param.color_type == ColorType::kMonoA,
            frame->HasAlpha());
  auto get_color_channel = [](SkColorChannel channel, SkColor color) {
    switch (channel) {
      case SkColorChannel::kR:
        return SkColorGetR(color);
      case SkColorChannel::kG:
        return SkColorGetG(color);
      case SkColorChannel::kB:
        return SkColorGetB(color);
      case SkColorChannel::kA:
        return SkColorGetA(color);
    }
  };
  auto color_difference = [get_color_channel](SkColorChannel channel,
                                              SkColor color1,
                                              SkColor color2) -> int {
    return std::abs(static_cast<int>(get_color_channel(channel, color1)) -
                    static_cast<int>(get_color_channel(channel, color2)));
  };
  for (const auto& expected : param.colors) {
    const SkBitmap& bitmap = frame->Bitmap();
    SkColor frame_color =
        bitmap.getColor(expected.point.x(), expected.point.y());

    EXPECT_LE(color_difference(SkColorChannel::kR, frame_color, expected.color),
              param.color_threshold);
    EXPECT_LE(color_difference(SkColorChannel::kG, frame_color, expected.color),
              param.color_threshold);
    EXPECT_LE(color_difference(SkColorChannel::kB, frame_color, expected.color),
              param.color_threshold);
    // TODO(ryoh): Create alpha_threshold field for alpha channels.
    EXPECT_LE(color_difference(SkColorChannel::kA, frame_color, expected.color),
              param.color_threshold);
    if (param.color_type == ColorType::kMono ||
        param.color_type == ColorType::kMonoA) {
      EXPECT_EQ(SkColorGetR(frame_color), SkColorGetG(frame_color));
      EXPECT_EQ(SkColorGetR(frame_color), SkColorGetB(frame_color));
    }
  }
}

}  // namespace blink
