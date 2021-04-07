// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateJXLDecoderWithArguments(
    const char* jxl_file,
    ImageDecoder::AlphaOption alpha_option,
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

void TestColorProfile(const char* jxl_file,
                      ColorBehavior color_behavior,
                      SkColor expected_color) {
  auto decoder = CreateJXLDecoderWithArguments(
      jxl_file, ImageDecoder::AlphaOption::kAlphaNotPremultiplied,
      color_behavior);
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
               int x,
               int y,
               IntSize expected_size,
               SkColor expected_color,
               ImageDecoder::AlphaOption alpha_option) {
  auto decoder = CreateJXLDecoderWithArguments(jxl_file, alpha_option,
                                               ColorBehavior::Tag());
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_EQ(expected_size, decoder->Size());
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  const SkBitmap& bitmap = frame->Bitmap();
  SkColor frame_color = bitmap.getColor(x, y);
  EXPECT_EQ(frame_color, expected_color);
}

TEST(JXLTests, SizeTest) {
  TestSize("/images/resources/jxl/alpha-lossless.jxl", IntSize(2, 10));
}

TEST(JXLTests, PixelTest) {
  TestPixel("/images/resources/jxl/red-10-default.jxl", 0, 0, IntSize(10, 10),
            SkColorSetARGB(255, 255, 0, 0),
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied);
  TestPixel("/images/resources/jxl/red-10-lossless.jxl", 0, 1, IntSize(10, 10),
            SkColorSetARGB(255, 255, 0, 0),
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied);
  TestPixel("/images/resources/jxl/red-10-container.jxl", 1, 0, IntSize(10, 10),
            SkColorSetARGB(255, 255, 0, 0),
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied);
  TestPixel("/images/resources/jxl/green-10-lossless.jxl", 2, 3,
            IntSize(10, 10), SkColorSetARGB(255, 0, 255, 0),
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied);
  TestPixel("/images/resources/jxl/blue-10-lossless.jxl", 9, 9, IntSize(10, 10),
            SkColorSetARGB(255, 0, 0, 255),
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied);
  TestPixel("/images/resources/jxl/alpha-lossless.jxl", 0, 1, IntSize(2, 10),
            SkColorSetARGB(0, 255, 255, 255),
            ImageDecoder::AlphaOption::kAlphaNotPremultiplied);
  TestPixel("/images/resources/jxl/alpha-lossless.jxl", 0, 1, IntSize(2, 10),
            SkColorSetARGB(0, 0, 0, 0),
            ImageDecoder::AlphaOption::kAlphaPremultiplied);
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
