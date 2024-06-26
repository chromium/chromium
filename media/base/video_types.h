// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_TYPES_H_
#define MEDIA_BASE_VIDEO_TYPES_H_

#include <stdint.h>
#include <iosfwd>
#include <string>

#include "build/build_config.h"
#include "media/base/media_shmem_export.h"

namespace media {

// Pixel formats roughly based on FOURCC labels, see:
// http://www.fourcc.org/rgb.php and http://www.fourcc.org/yuv.php
// Logged to UMA, so never reuse values. Leave gaps if necessary.
// Ordered as planar, semi-planar, YUV-packed, and RGB formats.
// When a VideoFrame is backed by native textures, VideoPixelFormat describes
// how those textures should be sampled and combined to produce the final
// pixels.
enum VideoPixelFormat {
  PIXEL_FORMAT_UNKNOWN = 0,  // Unknown or unspecified format value.
  PIXEL_FORMAT_I420 =
      1,  // 12bpp YUV planar 1x1 Y, 2x2 UV samples, a.k.a. YU12.
  PIXEL_FORMAT_YV12 = 2,   // 12bpp YVU planar 1x1 Y, 2x2 VU samples.
  PIXEL_FORMAT_I422 = 3,   // 16bpp YUV planar 1x1 Y, 2x1 UV samples.
  PIXEL_FORMAT_I420A = 4,  // 20bpp YUVA planar 1x1 Y, 2x2 UV, 1x1 A samples.
  PIXEL_FORMAT_I444 = 5,   // 24bpp YUV planar, no subsampling.
  PIXEL_FORMAT_NV12 =
      6,  // 12bpp with Y plane followed by a 2x2 interleaved UV plane.
  PIXEL_FORMAT_NV21 =
      7,  // 12bpp with Y plane followed by a 2x2 interleaved VU plane.
  PIXEL_FORMAT_UYVY =
      8,  // 16bpp interleaved 2x1 U, 1x1 Y, 2x1 V, 1x1 Y samples.
  PIXEL_FORMAT_YUY2 =
      9,  // 16bpp interleaved 1x1 Y, 2x1 U, 1x1 Y, 2x1 V samples.
  PIXEL_FORMAT_ARGB = 10,   // 32bpp BGRA (byte-order), 1 plane.
  PIXEL_FORMAT_XRGB = 11,   // 24bpp BGRX (byte-order), 1 plane.
  PIXEL_FORMAT_RGB24 = 12,  // 24bpp BGR (byte-order), 1 plane.

  /* PIXEL_FORMAT_RGB32 = 13,  Deprecated */
  PIXEL_FORMAT_MJPEG = 14,  // MJPEG compressed.
  /* PIXEL_FORMAT_MT21 = 15,  Deprecated */

  // The P* in the formats below designates the number of bits per pixel
  // component. I.e. P9 is 9-bits per pixel component, P10 is 10-bits per pixel
  // component, etc.
  PIXEL_FORMAT_YUV420P9 = 16,
  PIXEL_FORMAT_YUV420P10 = 17,
  PIXEL_FORMAT_YUV422P9 = 18,
  PIXEL_FORMAT_YUV422P10 = 19,
  PIXEL_FORMAT_YUV444P9 = 20,
  PIXEL_FORMAT_YUV444P10 = 21,
  PIXEL_FORMAT_YUV420P12 = 22,
  PIXEL_FORMAT_YUV422P12 = 23,
  PIXEL_FORMAT_YUV444P12 = 24,

  /* PIXEL_FORMAT_Y8 = 25, Deprecated */
  PIXEL_FORMAT_Y16 = 26,  // single 16bpp plane.

  PIXEL_FORMAT_ABGR = 27,  // 32bpp RGBA (byte-order), 1 plane.
  PIXEL_FORMAT_XBGR = 28,  // 24bpp RGBX (byte-order), 1 plane.

  // 15bpp YUV planar 1x1 Y, 2x2 interleaved UV, 10 bits per channel.
  // data in the high bits, zeros in the low bits, little-endian.
  PIXEL_FORMAT_P010LE = 29,

  PIXEL_FORMAT_XR30 =
      30,  // 32bpp BGRX, 10 bits per channel, 2 bits ignored, 1 plane
  PIXEL_FORMAT_XB30 =
      31,  // 32bpp RGBX, 10 bits per channel, 2 bits ignored, 1 plane

  PIXEL_FORMAT_BGRA = 32,  // 32bpp ARGB (byte-order), 1 plane.

  PIXEL_FORMAT_RGBAF16 = 33,  // Half float RGBA, 1 plane.

  PIXEL_FORMAT_I422A = 34,  // 24bpp YUVA planar 1x1 Y, 2x1 UV, 1x1 A samples.

  PIXEL_FORMAT_I444A = 35,  // 32bpp YUVA planar, no subsampling.

  // YUVA planar, 10 bits per pixel component.
  PIXEL_FORMAT_YUV420AP10 = 36,
  PIXEL_FORMAT_YUV422AP10 = 37,
  PIXEL_FORMAT_YUV444AP10 = 38,

  // 20bpp YUVA planar 1x1 Y, 2x2 interleaved UV, 1x1 A samples.
  PIXEL_FORMAT_NV12A = 39,

  // 16bpp YUV planar 1x1 Y, 2x1 interleaved UV, 8 bits per channel.
  PIXEL_FORMAT_NV16 = 40,

  // 24bpp YUV planar 1x1 Y, 1x1 interleaved UV, 8 bits per channel.
  PIXEL_FORMAT_NV24 = 41,

  // 20bpp YUV planar 1x1 Y, 2x1 interleaved UV, 10 bits per channel.
  // data in the high bits, zeros in the low bits, little-endian.
  PIXEL_FORMAT_P210LE = 42,

  // 30bpp YUV planar 1x1 Y, 1x1 interleaved UV, 10 bits per channel.
  // data in the high bits, zeros in the low bits, little-endian.
  PIXEL_FORMAT_P410LE = 43,

  // Please update UMA histogram enumeration when adding new formats here.
  PIXEL_FORMAT_MAX =
      PIXEL_FORMAT_P410LE,  // Must always be equal to largest entry logged.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VideoChromaSampling : uint8_t {
  kUnknown = 0,
  k420,  // 4:2:0 chroma channel has 1/2 height/width of luma channel.
  k422,  // 4:2:2 chroma channel has same height & 1/2 width of luma channel.
  k444,  // 4:4:4 chroma channel has same height/width of luma channel.
  k400,  // 4:0:0 monochrome without chroma subsampling.

  // Please update UMA histogram enumeration when adding new formats here.
  kMaxValue = k400,  // Must always be equal to largest entry logged.
};

// Return the name of chroma sampling format as a string.
MEDIA_SHMEM_EXPORT std::string VideoChromaSamplingToString(
    VideoChromaSampling chroma_sampling);

// Returns the name of a Format as a string.
MEDIA_SHMEM_EXPORT std::string VideoPixelFormatToString(
    VideoPixelFormat format);

// Stream operator of Format for logging etc.
MEDIA_SHMEM_EXPORT std::ostream& operator<<(std::ostream& os,
                                            VideoPixelFormat format);

// Returns human readable fourcc string.
// If any of the four characters is non-printable, it outputs
// "0x<32-bit integer in hex>", e.g. FourccToString(0x66616b00) returns
// "0x66616b00".
MEDIA_SHMEM_EXPORT std::string FourccToString(uint32_t fourcc);

// Returns the VideoChromaSampling corresponding to the VideoPixelFormat passed
// in.
MEDIA_SHMEM_EXPORT VideoChromaSampling
VideoPixelFormatToChromaSampling(VideoPixelFormat format);

// Returns true if |format| is a YUV format with multiple planes.
MEDIA_SHMEM_EXPORT bool IsYuvPlanar(VideoPixelFormat format);

// Returns true if |format| is an RGB format.
MEDIA_SHMEM_EXPORT bool IsRGB(VideoPixelFormat format);

// Returns true if |format| has no Alpha channel (hence is always opaque).
MEDIA_SHMEM_EXPORT bool IsOpaque(VideoPixelFormat format);

// Returns the number of significant bits per channel.
MEDIA_SHMEM_EXPORT size_t BitDepth(VideoPixelFormat format);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_TYPES_H_
