// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_FOURCC_H_
#define MEDIA_GPU_CHROMEOS_FOURCC_H_

#include <stdint.h>

#include <optional>
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
// Fourcc f1(Fourcc::NV12);
// EXPECT_EQ("NV12", f1.ToString());
// Fourcc f2 = Fourcc::FromVideoPixelFormat(PIXEL_FORMAT_NV12);
// EXPECT_EQ(f2, f1);
class MEDIA_GPU_EXPORT Fourcc {
 public:
  enum Value : uint32_t {
    // YUV420 single-planar formats.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-yuv420.html
    // Maps to PIXEL_FORMAT_I420, V4L2_PIX_FMT_YUV420, VA_FOURCC_I420.
    // 12bpp YUV planar 1x1 Y, 2x2 UV samples.
    YU12 = ComposeFourcc('Y', 'U', '1', '2'),
    // Maps to PIXEL_FORMAT_YV12, V4L2_PIX_FMT_YVU420, VA_FOURCC_YV12.
    // 12bpp YVU planar 1x1 Y, 2x2 VU samples.
    YV12 = ComposeFourcc('Y', 'V', '1', '2'),

    // YUV420 multi-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-yuv420m.html
    // Maps to PIXEL_FORMAT_I420, V4L2_PIX_FMT_YUV420M.
    YM12 = ComposeFourcc('Y', 'M', '1', '2'),
    // Maps to PIXEL_FORMAT_YV12, V4L2_PIX_FMT_YVU420M.
    YM21 = ComposeFourcc('Y', 'M', '2', '1'),

    // YUYV format.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-yuyv.html
    // Maps to PIXEL_FORMAT_YUY2, V4L2_PIX_FMT_YUYV, VA_FOURCC_YUY2.
    // 16bpp YUV planar (YUV 4:2:2), YUYV (byte-order), 1 plane.
    YUYV = ComposeFourcc('Y', 'U', 'Y', 'V'),

    // NV12 single-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-nv12.html
    // Maps to PIXEL_FORMAT_NV12, V4L2_PIX_FMT_NV12, VA_FOURCC_NV12.
    // 12bpp with Y plane followed by a 2x2 interleaved UV plane.
    NV12 = ComposeFourcc('N', 'V', '1', '2'),
    // Maps to PIXEL_FORMAT_NV21, V4L2_PIX_FMT_NV21, VA_FOURCC_NV21.
    // 12bpp with Y plane followed by a 2x2 interleaved VU plane.
    NV21 = ComposeFourcc('N', 'V', '2', '1'),

    // NV12 multi-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-nv12m.html
    // Maps to PIXEL_FORMAT_NV12, V4L2_PIX_FMT_NV12M,
    NM12 = ComposeFourcc('N', 'M', '1', '2'),
    // Maps to PIXEL_FORMAT_NV21, V4L2_PIX_FMT_NV21M.
    NM21 = ComposeFourcc('N', 'M', '2', '1'),

    // YUV422 single-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-yuv422p.html
    // Maps to PIXEL_FORMAT_I422, V4L2_PIX_FMT_YUV422P.
    YU16 = ComposeFourcc('4', '2', '2', 'P'),

    // YUV422 multi-planar format.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-yuv422m.html
    // Maps to PIXEL_FORMAT_I422, V4L2_PIX_FMT_YUV422M
    // 16bpp YUV planar 1x1 Y, 2x1 UV samples.
    YM16 = ComposeFourcc('Y', 'M', '1', '6'),

    // V4L2 proprietary format.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-reserved.html
    // Maps to V4L2_PIX_FMT_MT21C.
    // It is used for MT8173 hardware video decoder output and should be
    // converted by MT8173 image processor for compositor to render.
    MT21 = ComposeFourcc('M', 'T', '2', '1'),
    // Maps to V4L2_PIX_FMT_MM21.
    // It is used for MT8183 hardware video decoder.
    MM21 = ComposeFourcc('M', 'M', '2', '1'),

    // Two-plane 10-bit YUV 4:2:0. Each sample is a two-byte little-endian value
    // with the bottom six bits ignored.
    P010 = ComposeFourcc('P', '0', '1', '0'),

    // Two-plane Mediatek variant of P010. See
    // https://tinyurl.com/mtk-10bit-video-format for details.
    MT2T = ComposeFourcc('M', 'T', '2', 'T'),

    // Single plane 8-bit little-endian ARGB (bytes in reverse B-G-R-A order).
    AR24 = ComposeFourcc('A', 'R', '2', '4'),
    // V4L2 proprietary format.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-reserved.html
    // Opaque format that can only be scanned out as an overlay or composited by
    // the gpu.
    // Maps to V4L2_PIX_FMT_QC08C.
    Q08C = ComposeFourcc('Q', '0', '8', 'C'),
    // Maps to V4L2_PIX_FMT_QC10C.
    Q10C = ComposeFourcc('Q', '1', '0', 'C'),

    UNDEFINED = ComposeFourcc(0, 0, 0, 0),
  };

  constexpr Fourcc() = default;

  explicit constexpr Fourcc(Fourcc::Value fourcc) : value_(fourcc) {}

  bool operator==(const Fourcc& rhs) const { return value_ == rhs.value_; }

  // Factory methods:

  // Builds a Fourcc from a given fourcc code. This will return a valid
  // Fourcc if the argument is part of the |Value| enum, or nullopt otherwise.
  static std::optional<Fourcc> FromUint32(uint32_t fourcc);

  // Converts a VideoPixelFormat to Fourcc.
  // Returns nullopt for invalid input.
  // Note that a VideoPixelFormat may have two Fourcc counterparts. Caller has
  // to specify if it is for single-planar or multi-planar format.
  static std::optional<Fourcc> FromVideoPixelFormat(
      VideoPixelFormat pixel_format,
      bool single_planar = true);
#if BUILDFLAG(USE_V4L2_CODEC)
  // Converts a V4L2PixFmt to Fourcc.
  // Returns nullopt for invalid input.
  static std::optional<Fourcc> FromV4L2PixFmt(uint32_t v4l2_pix_fmt);
#elif BUILDFLAG(USE_VAAPI)
  // Converts a VAFourCC to Fourcc.
  // Returns nullopt for invalid input.
  static std::optional<Fourcc> FromVAFourCC(uint32_t va_fourcc);
#endif  // BUILDFLAG(USE_VAAPI)

  // Value getters:
  // Returns the VideoPixelFormat counterpart of the value.
  // Returns PIXEL_FORMAT_UNKNOWN if no mapping is found.
  VideoPixelFormat ToVideoPixelFormat() const;
#if BUILDFLAG(USE_V4L2_CODEC)
  // Returns the V4L2PixFmt counterpart of the value.
  // Returns 0 if no mapping is found.
  uint32_t ToV4L2PixFmt() const;
#elif BUILDFLAG(USE_VAAPI)
  // Returns the VAFourCC counterpart of the value.
  // Returns nullopt if no mapping is found.
  std::optional<uint32_t> ToVAFourCC() const;
#endif  // BUILDFLAG(USE_VAAPI)

  // Returns the single-planar Fourcc of the value. If value is a single-planar,
  // returns the same Fourcc. Returns nullopt if the value is neither
  // single-planar nor multi-planar or if the value is multi-planar but does not
  // have a single-planar equivalent.
  std::optional<Fourcc> ToSinglePlanar() const;

  // Returns whether |value_| is multi planar format.
  bool IsMultiPlanar() const;

  // Outputs human readable fourcc string, e.g. "NV12".
  std::string ToString() const;

 private:
  Value value_ = Fourcc::Value::UNDEFINED;
};

MEDIA_GPU_EXPORT bool operator!=(const Fourcc& lhs, const Fourcc& rhs);

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_FOURCC_H_
