// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/fourcc.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include <linux/videodev2.h>
#endif  // BUILDFLAG(USE_V4L2_CODEC)

#if BUILDFLAG(USE_VAAPI)
#include <va/va.h>
#endif  // BUILDFLAG(USE_VAAPI)

namespace media {

Fourcc::Fourcc() : value_(Fourcc::INVALID) {}
Fourcc::Fourcc(Fourcc::Value fourcc) : value_(fourcc) {}
Fourcc::~Fourcc() = default;
Fourcc& Fourcc::operator=(const Fourcc& other) = default;

// static
Fourcc Fourcc::FromVideoPixelFormat(VideoPixelFormat pixel_format,
                                    bool single_planar) {
  if (single_planar) {
    switch (pixel_format) {
      case PIXEL_FORMAT_ARGB:
        return Fourcc(Fourcc::AR24);
      case PIXEL_FORMAT_ABGR:
        return Fourcc(Fourcc::AB24);
      case PIXEL_FORMAT_XRGB:
        return Fourcc(Fourcc::XR24);
      case PIXEL_FORMAT_XBGR:
        return Fourcc(Fourcc::XB24);
      case PIXEL_FORMAT_BGRA:
        return Fourcc(Fourcc::RGB4);
      case PIXEL_FORMAT_I420:
        return Fourcc(Fourcc::YU12);
      case PIXEL_FORMAT_YV12:
        return Fourcc(Fourcc::YV12);
      case PIXEL_FORMAT_YUY2:
        return Fourcc(Fourcc::YUYV);
      case PIXEL_FORMAT_NV12:
        return Fourcc(Fourcc::NV12);
      case PIXEL_FORMAT_NV21:
        return Fourcc(Fourcc::NV21);
      case PIXEL_FORMAT_I422:
      case PIXEL_FORMAT_I420A:
      case PIXEL_FORMAT_I444:
      case PIXEL_FORMAT_RGB24:
      case PIXEL_FORMAT_MJPEG:
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
      case PIXEL_FORMAT_P016LE:
      case PIXEL_FORMAT_XR30:
      case PIXEL_FORMAT_XB30:
      case PIXEL_FORMAT_UNKNOWN:
        break;
    }
  } else {
    switch (pixel_format) {
      case PIXEL_FORMAT_I420:
        return Fourcc(Fourcc::YM12);
      case PIXEL_FORMAT_YV12:
        return Fourcc(Fourcc::YM21);
      case PIXEL_FORMAT_NV12:
        return Fourcc(Fourcc::NM12);
      case PIXEL_FORMAT_I422:
        return Fourcc(Fourcc::YM16);
      case PIXEL_FORMAT_NV21:
        return Fourcc(Fourcc::NM21);
      case PIXEL_FORMAT_I420A:
      case PIXEL_FORMAT_I444:
      case PIXEL_FORMAT_YUY2:
      case PIXEL_FORMAT_ARGB:
      case PIXEL_FORMAT_XRGB:
      case PIXEL_FORMAT_RGB24:
      case PIXEL_FORMAT_MJPEG:
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
      case PIXEL_FORMAT_P016LE:
      case PIXEL_FORMAT_XR30:
      case PIXEL_FORMAT_XB30:
      case PIXEL_FORMAT_BGRA:
      case PIXEL_FORMAT_UNKNOWN:
        break;
    }
  }
  NOTREACHED() << "Unmapped " << VideoPixelFormatToString(pixel_format)
               << " for " << (single_planar ? "single-planar" : "multi-planar");
  return Fourcc();
}

VideoPixelFormat Fourcc::ToVideoPixelFormat() const {
  switch (value_) {
    case Fourcc::INVALID:
      return PIXEL_FORMAT_UNKNOWN;
    case Fourcc::AR24:
      return PIXEL_FORMAT_ARGB;
    case Fourcc::AB24:
      return PIXEL_FORMAT_ABGR;
    case Fourcc::XR24:
      return PIXEL_FORMAT_XRGB;
    case Fourcc::XB24:
      return PIXEL_FORMAT_XBGR;
    case Fourcc::RGB4:
      return PIXEL_FORMAT_BGRA;
    case Fourcc::YU12:
    case Fourcc::YM12:
      return PIXEL_FORMAT_I420;
    case Fourcc::YV12:
    case Fourcc::YM21:
      return PIXEL_FORMAT_YV12;
    case Fourcc::YUYV:
      return PIXEL_FORMAT_YUY2;
    case Fourcc::NV12:
    case Fourcc::NM12:
      return PIXEL_FORMAT_NV12;
    case Fourcc::NV21:
    case Fourcc::NM21:
      return PIXEL_FORMAT_NV21;
    case Fourcc::YM16:
      return PIXEL_FORMAT_I422;
    // V4L2_PIX_FMT_MT21C is only used for MT8173 hardware video decoder output
    // and should be converted by MT8173 image processor for compositor to
    // render. Since it is an intermediate format for video decoder,
    // VideoPixelFormat shall not have its mapping. However, we need to create a
    // VideoFrameLayout for the format to process the intermediate frame. Hence
    // we map V4L2_PIX_FMT_MT21C to PIXEL_FORMAT_NV12 as their layout are the
    // same.
    case Fourcc::MT21:
    // V4L2_PIX_FMT_MM21 is used for MT8183 hardware video decoder. It is
    // similar to V4L2_PIX_FMT_MT21C but is not compressed ; thus it can also
    // be mapped to PIXEL_FORMAT_NV12.
    case Fourcc::MM21:
      return PIXEL_FORMAT_NV12;
  }
  DLOG(WARNING) << "Unmapped Fourcc: " << ToString();
  return PIXEL_FORMAT_UNKNOWN;
}

#if BUILDFLAG(USE_V4L2_CODEC)
// static
Fourcc Fourcc::FromV4L2PixFmt(uint32_t v4l2_pix_fmt) {
  // Temporary defined in v4l2/v4l2_device.h
  static constexpr uint32_t V4L2_MM21 = ComposeFourcc('M', 'M', '2', '1');
  switch (v4l2_pix_fmt) {
    case V4L2_PIX_FMT_ABGR32:
      return Fourcc(Fourcc::AR24);
#ifdef V4L2_PIX_FMT_RGBA32
    // V4L2_PIX_FMT_RGBA32 is defined since v5.2
    case V4L2_PIX_FMT_RGBA32:
      return Fourcc(Fourcc::AB24);
#endif  // V4L2_PIX_FMT_RGBA32
    case V4L2_PIX_FMT_XBGR32:
      return Fourcc(Fourcc::XR24);
#ifdef V4L2_PIX_FMT_RGBX32
    // V4L2_PIX_FMT_RGBX32 is defined since v5.2
    case V4L2_PIX_FMT_RGBX32:
      return Fourcc(Fourcc::XB24);
#endif  // V4L2_PIX_FMT_RGBX32
    case V4L2_PIX_FMT_RGB32:
      return Fourcc(Fourcc::RGB4);
    case V4L2_PIX_FMT_YUV420:
      return Fourcc(Fourcc::YU12);
    case V4L2_PIX_FMT_YVU420:
      return Fourcc(Fourcc::YV12);
    case V4L2_PIX_FMT_YUV420M:
      return Fourcc(Fourcc::YM12);
    case V4L2_PIX_FMT_YVU420M:
      return Fourcc(Fourcc::YM21);
    case V4L2_PIX_FMT_YUYV:
      return Fourcc(Fourcc::YUYV);
    case V4L2_PIX_FMT_NV12:
      return Fourcc(Fourcc::NV12);
    case V4L2_PIX_FMT_NV21:
      return Fourcc(Fourcc::NV21);
    case V4L2_PIX_FMT_NV12M:
      return Fourcc(Fourcc::NM12);
    case V4L2_PIX_FMT_YUV422M:
      return Fourcc(Fourcc::YM16);
    case V4L2_PIX_FMT_MT21C:
      return Fourcc(Fourcc::MT21);
    case V4L2_MM21:
      return Fourcc(Fourcc::MM21);
  }
  NOTREACHED() << "Unmapped V4L2PixFmt: " << FourccToString(v4l2_pix_fmt);
  return Fourcc();
}

uint32_t Fourcc::ToV4L2PixFmt() const {
  // Note that we can do that because we adopt the same internal definition of
  // Fourcc as V4L2.
  return static_cast<uint32_t>(value_);
}
#endif  // BUILDFLAG(USE_V4L2_CODEC)

#if BUILDFLAG(USE_VAAPI)
// static
Fourcc Fourcc::FromVAFourCC(uint32_t va_fourcc) {
  switch (va_fourcc) {
    case VA_FOURCC_I420:
      return Fourcc(Fourcc::YU12);
    case VA_FOURCC_NV12:
      return Fourcc(Fourcc::NV12);
    case VA_FOURCC_NV21:
      return Fourcc(Fourcc::NV21);
    case VA_FOURCC_YV12:
      return Fourcc(Fourcc::YV12);
    case VA_FOURCC_YUY2:
      return Fourcc(Fourcc::YUYV);
    case VA_FOURCC_RGBA:
      return Fourcc(Fourcc::AB24);
    case VA_FOURCC_RGBX:
      return Fourcc(Fourcc::XB24);
    case VA_FOURCC_BGRA:
      return Fourcc(Fourcc::AR24);
    case VA_FOURCC_BGRX:
      return Fourcc(Fourcc::XR24);
    case VA_FOURCC_ARGB:
      return Fourcc(Fourcc::RGB4);
  }
  DLOG(WARNING) << "Unmapped VAFourCC: " << FourccToString(va_fourcc);
  return Fourcc();
}

uint32_t Fourcc::ToVAFourCC() const {
  switch (value_) {
    case Fourcc::INVALID:
      return 0;
    case Fourcc::YU12:
      return VA_FOURCC_I420;
    case Fourcc::NV12:
      return VA_FOURCC_NV12;
    case Fourcc::NV21:
      return VA_FOURCC_NV21;
    case Fourcc::YV12:
      return VA_FOURCC_YV12;
    case Fourcc::YUYV:
      return VA_FOURCC_YUY2;
    case Fourcc::AB24:
      return VA_FOURCC_RGBA;
    case Fourcc::XB24:
      return VA_FOURCC_RGBX;
    case Fourcc::AR24:
      return VA_FOURCC_BGRA;
    case Fourcc::XR24:
      return VA_FOURCC_BGRX;
    case Fourcc::RGB4:
      return VA_FOURCC_ARGB;
    case Fourcc::YM12:
    case Fourcc::YM21:
    case Fourcc::NM12:
    case Fourcc::NM21:
    case Fourcc::YM16:
    case Fourcc::MT21:
    case Fourcc::MM21:
      break;
  }
  DLOG(WARNING) << "Unmapped fourcc: " << ToString();
  return 0;
}

#endif  // BUILDFLAG(USE_VAAPI)

bool operator==(uint32_t lhs, const Fourcc& rhs) {
  return rhs == lhs;
}
bool operator!=(const Fourcc& lhs, const Fourcc& rhs) {
  return !(lhs == rhs);
}
bool operator!=(uint32_t lhs, const Fourcc& rhs) {
  return !(rhs == lhs);
}
bool operator!=(const Fourcc& lhs, uint32_t rhs) {
  return !(lhs == rhs);
}

bool Fourcc::IsMultiPlanar() const {
  switch (value_) {
    case INVALID:
    case AR24:
    case AB24:
    case XR24:
    case XB24:
    case RGB4:
    case YU12:
    case YV12:
    case YUYV:
    case NV12:
    case NV21:
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
static_assert(Fourcc::AR24 == V4L2_PIX_FMT_ABGR32, "Mismatch Fourcc");
#ifdef V4L2_PIX_FMT_RGBA32
// V4L2_PIX_FMT_RGBA32 is defined since v5.2
static_assert(Fourcc::AB24 == V4L2_PIX_FMT_RGBA32, "Mismatch Fourcc");
#endif  // V4L2_PIX_FMT_RGBA32
static_assert(Fourcc::XR24 == V4L2_PIX_FMT_XBGR32, "Mismatch Fourcc");
#ifdef V4L2_PIX_FMT_RGBX32
// V4L2_PIX_FMT_RGBX32 is defined since v5.2
static_assert(Fourcc::XB24 == V4L2_PIX_FMT_RGBX32, "Mismatch Fourcc");
#endif  // V4L2_PIX_FMT_RGBX32
static_assert(Fourcc::RGB4 == V4L2_PIX_FMT_RGB32, "Mismatch Fourcc");
static_assert(Fourcc::YU12 == V4L2_PIX_FMT_YUV420, "Mismatch Fourcc");
static_assert(Fourcc::YV12 == V4L2_PIX_FMT_YVU420, "Mismatch Fourcc");
static_assert(Fourcc::YM12 == V4L2_PIX_FMT_YUV420M, "Mismatch Fourcc");
static_assert(Fourcc::YM21 == V4L2_PIX_FMT_YVU420M, "Mismatch Fourcc");
static_assert(Fourcc::YUYV == V4L2_PIX_FMT_YUYV, "Mismatch Fourcc");
static_assert(Fourcc::NV12 == V4L2_PIX_FMT_NV12, "Mismatch Fourcc");
static_assert(Fourcc::NV21 == V4L2_PIX_FMT_NV21, "Mismatch Fourcc");
static_assert(Fourcc::NM12 == V4L2_PIX_FMT_NV12M, "Mismatch Fourcc");
static_assert(Fourcc::NM21 == V4L2_PIX_FMT_NV21M, "Mismatch Fourcc");
static_assert(Fourcc::YM16 == V4L2_PIX_FMT_YUV422M, "Mismatch Fourcc");
static_assert(Fourcc::MT21 == V4L2_PIX_FMT_MT21C, "Mismatch Fourcc");
#ifdef V4L2_PIX_FMT_MM21
// V4L2_PIX_FMT_MM21 is not yet upstreamed.
static_assert(Fourcc::MM21 == V4L2_PIX_FMT_MM21, "Mismatch Fourcc");
#endif  // V4L2_PIX_FMT_MM21
#endif  // BUILDFLAG(USE_V4L2_CODEC)
}  // namespace media
