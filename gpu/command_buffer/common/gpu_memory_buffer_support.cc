// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

bool IsImageFromGpuMemoryBufferFormatSupported(
    gfx::BufferFormat format,
    const gpu::Capabilities& capabilities) {
  return capabilities.gpu_memory_buffer_formats.Has(format);
}

bool IsImageSizeValidForGpuMemoryBufferFormat(const gfx::Size& size,
                                              gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RG_1616:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_1010102:
    case gfx::BufferFormat::RGBA_1010102:
    case gfx::BufferFormat::RGBA_F16:
      return true;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
    case gfx::BufferFormat::P010:
#if BUILDFLAG(IS_CHROMEOS)
      // Allow odd size for CrOS.
      // TODO(https://crbug.com/1208788, https://crbug.com/1224781): Merge this
      // with the path that uses gfx::IsOddHeightMultiPlanarBuffersAllowed.
      return true;
#else
      // U and V planes are subsampled by a factor of 2.
      if (size.width() % 2 && !gfx::IsOddWidthMultiPlanarBuffersAllowed())
        return false;
      if (size.height() % 2 && !gfx::IsOddHeightMultiPlanarBuffersAllowed())
        return false;
      return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace gpu
