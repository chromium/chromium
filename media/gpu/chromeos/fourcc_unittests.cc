// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/fourcc.h"

#include "base/logging.h"
#include "media/gpu/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include <linux/videodev2.h>
#endif  // BUILDFLAG(USE_V4L2_CODEC)
#if BUILDFLAG(USE_VAAPI)
#include <va/va.h>
#endif  // BUILDFLAG(USE_VAAPI)

namespace media {

#if BUILDFLAG(USE_V4L2_CODEC)
TEST(FourccTest, V4L2PixFmtToVideoPixelFormat) {
  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_NV12).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_NV12M).ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_MT21C).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(ComposeFourcc('M', 'M', '2', '1'))
                .ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_I420,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YUV420).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_I420,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YUV420M).ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_YV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YVU420).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_YV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YVU420M).ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_I422,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YUV422M).ToVideoPixelFormat());

  // Noted that previously in V4L2Device::V4L2PixFmtToVideoPixelFormat(),
  // V4L2_PIX_FMT_RGB32 maps to PIXEL_FORMAT_ARGB. However, the mapping was
  // wrong. It should be mapped to PIXEL_FORMAT_BGRA.
  EXPECT_EQ(PIXEL_FORMAT_BGRA,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_RGB32).ToVideoPixelFormat());

  // Randomly pick an unmapped v4l2 fourcc.
#if DCHECK_IS_ON()
  EXPECT_DEATH(
      { Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_Z16).ToVideoPixelFormat(); },
      "Unmapped V4L2PixFmt: Z16");
#else   // DCHECK_IS_ON()
  EXPECT_EQ(PIXEL_FORMAT_UNKNOWN,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_Z16).ToVideoPixelFormat());
#endif  // DCHECK_IS_ON()
}

TEST(FourccTest, VideoPixelFormatToV4L2PixFmt) {
  EXPECT_EQ(
      V4L2_PIX_FMT_NV12,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV12, true).ToV4L2PixFmt());
  EXPECT_EQ(
      V4L2_PIX_FMT_NV12M,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV12, false).ToV4L2PixFmt());

  EXPECT_EQ(
      V4L2_PIX_FMT_YUV420,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_I420, true).ToV4L2PixFmt());
  EXPECT_EQ(
      V4L2_PIX_FMT_YUV420M,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_I420, false).ToV4L2PixFmt());

  EXPECT_EQ(
      V4L2_PIX_FMT_YVU420,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_YV12, true).ToV4L2PixFmt());
  EXPECT_EQ(
      V4L2_PIX_FMT_YVU420M,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_YV12, false).ToV4L2PixFmt());
}
#endif  // BUILDFLAG(USE_V4L2_CODEC)

#if BUILDFLAG(USE_VAAPI)
TEST(FourccTest, VAFourCCToVideoPixelFormat) {
  EXPECT_EQ(PIXEL_FORMAT_I420,
            Fourcc::FromVAFourCC(VA_FOURCC_I420).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromVAFourCC(VA_FOURCC_NV12).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_NV21,
            Fourcc::FromVAFourCC(VA_FOURCC_NV21).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_YV12,
            Fourcc::FromVAFourCC(VA_FOURCC_YV12).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_YUY2,
            Fourcc::FromVAFourCC(VA_FOURCC_YUY2).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_ABGR,
            Fourcc::FromVAFourCC(VA_FOURCC_RGBA).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_XBGR,
            Fourcc::FromVAFourCC(VA_FOURCC_RGBX).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_ARGB,
            Fourcc::FromVAFourCC(VA_FOURCC_BGRA).ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_XRGB,
            Fourcc::FromVAFourCC(VA_FOURCC_BGRX).ToVideoPixelFormat());
}

TEST(FourccTest, VideoPixelFormatToVAFourCC) {
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_I420),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_I420).ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_NV12),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV12).ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_NV21),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV21).ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_YV12),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_YV12).ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_YUY2),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_YUY2).ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_RGBA),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_ABGR).ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_RGBX),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_XBGR).ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_BGRA),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_ARGB).ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_BGRX),
            Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_XRGB).ToVAFourCC());
}
#endif  // BUILDFLAG(USE_VAAPI)

}  // namespace media
