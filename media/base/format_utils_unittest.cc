// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/format_utils.h"

#include "components/viz/common/resources/shared_image_format.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(FormatUtilsTest, SharedImageFormatToVideoPixelFormat) {
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kNV12),
            PIXEL_FORMAT_NV12);
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kNV16),
            PIXEL_FORMAT_NV16);
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kNV24),
            PIXEL_FORMAT_NV24);
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kNV12A),
            PIXEL_FORMAT_NV12A);
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kP010),
            PIXEL_FORMAT_P010LE);
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kP210),
            PIXEL_FORMAT_P210LE);
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kP410),
            PIXEL_FORMAT_P410LE);
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kI420),
            PIXEL_FORMAT_I420);
  EXPECT_EQ(SharedImageFormatToVideoPixelFormat(viz::MultiPlaneFormat::kI420A),
            PIXEL_FORMAT_I420A);

  EXPECT_EQ(
      SharedImageFormatToVideoPixelFormat(viz::SinglePlaneFormat::kBGRA_8888),
      PIXEL_FORMAT_ARGB);
  EXPECT_EQ(
      SharedImageFormatToVideoPixelFormat(viz::SinglePlaneFormat::kRGBX_8888),
      PIXEL_FORMAT_XBGR);
  EXPECT_EQ(
      SharedImageFormatToVideoPixelFormat(viz::SinglePlaneFormat::kBGRX_8888),
      PIXEL_FORMAT_XRGB);
}

TEST(FormatUtilsTest, VideoPixelFormatToSharedImageFormat) {
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_NV12),
            viz::MultiPlaneFormat::kNV12);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_NV16),
            viz::MultiPlaneFormat::kNV16);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_NV24),
            viz::MultiPlaneFormat::kNV24);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_NV12A),
            viz::MultiPlaneFormat::kNV12A);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_P010LE),
            viz::MultiPlaneFormat::kP010);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_P210LE),
            viz::MultiPlaneFormat::kP210);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_P410LE),
            viz::MultiPlaneFormat::kP410);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_I420),
            viz::MultiPlaneFormat::kI420);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_I420A),
            viz::MultiPlaneFormat::kI420A);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_XRGB),
            viz::SinglePlaneFormat::kBGRX_8888);
  EXPECT_EQ(VideoPixelFormatToSharedImageFormat(PIXEL_FORMAT_XBGR),
            viz::SinglePlaneFormat::kRGBX_8888);
}

}  // namespace media
