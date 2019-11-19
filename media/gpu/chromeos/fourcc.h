// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_FOURCC_H_
#define MEDIA_GPU_CHROMEOS_FOURCC_H_

#include <stdint.h>
#include <string>

#include "media/base/video_types.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Composes a Fourcc value.
constexpr uint32_t ComposeFourcc(char a, char b, char c, char d) {
  return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
         (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

// Fourcc enum holder and converters.
// Usage:
// Fourcc f1(Fourcc::AR24);
// EXPECT_EQ("AR24", f1.ToString());
// Fourcc f2 = Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_ARGB);
// EXPECT_EQ(f2, f1);
class MEDIA_GPU_EXPORT Fourcc {
 public:
  enum Value : uint32_t {
    // RGB formats.
    // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-rgb.html
    // Maps to PIXEL_FORMAT_ARGB, V4L2_PIX_FMT_ABGR32, VA_FOURCC_BGRA.
    // 32bpp BGRA (byte-order), 1 plane.
    AR24 = ComposeFourcc('A', 'R', '2', '4'),

    // Maps to PIXEL_FORMAT_ABGR, V4L2_PIX_FMT_RGBA32, VA_FOURCC_RGBA.
    // 32bpp RGBA (byte-order), 1 plane
    AB24 = ComposeFourcc('A', 'B', '2', '4'),

    // Maps to PIXEL_FORMAT_XRGB, V4L2_PIX_FMT_XBGR32, VA_FOURCC_BGRX.
    // 32bpp BGRX (byte-order), 1 plane.
    XR24 = ComposeFourcc('X', 'R', '2', '4'),

    // Maps to PIXEL_FORMAT_XBGR, V4L2_PIX_FMT_RGBX32, VA_FOURCC_RGBX.
    // 32bpp RGBX (byte-order), 1 plane.
    XB24 = ComposeFourcc('X', 'B', '2', '4'),

    // Maps to PIXEL_FORMAT_BGRA, V4L2_PIX_FMT_RGB32, VA_FOURCC_ARGB.
    // 32bpp ARGB (byte-order), 1 plane.
    // Note that V4L2_PIX_FMT_RGB32("RGB4") is deprecated and replaced by
    // V4L2_PIX_FMT_ARGB32("BA24"), however, some board relies on the fourcc
    // mapping so we keep it as-is.
    RGB4 = ComposeFourcc('R', 'G', 'B', '4'),

    // YUV420 single-planar formats.
    // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-yuv420.html
    // Maps to PIXEL_FORMAT_I420, V4L2_PIX_FMT_YUV420, VA_FOURCC_I420.
    // 12bpp YUV planar 1x1 Y, 2x2 UV samples.
    YU12 = ComposeFourcc('Y', 'U', '1', '2'),
    // Maps to PIXEL_FORMAT_YV12, V4L2_PIX_FMT_YVU420, VA_FOURCC_YV12.
    // 12bpp YVU planar 1x1 Y, 2x2 VU samples.
    YV12 = ComposeFourcc('Y', 'V', '1', '2'),

    // YUV420 multi-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-yuv420m.htm
    // Maps to PIXEL_FORMAT_I420, V4L2_PIX_FMT_YUV420M.
    YM12 = ComposeFourcc('Y', 'M', '1', '2'),
    // Maps to PIXEL_FORMAT_YV12, V4L2_PIX_FMT_YVU420M.
    YM21 = ComposeFourcc('Y', 'M', '2', '1'),

    // YUYV format.
    // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-yuyv.html
    // Maps to PIXEL_FORMAT_YUY2, V4L2_PIX_FMT_YUYV, VA_FOURCC_YUY2.
    // 16bpp YUV planar (YUV 4:2:2), YUYV (byte-order), 1 plane.
    YUYV = ComposeFourcc('Y', 'U', 'Y', 'V'),

    // NV12 single-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-nv12.html
    // Maps to PIXEL_FORMAT_NV12, V4L2_PIX_FMT_NV12, VA_FOURCC_NV12.
    // 12bpp with Y plane followed by a 2x2 interleaved UV plane.
    NV12 = ComposeFourcc('N', 'V', '1', '2'),
    // Maps to PIXEL_FORMAT_NV21, V4L2_PIX_FMT_NV21, VA_FOURCC_NV21.
    // 12bpp with Y plane followed by a 2x2 interleaved VU plane.
    NV21 = ComposeFourcc('N', 'V', '2', '1'),

    // NV12 multi-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-nv12m.html
    // Maps to PIXEL_FORMAT_NV12, V4L2_PIX_FMT_NV12M,
    NM12 = ComposeFourcc('N', 'M', '1', '2'),
    // Maps to PIXEL_FORMAT_NV21, V4L2_PIX_FMT_NV21M.
    NM21 = ComposeFourcc('N', 'M', '2', '1'),

    // YUV422 multi-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-yuv422m.html
    // Maps to PIXEL_FORMAT_I422, V4L2_PIX_FMT_YUV422M
    // 16bpp YUV planar 1x1 Y, 2x1 UV samples.
    YM16 = ComposeFourcc('Y', 'M', '1', '6'),

    // V4L2 proprietary format.
    // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-reserved.html
    // Maps to V4L2_PIX_FMT_MT21C.
    // It is used for MT8173 hardware video decoder output and should be
    // converted by MT8173 image processor for compositor to render.
    MT21 = ComposeFourcc('M', 'T', '2', '1'),
    // Maps to V4L2_PIX_FMT_MM21.
    // It is used for MT8183 hardware video decoder.
    MM21 = ComposeFourcc('M', 'M', '2', '1'),

    // Invalid
    INVALID = 0,
  };

  // Constructor for invalid Fourcc.
  Fourcc();
  explicit Fourcc(Fourcc::Value fourcc);
  Fourcc& operator=(const Fourcc& fourcc);
  ~Fourcc();

  bool operator==(const Fourcc& rhs) const { return value_ == rhs.value_; }
  bool operator==(uint32_t rhs) const {
    return static_cast<uint32_t>(value_) == rhs;
  }
  explicit operator bool() const { return value_ != Fourcc::INVALID; }

  // Factory methods:
  // Converts a VideoPixelFormat to Fourcc.
  // Returns Fourcc::INVALID for invalid input.
  // Note that a VideoPixelFormat may have two Fourcc counterparts. Caller has
  // to specify if it is for single-planar or multi-planar format.
  static Fourcc FromVideoPixelFormat(VideoPixelFormat pixel_format,
                                     bool single_planar = true);
#if BUILDFLAG(USE_V4L2_CODEC)
  // Converts a V4L2PixFmt to Fourcc.
  // Returns Fourcc::INVALID for invalid input.
  static Fourcc FromV4L2PixFmt(uint32_t v4l2_pix_fmt);
#endif  // BUILDFLAG(USE_V4L2_CODEC)
#if BUILDFLAG(USE_VAAPI)
  // Converts a VAFourCC to Fourcc.
  // Returns Fourcc::INVALID for invalid input.
  static Fourcc FromVAFourCC(uint32_t va_fourcc);
#endif  // BUILDFLAG(USE_VAAPI)

  // Value getters:
  // Returns the VideoPixelFormat counterpart of the value.
  // Returns PIXEL_FORMAT_UNKNOWN if no mapping is found.
  VideoPixelFormat ToVideoPixelFormat() const;
#if BUILDFLAG(USE_V4L2_CODEC)
  // Returns the V4L2PixFmt counterpart of the value.
  // Returns 0 if no mapping is found.
  uint32_t ToV4L2PixFmt() const;
#endif  // BUILDFLAG(USE_V4L2_CODEC)
#if BUILDFLAG(USE_VAAPI)
  // Returns the VAFourCC counterpart of the value.
  // Returns 0 if no mapping is found.
  uint32_t ToVAFourCC() const;
#endif  // BUILDFLAG(USE_VAAPI)

  // Returns whether |value_| is multi planar format.
  bool IsMultiPlanar() const;

  // Outputs human readable fourcc string, e.g. "NV12".
  std::string ToString() const;

 private:
  Value value_;
};

MEDIA_GPU_EXPORT bool operator==(uint32_t lhs, const Fourcc& rhs);
MEDIA_GPU_EXPORT bool operator!=(const Fourcc& lhs, const Fourcc& rhs);
MEDIA_GPU_EXPORT bool operator!=(uint32_t lhs, const Fourcc& rhs);
MEDIA_GPU_EXPORT bool operator!=(const Fourcc& lhs, uint32_t rhs);

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_FOURCC_H_
