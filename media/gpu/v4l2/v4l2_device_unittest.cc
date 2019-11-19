// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_device.h"

#include <cstring>
#include <sstream>
#include <vector>

#include "media/base/color_plane_layout.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_pixmap_handle.h"

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
  std::vector<ColorPlaneLayout> expected_planes(
      {{320, 0u, 86400u}, {320, 57600u, 28800u}});
  EXPECT_EQ(expected_planes, layout->planes());
  EXPECT_EQ(layout->is_multi_planar(), false);
  std::ostringstream ostream;
  ostream << *layout;
  const std::string kNoModifierStr =
      std::to_string(gfx::NativePixmapHandle::kNoModifier);
  EXPECT_EQ(
      ostream.str(),
      "VideoFrameLayout(format: PIXEL_FORMAT_NV12, coded_size: 300x180, "
      "planes (stride, offset, size): [(320, 0, 86400), (320, 57600, 28800)], "
      "is_multi_planar: 0, buffer_addr_align: 4096, modifier: " +
          kNoModifierStr + ")");
}

// Test V4L2FormatToVideoFrameLayout with NV12M pixelformat, which has two
// buffers and two color planes.
TEST(V4L2DeviceTest, V4L2FormatToVideoFrameLayoutNV12M) {
  auto layout = V4L2Device::V4L2FormatToVideoFrameLayout(
      V4L2FormatVideoOutputMplane(300, 180, V4L2_PIX_FMT_NV12, V4L2_FIELD_ANY,
                                  {320, 320}, {57600, 28800}));
  ASSERT_TRUE(layout.has_value());
  EXPECT_EQ(PIXEL_FORMAT_NV12, layout->format());
  EXPECT_EQ(gfx::Size(300, 180), layout->coded_size());
  std::vector<ColorPlaneLayout> expected_planes(
      {{320, 0u, 57600u}, {320, 0u, 28800u}});
  EXPECT_EQ(expected_planes, layout->planes());
  EXPECT_EQ(layout->is_multi_planar(), true);
  std::ostringstream ostream;
  ostream << *layout;
  const std::string kNoModifierStr =
      std::to_string(gfx::NativePixmapHandle::kNoModifier);
  EXPECT_EQ(
      ostream.str(),
      "VideoFrameLayout(format: PIXEL_FORMAT_NV12, coded_size: 300x180, "
      "planes (stride, offset, size): [(320, 0, 57600), (320, 0, 28800)], "
      "is_multi_planar: 1, buffer_addr_align: 4096, modifier: " +
          kNoModifierStr + ")");
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
  std::vector<ColorPlaneLayout> expected_planes(
      {{320, 0u, 86400}, {160, 57600u, 14400u}, {160, 72000u, 14400u}});
  EXPECT_EQ(expected_planes, layout->planes());
  std::ostringstream ostream;
  ostream << *layout;
  const std::string kNoModifierStr =
      std::to_string(gfx::NativePixmapHandle::kNoModifier);
  EXPECT_EQ(ostream.str(),
            "VideoFrameLayout(format: PIXEL_FORMAT_I420, coded_size: 300x180, "
            "planes (stride, offset, size): [(320, 0, 86400), (160, 57600, "
            "14400), (160, 72000, 14400)], "
            "is_multi_planar: 0, buffer_addr_align: 4096, modifier: " +
                kNoModifierStr + ")");
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

// Test GetNumPlanesOfV4L2PixFmt.
TEST(V4L2DeviceTest, GetNumPlanesOfV4L2PixFmt) {
  EXPECT_EQ(1u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_NV12));
  EXPECT_EQ(1u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_YUV420));
  EXPECT_EQ(1u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_YVU420));
  EXPECT_EQ(1u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_RGB32));

  EXPECT_EQ(2u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_NV12M));
  EXPECT_EQ(2u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_MT21C));

  EXPECT_EQ(3u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_YUV420M));
  EXPECT_EQ(3u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_YVU420M));
  EXPECT_EQ(3u, V4L2Device::GetNumPlanesOfV4L2PixFmt(V4L2_PIX_FMT_YUV422M));
}

}  // namespace media
