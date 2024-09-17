// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/buffer_validation.h"

#include <algorithm>
#include <cstdint>

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <sys/types.h>
#include <unistd.h>
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace media {

bool GetFileSize(const int fd, size_t* size) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (fd < 0) {
    VLOG(1) << "Invalid file descriptor";
    return false;
  }

  const off_t fd_size = lseek(fd, 0, SEEK_END);
  if (fd_size == static_cast<off_t>(-1)) {
    VPLOG(1) << "Failed to get the size of the dma-buf";
    return false;
  }
  if (lseek(fd, 0, SEEK_SET) == static_cast<off_t>(-1)) {
    VPLOG(1) << "Failed to reset the file offset of the dma-buf";
    return false;
  }

  if (!base::IsValueInRangeForNumericType<size_t>(fd_size)) {
    VLOG(1) << "fd_size is out of range of size_t"
            << ", size=" << size
            << ", size_t max=" << std::numeric_limits<size_t>::max()
            << ", size_t min=" << std::numeric_limits<size_t>::min();
    return false;
  }

  *size = base::checked_cast<size_t>(fd_size);
  return true;
#else
  NOTIMPLEMENTED();
  return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

bool VerifyGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::GpuMemoryBufferHandle& gmb_handle,
    GetFileSizeCBForTesting file_size_cb_for_testing) {
  if (gmb_handle.type != gfx::NATIVE_PIXMAP) {
    VLOG(1) << "Unexpected GpuMemoryBufferType: " << gmb_handle.type;
    return false;
  }
  if (!media::VideoFrame::IsValidCodedSize(coded_size)) {
    VLOG(1) << "Coded size is beyond allowed dimensions: "
            << coded_size.ToString();
    return false;
  }
  // YV12 is used by ARC++ on MTK8173. Consider removing it.
  if (pixel_format != PIXEL_FORMAT_I420 && pixel_format != PIXEL_FORMAT_YV12 &&
      pixel_format != PIXEL_FORMAT_NV12 &&
      pixel_format != PIXEL_FORMAT_P010LE &&
      pixel_format != PIXEL_FORMAT_ARGB) {
    VLOG(1) << "Unsupported: " << pixel_format;
    return false;
  }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format);
  if (num_planes != gmb_handle.native_pixmap_handle.planes.size() ||
      num_planes == 0) {
    VLOG(1) << "Invalid number of dmabuf planes passed: "
            << gmb_handle.native_pixmap_handle.planes.size()
            << ", expected: " << num_planes;
    return false;
  }

  // Strides monotonically decrease.
  for (size_t i = 1; i < num_planes; i++) {
    if (gmb_handle.native_pixmap_handle.planes[i - 1].stride <
        gmb_handle.native_pixmap_handle.planes[i].stride) {
      return false;
    }
  }

  for (size_t i = 0; i < num_planes; i++) {
    const auto& plane = gmb_handle.native_pixmap_handle.planes[i];
    DVLOG(4) << "Plane " << i << ", offset: " << plane.offset
             << ", stride: " << plane.stride;

    size_t file_size_in_bytes;
    if (file_size_cb_for_testing) {
      file_size_in_bytes = file_size_cb_for_testing.Run();
    } else if (!plane.fd.is_valid() ||
               !GetFileSize(plane.fd.get(), &file_size_in_bytes)) {
      return false;
    }
    const size_t plane_height =
        media::VideoFrame::Rows(i, pixel_format, coded_size.height());
    base::CheckedNumeric<size_t> min_plane_size =
        base::CheckMul(base::strict_cast<size_t>(plane.stride), plane_height);
    const size_t plane_pixel_width =
        media::VideoFrame::RowBytes(i, pixel_format, coded_size.width());
    if (!min_plane_size.IsValid<uint64_t>() ||
        min_plane_size.ValueOrDie<uint64_t>() > plane.size ||
        base::strict_cast<size_t>(plane.stride) < plane_pixel_width) {
      VLOG(1) << "Invalid strides/sizes";
      return false;
    }

    // Check |offset| + (the size of a plane) on each plane is not larger than
    // |file_size_in_bytes|. This ensures we don't access out of a buffer
    // referred by |fd|.
    base::CheckedNumeric<uint64_t> min_buffer_size =
        base::CheckAdd(plane.offset, plane.size);
    if (!min_buffer_size.IsValid() ||
        min_buffer_size.ValueOrDie() >
            base::strict_cast<uint64_t>(file_size_in_bytes)) {
      VLOG(1) << "Invalid strides/offsets";
      return false;
    }
  }
  return true;
#else
  NOTIMPLEMENTED();
  return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

}  // namespace media
