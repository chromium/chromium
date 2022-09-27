// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/point.h"

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateJXLDecoderWithArguments(
    const char* jxl_file,
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_decoding_option,
    ColorBehavior color_behavior) {
  auto decoder = std::make_unique<JXLImageDecoder>(
      alpha_option, high_bit_depth_decoding_option, color_behavior,
      ImageDecoder::kNoDecodedImageByteLimit);
  scoped_refptr<SharedBuffer> data = ReadFile(jxl_file);
  EXPECT_FALSE(data->empty());
  decoder->SetData(data.get(), true);
  return decoder;
}

std::unique_ptr<ImageDecoder> CreateJXLDecoder() {
  return std::make_unique<JXLImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::Tag(), ImageDecoder::kNoDecodedImageByteLimit);
}

std::unique_ptr<ImageDecoder> CreateJXLDecoderWithData(const char* jxl_file) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data = ReadFile(jxl_file);
  EXPECT_FALSE(data->empty());
  decoder->SetData(data.get(), true);
  return decoder;
}

// expected_color must match the expected top left pixel
void TestColorProfile(const char* jxl_file,
                      ColorBehavior color_behavior,
                      SkColor expected_color) {
  auto decoder = CreateJXLDecoderWithArguments(
      jxl_file, ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
      ImageDecoder::kDefaultBitDepth, color_behavior);
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  const SkBitmap& bitmap = frame->Bitmap();
  SkColor frame_color = bitmap.getColor(0, 0);
  for (int i = 0; i < 4; ++i) {
    int frame_comp = (frame_color >> (8 * i)) & 255;
    int expected_comp = (expected_color >> (8 * i)) & 255;
    EXPECT_GE(1, abs(frame_comp - expected_comp));
  }
}

// Convert from float16 bits in a uint16_t, to 32-bit float, for testing
static float FromFloat16(uint16_t a) {
  // 5 bits exponent
  int exp = (a >> 10) & 31;
  // 10 bits fractional part
  float frac = a & 1023;
  // 1 bit sign
  int sign = (a & 32768) ? 1 : 0;
  bool subnormal = exp == 0;
  // Infinity and NaN are not supported here.
  exp -= 15;
  if (subnormal)
    exp++;
  frac /= 1024.0;
  if (!subnormal)
    frac++;
  frac *= std::pow(2, exp);
  if (sign)
    frac = -frac;
  return frac;
}

// expected_color must match the expected top left pixel
void TestHDR(const char* jxl_file,
             ColorBehavior color_behavior,
             bool expect_f16,
             float expected_r,
             float expected_g,
             float expected_b,
             float expected_a) {
  auto decoder = CreateJXLDecoderWithArguments(
      jxl_file, ImageDecoder::AlphaOption::kAlphaPremultiplied,
      ImageDecoder::kHighBitDepthToHalfFloat, color_behavior);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  float r, g, b, a;
  if (expect_f16) {
    EXPECT_EQ(ImageFrame::kRGBA_F16, frame->GetPixelFormat());
  } else {
    EXPECT_EQ(ImageFrame::kN32, frame->GetPixelFormat());
  }
  if (ImageFrame::kRGBA_F16 == frame->GetPixelFormat()) {
    uint64_t first_pixel = *frame->GetAddrF16(0, 0);
    r = FromFloat16(first_pixel >> 0);
    g = FromFloat16(first_pixel >> 16);
    b = FromFloat16(first_pixel >> 32);
    a = FromFloat16(first_pixel >> 48);
  } else {
    uint32_t first_pixel = *frame->GetAddr(0, 0);
    a = ((first_pixel >> SK_A32_SHIFT) & 255) / 255.0;
    r = ((first_pixel >> SK_R32_SHIFT) & 255) / 255.0;
    g = ((first_pixel >> SK_G32_SHIFT) & 255) / 255.0;
    b = ((first_pixel >> SK_B32_SHIFT) & 255) / 255.0;
  }
  constexpr float eps = 0.01;
  EXPECT_NEAR(expected_r, r, eps);
  EXPECT_NEAR(expected_g, g, eps);
  EXPECT_NEAR(expected_b, b, eps);
  EXPECT_NEAR(expected_a, a, eps);
}

void TestSize(const char* jxl_file, gfx::Size expected_size) {
  auto decoder = CreateJXLDecoderWithData(jxl_file);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(expected_size, decoder->Size());
}

struct FramePoint {
  size_t frame;
  gfx::Point point;
};

void TestPixel(const char* jxl_file,
               gfx::Size expected_size,
               const WTF::Vector<FramePoint>& coordinates,
               const WTF::Vector<SkColor>& expected_colors,
               ImageDecoder::AlphaOption alpha_option,
               ColorBehavior color_behavior,
               int accuracy,
               size_t num_frames = 1) {
  SCOPED_TRACE(testing::Message()
               << "TestPixel jxl_file: " << jxl_file
               << ", alpha_option:" << static_cast<int>(alpha_option));
  EXPECT_EQ(coordinates.size(), expected_colors.size());
  auto decoder = CreateJXLDecoderWithArguments(
      jxl_file, alpha_option, ImageDecoder::kDefaultBitDepth, color_behavior);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(expected_size, decoder->Size());
  ASSERT_EQ(num_frames, decoder->FrameCount());
  for (size_t i = 0; i < num_frames; ++i) {
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  }
  EXPECT_FALSE(decoder->Failed());
  for (size_t i = 0; i < coordinates.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "Coordinate: " << i);
    const SkBitmap& bitmap =
        decoder->DecodeFrameBufferAtIndex(coordinates[i].frame)->Bitmap();
    EXPECT_TRUE(SkColorSpace::Equals(bitmap.colorSpace(),
                                     decoder->ColorSpaceForSkImages().get()));
    int x = coordinates[i].point.x();
    int y = coordinates[i].point.y();
    SkColor frame_color = bitmap.getColor(x, y);
    int r_expected = (expected_colors[i] >> 16) & 255;
    int g_expected = (expected_colors[i] >> 8) & 255;
    int b_expected = (expected_colors[i] >> 0) & 255;
    int a_expected = (expected_colors[i] >> 24) & 255;
    int r_actual = (frame_color >> 16) & 255;
    int g_actual = (frame_color >> 8) & 255;
    int b_actual = (frame_color >> 0) & 255;
    int a_actual = (frame_color >> 24) & 255;
    EXPECT_NEAR(r_expected, r_actual, accuracy);
    EXPECT_NEAR(g_expected, g_actual, accuracy);
    EXPECT_NEAR(b_expected, b_actual, accuracy);
    // Alpha is always lossless.
    EXPECT_EQ(a_expected, a_actual);
  }
}

// SegmentReader implementation for testing, which always returns segments
// of size 1. This allows to test whether the decoder handles streaming
// correctly in the most fine-grained case.
class PerByteSegmentReader : public SegmentReader {
 public:
  PerByteSegmentReader(SharedBuffer& buffer) : buffer_(buffer) {}
  size_t size() const override { return buffer_.size(); }
  size_t GetSomeData(const char*& data, size_t position) const override {
    if (position >= buffer_.size()) {
      return 0;
    }
    data = buffer_.Data() + position;
    return 1;
  }
  sk_sp<SkData> GetAsSkData() const override { return nullptr; }

 private:
  SharedBuffer& buffer_;
};

// Tests whether the decoder successfully parses the file without errors or
// infinite loop in the worst case of the reader returning 1-byte segments.
void TestSegmented(const char* jxl_file, gfx::Size expected_size) {
  auto decoder = std::make_unique<JXLImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::Tag(), ImageDecoder::kNoDecodedImageByteLimit);
  scoped_refptr<SharedBuffer> data = ReadFile(jxl_file);
  EXPECT_FALSE(data->empty());

  scoped_refptr<SegmentReader> reader =
      base::AdoptRef(new PerByteSegmentReader(*data.get()));
  decoder->SetData(reader, true);

  ImageFrame* frame;
  for (;;) {
    frame = decoder->DecodeFrameBufferAtIndex(0);
    if (decoder->Failed())
      break;
    if (frame)
      break;
  }

  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_LE(1u, decoder->FrameCount());
  EXPECT_TRUE(!!frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(expected_size, decoder->Size());
}

TEST(JXLTests, SegmentedTest) {
  TestSegmented("/images/resources/jxl/alpha-lossless.jxl", gfx::Size(2, 10));
  TestSegmented("/images/resources/jxl/3x3_srgb_lossy.jxl", gfx::Size(3, 3));
  TestSegmented("/images/resources/jxl/pq_gradient_icc_lossy.jxl",
                gfx::Size(16, 16));
  TestSegmented("/images/resources/jxl/animated.jxl", gfx::Size(16, 16));
}

TEST(JXLTests, SizeTest) {
  TestSize("/images/resources/jxl/alpha-lossless.jxl", gfx::Size(2, 10));
}

TEST(JXLTests, PixelTest) {
  TestPixel("/images/resources/jxl/red-10-default.jxl", gfx::Size(10, 10),
            {{0, {0, 0}}}, {SkColorSetARGB(255, 255, 0, 0)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/red-10-lossless.jxl", gfx::Size(10, 10),
            {{0, {0, 1}}}, {SkColorSetARGB(255, 255, 0, 0)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/red-10-container.jxl", gfx::Size(10, 10),
            {{0, {1, 0}}}, {SkColorSetARGB(255, 255, 0, 0)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/green-10-lossless.jxl", gfx::Size(10, 10),
            {{0, {2, 3}}}, {SkColorSetARGB(255, 0, 255, 0)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/blue-10-lossless.jxl", gfx::Size(10, 10),
            {{0, {9, 9}}}, {SkColorSetARGB(255, 0, 0, 255)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/alpha-lossless.jxl", gfx::Size(2, 10),
            {{0, {0, 1}}}, {SkColorSetARGB(0, 255, 255, 255)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/alpha-lossless.jxl", gfx::Size(2, 10),
            {{0, {0, 1}}}, {SkColorSetARGB(0, 0, 0, 0)},
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 0);

  WTF::Vector<FramePoint> coordinates_3x3 = {
      {0, {0, 0}}, {0, {1, 0}}, {0, {2, 0}}, {0, {0, 1}}, {0, {1, 1}},
      {0, {2, 1}}, {0, {0, 2}}, {0, {1, 2}}, {0, {2, 2}},
  };

  TestPixel("/images/resources/jxl/3x3_srgb_lossless.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 0, 255, 0),
                SkColorSetARGB(255, 0, 0, 255),
                SkColorSetARGB(255, 128, 64, 64),
                SkColorSetARGB(255, 64, 128, 64),
                SkColorSetARGB(255, 64, 64, 128),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 128, 128, 128),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 0);

  TestPixel("/images/resources/jxl/3x3_srgb_lossy.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 0, 255, 0),
                SkColorSetARGB(255, 0, 0, 255),
                SkColorSetARGB(255, 128, 64, 64),
                SkColorSetARGB(255, 64, 128, 64),
                SkColorSetARGB(255, 64, 64, 128),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 128, 128, 128),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 15);

  TestPixel("/images/resources/jxl/3x3a_srgb_lossless.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 0, 255, 0),
                SkColorSetARGB(128, 0, 0, 255),
                SkColorSetARGB(128, 128, 64, 64),
                SkColorSetARGB(128, 64, 128, 64),
                SkColorSetARGB(128, 64, 64, 128),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 128, 128, 128),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 0);

  TestPixel("/images/resources/jxl/3x3a_srgb_lossy.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 0, 255, 0),
                SkColorSetARGB(128, 0, 0, 255),
                SkColorSetARGB(128, 128, 64, 64),
                SkColorSetARGB(128, 64, 128, 64),
                SkColorSetARGB(128, 64, 64, 128),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 128, 128, 128),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 15);

  // Lossless, but allow some inaccuracy due to the color profile conversion.
  TestPixel("/images/resources/jxl/3x3_gbr_lossless.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 0, 255, 0),
                SkColorSetARGB(255, 0, 0, 255),
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 64, 128, 64),
                SkColorSetARGB(255, 64, 64, 128),
                SkColorSetARGB(255, 128, 64, 64),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 128, 128, 128),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 3);

  TestPixel("/images/resources/jxl/3x3_gbr_lossy.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 0, 255, 0),
                SkColorSetARGB(255, 0, 0, 255),
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 64, 128, 64),
                SkColorSetARGB(255, 64, 64, 128),
                SkColorSetARGB(255, 128, 64, 64),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 128, 128, 128),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 35);

  // Lossless, but allow some inaccuracy due to the color profile conversion.
  TestPixel("/images/resources/jxl/3x3a_gbr_lossless.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 0, 255, 0),
                SkColorSetARGB(128, 0, 0, 255),
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 64, 128, 64),
                SkColorSetARGB(128, 64, 64, 128),
                SkColorSetARGB(128, 128, 64, 64),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 128, 128, 128),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 3);

  TestPixel("/images/resources/jxl/3x3a_gbr_lossy.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 0, 255, 0),
                SkColorSetARGB(128, 0, 0, 255),
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 64, 128, 64),
                SkColorSetARGB(128, 64, 64, 128),
                SkColorSetARGB(128, 128, 64, 64),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 128, 128, 128),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 35);

  // Lossless, but allow some inaccuracy due to the color profile conversion.
  TestPixel("/images/resources/jxl/3x3_pq_lossless.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 0, 255, 0),
                SkColorSetARGB(255, 0, 0, 255),
                SkColorSetARGB(255, 128, 64, 64),
                SkColorSetARGB(255, 64, 128, 64),
                SkColorSetARGB(255, 64, 64, 128),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 128, 128, 128),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 2);

  TestPixel("/images/resources/jxl/3x3_pq_lossy.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 64, 255, 64),
                SkColorSetARGB(255, 39, 76, 255),
                SkColorSetARGB(255, 128, 64, 64),
                SkColorSetARGB(255, 64, 128, 64),
                SkColorSetARGB(255, 64, 64, 128),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 128, 128, 128),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 15);

  TestPixel("/images/resources/jxl/3x3a_pq_lossless.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 0, 255, 0),
                SkColorSetARGB(128, 0, 0, 255),
                SkColorSetARGB(128, 128, 64, 64),
                SkColorSetARGB(128, 64, 128, 64),
                SkColorSetARGB(128, 64, 64, 128),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 128, 128, 128),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 2);

  TestPixel("/images/resources/jxl/3x3a_pq_lossy.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 64, 255, 64),
                SkColorSetARGB(128, 40, 82, 255),
                SkColorSetARGB(128, 128, 64, 64),
                SkColorSetARGB(128, 64, 128, 64),
                SkColorSetARGB(128, 64, 64, 128),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 128, 128, 128),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 15);

  TestPixel("/images/resources/jxl/3x3_hlg_lossless.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 0, 255, 0),
                SkColorSetARGB(255, 0, 0, 255),
                SkColorSetARGB(255, 86, 46, 46),
                SkColorSetARGB(255, 46, 86, 46),
                SkColorSetARGB(255, 46, 46, 86),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 85, 85, 85),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 2);

  TestPixel("/images/resources/jxl/3x3_hlg_lossy.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 255, 13, 13),
                SkColorSetARGB(255, 13, 255, 13),
                SkColorSetARGB(255, 13, 13, 255),
                SkColorSetARGB(255, 128, 64, 64),
                SkColorSetARGB(255, 64, 128, 64),
                SkColorSetARGB(255, 64, 64, 128),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 128, 128, 128),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 15);

  TestPixel("/images/resources/jxl/3x3a_hlg_lossless.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 0, 255, 0),
                SkColorSetARGB(128, 0, 0, 255),
                SkColorSetARGB(128, 86, 46, 46),
                SkColorSetARGB(128, 46, 86, 46),
                SkColorSetARGB(128, 46, 46, 86),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 85, 85, 85),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 6);

  TestPixel("/images/resources/jxl/3x3a_hlg_lossy.jxl", gfx::Size(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 255, 13, 13),
                SkColorSetARGB(128, 13, 255, 13),
                SkColorSetARGB(128, 13, 13, 255),
                SkColorSetARGB(128, 128, 64, 64),
                SkColorSetARGB(128, 64, 128, 64),
                SkColorSetARGB(128, 74, 64, 128),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 128, 128, 128),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 15);
}

TEST(JXLTests, ColorProfileTest) {
  TestColorProfile("/images/resources/jxl/icc-v2-gbr.jxl", ColorBehavior::Tag(),
                   SkColorSetARGB(255, 0xaf, 0xfe, 0x6b));
  TestColorProfile("/images/resources/jxl/icc-v2-gbr.jxl",
                   ColorBehavior::TransformToSRGB(),
                   SkColorSetARGB(255, 0x6b, 0xb1, 0xfe));
  TestColorProfile("/images/resources/jxl/icc-v2-gbr.jxl",
                   ColorBehavior::Ignore(),
                   SkColorSetARGB(255, 0xaf, 0xfe, 0x6b));
}

TEST(JXLTests, AnimatedPixelTest) {
  TestPixel(
      "/images/resources/jxl/animated.jxl", gfx::Size(16, 16),
      {{0, {0, 0}}, {1, {0, 0}}},
      {SkColorSetARGB(255, 204, 0, 153), SkColorSetARGB(255, 0, 102, 102)},
      ImageDecoder::AlphaOption::kAlphaNotPremultiplied, ColorBehavior::Tag(),
      0, 2);
}

TEST(JXLTests, JXLHDRTest) {
  // PQ tests
  // PQ values, as expected
  TestHDR("/images/resources/jxl/pq_gradient_lossy.jxl",
          ColorBehavior::Ignore(), false, 0.58039218187332153,
          0.73333334922790527, 0.43921568989753723, 1);
  // sRGB as expected, but not an exact match
  TestHDR("/images/resources/jxl/pq_gradient_lossy.jxl",
          ColorBehavior::TransformToSRGB(), true, -0.9248046875, 1.943359375,
          -0.4443359375, 1);

  // linear sRGB as expected.
  TestHDR("/images/resources/jxl/pq_gradient_lossy.jxl", ColorBehavior::Tag(),
          true, 0.58039218187332153, 0.73333334922790527, 0.43921568989753723,
          1);

  // correct, original PQ values
  TestHDR("/images/resources/jxl/pq_gradient_lossless.jxl",
          ColorBehavior::Ignore(), false, 0.58039218187332153,
          0.73725491762161255, 0.45098039507865906, 1);
  TestHDR("/images/resources/jxl/pq_gradient_lossless.jxl",
          ColorBehavior::TransformToSRGB(), true, -0.95751953125, 1.9677734375,
          -0.416748046875, 1);
  // correct, original PQ values
  TestHDR("/images/resources/jxl/pq_gradient_lossless.jxl",
          ColorBehavior::Tag(), true, 0.58056640625, 0.7373046875,
          0.450927734375, 1);

  // with ICC
  // clipped linear sRGB, as expected from current JXL implementation
  TestHDR("/images/resources/jxl/pq_gradient_icc_lossy.jxl",
          ColorBehavior::Ignore(), false, 0, 0.0930381, 0, 1);

  TestHDR("/images/resources/jxl/pq_gradient_icc_lossy.jxl",
          ColorBehavior::TransformToSRGB(), false, 0, 0.338623046875, 0, 1);
  TestHDR("/images/resources/jxl/pq_gradient_icc_lossy.jxl",
          ColorBehavior::Tag(), false, 0, 0.0930381, 0, 1);

  TestHDR("/images/resources/jxl/pq_gradient_icc_lossless.jxl",
          ColorBehavior::Ignore(), false, 0.58039218187332153,
          0.73725491762161255, 0.45098039507865906, 1);
  TestHDR("/images/resources/jxl/pq_gradient_icc_lossless.jxl",
          ColorBehavior::TransformToSRGB(), true, -0.95751953125, 1.9677734375,
          -0.416748046875, 1);
  TestHDR("/images/resources/jxl/pq_gradient_icc_lossless.jxl",
          ColorBehavior::Tag(), true, 0.58039218187332153, 0.73725491762161255,
          0.45098039507865906, 1);
}

TEST(JXLTests, RandomFrameDecode) {
  TestRandomFrameDecode(&CreateJXLDecoder, "/images/resources/jxl/count.jxl");
}

TEST(JXLTests, RandomDecodeAfterClearFrameBufferCache) {
  TestRandomDecodeAfterClearFrameBufferCache(&CreateJXLDecoder,
                                             "/images/resources/jxl/count.jxl");
}

}  // namespace
}  // namespace blink
