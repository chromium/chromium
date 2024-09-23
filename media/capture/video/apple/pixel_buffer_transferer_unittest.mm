// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/pixel_buffer_transferer.h"

#include <cmath>
#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "media/capture/video/apple/pixel_buffer_pool.h"
#include "media/capture/video/apple/test/pixel_buffer_test_utils.h"
#include "media/capture/video/apple/video_capture_device_avfoundation_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr uint8_t kColorR = 255u;
constexpr uint8_t kColorG = 127u;
constexpr uint8_t kColorB = 63u;

// Common pixel formats that we want to test. This is partially based on
// VideoCaptureDeviceAVFoundation::FourCCToChromiumPixelFormat but we do not
// include MJPEG because compressed formats are not supported by the
// PixelBufferPool. In addition to the formats supported for capturing, we also
// test I420, which all captured formats are normally converted to in software
// making it a sensible destination format.

// media::PIXEL_FORMAT_NV12 a.k.a. "420v"
constexpr OSType kPixelFormatNv12 =
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
// media::PIXEL_FORMAT_UYVY a.k.a. "2vuy"
constexpr OSType kPixelFormatUyvy = kCVPixelFormatType_422YpCbCr8;
// media::PIXEL_FORMAT_YUY2 a.k.a. "yuvs"
constexpr OSType kPixelFormatYuvs = kCVPixelFormatType_422YpCbCr8_yuvs;
// media::PIXEL_FORMAT_I420 a.k.a. "y420"
constexpr OSType kPixelFormatI420 = kCVPixelFormatType_420YpCbCr8Planar;

}  // namespace

TEST(PixelBufferTransfererTest, CanCopyYuvsAndVerifyColor) {
  constexpr OSType kPixelFormat = kPixelFormatYuvs;
  constexpr int kWidth = 32;
  constexpr int kHeight = 32;
  PixelBufferTransferer transferer;
  // Source: A single colored buffer.
  std::unique_ptr<ByteArrayPixelBuffer> source =
      CreateYuvsPixelBufferFromSingleRgbColor(kWidth, kHeight, kColorR, kColorG,
                                              kColorB);
  // Destination buffer: A same-sized YUVS buffer.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormat, kWidth, kHeight, 1)->CreateBuffer();
  EXPECT_TRUE(
      transferer.TransferImage(source->pixel_buffer.get(), destination.get()));
  // Verify the result is the same color.
  EXPECT_TRUE(YuvsIOSurfaceIsSingleColor(
      CVPixelBufferGetIOSurface(destination.get()), kColorR, kColorG, kColorB));
}

#if defined(ARCH_CPU_ARM64)
// Bulk-disabled as part of arm64 bot stabilization: https://crbug.com/1154345
#define MAYBE_CanScaleYuvsAndVerifyColor DISABLED_CanScaleYuvsAndVerifyColor
#else
#define MAYBE_CanScaleYuvsAndVerifyColor CanScaleYuvsAndVerifyColor
#endif

TEST(PixelBufferTransfererTest, MAYBE_CanScaleYuvsAndVerifyColor) {
  constexpr OSType kPixelFormat = kPixelFormatYuvs;
  constexpr int kSourceWidth = 32;
  constexpr int kSourceHeight = 32;
  constexpr int kDestinationWidth = 16;
  constexpr int kDestinationHeight = 16;
  PixelBufferTransferer transferer;
  // Source: A single colored buffer.
  std::unique_ptr<ByteArrayPixelBuffer> source =
      CreateYuvsPixelBufferFromSingleRgbColor(kSourceWidth, kSourceHeight,
                                              kColorR, kColorG, kColorB);
  // Destination buffer: A downscaled YUVS buffer.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormat, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(
      transferer.TransferImage(source->pixel_buffer.get(), destination.get()));
  // Verify the result is the same color.
  EXPECT_TRUE(YuvsIOSurfaceIsSingleColor(
      CVPixelBufferGetIOSurface(destination.get()), kColorR, kColorG, kColorB));
}

TEST(PixelBufferTransfererTest, CanScaleYuvsAndVerifyCheckerPattern) {
  // Note: The ARGB -> YUVS -> ARGB conversions results in a small loss of
  // information, so for the checker pattern to be intact the buffer can't be
  // tiny (e.g. 4x4).
  constexpr int kSourceWidth = 64;
  constexpr int kSourceHeight = 64;
  constexpr int kSourceNumTilesAcross = 4;
  constexpr int kDestinationWidth = 32;
  constexpr int kDestinationHeight = 32;
  PixelBufferTransferer transferer;
  // Source: A single colored buffer.
  std::unique_ptr<ByteArrayPixelBuffer> source =
      CreateYuvsPixelBufferFromArgbBuffer(
          kSourceWidth, kSourceHeight,
          CreateArgbCheckerPatternBuffer(kSourceWidth, kSourceHeight,
                                         kSourceNumTilesAcross));
  // Destination buffer: A downscaled YUVS buffer.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormatYuvs, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(
      transferer.TransferImage(source->pixel_buffer.get(), destination.get()));
  // Verify the result has the same number of checker tiles.
  auto [num_tiles_across_x, num_tiles_across_y] =
      GetCheckerPatternNumTilesAccross(
          CreateArgbBufferFromYuvsIOSurface(
              CVPixelBufferGetIOSurface(destination.get())),
          kDestinationWidth, kDestinationHeight);
  EXPECT_EQ(num_tiles_across_x, kSourceNumTilesAcross);
  EXPECT_EQ(num_tiles_across_y, kSourceNumTilesAcross);
}

#if defined(ARCH_CPU_ARM64)
// Bulk-disabled as part of arm64 bot stabilization: https://crbug.com/1154345
#define MAYBE_CanStretchYuvsAndVerifyCheckerPattern \
  DISABLED_CanStretchYuvsAndVerifyCheckerPattern
#else
#define MAYBE_CanStretchYuvsAndVerifyCheckerPattern \
  CanStretchYuvsAndVerifyCheckerPattern
#endif

TEST(PixelBufferTransfererTest, MAYBE_CanStretchYuvsAndVerifyCheckerPattern) {
  // Note: The ARGB -> YUVS -> ARGB conversions results in a small loss of
  // information, so for the checker pattern to be intact the buffer can't be
  // tiny (e.g. 4x4).
  constexpr int kSourceWidth = 64;
  constexpr int kSourceHeight = 64;
  constexpr int kSourceNumTilesAcross = 4;
  constexpr int kDestinationWidth = 48;
  constexpr int kDestinationHeight = 32;
  PixelBufferTransferer transferer;
  // Source: A single colored buffer.
  std::unique_ptr<ByteArrayPixelBuffer> source =
      CreateYuvsPixelBufferFromArgbBuffer(
          kSourceWidth, kSourceHeight,
          CreateArgbCheckerPatternBuffer(kSourceWidth, kSourceHeight,
                                         kSourceNumTilesAcross));
  // Destination buffer: A downscaled YUVS buffer.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormatYuvs, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(
      transferer.TransferImage(source->pixel_buffer.get(), destination.get()));
  // Verify the result has the same number of checker tiles.
  auto [num_tiles_across_x, num_tiles_across_y] =
      GetCheckerPatternNumTilesAccross(
          CreateArgbBufferFromYuvsIOSurface(
              CVPixelBufferGetIOSurface(destination.get())),
          kDestinationWidth, kDestinationHeight);
  EXPECT_EQ(num_tiles_across_x, kSourceNumTilesAcross);
  EXPECT_EQ(num_tiles_across_y, kSourceNumTilesAcross);
}

#if defined(ARCH_CPU_ARM64)
// Bulk-disabled as part of arm64 bot stabilization: https://crbug.com/1154345
#define MAYBE_CanStretchYuvsAndVerifyColor DISABLED_CanStretchYuvsAndVerifyColor
#else
#define MAYBE_CanStretchYuvsAndVerifyColor CanStretchYuvsAndVerifyColor
#endif

TEST(PixelBufferTransfererTest, MAYBE_CanStretchYuvsAndVerifyColor) {
  constexpr OSType kPixelFormat = kPixelFormatYuvs;
  constexpr int kSourceWidth = 32;
  constexpr int kSourceHeight = 32;
  constexpr int kDestinationWidth = 48;  // Aspect ratio does not match source.
  constexpr int kDestinationHeight = 16;
  PixelBufferTransferer transferer;
  // Source: A single colored buffer.
  std::unique_ptr<ByteArrayPixelBuffer> source =
      CreateYuvsPixelBufferFromSingleRgbColor(kSourceWidth, kSourceHeight,
                                              kColorR, kColorG, kColorB);
  // Destination buffer: A stretched YUVS buffer.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormat, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(
      transferer.TransferImage(source->pixel_buffer.get(), destination.get()));
  // Verify the result is the same color.
  EXPECT_TRUE(YuvsIOSurfaceIsSingleColor(
      CVPixelBufferGetIOSurface(destination.get()), kColorR, kColorG, kColorB));
}

TEST(PixelBufferTransfererTest, CanConvertAndStretchSimultaneouslyYuvsToNv12) {
  // Source pixel format: YUVS
  constexpr int kSourceWidth = 32;
  constexpr int kSourceHeight = 32;
  constexpr OSType kDestinationPixelFormat = kPixelFormatNv12;
  constexpr int kDestinationWidth = 48;  // Aspect ratio does not match source.
  constexpr int kDestinationHeight = 16;
  PixelBufferTransferer transferer;
  // Source: A single colored buffer.
  std::unique_ptr<ByteArrayPixelBuffer> source =
      CreateYuvsPixelBufferFromSingleRgbColor(kSourceWidth, kSourceHeight,
                                              kColorR, kColorG, kColorB);
  // Destination buffer: A stretched NV12 buffer.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kDestinationPixelFormat, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(
      transferer.TransferImage(source->pixel_buffer.get(), destination.get()));
}

class PixelBufferTransfererParameterizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<OSType, OSType>> {};

// We do not have the testing utils necessary to create and verify pixel buffers
// in other formats than YUVS, so in order to test the full conversion matrix of
// all supported formats X -> Y, this parameterized test performs:
// YUVS -> X -> Y -> YUVS
TEST_P(PixelBufferTransfererParameterizedTest,
       CanConvertFromXToYAndVerifyColor) {
  auto [pixel_format_from, pixel_format_to] = GetParam();
  LOG(INFO) << "Running Test: " << MacFourCCToString(pixel_format_from)
            << " -> " << MacFourCCToString(pixel_format_to);

  constexpr int kWidth = 32;
  constexpr int kHeight = 32;
  PixelBufferTransferer transferer;
  // We always start with YUVS because this is the format that the testing
  // utilities can convert to/from RGB.
  std::unique_ptr<ByteArrayPixelBuffer> original_yuvs_buffer =
      CreateYuvsPixelBufferFromSingleRgbColor(kWidth, kHeight, kColorR, kColorG,
                                              kColorB);
  // YUVS -> pixel_format_from
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer_from;
  if (pixel_format_from == kPixelFormatYuvs) {
    pixel_buffer_from = original_yuvs_buffer->pixel_buffer;
  } else {
    pixel_buffer_from =
        PixelBufferPool::Create(pixel_format_from, kWidth, kHeight, 1)
            ->CreateBuffer();
    transferer.TransferImage(original_yuvs_buffer->pixel_buffer.get(),
                             pixel_buffer_from.get());
  }
  ASSERT_TRUE(pixel_buffer_from);

  // pixel_format_from -> pixel_format_to
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer_to =
      PixelBufferPool::Create(pixel_format_to, kWidth, kHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(
      transferer.TransferImage(pixel_buffer_from.get(), pixel_buffer_to.get()));

  // We always convert back to YUVS because this is the only format that the
  // testing utilities can convert to/from RGB.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> final_yuvs_buffer;
  // pixel_format_to -> YUVS
  if (pixel_format_to == kPixelFormatYuvs) {
    final_yuvs_buffer = pixel_buffer_to;
  } else {
    final_yuvs_buffer =
        PixelBufferPool::Create(kPixelFormatYuvs, kWidth, kHeight, 1)
            ->CreateBuffer();
    transferer.TransferImage(pixel_buffer_to.get(), final_yuvs_buffer.get());
  }
  ASSERT_TRUE(final_yuvs_buffer);
  // Verify that after our "conversion dance" we end up with the same color that
  // we started with.
  EXPECT_TRUE(YuvsIOSurfaceIsSingleColor(
      CVPixelBufferGetIOSurface(final_yuvs_buffer.get()), kColorR, kColorG,
      kColorB));
}

INSTANTIATE_TEST_SUITE_P(
    PixelBufferTransfererTest,
    PixelBufferTransfererParameterizedTest,
    ::testing::Combine(::testing::Values(kPixelFormatNv12,
                                         kPixelFormatUyvy,
                                         kPixelFormatYuvs,
                                         kPixelFormatI420),
                       ::testing::Values(kPixelFormatNv12,
                                         kPixelFormatUyvy,
                                         kPixelFormatYuvs,
                                         kPixelFormatI420)));

}  // namespace media
