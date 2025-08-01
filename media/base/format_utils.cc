// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/format_utils.h"
#include "base/logging.h"
#include "ui/gfx/buffer_format_util.h"

namespace media {

std::optional<VideoPixelFormat> GfxBufferFormatToVideoPixelFormat(
    gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGRX_8888:
      return PIXEL_FORMAT_XRGB;

    case gfx::BufferFormat::BGRA_8888:
      return PIXEL_FORMAT_ARGB;

    case gfx::BufferFormat::RGBA_8888:
      return PIXEL_FORMAT_ABGR;

    case gfx::BufferFormat::RGBA_F16:
      return PIXEL_FORMAT_RGBAF16;

    // There is no PIXEL_FORMAT_XBGR which would have been the right mapping.
    // See ui/ozone drm_util.cc::GetFourCCFormatFromBufferFormat as reference.
    // But here it is only about indicating to not consider the alpha channel.
    // Useful for the compositor to avoid drawing behind as mentioned in
    // https://chromium-review.googlesource.com/590772.
    case gfx::BufferFormat::RGBX_8888:
      return PIXEL_FORMAT_XRGB;

    case gfx::BufferFormat::YVU_420:
      return PIXEL_FORMAT_YV12;

    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return PIXEL_FORMAT_NV12;

    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return PIXEL_FORMAT_NV12A;

    case gfx::BufferFormat::P010:
      return PIXEL_FORMAT_P010LE;

    case gfx::BufferFormat::RGBA_1010102:
      return PIXEL_FORMAT_XR30;

    default:
      DLOG(WARNING) << "Unsupported BufferFormat: "
                    << gfx::BufferFormatToString(format);
      return std::nullopt;
  }
}

std::optional<gfx::BufferFormat> VideoPixelFormatToGfxBufferFormat(
    VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case PIXEL_FORMAT_ARGB:
      return gfx::BufferFormat::BGRA_8888;

    case PIXEL_FORMAT_XRGB:
      return gfx::BufferFormat::BGRX_8888;

    case PIXEL_FORMAT_YV12:
      return gfx::BufferFormat::YVU_420;

    case PIXEL_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;

    case PIXEL_FORMAT_NV12A:
      return gfx::BufferFormat::YUVA_420_TRIPLANAR;

    case PIXEL_FORMAT_ABGR:
      return gfx::BufferFormat::RGBA_8888;

    case PIXEL_FORMAT_XBGR:
      return gfx::BufferFormat::RGBX_8888;

    case PIXEL_FORMAT_P010LE:
      return gfx::BufferFormat::P010;

    case PIXEL_FORMAT_XR30:
      return gfx::BufferFormat::RGBA_1010102;

    case PIXEL_FORMAT_RGBAF16:
      return gfx::BufferFormat::RGBA_F16;

    default:
      DLOG(WARNING) << "Unsupported VideoPixelFormat: " << pixel_format;
      return std::nullopt;
  }
}

std::optional<VideoPixelFormat> SharedImageFormatToVideoPixelFormat(
    viz::SharedImageFormat format) {
  if (format == viz::SinglePlaneFormat::kBGRX_8888) {
    return PIXEL_FORMAT_XRGB;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return PIXEL_FORMAT_ARGB;
  } else if (format == viz::SinglePlaneFormat::kRGBX_8888) {
    // There is no PIXEL_FORMAT_XBGR which would have been the right mapping.
    // See ui/ozone drm_util.cc::GetFourCCFormatFromBufferFormat as reference.
    // But here it is only about indicating to not consider the alpha channel.
    // Useful for the compositor to avoid drawing behind as mentioned in
    // https://chromium-review.googlesource.com/590772.
    return PIXEL_FORMAT_XRGB;
  } else if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return PIXEL_FORMAT_ABGR;
  } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
    return PIXEL_FORMAT_XR30;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return PIXEL_FORMAT_RGBAF16;
  } else if (format == viz::MultiPlaneFormat::kYV12) {
    return PIXEL_FORMAT_YV12;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    return PIXEL_FORMAT_NV12;
  } else if (format == viz::MultiPlaneFormat::kNV12A) {
    return PIXEL_FORMAT_NV12A;
  } else if (format == viz::MultiPlaneFormat::kP010) {
    return PIXEL_FORMAT_P010LE;
  } else {
    DLOG(WARNING) << "Unsupported SharedImageFormat: " << format.ToString();
    return std::nullopt;
  }
}

std::optional<viz::SharedImageFormat> VideoPixelFormatToSharedImageFormat(
    VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case PIXEL_FORMAT_ARGB:
      return viz::SinglePlaneFormat::kBGRA_8888;
    case PIXEL_FORMAT_XRGB:
      return viz::SinglePlaneFormat::kBGRX_8888;
    case PIXEL_FORMAT_ABGR:
      return viz::SinglePlaneFormat::kRGBA_8888;
    case PIXEL_FORMAT_XBGR:
      return viz::SinglePlaneFormat::kRGBX_8888;
    case PIXEL_FORMAT_XR30:
      return viz::SinglePlaneFormat::kRGBA_1010102;
    case PIXEL_FORMAT_RGBAF16:
      return viz::SinglePlaneFormat::kRGBA_F16;
    case PIXEL_FORMAT_YV12:
      return viz::MultiPlaneFormat::kYV12;
    case PIXEL_FORMAT_NV12:
      return viz::MultiPlaneFormat::kNV12;
    case PIXEL_FORMAT_NV16:
      return viz::MultiPlaneFormat::kNV16;
    case PIXEL_FORMAT_NV24:
      return viz::MultiPlaneFormat::kNV24;
    case PIXEL_FORMAT_NV12A:
      return viz::MultiPlaneFormat::kNV12A;
    case PIXEL_FORMAT_P010LE:
      return viz::MultiPlaneFormat::kP010;
    case PIXEL_FORMAT_P210LE:
      return viz::MultiPlaneFormat::kP210;
    case PIXEL_FORMAT_P410LE:
      return viz::MultiPlaneFormat::kP410;
    case PIXEL_FORMAT_I420:
      return viz::MultiPlaneFormat::kI420;
    case PIXEL_FORMAT_I420A:
      return viz::MultiPlaneFormat::kI420A;
    default:
      DLOG(WARNING) << "Unsupported VideoPixelFormat: " << pixel_format;
      return std::nullopt;
  }
}

}  // namespace media
