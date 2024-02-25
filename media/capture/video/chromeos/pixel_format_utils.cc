// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/pixel_format_utils.h"

#include <drm_fourcc.h>

namespace media {

namespace {

struct SupportedFormat {
  cros::mojom::HalPixelFormat hal_format;
  ChromiumPixelFormat cr_format;
} const kSupportedFormats[] = {
    // The Android camera HAL v3 has three types of mandatory pixel formats:
    //
    //   1. HAL_PIXEL_FORMAT_YCbCr_420_888 (YUV flexible format).
    //   2. HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED (platform-specific format).
    //   3. HAL_PIXEL_FORMAT_BLOB (for JPEG).
    //
    // We can't use HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED as it is highly
    // platform specific and there is no way for Chrome to query the exact
    // pixel layout of the implementation-defined buffer.
    //
    // On Android the framework requests the preview stream with the
    // implementation-defined format, and as a result some camera HALs support
    // only implementation-defined preview buffers.  We should use the video
    // capture stream in Chrome VCD as it is mandatory for the camera HAL to
    // support YUV flexbile format video streams.
    {cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888,
     {PIXEL_FORMAT_NV12, gfx::BufferFormat::YUV_420_BIPLANAR}},
    // FIXME(jcliang): MJPEG is not accurate; we should have BLOB or JPEG
    {cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB,
     {PIXEL_FORMAT_MJPEG, gfx::BufferFormat::R_8}},
    // Add more mappings when we have more devices.
};

}  // namespace

std::vector<ChromiumPixelFormat> PixFormatHalToChromium(
    cros::mojom::HalPixelFormat from) {
  std::vector<ChromiumPixelFormat> ret;
  for (const auto& it : kSupportedFormats) {
    if (it.hal_format == from) {
      ret.push_back(it.cr_format);
    }
  }
  return ret;
}

uint32_t PixFormatVideoToDrm(VideoPixelFormat from) {
  switch (from) {
    case PIXEL_FORMAT_NV12:
      return DRM_FORMAT_NV12;
    case PIXEL_FORMAT_MJPEG:
      return DRM_FORMAT_R8;
    default:
      // Unsupported format.
      return 0;
  }
}

std::optional<gfx::BufferFormat> PixFormatVideoToGfx(
    VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case PIXEL_FORMAT_MJPEG:
      return gfx::BufferFormat::R_8;
    case PIXEL_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    default:
      return std::nullopt;
  }
}

}  // namespace media
