// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_device.h"

#include <cstring>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Composes a v4l2_format of type V4L2_BUF_TYPE_VIDEO_OUTPUT.
v4l2_format V4L2FormatVideoOutput(uint32_t width,
                                  uint32_t height,
                                  uint32_t pixelformat,
                                  uint32_t field,
                                  uint32_t bytesperline,
                                  uint32_t sizeimage) {
  v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  struct v4l2_pix_format* pix = &format.fmt.pix;
  pix->width = width;
  pix->height = height;
  pix->pixelformat = pixelformat;
  pix->field = field;
  pix->bytesperline = bytesperline;
  pix->sizeimage = sizeimage;
  return format;
}

// Composes a v4l2_format of type V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE.
// If anything goes wrong, it returns v4l2_format with
// pix_mp.pixelformat = 0.
v4l2_format V4L2FormatVideoOutputMplane(uint32_t width,
                                        uint32_t height,
                                        uint32_t pixelformat,
                                        uint32_t field,
                                        std::vector<uint32_t> bytesperlines,
                                        std::vector<uint32_t> sizeimages) {
  v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  struct v4l2_pix_format_mplane* pix_mp = &format.fmt.pix_mp;
  if (bytesperlines.size() != sizeimages.size() ||
      bytesperlines.size() > static_cast<size_t>(VIDEO_MAX_PLANES)) {
    // Used to emit test error message.
    EXPECT_EQ(bytesperlines.size(), sizeimages.size());
    EXPECT_LE(bytesperlines.size(), static_cast<size_t>(VIDEO_MAX_PLANES));
    return format;
  }
  pix_mp->width = width;
  pix_mp->height = height;
  pix_mp->pixelformat = pixelformat;
  pix_mp->field = field;
  pix_mp->num_planes = bytesperlines.size();
  for (size_t i = 0; i < pix_mp->num_planes; ++i) {
    pix_mp->plane_fmt[i].bytesperline = bytesperlines[i];
    pix_mp->plane_fmt[i].sizeimage = sizeimages[i];
  }
  return format;
}

}  // namespace

namespace media {

// Test V4L2FormatToVideoFrameLayout with NV12 pixelformat, which has one buffer
// and two color planes.
TEST(V4L2DeviceTest, V4L2FormatToVideoFrameLayoutNV12) {
  auto layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(V4L2FormatVideoOutputMplane(
          300, 180, V4L2_PIX_FMT_NV12, V4L2_FIELD_ANY, {320}, {86400}));
  ASSERT_TRUE(layout.has_value());
  EXPECT_EQ(PIXEL_FORMAT_NV12, layout->format());
  EXPECT_EQ(gfx::Size(300, 180), layout->coded_size());
  std::vector<VideoFrameLayout::Plane> expected_planes(
      {{320, 0u}, {320, 57600u}});
  EXPECT_EQ(expected_planes, layout->planes());
  EXPECT_EQ(std::vector<size_t>({86400u}), layout->buffer_sizes());
  EXPECT_EQ(86400u, layout->GetTotalBufferSize());
  EXPECT_EQ(
      "VideoFrameLayout format: PIXEL_FORMAT_NV12, coded_size: 300x180, "
      "num_buffers: 1, buffer_sizes: [86400], num_planes: 2, "
      "planes (stride, offset): [(320, 0), (320, 57600)]",
      layout->ToString());
}

// Test V4L2FormatToVideoFrameLayout with YUV420 pixelformat, which has one
// buffer and three color planes.
TEST(V4L2DeviceTest, V4L2FormatToVideoFrameLayoutYUV420) {
  auto layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(V4L2FormatVideoOutputMplane(
          300, 180, V4L2_PIX_FMT_YUV420, V4L2_FIELD_ANY, {320}, {86400}));
  ASSERT_TRUE(layout.has_value());
  EXPECT_EQ(PIXEL_FORMAT_I420, layout->format());
  EXPECT_EQ(gfx::Size(300, 180), layout->coded_size());
  std::vector<VideoFrameLayout::Plane> expected_planes(
      {{320, 0u}, {160, 57600u}, {160, 72000}});
  EXPECT_EQ(expected_planes, layout->planes());
  EXPECT_EQ(std::vector<size_t>({86400u}), layout->buffer_sizes());
  EXPECT_EQ(86400u, layout->GetTotalBufferSize());
  EXPECT_EQ(
      "VideoFrameLayout format: PIXEL_FORMAT_I420, coded_size: 300x180, "
      "num_buffers: 1, buffer_sizes: [86400], num_planes: 3, "
      "planes (stride, offset): [(320, 0), (160, 57600), (160, 72000)]",
      layout->ToString());
}

// Test V4L2FormatToVideoFrameLayout with single planar v4l2_format.
// Expect an invalid VideoFrameLayout.
TEST(V4L2DeviceTest, V4L2FormatToVideoFrameLayoutNoMultiPlanar) {
  auto layout = V4L2Device::V4L2FormatToVideoFrameLayout(V4L2FormatVideoOutput(
      300, 180, V4L2_PIX_FMT_NV12, V4L2_FIELD_ANY, 320, 86400));
  EXPECT_FALSE(layout.has_value());
}

// Test V4L2FormatToVideoFrameLayout with unsupported v4l2_format pixelformat,
// e.g. V4L2_PIX_FMT_NV16. Expect an invalid VideoFrameLayout.
TEST(V4L2DeviceTest, V4L2FormatToVideoFrameLayoutUnsupportedPixelformat) {
  auto layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(V4L2FormatVideoOutputMplane(
          300, 180, V4L2_PIX_FMT_NV16, V4L2_FIELD_ANY, {320}, {86400}));
  EXPECT_FALSE(layout.has_value());
}

// Test V4L2FormatToVideoFrameLayout with unsupported pixelformat which's
// #color planes > #buffers, e.g. V4L2_PIX_FMT_YUV422M.
// Expect an invalid VideoFrameLayout.
TEST(V4L2DeviceTest, V4L2FormatToVideoFrameLayoutUnsupportedStrideCalculation) {
  auto layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(V4L2FormatVideoOutputMplane(
          300, 180, V4L2_PIX_FMT_YUV422M, V4L2_FIELD_ANY, {320}, {86400}));
  EXPECT_FALSE(layout.has_value());
}

// Test V4L2FormatToVideoFrameLayout with wrong stride value (expect even).
// Expect an invalid VideoFrameLayout.
TEST(V4L2DeviceTest, V4L2FormatToVideoFrameLayoutWrongStrideValue) {
  auto layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(V4L2FormatVideoOutputMplane(
          300, 180, V4L2_PIX_FMT_YUV420, V4L2_FIELD_ANY, {319}, {86400}));
  EXPECT_FALSE(layout.has_value());
}

}  // namespace media
