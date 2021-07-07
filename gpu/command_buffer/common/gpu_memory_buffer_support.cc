// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "ui/gfx/buffer_format_util.h"

namespace gpu {

#if defined(OS_MAC)
static uint32_t macos_specific_texture_target = GL_TEXTURE_RECTANGLE_ARB;
#endif  // defined(OS_MAC)

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
    case gfx::BufferFormat::P010:
      // U and V planes are subsampled by a factor of 2.
      if (size.width() % 2)
        return false;
      if (size.height() % 2 && !gfx::AllowOddHeightMultiPlanarBuffers())
        return false;
      return true;
  }

  NOTREACHED();
  return false;
}

GPU_EXPORT bool IsPlaneValidForGpuMemoryBufferFormat(gfx::BufferPlane plane,
                                                     gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::YVU_420:
      return plane == gfx::BufferPlane::DEFAULT ||
             plane == gfx::BufferPlane::Y || plane == gfx::BufferPlane::U ||
             plane == gfx::BufferPlane::V;
      break;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return plane == gfx::BufferPlane::DEFAULT ||
             plane == gfx::BufferPlane::Y || plane == gfx::BufferPlane::UV;
      break;
    default:
      return plane == gfx::BufferPlane::DEFAULT;
      break;
  }
  NOTREACHED();
  return false;
}

gfx::BufferFormat GetPlaneBufferFormat(gfx::BufferPlane plane,
                                       gfx::BufferFormat format) {
  switch (plane) {
    case gfx::BufferPlane::DEFAULT:
      return format;
    case gfx::BufferPlane::Y:
      if (format == gfx::BufferFormat::YVU_420 ||
          format == gfx::BufferFormat::YUV_420_BIPLANAR) {
        return gfx::BufferFormat::R_8;
      }
      if (format == gfx::BufferFormat::P010) {
        return gfx::BufferFormat::R_16;
      }
      NOTREACHED();
      break;
    case gfx::BufferPlane::UV:
      if (format == gfx::BufferFormat::YUV_420_BIPLANAR) {
        return gfx::BufferFormat::RG_88;
      }
      if (format == gfx::BufferFormat::P010) {
        // There does not yet exist a gfx::BufferFormat::RG_16, which would be
        // required for P010.
        NOTIMPLEMENTED();
      }
      break;
    case gfx::BufferPlane::U:
      if (format == gfx::BufferFormat::YVU_420)
        return gfx::BufferFormat::R_8;
      break;
    case gfx::BufferPlane::V:
      if (format == gfx::BufferFormat::YVU_420)
        return gfx::BufferFormat::R_8;
      break;
  }

  NOTREACHED();
  return format;
}

uint32_t GetPlatformSpecificTextureTarget() {
#if defined(OS_MAC)
  return macos_specific_texture_target;
#elif defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_WIN)
  return GL_TEXTURE_EXTERNAL_OES;
#elif defined(OS_FUCHSIA)
  return GL_TEXTURE_2D;
#elif defined(OS_NACL)
  NOTREACHED();
  return 0;
#else
#error Unsupported OS
#endif
}

#if defined(OS_MAC)
GPU_EXPORT void SetMacOSSpecificTextureTarget(uint32_t texture_target) {
  DCHECK(texture_target == GL_TEXTURE_2D ||
         texture_target == GL_TEXTURE_RECTANGLE_ARB);
  macos_specific_texture_target = texture_target;
}
#endif  // defined(OS_MAC)

GPU_EXPORT uint32_t GetBufferTextureTarget(gfx::BufferUsage usage,
                                           gfx::BufferFormat format,
                                           const Capabilities& capabilities) {
  bool found = base::Contains(capabilities.texture_target_exception_list,
                              gfx::BufferUsageAndFormat(usage, format));
  return found ? gpu::GetPlatformSpecificTextureTarget() : GL_TEXTURE_2D;
}

GPU_EXPORT bool NativeBufferNeedsPlatformSpecificTextureTarget(
    gfx::BufferFormat format) {
#if defined(USE_OZONE) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_WIN)
  // Always use GL_TEXTURE_2D as the target for RGB textures.
  // https://crbug.com/916728
  if (format == gfx::BufferFormat::R_8 || format == gfx::BufferFormat::RG_88 ||
      format == gfx::BufferFormat::RGBA_8888 ||
      format == gfx::BufferFormat::BGRA_8888 ||
      format == gfx::BufferFormat::RGBX_8888 ||
      format == gfx::BufferFormat::BGRX_8888 ||
      format == gfx::BufferFormat::RGBA_1010102 ||
      format == gfx::BufferFormat::BGRA_1010102) {
    return false;
  }
#elif defined(OS_ANDROID)
  if (format == gfx::BufferFormat::BGR_565 ||
      format == gfx::BufferFormat::RGBA_8888) {
    return false;
  }
#endif
  return true;
}

}  // namespace gpu
