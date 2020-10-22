// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/pixel_buffer_transferer_mac.h"

#include <cmath>
#include <vector>

#include "base/logging.h"
#include "media/capture/video/mac/pixel_buffer_pool_mac.h"
#include "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"

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

// ARGB has 4 bytes per pixel and no padding.
size_t GetArgbStride(size_t width) {
  return width * 4;
}
// YUVS is a 4:2:2 pixel format that is packed to be 2 bytes per pixel.
// https://gstreamer.freedesktop.org/documentation/additional/design/mediatype-video-raw.html?gi-language=c
size_t GetYuvsStride(size_t width) {
  return width * 2;
}

std::vector<uint8_t> CreateArgbBufferFromSingleRgbColor(int width,
                                                        int height,
                                                        uint8_t r,
                                                        uint8_t g,
                                                        uint8_t b) {
  std::vector<uint8_t> argb_buffer;
  argb_buffer.resize(GetArgbStride(width) * height);
  for (size_t i = 0; i < argb_buffer.size(); i += 4) {
    // ARGB little endian = BGRA in memory.
    argb_buffer[i + 0] = b;
    argb_buffer[i + 1] = g;
    argb_buffer[i + 2] = r;
    argb_buffer[i + 3] = 255u;
  }
  return argb_buffer;
}

bool IsArgbPixelWhite(const std::vector<uint8_t>& argb_buffer, size_t i) {
  // ARGB little endian = BGRA in memory.
  uint8_t b = argb_buffer[i + 0];
  uint8_t g = argb_buffer[i + 1];
  uint8_t r = argb_buffer[i + 2];
  return (r + g + b) / 3 >= 255 / 2;
}

bool ArgbBufferIsSingleColor(const std::vector<uint8_t>& argb_buffer,
                             uint8_t r,
                             uint8_t g,
                             uint8_t b) {
  int signed_r = r;
  int signed_g = g;
  int signed_b = b;
  // ~5% error tolerance.
  constexpr int kErrorTolerance = 0.05 * 255;
  for (size_t i = 0; i < argb_buffer.size(); i += 4) {
    // ARGB little endian = BGRA in memory.
    int pixel_b = argb_buffer[i + 0];
    int pixel_g = argb_buffer[i + 1];
    int pixel_r = argb_buffer[i + 2];
    if (std::abs(pixel_r - signed_r) > kErrorTolerance ||
        std::abs(pixel_g - signed_g) > kErrorTolerance ||
        std::abs(pixel_b - signed_b) > kErrorTolerance) {
      return false;
    }
  }
  return true;
}

std::vector<uint8_t> CreateArgbCheckerPatternBuffer(int width,
                                                    int height,
                                                    int num_tiles_across) {
  std::vector<uint8_t> argb_buffer;
  int tile_width = width / num_tiles_across;
  int tile_height = height / num_tiles_across;
  argb_buffer.resize(GetArgbStride(width) * height);
  for (size_t i = 0; i < argb_buffer.size(); i += 4) {
    size_t pixel_number = i / 4;
    size_t x = pixel_number % width;
    size_t y = pixel_number / width;
    bool is_white = ((x / tile_width) % 2 != 0) != ((y / tile_height) % 2 != 0);
    // ARGB little endian = BGRA in memory.
    argb_buffer[i + 0] = argb_buffer[i + 1] = argb_buffer[i + 2] =
        is_white ? 255u : 100u;
    argb_buffer[i + 3] = 255u;
  }
  return argb_buffer;
}

std::tuple<int, int> GetCheckerPatternNumTilesAccross(
    const std::vector<uint8_t>& argb_buffer,
    int width,
    int height) {
  int num_tiles_across_x = 0;
  bool prev_tile_is_white = false;
  for (int x = 0; x < width; ++x) {
    size_t i = (x * 4);
    bool current_tile_is_white = IsArgbPixelWhite(argb_buffer, i);
    if (x == 0 || prev_tile_is_white != current_tile_is_white) {
      prev_tile_is_white = current_tile_is_white;
      ++num_tiles_across_x;
    }
  }
  int num_tiles_across_y = 0;
  prev_tile_is_white = false;
  for (int y = 0; y < height; ++y) {
    size_t i = y * GetArgbStride(width);
    bool current_tile_is_white = IsArgbPixelWhite(argb_buffer, i);
    if (y == 0 || prev_tile_is_white != current_tile_is_white) {
      prev_tile_is_white = current_tile_is_white;
      ++num_tiles_across_y;
    }
  }
  return std::make_pair(num_tiles_across_x, num_tiles_across_y);
}

struct ByteArrayPixelBuffer {
  std::vector<uint8_t> byte_array;
  base::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;
};

std::unique_ptr<ByteArrayPixelBuffer> CreateYuvsPixelBufferFromArgbBuffer(
    int width,
    int height,
    const std::vector<uint8_t>& argb_buffer) {
  std::unique_ptr<ByteArrayPixelBuffer> result =
      std::make_unique<ByteArrayPixelBuffer>();
  size_t yuvs_stride = GetYuvsStride(width);

  // ARGB -> YUVS (a.k.a. YUY2).
  result->byte_array.resize(yuvs_stride * height);
  libyuv::ARGBToYUY2(&argb_buffer[0], GetArgbStride(width),
                     &result->byte_array[0], yuvs_stride, width, height);

  CVReturn error = CVPixelBufferCreateWithBytes(
      nil, width, height, kPixelFormatYuvs, (void*)&result->byte_array[0],
      yuvs_stride, nil, nil, nil, result->pixel_buffer.InitializeInto());
  CHECK(error == noErr);
  return result;
}

std::unique_ptr<ByteArrayPixelBuffer> CreateYuvsPixelBufferFromSingleRgbColor(
    int width,
    int height,
    uint8_t r,
    uint8_t g,
    uint8_t b) {
  return CreateYuvsPixelBufferFromArgbBuffer(
      width, height,
      CreateArgbBufferFromSingleRgbColor(width, height, r, g, b));
}

std::vector<uint8_t> CreateArgbBufferFromYuvsIOSurface(
    IOSurfaceRef io_surface) {
  DCHECK(io_surface);
  size_t width = IOSurfaceGetWidth(io_surface);
  size_t height = IOSurfaceGetHeight(io_surface);
  size_t argb_stride = GetArgbStride(width);
  size_t yuvs_stride = GetYuvsStride(width);
  uint8_t* pixels = static_cast<uint8_t*>(IOSurfaceGetBaseAddress(io_surface));
  DCHECK(pixels);
  std::vector<uint8_t> argb_buffer;
  argb_buffer.resize(argb_stride * height);
  libyuv::YUY2ToARGB(pixels, yuvs_stride, &argb_buffer[0], argb_stride, width,
                     height);
  return argb_buffer;
}

bool YuvsIOSurfaceIsSingleColor(IOSurfaceRef io_surface,
                                uint8_t r,
                                uint8_t g,
                                uint8_t b) {
  return ArgbBufferIsSingleColor(CreateArgbBufferFromYuvsIOSurface(io_surface),
                                 r, g, b);
}

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
  base::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormat, kWidth, kHeight, 1)->CreateBuffer();
  EXPECT_TRUE(transferer.TransferImage(source->pixel_buffer, destination));
  // Verify the result is the same color.
  EXPECT_TRUE(YuvsIOSurfaceIsSingleColor(CVPixelBufferGetIOSurface(destination),
                                         kColorR, kColorG, kColorB));
}

TEST(PixelBufferTransfererTest, CanScaleYuvsAndVerifyColor) {
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
  base::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormat, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(transferer.TransferImage(source->pixel_buffer, destination));
  // Verify the result is the same color.
  EXPECT_TRUE(YuvsIOSurfaceIsSingleColor(CVPixelBufferGetIOSurface(destination),
                                         kColorR, kColorG, kColorB));
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
  base::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormatYuvs, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(transferer.TransferImage(source->pixel_buffer, destination));
  // Verify the result has the same number of checker tiles.
  int num_tiles_across_x;
  int num_tiles_across_y;
  std::tie(num_tiles_across_x, num_tiles_across_y) =
      GetCheckerPatternNumTilesAccross(
          CreateArgbBufferFromYuvsIOSurface(
              CVPixelBufferGetIOSurface(destination)),
          kDestinationWidth, kDestinationHeight);
  EXPECT_EQ(num_tiles_across_x, kSourceNumTilesAcross);
  EXPECT_EQ(num_tiles_across_y, kSourceNumTilesAcross);
}

TEST(PixelBufferTransfererTest, CanStretchYuvsAndVerifyCheckerPattern) {
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
  base::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormatYuvs, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(transferer.TransferImage(source->pixel_buffer, destination));
  // Verify the result has the same number of checker tiles.
  int num_tiles_across_x;
  int num_tiles_across_y;
  std::tie(num_tiles_across_x, num_tiles_across_y) =
      GetCheckerPatternNumTilesAccross(
          CreateArgbBufferFromYuvsIOSurface(
              CVPixelBufferGetIOSurface(destination)),
          kDestinationWidth, kDestinationHeight);
  EXPECT_EQ(num_tiles_across_x, kSourceNumTilesAcross);
  EXPECT_EQ(num_tiles_across_y, kSourceNumTilesAcross);
}

TEST(PixelBufferTransfererTest, CanStretchYuvsAndVerifyColor) {
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
  // Destination buffer: A streched YUVS buffer.
  base::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kPixelFormat, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(transferer.TransferImage(source->pixel_buffer, destination));
  // Verify the result is the same color.
  EXPECT_TRUE(YuvsIOSurfaceIsSingleColor(CVPixelBufferGetIOSurface(destination),
                                         kColorR, kColorG, kColorB));
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
  // Destination buffer: A streched NV12 buffer.
  base::ScopedCFTypeRef<CVPixelBufferRef> destination =
      PixelBufferPool::Create(kDestinationPixelFormat, kDestinationWidth,
                              kDestinationHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(transferer.TransferImage(source->pixel_buffer, destination));
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
  OSType pixel_format_from;
  OSType pixel_format_to;
  std::tie(pixel_format_from, pixel_format_to) = GetParam();
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
  base::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer_from;
  if (pixel_format_from == kPixelFormatYuvs) {
    pixel_buffer_from = original_yuvs_buffer->pixel_buffer;
  } else {
    pixel_buffer_from =
        PixelBufferPool::Create(pixel_format_from, kWidth, kHeight, 1)
            ->CreateBuffer();
    transferer.TransferImage(original_yuvs_buffer->pixel_buffer,
                             pixel_buffer_from);
  }
  ASSERT_TRUE(pixel_buffer_from);

  // pixel_format_from -> pixel_format_to
  base::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer_to =
      PixelBufferPool::Create(pixel_format_to, kWidth, kHeight, 1)
          ->CreateBuffer();
  EXPECT_TRUE(transferer.TransferImage(pixel_buffer_from, pixel_buffer_to));

  // We always convert back to YUVS because this is the only format that the
  // testing utilities can convert to/from RGB.
  base::ScopedCFTypeRef<CVPixelBufferRef> final_yuvs_buffer;
  // pixel_format_to -> YUVS
  if (pixel_format_to == kPixelFormatYuvs) {
    final_yuvs_buffer = pixel_buffer_to;
  } else {
    final_yuvs_buffer =
        PixelBufferPool::Create(kPixelFormatYuvs, kWidth, kHeight, 1)
            ->CreateBuffer();
    transferer.TransferImage(pixel_buffer_to, final_yuvs_buffer);
  }
  ASSERT_TRUE(final_yuvs_buffer);
  // Verify that after our "conversion dance" we end up with the same color that
  // we started with.
  EXPECT_TRUE(YuvsIOSurfaceIsSingleColor(
      CVPixelBufferGetIOSurface(final_yuvs_buffer), kColorR, kColorG, kColorB));
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
