// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/fake_gpu_memory_buffer.h"

#include "build/build_config.h"
#include "ui/gfx/buffer_format_util.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <fcntl.h>
#endif

namespace gpu {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
gfx::GpuMemoryBufferHandle CreatePixmapHandleForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format) {
  gfx::NativePixmapHandle native_pixmap_handle;
  for (size_t i = 0; i < gfx::NumberOfPlanesForLinearBufferFormat(format);
       i++) {
    size_t height_in_pixels;
    CHECK(gfx::PlaneHeightForBufferFormatChecked(size.height(), format, i,
                                                 &height_in_pixels));
    size_t stride = gfx::RowSizeForBufferFormat(size.width(), format, i);
    native_pixmap_handle.planes.emplace_back(
        stride, 0, height_in_pixels * stride,
        base::ScopedFD(open("/dev/zero", O_RDWR)));
  }

  return gfx::GpuMemoryBufferHandle(std::move(native_pixmap_handle));
}
#endif

}  // namespace gpu
