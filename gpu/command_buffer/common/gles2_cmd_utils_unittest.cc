// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/gles2_cmd_utils.h"

#include <limits>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class GLES2UtilTest : public testing:: Test {
 protected:
  GLES2Util util_;
};

TEST_F(GLES2UtilTest, GLGetNumValuesReturned) {
  EXPECT_EQ(0, util_.GLGetNumValuesReturned(GL_COMPRESSED_TEXTURE_FORMATS));
  EXPECT_EQ(0, util_.GLGetNumValuesReturned(GL_SHADER_BINARY_FORMATS));

  EXPECT_EQ(0, util_.num_compressed_texture_formats());
  EXPECT_EQ(0, util_.num_shader_binary_formats());

  util_.set_num_compressed_texture_formats(1);
  util_.set_num_shader_binary_formats(2);

  EXPECT_EQ(1, util_.GLGetNumValuesReturned(GL_COMPRESSED_TEXTURE_FORMATS));
  EXPECT_EQ(2, util_.GLGetNumValuesReturned(GL_SHADER_BINARY_FORMATS));

  EXPECT_EQ(1, util_.num_compressed_texture_formats());
  EXPECT_EQ(2, util_.num_shader_binary_formats());
}

TEST_F(GLES2UtilTest, ComputeImageDataSizesFormats) {
  const uint32_t kWidth = 16;
  const uint32_t kHeight = 12;
  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGB, GL_UNSIGNED_BYTE, 1,
      &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 3, size);
  EXPECT_EQ(kWidth * 3, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 1, size);
  EXPECT_EQ(kWidth * 1, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_ALPHA, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 1, size);
  EXPECT_EQ(kWidth * 1, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, 1,
      &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, 1,
      &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 3, size);
  EXPECT_EQ(kWidth * 3, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RG, GL_UNSIGNED_BYTE, 1,
      &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RG_INTEGER, GL_UNSIGNED_BYTE, 1,
      &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RED, GL_UNSIGNED_BYTE, 1,
      &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 1, size);
  EXPECT_EQ(kWidth * 1, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1,
      &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 1, size);
  EXPECT_EQ(kWidth * 1, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
}

TEST_F(GLES2UtilTest, ComputeImageDataSizeTypes) {
  const uint32_t kWidth = 16;
  const uint32_t kHeight = 12;
  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGBA, GL_UNSIGNED_INT_5_9_9_9_REV, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
      1, &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 8, size);
  EXPECT_EQ(kWidth * 8, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGBA, GL_HALF_FLOAT,
      1, &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 8, size);
  EXPECT_EQ(kWidth * 8, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGBA, GL_HALF_FLOAT_OES,
      1, &size, &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 8, size);
  EXPECT_EQ(kWidth * 8, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
}

TEST_F(GLES2UtilTest, ComputeImageDataSizesUnpackAlignment) {
  const uint32_t kWidth = 19;
  const uint32_t kHeight = 12;
  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGB, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3, padded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGB, GL_UNSIGNED_BYTE, 2, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 1) * (kHeight - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 1, padded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGB, GL_UNSIGNED_BYTE, 4, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 3) * (kHeight - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 3, padded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, GL_RGB, GL_UNSIGNED_BYTE, 8, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 7) * (kHeight - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 7, padded_row_size);
}

TEST_F(GLES2UtilTest, ComputeImageDataSizeDepth) {
  const uint32_t kWidth = 19;
  const uint32_t kHeight = 12;
  const uint32_t kDepth = 3;
  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * kDepth * 3, size);
  EXPECT_EQ(kWidth * 3, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, 2, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 1) * (kHeight * kDepth - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 1, padded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, 4, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 3) * (kHeight * kDepth - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 3, padded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, 8, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 7) * (kHeight * kDepth - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 7, padded_row_size);
}

TEST_F(GLES2UtilTest, ComputeImageDataSizePixelStoreParams) {
  const uint32_t kWidth = 3;
  const uint32_t kHeight = 3;
  const uint32_t kDepth = 3;
  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  uint32_t skip_size;
  uint32_t padding;

  {  // Default
    PixelStoreParams params;
    EXPECT_TRUE(GLES2Util::ComputeImageDataSizesES3(
        kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, params,
        &size, &unpadded_row_size, &padded_row_size, &skip_size, &padding));
    EXPECT_EQ(kWidth * 3, unpadded_row_size);
    EXPECT_EQ(kWidth * 3 + 3, padded_row_size);
    EXPECT_EQ(padded_row_size * (kHeight * kDepth - 1) + unpadded_row_size,
              size);
    EXPECT_EQ(0u, skip_size);
    EXPECT_EQ(3u, padding);
  }

  {  // row_length > width
    PixelStoreParams params;
    params.row_length = kWidth + 2;
    uint32_t kPadding = 1;  // 5 * 3 = 15 -> 16
    EXPECT_TRUE(GLES2Util::ComputeImageDataSizesES3(
        kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, params,
        &size, &unpadded_row_size, &padded_row_size, &skip_size, &padding));
    EXPECT_EQ(static_cast<uint32_t>(kWidth * 3), unpadded_row_size);
    EXPECT_EQ(static_cast<uint32_t>(params.row_length * 3 + kPadding),
              padded_row_size);
    EXPECT_EQ(padded_row_size * (kHeight * kDepth - 1) + unpadded_row_size,
              size);
    EXPECT_EQ(0u, skip_size);
    EXPECT_EQ(kPadding, padding);
  }

  {  // row_length < width
    PixelStoreParams params;
    params.row_length = kWidth - 1;
    uint32_t kPadding = 2;  // 2 * 3 = 6 -> 8
    EXPECT_TRUE(GLES2Util::ComputeImageDataSizesES3(
        kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, params,
        &size, &unpadded_row_size, &padded_row_size, &skip_size, &padding));
    EXPECT_EQ(static_cast<uint32_t>(kWidth * 3), unpadded_row_size);
    EXPECT_EQ(static_cast<uint32_t>(params.row_length * 3 + kPadding),
              padded_row_size);
    EXPECT_EQ(padded_row_size * (kHeight * kDepth - 1) + unpadded_row_size,
              size);
    EXPECT_EQ(0u, skip_size);
    EXPECT_EQ(kPadding, padding);
  }

  {  // image_height > height
    PixelStoreParams params;
    params.image_height = kHeight + 1;
    uint32_t kPadding = 3; // 3 * 3 = 9 -> 21
    EXPECT_TRUE(GLES2Util::ComputeImageDataSizesES3(
        kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, params,
        &size, &unpadded_row_size, &padded_row_size, &skip_size, &padding));
    EXPECT_EQ(kWidth * 3, unpadded_row_size);
    EXPECT_EQ(kWidth * 3 + kPadding, padded_row_size);
    EXPECT_EQ((params.image_height * (kDepth - 1) + kHeight - 1) *
              padded_row_size + unpadded_row_size, size);
    EXPECT_EQ(0u, skip_size);
    EXPECT_EQ(kPadding, padding);
  }

  {  // image_height < height
    PixelStoreParams params;
    params.image_height = kHeight - 1;
    uint32_t kPadding = 3; // 3 * 3 = 9 -> 12
    EXPECT_TRUE(GLES2Util::ComputeImageDataSizesES3(
        kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, params,
        &size, &unpadded_row_size, &padded_row_size, &skip_size, &padding));
    EXPECT_EQ(kWidth * 3, unpadded_row_size);
    EXPECT_EQ(kWidth * 3 + kPadding, padded_row_size);
    EXPECT_EQ((params.image_height * (kDepth - 1) + kHeight - 1) *
              padded_row_size + unpadded_row_size, size);
    EXPECT_EQ(0u, skip_size);
    EXPECT_EQ(kPadding, padding);
  }

  {  // skip_pixels, skip_rows, skip_images, alignment = 4, RGB
    PixelStoreParams params;
    params.skip_pixels = 1;
    params.skip_rows = 10;
    params.skip_images = 2;
    uint32_t kPadding = 3; // 3 * 3 = 9 -> 12
    EXPECT_TRUE(GLES2Util::ComputeImageDataSizesES3(
        kWidth, kHeight, kDepth, GL_RGB, GL_UNSIGNED_BYTE, params,
        &size, &unpadded_row_size, &padded_row_size, &skip_size, &padding));
    EXPECT_EQ(kWidth * 3, unpadded_row_size);
    EXPECT_EQ(kWidth * 3 + kPadding, padded_row_size);
    EXPECT_EQ(padded_row_size * kHeight * params.skip_images +
              padded_row_size * params.skip_rows + 3 * params.skip_pixels,
              skip_size);
    EXPECT_EQ(padded_row_size * (kWidth * kDepth - 1) + unpadded_row_size,
              size);
    EXPECT_EQ(kPadding, padding);
  }

  {  // skip_pixels, skip_rows, skip_images, alignment = 8, RGBA
    PixelStoreParams params;
    params.skip_pixels = 1;
    params.skip_rows = 10;
    params.skip_images = 2;
    params.alignment = 8;
    uint32_t kPadding = 4; // 3 * 4 = 12 -> 16
    EXPECT_TRUE(GLES2Util::ComputeImageDataSizesES3(
        kWidth, kHeight, kDepth, GL_RGBA, GL_UNSIGNED_BYTE, params,
        &size, &unpadded_row_size, &padded_row_size, &skip_size, &padding));
    EXPECT_EQ(kWidth * 4, unpadded_row_size);
    EXPECT_EQ(kWidth * 4 + kPadding, padded_row_size);
    EXPECT_EQ(padded_row_size * kHeight * params.skip_images +
              padded_row_size * params.skip_rows + 4 * params.skip_pixels,
              skip_size);
    EXPECT_EQ(padded_row_size * (kWidth * kDepth - 1) + unpadded_row_size,
              size);
    EXPECT_EQ(kPadding, padding);
  }
}

TEST_F(GLES2UtilTest, RenderbufferBytesPerPixel) {
   EXPECT_EQ(1u, GLES2Util::RenderbufferBytesPerPixel(GL_STENCIL_INDEX8));
   EXPECT_EQ(2u, GLES2Util::RenderbufferBytesPerPixel(GL_RGBA4));
   EXPECT_EQ(2u, GLES2Util::RenderbufferBytesPerPixel(GL_RGB565));
   EXPECT_EQ(2u, GLES2Util::RenderbufferBytesPerPixel(GL_RGB5_A1));
   EXPECT_EQ(2u, GLES2Util::RenderbufferBytesPerPixel(GL_DEPTH_COMPONENT16));
   EXPECT_EQ(4u, GLES2Util::RenderbufferBytesPerPixel(GL_RGB));
   EXPECT_EQ(4u, GLES2Util::RenderbufferBytesPerPixel(GL_RGBA));
   EXPECT_EQ(
       4u, GLES2Util::RenderbufferBytesPerPixel(GL_DEPTH24_STENCIL8_OES));
   EXPECT_EQ(4u, GLES2Util::RenderbufferBytesPerPixel(GL_RGB8_OES));
   EXPECT_EQ(4u, GLES2Util::RenderbufferBytesPerPixel(GL_RGBA8_OES));
   EXPECT_EQ(
       4u, GLES2Util::RenderbufferBytesPerPixel(GL_DEPTH_COMPONENT24_OES));
   EXPECT_EQ(0u, GLES2Util::RenderbufferBytesPerPixel(-1));
}

TEST_F(GLES2UtilTest, GetChannelsForCompressedFormat) {
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(GL_ETC1_RGB8_OES));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(GL_ATC_RGB_AMD));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_ATC_RGBA_EXPLICIT_ALPHA_AMD));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG));
  EXPECT_EQ(0u, GLES2Util::GetChannelsForFormat(
      GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG));
}

TEST_F(GLES2UtilTest, GLSLArrayNameParsingNotArray) {
  const char* kNotArrayNames[] = {
      "u_name",     "u_name[]",    "u_name]", "u_name[0a]",
      "u_name[a0]", "u_name[0a0]", "[3]",     ""};
  for (auto* name : kNotArrayNames) {
    GLSLArrayName parsed_name(name);
    EXPECT_FALSE(parsed_name.IsArrayName());
  }
}

TEST_F(GLES2UtilTest, GLSLArrayNameParsing) {
  struct {
    const char* name;
    const char* base_name;
    int element_index;
  } testcases[] = {{"u_name[0]", "u_name", 0},
                   {"u_name[2]", "u_name", 2},
                   {
                       "u_name[02]", "u_name", 2,
                   },
                   {
                       "u_name[20]", "u_name", 20,
                   },
                   {"u_name[020]", "u_name", 20},
                   {"u_name[0][0]", "u_name[0]", 0},
                   {"u_name[3][2]", "u_name[3]", 2},
                   {"u_name[03][02]", "u_name[03]", 2},
                   {"u_name[30][20]", "u_name[30]", 20},
                   {"u_name[030][020]", "u_name[030]", 20}};
  for (auto& testcase : testcases) {
    GLSLArrayName parsed_name(testcase.name);
    EXPECT_TRUE(parsed_name.IsArrayName());
    if (!parsed_name.IsArrayName()) {
      continue;
    }
    EXPECT_EQ(testcase.base_name, parsed_name.base_name());
    EXPECT_EQ(testcase.element_index, parsed_name.element_index());
  }
}

}  // namespace gles2
}  // namespace gpu
