// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/test/pixel_buffer_test_utils.h"

#include "media/capture/video/apple/pixel_buffer_pool.h"
#include "media/capture/video/apple/pixel_buffer_transferer.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"

namespace media {

namespace {

// media::PIXEL_FORMAT_YUY2 a.k.a. "yuvs"
constexpr OSType kPixelFormatYuvs = kCVPixelFormatType_422YpCbCr8_yuvs;

// ARGB has 4 bytes per pixel and no padding.
size_t GetArgbStride(size_t width) {
  return width * 4;
}

// YUVS is a 4:2:2 pixel format that is packed to be 2 bytes per pixel.
// https://gstreamer.freedesktop.org/documentation/additional/design/mediatype-video-raw.html?gi-language=c
size_t GetYuvsStride(size_t width) {
  return width * 2;
}

}  // namespace

ByteArrayPixelBuffer::ByteArrayPixelBuffer() {}

ByteArrayPixelBuffer::~ByteArrayPixelBuffer() {}

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
  // These utility methods don't work well with widths that aren't multiples of
  // 16. There could be assumptions about memory alignment, or there could
  // simply be a loss of information in the YUVS <-> ARGB conversions since YUVS
  // is packed. Either way, the pixels may change, so we avoid these widths.
  DCHECK(width % 16 == 0);
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

bool PixelBufferIsSingleColor(CVPixelBufferRef pixel_buffer,
                              uint8_t r,
                              uint8_t g,
                              uint8_t b) {
  OSType pixel_format = CVPixelBufferGetPixelFormatType(pixel_buffer);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> yuvs_pixel_buffer;
  if (pixel_format == kPixelFormatYuvs) {
    // The pixel buffer is already YUVS, so we know how to check its color.
    yuvs_pixel_buffer.reset(pixel_buffer, base::scoped_policy::RETAIN);
  } else {
    // Convert to YUVS. We only know how to check the color of YUVS.
    yuvs_pixel_buffer =
        PixelBufferPool::Create(kPixelFormatYuvs,
                                CVPixelBufferGetWidth(pixel_buffer),
                                CVPixelBufferGetHeight(pixel_buffer), 1)
            ->CreateBuffer();
    PixelBufferTransferer transferer;
    bool transfer_success =
        transferer.TransferImage(pixel_buffer, yuvs_pixel_buffer.get());
    DCHECK(transfer_success);
  }
  IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(yuvs_pixel_buffer.get());
  DCHECK(io_surface);
  return YuvsIOSurfaceIsSingleColor(io_surface, r, g, b);
}

}  // namespace media
