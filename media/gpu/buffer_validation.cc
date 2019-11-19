// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/buffer_validation.h"

#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(OS_LINUX)
#include <sys/types.h>
#include <unistd.h>
#endif  // defined(OS_LINUX)

namespace media {

bool GetFileSize(const int fd, size_t* size) {
#if defined(OS_LINUX)
  if (fd < 0) {
    VLOGF(1) << "Invalid file descriptor";
    return false;
  }

  off_t fd_size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);
  if (fd_size < 0u) {
    VPLOGF(1) << "Fail to find the size of fd";
    return false;
  }

  if (!base::IsValueInRangeForNumericType<size_t>(fd_size)) {
    VLOGF(1) << "fd_size is out of range of size_t"
             << ", size=" << size
             << ", size_t max=" << std::numeric_limits<size_t>::max()
             << ", size_t min=" << std::numeric_limits<size_t>::min();
    return false;
  }

  *size = static_cast<size_t>(fd_size);
  return true;
#else
  NOTIMPLEMENTED();
  return false;
#endif  // defined(OS_LINUX)
}

bool VerifyGpuMemoryBufferHandle(media::VideoPixelFormat pixel_format,
                                 const gfx::Size& coded_size,
                                 const gfx::GpuMemoryBufferHandle& gmb_handle) {
  if (gmb_handle.type != gfx::NATIVE_PIXMAP) {
    VLOGF(1) << "Unexpected GpuMemoryBufferType: " << gmb_handle.type;
    return false;
  }
#if defined(OS_LINUX)
  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format);
  if (num_planes != gmb_handle.native_pixmap_handle.planes.size() ||
      num_planes == 0) {
    VLOGF(1) << "Invalid number of dmabuf planes passed: "
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
    DVLOGF(4) << "Plane " << i << ", offset: " << plane.offset
              << ", stride: " << plane.stride;

    size_t file_size_in_bytes;
    if (!plane.fd.is_valid() ||
        !GetFileSize(plane.fd.get(), &file_size_in_bytes))
      return false;

    size_t plane_height =
        media::VideoFrame::Rows(i, pixel_format, coded_size.height());
    base::CheckedNumeric<size_t> min_plane_size =
        base::CheckMul(plane.stride, plane_height);
    if (!min_plane_size.IsValid() || min_plane_size.ValueOrDie() > plane.size) {
      VLOGF(1) << "Invalid strides/sizes";
      return false;
    }

    // Check |offset| + (the size of a plane) on each plane is not larger than
    // |file_size_in_bytes|. This ensures we don't access out of a buffer
    // referred by |fd|.
    base::CheckedNumeric<size_t> min_buffer_size =
        base::CheckAdd(plane.offset, plane.size);
    if (!min_buffer_size.IsValid() ||
        min_buffer_size.ValueOrDie() > file_size_in_bytes) {
      VLOGF(1) << "Invalid strides/offsets";
      return false;
    }
  }
  return true;
#else
  NOTIMPLEMENTED();
  return false;
#endif  // defined(OS_LINUX)
}

}  // namespace media
