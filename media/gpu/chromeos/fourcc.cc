// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/fourcc.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "media/gpu/macros.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include <linux/videodev2.h>
#elif BUILDFLAG(USE_VAAPI)
#include <va/va.h>
#endif  // BUILDFLAG(USE_VAAPI)

namespace media {

// static
std::optional<Fourcc> Fourcc::FromUint32(uint32_t fourcc) {
  switch (fourcc) {
    case YU12:
    case YV12:
    case YM12:
    case YM21:
    case YUYV:
    case NV12:
    case NV21:
    case NM12:
    case NM21:
    case YU16:
    case YM16:
    case MT21:
    case MM21:
    case P010:
    case MT2T:
    case AR24:
    case Q08C:
    case Q10C:
      return Fourcc(static_cast<Value>(fourcc));
  }
  DVLOGF(4) << "Unmapped fourcc: " << FourccToString(fourcc);
  return std::nullopt;
}

// static
std::optional<Fourcc> Fourcc::FromVideoPixelFormat(
    VideoPixelFormat pixel_format,
    bool single_planar) {
  if (single_planar) {
    switch (pixel_format) {
      case PIXEL_FORMAT_I420:
        return Fourcc(YU12);
      case PIXEL_FORMAT_YV12:
        return Fourcc(YV12);
      case PIXEL_FORMAT_YUY2:
        return Fourcc(YUYV);
      case PIXEL_FORMAT_NV12:
        return Fourcc(NV12);
      case PIXEL_FORMAT_NV21:
        return Fourcc(NV21);
      case PIXEL_FORMAT_I422:
        return Fourcc(YU16);
      case PIXEL_FORMAT_P010LE:
        return Fourcc(P010);
      case PIXEL_FORMAT_ARGB:
        return Fourcc(AR24);
      case PIXEL_FORMAT_UYVY:
        NOTREACHED_IN_MIGRATION();
        [[fallthrough]];
      case PIXEL_FORMAT_ABGR:
      case PIXEL_FORMAT_XRGB:
      case PIXEL_FORMAT_XBGR:
      case PIXEL_FORMAT_BGRA:
      case PIXEL_FORMAT_I420A:
      case PIXEL_FORMAT_I444:
      case PIXEL_FORMAT_RGB24:
      case PIXEL_FORMAT_MJPEG:
      case PIXEL_FORMAT_NV12A:
      case PIXEL_FORMAT_NV16:
      case PIXEL_FORMAT_NV24:
      case PIXEL_FORMAT_P210LE:
      case PIXEL_FORMAT_P410LE:
      case PIXEL_FORMAT_YUV420P9:
      case PIXEL_FORMAT_YUV420P10:
      case PIXEL_FORMAT_YUV422P9:
      case PIXEL_FORMAT_YUV422P10:
      case PIXEL_FORMAT_YUV444P9:
      case PIXEL_FORMAT_YUV444P10:
      case PIXEL_FORMAT_YUV420P12:
      case PIXEL_FORMAT_YUV422P12:
      case PIXEL_FORMAT_YUV444P12:
      case PIXEL_FORMAT_Y16:
      case PIXEL_FORMAT_XR30:
      case PIXEL_FORMAT_XB30:
      case PIXEL_FORMAT_RGBAF16:
      case PIXEL_FORMAT_I422A:
      case PIXEL_FORMAT_I444A:
      case PIXEL_FORMAT_YUV420AP10:
      case PIXEL_FORMAT_YUV422AP10:
      case PIXEL_FORMAT_YUV444AP10:
      case PIXEL_FORMAT_UNKNOWN:
        break;
    }
  } else {
    switch (pixel_format) {
      case PIXEL_FORMAT_I420:
        return Fourcc(YM12);
      case PIXEL_FORMAT_YV12:
        return Fourcc(YM21);
      case PIXEL_FORMAT_NV12:
        return Fourcc(NM12);
      case PIXEL_FORMAT_I422:
        return Fourcc(YM16);
      case PIXEL_FORMAT_NV21:
        return Fourcc(NM21);
      case PIXEL_FORMAT_UYVY:
        NOTREACHED_IN_MIGRATION();
        [[fallthrough]];
      case PIXEL_FORMAT_I420A:
      case PIXEL_FORMAT_I444:
      case PIXEL_FORMAT_YUY2:
      case PIXEL_FORMAT_ARGB:
      case PIXEL_FORMAT_XRGB:
      case PIXEL_FORMAT_RGB24:
      case PIXEL_FORMAT_MJPEG:
      case PIXEL_FORMAT_NV12A:
      case PIXEL_FORMAT_NV16:
      case PIXEL_FORMAT_NV24:
      case PIXEL_FORMAT_P210LE:
      case PIXEL_FORMAT_P410LE:
      case PIXEL_FORMAT_YUV420P9:
      case PIXEL_FORMAT_YUV420P10:
      case PIXEL_FORMAT_YUV422P9:
      case PIXEL_FORMAT_YUV422P10:
      case PIXEL_FORMAT_YUV444P9:
      case PIXEL_FORMAT_YUV444P10:
      case PIXEL_FORMAT_YUV420P12:
      case PIXEL_FORMAT_YUV422P12:
      case PIXEL_FORMAT_YUV444P12:
      case PIXEL_FORMAT_Y16:
      case PIXEL_FORMAT_ABGR:
      case PIXEL_FORMAT_XBGR:
      case PIXEL_FORMAT_P010LE:
      case PIXEL_FORMAT_XR30:
      case PIXEL_FORMAT_XB30:
      case PIXEL_FORMAT_BGRA:
      case PIXEL_FORMAT_RGBAF16:
      case PIXEL_FORMAT_I422A:
      case PIXEL_FORMAT_I444A:
      case PIXEL_FORMAT_YUV420AP10:
      case PIXEL_FORMAT_YUV422AP10:
      case PIXEL_FORMAT_YUV444AP10:
      case PIXEL_FORMAT_UNKNOWN:
        break;
    }
  }
  DVLOGF(3) << "Unmapped " << VideoPixelFormatToString(pixel_format) << " for "
            << (single_planar ? "single-planar" : "multi-planar");
  return std::nullopt;
}

VideoPixelFormat Fourcc::ToVideoPixelFormat() const {
  switch (value_) {
    case YU12:
    case YM12:
      return PIXEL_FORMAT_I420;
    case YV12:
    case YM21:
      return PIXEL_FORMAT_YV12;
    case YUYV:
      return PIXEL_FORMAT_YUY2;
    case NV12:
    case NM12:
      return PIXEL_FORMAT_NV12;
    case NV21:
    case NM21:
      return PIXEL_FORMAT_NV21;
    case YU16:
    case YM16:
      return PIXEL_FORMAT_I422;
    // V4L2_PIX_FMT_MT21C is only used for MT8173 hardware video decoder output
    // and should be converted by MT8173 image processor for compositor to
    // render. Since it is an intermediate format for video decoder,
    // VideoPixelFormat shall not have its mapping. However, we need to create a
    // VideoFrameLayout for the format to process the intermediate frame. Hence
    // we map V4L2_PIX_FMT_MT21C to PIXEL_FORMAT_NV12 as their layout are the
    // same.
    case MT21:
    // V4L2_PIX_FMT_MM21 is used for MT8183 hardware video decoder. It is
    // similar to V4L2_PIX_FMT_MT21C but is not compressed ; thus it can also
    // be mapped to PIXEL_FORMAT_NV12.
    case MM21:
      return PIXEL_FORMAT_NV12;
    case P010:
      return PIXEL_FORMAT_P010LE;
    case MT2T:
      return PIXEL_FORMAT_P010LE;
    case AR24:
      return PIXEL_FORMAT_ARGB;
    // V4L2_PIX_FMT_QC08C is a proprietary Qualcomm compressed format that can
    // only be scanned out directly or composited with the gpu. It has the
    // same bitdepth and internal layout as NV12 (with additional space for
    // the compressed data).
    case Q08C:
      return PIXEL_FORMAT_NV12;
    // V4L2_PIX_FMT_QC10C is similar to V4L2_PIX_FMT_QC08C, but has the same
    // bitdepth and internal layout as P010.
    case Q10C:
      return PIXEL_FORMAT_P010LE;
    case UNDEFINED:
      break;
  }
  NOTREACHED_IN_MIGRATION() << "Unmapped Fourcc: " << ToString();
  return PIXEL_FORMAT_UNKNOWN;
}

#if BUILDFLAG(USE_V4L2_CODEC)
// static
std::optional<Fourcc> Fourcc::FromV4L2PixFmt(uint32_t v4l2_pix_fmt) {
  // We can do that because we adopt the same internal definition of Fourcc as
  // V4L2.
  return FromUint32(v4l2_pix_fmt);
}

uint32_t Fourcc::ToV4L2PixFmt() const {
  // Note that we can do that because we adopt the same internal definition of
  // Fourcc as V4L2.
  return static_cast<uint32_t>(value_);
}
#elif BUILDFLAG(USE_VAAPI)
// static
std::optional<Fourcc> Fourcc::FromVAFourCC(uint32_t va_fourcc) {
  switch (va_fourcc) {
    case VA_FOURCC_I420:
      return Fourcc(YU12);
    case VA_FOURCC_NV12:
      return Fourcc(NV12);
    case VA_FOURCC_NV21:
      return Fourcc(NV21);
    case VA_FOURCC_YV12:
      return Fourcc(YV12);
    case VA_FOURCC_YUY2:
      return Fourcc(YUYV);
    case VA_FOURCC_P010:
      return Fourcc(P010);
    case VA_FOURCC_ARGB:
      return Fourcc(AR24);
  }
  DVLOGF(3) << "Unmapped VAFourCC: " << FourccToString(va_fourcc);
  return std::nullopt;
}

std::optional<uint32_t> Fourcc::ToVAFourCC() const {
  switch (value_) {
    case YU12:
      return VA_FOURCC_I420;
    case NV12:
      return VA_FOURCC_NV12;
    case NV21:
      return VA_FOURCC_NV21;
    case YV12:
      return VA_FOURCC_YV12;
    case YUYV:
      return VA_FOURCC_YUY2;
    case P010:
      return VA_FOURCC_P010;
    case AR24:
      return VA_FOURCC_ARGB;
    case YM12:
    case YM21:
    case NM12:
    case NM21:
    case YU16:
    case YM16:
    case MT21:
    case MM21:
    case MT2T:
    case Q08C:
    case Q10C:
    case UNDEFINED:
      // VAAPI does not know about these formats, so signal this by returning
      // nullopt.
      DVLOGF(3) << "Fourcc not convertible to VaFourCC: " << ToString();
      return std::nullopt;
  }
  NOTREACHED_IN_MIGRATION() << "Unmapped Fourcc: " << ToString();
  return std::nullopt;
}

#endif  // BUILDFLAG(USE_VAAPI)

std::optional<Fourcc> Fourcc::ToSinglePlanar() const {
  switch (value_) {
    case YU12:
    case YV12:
    case YUYV:
    case NV12:
    case NV21:
    case P010:
    case MM21:
    case MT2T:
    case AR24:
      return Fourcc(value_);
    case YM12:
      return Fourcc(YU12);
    case YM21:
      return Fourcc(YV12);
    case NM12:
      return Fourcc(NV12);
    case NM21:
      return Fourcc(NV21);
    case YU16:
    case YM16:
      return Fourcc(YU16);
    case MT21:
    case Q08C:
    case Q10C:
    case UNDEFINED:
      return std::nullopt;
  }
}

bool operator!=(const Fourcc& lhs, const Fourcc& rhs) {
  return !(lhs == rhs);
}

bool Fourcc::IsMultiPlanar() const {
  switch (value_) {
    case YU12:
    case YV12:
    case YUYV:
    case NV12:
    case NV21:
    case YU16:
    case P010:
    case MT2T:
    case AR24:
    case Q08C:
    case Q10C:
    case UNDEFINED:
      return false;
    case YM12:
    case YM21:
    case NM12:
    case NM21:
    case YM16:
    case MT21:
    case MM21:
      return true;
  }
}

std::string Fourcc::ToString() const {
  return FourccToString(static_cast<uint32_t>(value_));
}

#if BUILDFLAG(USE_V4L2_CODEC)
static_assert(Fourcc::YU12 == V4L2_PIX_FMT_YUV420, "Mismatch Fourcc");
static_assert(Fourcc::YV12 == V4L2_PIX_FMT_YVU420, "Mismatch Fourcc");
static_assert(Fourcc::YM12 == V4L2_PIX_FMT_YUV420M, "Mismatch Fourcc");
static_assert(Fourcc::YM21 == V4L2_PIX_FMT_YVU420M, "Mismatch Fourcc");
static_assert(Fourcc::YUYV == V4L2_PIX_FMT_YUYV, "Mismatch Fourcc");
static_assert(Fourcc::NV12 == V4L2_PIX_FMT_NV12, "Mismatch Fourcc");
static_assert(Fourcc::NV21 == V4L2_PIX_FMT_NV21, "Mismatch Fourcc");
static_assert(Fourcc::NM12 == V4L2_PIX_FMT_NV12M, "Mismatch Fourcc");
static_assert(Fourcc::NM21 == V4L2_PIX_FMT_NV21M, "Mismatch Fourcc");
static_assert(Fourcc::YU16 == V4L2_PIX_FMT_YUV422P, "Mismatch Fourcc");
static_assert(Fourcc::YM16 == V4L2_PIX_FMT_YUV422M, "Mismatch Fourcc");
static_assert(Fourcc::MM21 == V4L2_PIX_FMT_MM21, "Mismatch Fourcc");
static_assert(Fourcc::MT21 == V4L2_PIX_FMT_MT21C, "Mismatch Fourcc");
static_assert(Fourcc::AR24 == V4L2_PIX_FMT_ABGR32, "Mismatch Fourcc");
static_assert(Fourcc::P010 == V4L2_PIX_FMT_P010, "Mismatch Fourcc");
// MT2T has not been upstreamed yet
#ifdef V4L2_PIX_FMT_MT2T
static_assert(Fourcc::MT2T == V4L2_PIX_FMT_MT2T, "Mismatch Fourcc");
#endif  // V4L2_PIX_FMT_MT2T
// TODO(b/189218019): The following formats are upstream, but not in the
// ChromeOS headers
#ifdef V4L2_PIX_FMT_QC08C
static_assert(Fourcc::Q08C == V4L2_PIX_FMT_QC08C, "Mismatch Fourcc");
#endif  // V4L2_PIX_FMT_QC08C
#ifdef V4L2_PIX_FMT_QC10C
static_assert(Fourcc::Q10C == V4L2_PIX_FMT_QC10C, "Mismatch Fourcc");
#endif  // V4L2_PIX_FMT_QC10C
#endif  // BUILDFLAG(USE_V4L2_CODEC)
}  // namespace media
