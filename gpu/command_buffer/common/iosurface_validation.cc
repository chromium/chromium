// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/iosurface_validation.h"

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurfaceRef.h>

namespace gpu {

bool ValidateIOSurface(const gfx::ScopedIOSurface& io_surface,
                       viz::SharedImageFormat format,
                       gfx::Size size,
                       bool cpu_access,
                       std::string* out_error_str) {
  // Validate top-level IOSurface format.
  uint32_t io_surface_format = IOSurfaceGetPixelFormat(io_surface.get());
  if (!gfx::IOSurfacePixelFormatMatchesSharedImageFormat(
          io_surface_format, format, /*match_rgba_and_bgra=*/true)) {
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

  // Verify that the IOSurface format allows CPU access if it is expected.
  const bool io_surface_format_supports_cpu_access =
      gfx::IOSurfacePixelFormatSupportsCpuAccess(io_surface_format);
  if (cpu_access && !io_surface_format_supports_cpu_access) {
    if (out_error_str) {
      *out_error_str = "IOSurface format does not allow CPU access.";
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
    // potential out-of-bounds access when copying or accessing the buffer on
    // the CPU. Do this for all CPU-accessible formats, even if CPU access was
    // not requested.
    if (io_surface_format_supports_cpu_access) {
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
  }

  return true;
}

}  // namespace gpu
