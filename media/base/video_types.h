// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_TYPES_H_
#define MEDIA_BASE_VIDEO_TYPES_H_

#include <stdint.h>
#include <iosfwd>
#include <string>

#include "build/build_config.h"
#include "media/base/media_export.h"

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

  // Note: Chrome does not actually support YVU compositing, so you probably
  // don't actually want to use this. See http://crbug.com/784627.
  PIXEL_FORMAT_YV12 = 2,  // 12bpp YVU planar 1x1 Y, 2x2 VU samples.

  PIXEL_FORMAT_I422 = 3,   // 16bpp YUV planar 1x1 Y, 2x1 UV samples.
  PIXEL_FORMAT_I420A = 4,  // 20bpp YUVA planar 1x1 Y, 2x2 UV, 1x1 A samples.
  PIXEL_FORMAT_I444 = 5,   // 24bpp YUV planar, no subsampling.
  PIXEL_FORMAT_NV12 =
      6,  // 12bpp with Y plane followed by a 2x2 interleaved UV plane.
  PIXEL_FORMAT_NV21 =
      7,  // 12bpp with Y plane followed by a 2x2 interleaved VU plane.
  /* PIXEL_FORMAT_UYVY = 8,  Deprecated */
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

  PIXEL_FORMAT_P016LE = 29,  // 24bpp NV12, 16 bits per channel

  PIXEL_FORMAT_XR30 =
      30,  // 32bpp BGRX, 10 bits per channel, 2 bits ignored, 1 plane
  PIXEL_FORMAT_XB30 =
      31,  // 32bpp RGBX, 10 bits per channel, 2 bits ignored, 1 plane

  PIXEL_FORMAT_BGRA = 32,  // 32bpp ARGB (byte-order), 1 plane.

  // Please update UMA histogram enumeration when adding new formats here.
  PIXEL_FORMAT_MAX =
      PIXEL_FORMAT_BGRA,  // Must always be equal to largest entry logged.
};

// Returns the name of a Format as a string.
MEDIA_EXPORT std::string VideoPixelFormatToString(VideoPixelFormat format);

// Stream operator of Format for logging etc.
MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      VideoPixelFormat format);

// Returns human readable fourcc string.
// If any of the four characters is non-printable, it outputs
// "0x<32-bit integer in hex>", e.g. FourccToString(0x66616b00) returns
// "0x66616b00".
MEDIA_EXPORT std::string FourccToString(uint32_t fourcc);

// Returns true if |format| is a YUV format with multiple planes.
MEDIA_EXPORT bool IsYuvPlanar(VideoPixelFormat format);

// Returns true if |format| has no Alpha channel (hence is always opaque).
MEDIA_EXPORT bool IsOpaque(VideoPixelFormat format);

// Returns the number of significant bits per channel.
MEDIA_EXPORT size_t BitDepth(VideoPixelFormat format);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_TYPES_H_
