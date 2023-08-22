// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_TEST_PIXEL_BUFFER_TEST_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_TEST_PIXEL_BUFFER_TEST_UTILS_H_

#include <memory>
#include <tuple>
#include <vector>

#import <CoreVideo/CoreVideo.h>
#import <IOSurface/IOSurfaceRef.h>

#include "base/apple/scoped_cftyperef.h"

namespace media {

struct ByteArrayPixelBuffer {
  ByteArrayPixelBuffer();
  ~ByteArrayPixelBuffer();

  std::vector<uint8_t> byte_array;
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;
};

// All pixels of the resulting buffer have the specified RGB. Alpha is 255.
std::vector<uint8_t> CreateArgbBufferFromSingleRgbColor(int width,
                                                        int height,
                                                        uint8_t r,
                                                        uint8_t g,
                                                        uint8_t b);

// True if the RGB color is closer to white (255,255,255) than black (0,0,0).
bool IsArgbPixelWhite(const std::vector<uint8_t>& argb_buffer, size_t i);

// True if all pixels are the specified RGB color, within some margin of error.
bool ArgbBufferIsSingleColor(const std::vector<uint8_t>& argb_buffer,
                             uint8_t r,
                             uint8_t g,
                             uint8_t b);

// Creates a checker pattern with tiles being white (255,255,255) or dark gray
// (100,100,100). Alpha is 255. The top-left corner is white.
std::vector<uint8_t> CreateArgbCheckerPatternBuffer(int width,
                                                    int height,
                                                    int num_tiles_across);

// Traverse the top and left border pixels, counting the number of tiles if this
// is a checker pattern. (Each pixel is characterized as white or non-white and
// this function counts the number of transitions between these colors.)
std::tuple<int, int> GetCheckerPatternNumTilesAccross(
    const std::vector<uint8_t>& argb_buffer,
    int width,
    int height);

// Creates a CVPixelBuffer that is backed by an in-memory byte array.
std::unique_ptr<ByteArrayPixelBuffer> CreateYuvsPixelBufferFromArgbBuffer(
    int width,
    int height,
    const std::vector<uint8_t>& argb_buffer);
std::unique_ptr<ByteArrayPixelBuffer> CreateYuvsPixelBufferFromSingleRgbColor(
    int width,
    int height,
    uint8_t r,
    uint8_t g,
    uint8_t b);

// YUVS IOSurface -> ARGB byte array.
std::vector<uint8_t> CreateArgbBufferFromYuvsIOSurface(IOSurfaceRef io_surface);

// True if all pixels of the YUVS IOSurface are the specified RGB color, within
// some margin of error.
bool YuvsIOSurfaceIsSingleColor(IOSurfaceRef io_surface,
                                uint8_t r,
                                uint8_t g,
                                uint8_t b);

// True if all pixels of the pixel buffer are the specified RGB color, within
// some margin of error.
bool PixelBufferIsSingleColor(CVPixelBufferRef pixel_buffer,
                              uint8_t r,
                              uint8_t g,
                              uint8_t b);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_APPLE_TEST_PIXEL_BUFFER_TEST_UTILS_H_
