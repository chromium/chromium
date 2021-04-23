// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateJXLDecoderWithArguments(
    const char* jxl_file,
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption,
    ColorBehavior color_behavior) {
  auto decoder = std::make_unique<JXLImageDecoder>(
      alpha_option, color_behavior, ImageDecoder::kNoDecodedImageByteLimit);
  scoped_refptr<SharedBuffer> data = ReadFile(jxl_file);
  EXPECT_FALSE(data->IsEmpty());
  decoder->SetData(data.get(), true);
  return decoder;
}

std::unique_ptr<ImageDecoder> CreateJXLDecoder() {
  return std::make_unique<JXLImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::Tag(),
      ImageDecoder::kNoDecodedImageByteLimit);
}

std::unique_ptr<ImageDecoder> CreateJXLDecoderWithData(const char* jxl_file) {
  auto decoder = CreateJXLDecoder();
  scoped_refptr<SharedBuffer> data = ReadFile(jxl_file);
  EXPECT_FALSE(data->IsEmpty());
  decoder->SetData(data.get(), true);
  return decoder;
}

// The expected_color must match the expected top left pixel.
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

void TestSize(const char* jxl_file, IntSize expected_size) {
  auto decoder = CreateJXLDecoderWithData(jxl_file);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(expected_size, decoder->Size());
}

void TestPixel(const char* jxl_file,
               IntSize expected_size,
               const WTF::Vector<IntPoint>& coordinates,
               const WTF::Vector<SkColor>& expected_colors,
               ImageDecoder::AlphaOption alpha_option,
               ColorBehavior color_behavior,
               int accuracy) {
  EXPECT_EQ(coordinates.size(), expected_colors.size());
  auto decoder = CreateJXLDecoderWithArguments(
      jxl_file, alpha_option, ImageDecoder::kDefaultBitDepth, color_behavior);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(expected_size, decoder->Size());
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  const SkBitmap& bitmap = frame->Bitmap();
  for (size_t i = 0; i < coordinates.size(); ++i) {
    int x = coordinates[i].X();
    int y = coordinates[i].Y();
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

TEST(JXLTests, SizeTest) {
  TestSize("/images/resources/jxl/alpha-lossless.jxl", IntSize(2, 10));
}

TEST(JXLTests, PixelTest) {
  TestPixel("/images/resources/jxl/red-10-default.jxl", IntSize(10, 10),
            {{0, 0}}, {SkColorSetARGB(255, 255, 0, 0)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/red-10-lossless.jxl", IntSize(10, 10),
            {{0, 1}}, {SkColorSetARGB(255, 255, 0, 0)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/red-10-container.jxl", IntSize(10, 10),
            {{1, 0}}, {SkColorSetARGB(255, 255, 0, 0)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/green-10-lossless.jxl", IntSize(10, 10),
            {{2, 3}}, {SkColorSetARGB(255, 0, 255, 0)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/blue-10-lossless.jxl", IntSize(10, 10),
            {{9, 9}}, {SkColorSetARGB(255, 0, 0, 255)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/alpha-lossless.jxl", IntSize(2, 10),
            {{0, 1}}, {SkColorSetARGB(0, 255, 255, 255)},
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
            ColorBehavior::Tag(), 0);
  TestPixel("/images/resources/jxl/alpha-lossless.jxl", IntSize(2, 10),
            {{0, 1}}, {SkColorSetARGB(0, 0, 0, 0)},
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::Tag(), 0);

  WTF::Vector<IntPoint> coordinates_3x3 = {
      {0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}, {0, 2}, {1, 2}, {2, 2},
  };

  TestPixel("/images/resources/jxl/3x3_srgb_lossless.jxl", IntSize(3, 3),
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

  TestPixel("/images/resources/jxl/3x3_srgb_lossy.jxl", IntSize(3, 3),
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

  TestPixel("/images/resources/jxl/3x3a_srgb_lossless.jxl", IntSize(3, 3),
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

  TestPixel("/images/resources/jxl/3x3a_srgb_lossy.jxl", IntSize(3, 3),
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
  TestPixel("/images/resources/jxl/3x3_gbr_lossless.jxl", IntSize(3, 3),
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

  TestPixel("/images/resources/jxl/3x3_gbr_lossy.jxl", IntSize(3, 3),
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
  TestPixel("/images/resources/jxl/3x3a_gbr_lossless.jxl", IntSize(3, 3),
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

  TestPixel("/images/resources/jxl/3x3a_gbr_lossy.jxl", IntSize(3, 3),
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
  TestPixel("/images/resources/jxl/3x3_pq_lossless.jxl", IntSize(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 0, 255, 0),
                SkColorSetARGB(255, 0, 0, 255),
                SkColorSetARGB(255, 33, 0, 0),
                SkColorSetARGB(255, 0, 26, 0),
                SkColorSetARGB(255, 0, 1, 26),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 24, 24, 24),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 2);

  TestPixel("/images/resources/jxl/3x3_pq_lossy.jxl", IntSize(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(255, 255, 0, 0),
                SkColorSetARGB(255, 0, 255, 0),
                SkColorSetARGB(255, 0, 0, 255),
                SkColorSetARGB(255, 33, 0, 0),
                SkColorSetARGB(255, 0, 26, 0),
                SkColorSetARGB(255, 0, 1, 26),
                SkColorSetARGB(255, 255, 255, 255),
                SkColorSetARGB(255, 24, 24, 24),
                SkColorSetARGB(255, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 15);

  TestPixel("/images/resources/jxl/3x3a_pq_lossless.jxl", IntSize(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 0, 255, 0),
                SkColorSetARGB(128, 0, 0, 255),
                SkColorSetARGB(128, 33, 0, 0),
                SkColorSetARGB(128, 0, 26, 0),
                SkColorSetARGB(128, 0, 1, 26),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 24, 24, 24),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 2);

  TestPixel("/images/resources/jxl/3x3a_pq_lossy.jxl", IntSize(3, 3),
            coordinates_3x3,
            {
                SkColorSetARGB(128, 255, 0, 0),
                SkColorSetARGB(128, 0, 255, 0),
                SkColorSetARGB(128, 0, 0, 255),
                SkColorSetARGB(128, 33, 0, 0),
                SkColorSetARGB(128, 0, 26, 0),
                SkColorSetARGB(128, 0, 1, 26),
                SkColorSetARGB(128, 255, 255, 255),
                SkColorSetARGB(128, 24, 24, 24),
                SkColorSetARGB(128, 0, 0, 0),
            },
            ImageDecoder::AlphaOption::kAlphaPremultiplied,
            ColorBehavior::TransformToSRGB(), 15);
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

}  // namespace
}  // namespace blink
