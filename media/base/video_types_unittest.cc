// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(VideoTypesTest, FourccToString) {
  // Test fourccc code used in V4L2Device::V4L2PixFmtToVideoPixelFormat().
  // V4L2_PIX_FMT_NV12
  EXPECT_EQ("NV12", FourccToString(0x3231564e));
  // V4L2_PIX_FMT_NV12M
  EXPECT_EQ("NM12", FourccToString(0x32314d4e));
  // V4L2_PIX_FMT_MT21
  EXPECT_EQ("MT21", FourccToString(0x3132544d));
  // V4L2_PIX_FMT_YUV420
  EXPECT_EQ("YU12", FourccToString(0x32315559));
  // V4L2_PIX_FMT_YUV420M
  EXPECT_EQ("YM12", FourccToString(0x32314d59));
  // V4L2_PIX_FMT_YVU420
  EXPECT_EQ("YV12", FourccToString(0x32315659));
  // V4L2_PIX_FMT_YUV422M
  EXPECT_EQ("YM16", FourccToString(0x36314d59));
  // V4L2_PIX_FMT_RGB32
  EXPECT_EQ("RGB4", FourccToString(0x34424752));
}

TEST(VideoTypesTest, FourccToStringHasUnprintableChar) {
  EXPECT_EQ("0x66616b00", FourccToString(0x66616b00));
}

}  // namespace media
