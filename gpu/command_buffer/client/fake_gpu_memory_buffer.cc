// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/fake_gpu_memory_buffer.h"

#include "build/build_config.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "ui/gfx/buffer_format_util.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace gpu {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
base::ScopedFD GetDummyFD() {
  base::ScopedFD fd(open("/dev/zero", O_RDWR));
  DCHECK(fd.is_valid());
  return fd;
}
#endif

}  // namespace

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
gfx::GpuMemoryBufferHandle CreatePixmapHandleForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint64_t modifier) {
  std::optional<media::VideoPixelFormat> video_pixel_format =
      media::GfxBufferFormatToVideoPixelFormat(format);
  CHECK(video_pixel_format);

  gfx::NativePixmapHandle native_pixmap_handle;
  for (size_t i = 0; i < media::VideoFrame::NumPlanes(*video_pixel_format);
       i++) {
    const gfx::Size plane_size_in_bytes =
        media::VideoFrame::PlaneSize(*video_pixel_format, i, size);
    native_pixmap_handle.planes.emplace_back(plane_size_in_bytes.width(), 0,
                                             plane_size_in_bytes.GetArea(),
                                             GetDummyFD());
  }
  native_pixmap_handle.modifier = modifier;

  gfx::GpuMemoryBufferHandle handle(std::move(native_pixmap_handle));
  return handle;
}
#endif

}  // namespace gpu
