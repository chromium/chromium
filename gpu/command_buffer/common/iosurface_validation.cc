// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/iosurface_validation.h"

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurfaceRef.h>

namespace gpu {

uint32_t SharedImageFormatToIOSurfacePixelFormat(viz::SharedImageFormat format,
                                                 bool override_rgba_to_bgra) {
  if (format.is_single_plane()) {
    if (format == viz::SinglePlaneFormat::kR_8) {
      return kCVPixelFormatType_OneComponent8;
    } else if (format == viz::SinglePlaneFormat::kRG_88) {
      return kCVPixelFormatType_TwoComponent8;
    } else if (format == viz::SinglePlaneFormat::kR_16) {
      return kCVPixelFormatType_OneComponent16;
    } else if (format == viz::SinglePlaneFormat::kRG_1616) {
      return kCVPixelFormatType_TwoComponent16;
    } else if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
      return kCVPixelFormatType_ARGB2101010LEPacked;
    } else if (format == viz::SinglePlaneFormat::kBGRA_8888 ||
               format == viz::SinglePlaneFormat::kBGRX_8888) {
      return kCVPixelFormatType_32BGRA;
    } else if (format == viz::SinglePlaneFormat::kRGBA_8888 ||
               format == viz::SinglePlaneFormat::kRGBX_8888) {
      return override_rgba_to_bgra ? kCVPixelFormatType_32BGRA
                                   : kCVPixelFormatType_32RGBA;
    } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
      return kCVPixelFormatType_64RGBAHalf;
    } else if (format == viz::SinglePlaneFormat::kBGR_565 ||
               format == viz::SinglePlaneFormat::kRGBA_4444 ||
               format == viz::SinglePlaneFormat::kRGBA_1010102) {
      // Technically RGBA_1010102 should be accepted as 'R10k', but
      // then it won't be supported by CGLTexImageIOSurface2D(), so
      // it's best to reject it here.
      return 0;
    }
  } else if (format.is_multi_plane()) {
    if (format == viz::MultiPlaneFormat::kNV12) {
      return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kNV16) {
      return kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kNV24) {
      return kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kNV12A) {
      return kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
    } else if (format == viz::MultiPlaneFormat::kP010) {
      return kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kP210) {
      return kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kP410) {
      return kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange;
    } else if (format == viz::MultiPlaneFormat::kI420) {
      return kCVPixelFormatType_420YpCbCr8Planar;
    } else if (format == viz::MultiPlaneFormat::kYV12) {
      return 0;
    }
  }
  NOTREACHED();
}

bool ValidateIOSurface(const gfx::ScopedIOSurface& io_surface,
                       viz::SharedImageFormat format,
                       gfx::Size size,
                       std::string* out_error_str) {
  // Validate top-level IOSurface format.
  uint32_t io_surface_format = IOSurfaceGetPixelFormat(io_surface.get());
  // Always treat RGBA as BGRA. It makes no difference for validating size since
  // they have the same size, and avoids the complexity of actually trying to
  // figure out if conversion should happen.
  if (io_surface_format == kCVPixelFormatType_32RGBA) {
    io_surface_format = kCVPixelFormatType_32BGRA;
  }
  const bool override_rgba_to_bgra = true;
  if (io_surface_format !=
      SharedImageFormatToIOSurfacePixelFormat(format, override_rgba_to_bgra)) {
    if (out_error_str) {
      *out_error_str =
          "IOSurface pixel format does not match specified shared "
          "image format.";
    }
    return false;
  }

  // Validate top-level IOSurface dimensions.
  if (IOSurfaceGetWidth(io_surface.get()) !=
          static_cast<size_t>(size.width()) ||
      IOSurfaceGetHeight(io_surface.get()) !=
          static_cast<size_t>(size.height())) {
    if (out_error_str) {
      *out_error_str = "IOSurface size does not match specified size.";
    }
    return false;
  }

  // Ensure the IOSurface has at least as many planes as the requested format.
  // For single-planar IOSurfaces, IOSurfaceGetPlaneCount returns 0.
  size_t io_surface_plane_count =
      std::max<size_t>(1, IOSurfaceGetPlaneCount(io_surface.get()));
  if (io_surface_plane_count < static_cast<size_t>(format.NumberOfPlanes())) {
    if (out_error_str) {
      *out_error_str = "IOSurface plane count is too small.";
    }
    return false;
  }

  // Validate per-plane dimensions and stride. A malformed IOSurface could
  // have planes with dimensions inconsistent with its top-level size and
  // format, leading to out-of-bounds access during buffer operations.
  for (int plane_index = 0; plane_index < format.NumberOfPlanes();
       ++plane_index) {
    gfx::Size plane_size = format.GetPlaneSize(plane_index, size);
    if (IOSurfaceGetWidthOfPlane(io_surface.get(), plane_index) !=
            static_cast<size_t>(plane_size.width()) ||
        IOSurfaceGetHeightOfPlane(io_surface.get(), plane_index) !=
            static_cast<size_t>(plane_size.height())) {
      if (out_error_str) {
        *out_error_str = "IOSurface plane size does not match specified size.";
      }
      return false;
    }

    // Ensure the IOSurface has enough bytes per row for the plane to prevent
    // potential out-of-bounds access when copying or accessing the buffer.
    size_t io_surface_bytes_per_row =
        IOSurfaceGetBytesPerRowOfPlane(io_surface.get(), plane_index);
    size_t min_bytes_per_row;
    if (format.is_single_plane()) {
      CHECK(!format.IsCompressed());
      min_bytes_per_row = static_cast<size_t>(format.BytesPerPixel()) *
                          static_cast<size_t>(plane_size.width());
    } else {
      min_bytes_per_row =
          static_cast<size_t>(format.MultiplanarStorageBytesPerChannel()) *
          static_cast<size_t>(format.NumChannelsInPlane(plane_index)) *
          static_cast<size_t>(plane_size.width());
    }
    if (io_surface_bytes_per_row < min_bytes_per_row) {
      if (out_error_str) {
        *out_error_str = "IOSurface bytes per row is too small.";
      }
      return false;
    }
  }

  return true;
}

}  // namespace gpu
