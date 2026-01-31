// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/fourcc.h"

#include <optional>

#include "media/gpu/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include <linux/videodev2.h>
#elif BUILDFLAG(USE_VAAPI)
#include <va/va.h>
#endif  // BUILDFLAG(USE_VAAPI)

namespace media {

#if BUILDFLAG(USE_V4L2_CODEC)
// Checks that converting a V4L2 pixel format to Fourcc and back to V4L2
// yields the same format as the original one.
static void CheckFromV4L2PixFmtAndBack(uint32_t fmt) {
  std::optional<Fourcc> fourcc = Fourcc::FromV4L2PixFmt(fmt);
  EXPECT_NE(fourcc, std::nullopt);
  EXPECT_EQ(fourcc->ToV4L2PixFmt(), fmt);
}

TEST(FourccTest, V4L2PixFmtToV4L2PixFmt) {
  // Temporary defined in v4l2/v4l2_device.h
  static constexpr uint32_t V4L2_MM21 = ComposeFourcc('M', 'M', '2', '1');
  static constexpr uint32_t V4L2_P010 = ComposeFourcc('P', '0', '1', '0');
  static constexpr uint32_t V4L2_QC08C = ComposeFourcc('Q', '0', '8', 'C');
  static constexpr uint32_t V4L2_QC10C = ComposeFourcc('Q', '1', '0', 'C');

  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_YUV420);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_YVU420);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_YUV420M);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_YVU420M);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_YUYV);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_NV12);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_NV21);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_NV12M);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_YUV422P);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_YUV422M);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_MT21C);
  CheckFromV4L2PixFmtAndBack(V4L2_P010);
  CheckFromV4L2PixFmtAndBack(V4L2_MM21);
  CheckFromV4L2PixFmtAndBack(V4L2_PIX_FMT_ABGR32);
  CheckFromV4L2PixFmtAndBack(V4L2_QC08C);
  CheckFromV4L2PixFmtAndBack(V4L2_QC10C);
}

TEST(FourccTest, V4L2PixFmtToVideoPixelFormat) {
  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_NV12)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_NV12M)->ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_MT21C)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(ComposeFourcc('M', 'M', '2', '1'))
                ->ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromV4L2PixFmt(ComposeFourcc('Q', '0', '8', 'C'))
                ->ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_I420,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YUV420)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_I420,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YUV420M)->ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_YV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YVU420)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_YV12,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YVU420M)->ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_I422,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YUV422P)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_I422,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_YUV422M)->ToVideoPixelFormat());

  EXPECT_EQ(PIXEL_FORMAT_ARGB,
            Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_ABGR32)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_P010LE,
            Fourcc::FromV4L2PixFmt(ComposeFourcc('P', '0', '1', '0'))
                ->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_P010LE,
            Fourcc::FromV4L2PixFmt(ComposeFourcc('Q', '1', '0', 'C'))
                ->ToVideoPixelFormat());

  // Randomly pick an unmapped v4l2 fourcc.
  EXPECT_EQ(std::nullopt, Fourcc::FromV4L2PixFmt(V4L2_PIX_FMT_Z16));
}

TEST(FourccTest, VideoPixelFormatToV4L2PixFmt) {
  EXPECT_EQ(
      V4L2_PIX_FMT_NV12,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV12, true)->ToV4L2PixFmt());
  EXPECT_EQ(
      V4L2_PIX_FMT_NV12M,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV12, false)->ToV4L2PixFmt());

  EXPECT_EQ(
      V4L2_PIX_FMT_YUV420,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_I420, true)->ToV4L2PixFmt());
  EXPECT_EQ(
      V4L2_PIX_FMT_YUV420M,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_I420, false)->ToV4L2PixFmt());

  EXPECT_EQ(
      V4L2_PIX_FMT_YVU420,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_YV12, true)->ToV4L2PixFmt());
  EXPECT_EQ(
      V4L2_PIX_FMT_YVU420M,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_YV12, false)->ToV4L2PixFmt());

  EXPECT_EQ(
      V4L2_PIX_FMT_YUV422P,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_I422, true)->ToV4L2PixFmt());
  EXPECT_EQ(
      V4L2_PIX_FMT_YUV422M,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_I422, false)->ToV4L2PixFmt());

  EXPECT_EQ(
      V4L2_PIX_FMT_ABGR32,
      Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_ARGB, true)->ToV4L2PixFmt());
}

#elif BUILDFLAG(USE_VAAPI)

// Checks that converting a VaFourCC to Fourcc and back to VaFourCC
// yields the same format as the original one.
static void CheckFromVAFourCCAndBack(uint32_t va_fourcc) {
  std::optional<Fourcc> fourcc = Fourcc::FromVAFourCC(va_fourcc);
  EXPECT_NE(fourcc, std::nullopt);
  std::optional<uint32_t> to_va_fourcc = fourcc->ToVAFourCC();
  EXPECT_NE(to_va_fourcc, std::nullopt);
  EXPECT_EQ(*to_va_fourcc, va_fourcc);
}

TEST(FourccTest, FromVaFourCCAndBack) {
  CheckFromVAFourCCAndBack(VA_FOURCC_I420);
  CheckFromVAFourCCAndBack(VA_FOURCC_NV12);
  CheckFromVAFourCCAndBack(VA_FOURCC_NV21);
  CheckFromVAFourCCAndBack(VA_FOURCC_YV12);
  CheckFromVAFourCCAndBack(VA_FOURCC_YUY2);
  CheckFromVAFourCCAndBack(VA_FOURCC_P010);
  CheckFromVAFourCCAndBack(VA_FOURCC_ARGB);
}

TEST(FourccTest, VAFourCCToVideoPixelFormat) {
  EXPECT_EQ(PIXEL_FORMAT_I420,
            Fourcc::FromVAFourCC(VA_FOURCC_I420)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_NV12,
            Fourcc::FromVAFourCC(VA_FOURCC_NV12)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_NV21,
            Fourcc::FromVAFourCC(VA_FOURCC_NV21)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_YV12,
            Fourcc::FromVAFourCC(VA_FOURCC_YV12)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_YUY2,
            Fourcc::FromVAFourCC(VA_FOURCC_YUY2)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_P010LE,
            Fourcc::FromVAFourCC(VA_FOURCC_P010)->ToVideoPixelFormat());
  EXPECT_EQ(PIXEL_FORMAT_ARGB,
            Fourcc::FromVAFourCC(VA_FOURCC_ARGB)->ToVideoPixelFormat());
}

TEST(FourccTest, VideoPixelFormatToVAFourCC) {
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_I420),
            *Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_I420)->ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_NV12),
            *Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV12)->ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_NV21),
            *Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV21)->ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_YV12),
            *Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_YV12)->ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_YUY2),
            *Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_YUY2)->ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_P010),
            *Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_P010LE)->ToVAFourCC());
  EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_ARGB),
            *Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_ARGB)->ToVAFourCC());
}
#endif

TEST(FourccTest, FourccToSinglePlanar) {
  EXPECT_EQ(Fourcc(Fourcc::YU12).ToSinglePlanar(), Fourcc(Fourcc::YU12));
  EXPECT_EQ(Fourcc(Fourcc::YV12).ToSinglePlanar(), Fourcc(Fourcc::YV12));
  EXPECT_EQ(Fourcc(Fourcc::YUYV).ToSinglePlanar(), Fourcc(Fourcc::YUYV));
  EXPECT_EQ(Fourcc(Fourcc::NV12).ToSinglePlanar(), Fourcc(Fourcc::NV12));
  EXPECT_EQ(Fourcc(Fourcc::NV21).ToSinglePlanar(), Fourcc(Fourcc::NV21));
  EXPECT_EQ(Fourcc(Fourcc::YU16).ToSinglePlanar(), Fourcc(Fourcc::YU16));
  EXPECT_EQ(Fourcc(Fourcc::YM16).ToSinglePlanar(), Fourcc(Fourcc::YU16));
  EXPECT_EQ(Fourcc(Fourcc::P010).ToSinglePlanar(), Fourcc(Fourcc::P010));
  EXPECT_EQ(Fourcc(Fourcc::AR24).ToSinglePlanar(), Fourcc(Fourcc::AR24));
  EXPECT_EQ(Fourcc(Fourcc::YM12).ToSinglePlanar(),
            Fourcc(Fourcc::YU12).ToSinglePlanar());
  EXPECT_EQ(Fourcc(Fourcc::YM21).ToSinglePlanar(),
            Fourcc(Fourcc::YV12).ToSinglePlanar());
  EXPECT_EQ(Fourcc(Fourcc::NM12).ToSinglePlanar(),
            Fourcc(Fourcc::NV12).ToSinglePlanar());
  EXPECT_EQ(Fourcc(Fourcc::NM21).ToSinglePlanar(),
            Fourcc(Fourcc::NV21).ToSinglePlanar());
  EXPECT_EQ(Fourcc(Fourcc::MT21).ToSinglePlanar(), std::nullopt);
  EXPECT_EQ(Fourcc(Fourcc::Q08C).ToSinglePlanar(), std::nullopt);
  EXPECT_EQ(Fourcc(Fourcc::Q10C).ToSinglePlanar(), std::nullopt);
}
}  // namespace media
