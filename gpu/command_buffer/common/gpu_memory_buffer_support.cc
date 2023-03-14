// Copyright 2016 The Chromium Authors
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
#include "ui/gfx/geometry/size.h"

namespace gpu {

#if BUILDFLAG(IS_MAC)
static uint32_t macos_specific_texture_target = GL_TEXTURE_RECTANGLE_ARB;
#endif  // BUILDFLAG(IS_MAC)

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

  NOTREACHED();
  return false;
}

GPU_EXPORT bool IsPlaneValidForGpuMemoryBufferFormat(gfx::BufferPlane plane,
                                                     gfx::BufferFormat format) {
#if BUILDFLAG(IS_APPLE)
  // On macOS and iOS each plane of a YUV GpuMemoryBuffer must be sampled
  // separately.
  switch (format) {
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::P010:
      return plane == gfx::BufferPlane::Y || plane == gfx::BufferPlane::UV;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return plane == gfx::BufferPlane::Y || plane == gfx::BufferPlane::UV ||
             plane == gfx::BufferPlane::A;
    default:
      return plane == gfx::BufferPlane::DEFAULT;
  }
#else
  switch (format) {
    case gfx::BufferFormat::YVU_420:
      return plane == gfx::BufferPlane::DEFAULT ||
             plane == gfx::BufferPlane::Y || plane == gfx::BufferPlane::U ||
             plane == gfx::BufferPlane::V;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return plane == gfx::BufferPlane::DEFAULT ||
             plane == gfx::BufferPlane::Y || plane == gfx::BufferPlane::UV;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return plane == gfx::BufferPlane::Y || plane == gfx::BufferPlane::UV ||
             plane == gfx::BufferPlane::A;
    default:
      return plane == gfx::BufferPlane::DEFAULT;
  }
#endif
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
          format == gfx::BufferFormat::YUV_420_BIPLANAR ||
          format == gfx::BufferFormat::YUVA_420_TRIPLANAR) {
        return gfx::BufferFormat::R_8;
      }
      if (format == gfx::BufferFormat::P010) {
        return gfx::BufferFormat::R_16;
      }
      break;
    case gfx::BufferPlane::UV:
      if (format == gfx::BufferFormat::YUV_420_BIPLANAR ||
          format == gfx::BufferFormat::YUVA_420_TRIPLANAR)
        return gfx::BufferFormat::RG_88;
      if (format == gfx::BufferFormat::P010)
        return gfx::BufferFormat::RG_1616;
      break;
    case gfx::BufferPlane::U:
      if (format == gfx::BufferFormat::YVU_420)
        return gfx::BufferFormat::R_8;
      break;
    case gfx::BufferPlane::V:
      if (format == gfx::BufferFormat::YVU_420)
        return gfx::BufferFormat::R_8;
      break;
    case gfx::BufferPlane::A:
      if (format == gfx::BufferFormat::YUVA_420_TRIPLANAR)
        return gfx::BufferFormat::R_8;
      break;
  }

  NOTREACHED();
  return format;
}

int32_t GetPlaneIndex(gfx::BufferPlane plane, gfx::BufferFormat format) {
  switch (plane) {
    case gfx::BufferPlane::DEFAULT:
    case gfx::BufferPlane::Y:
      return 0;
    case gfx::BufferPlane::U:
    case gfx::BufferPlane::UV:
      return 1;
    case gfx::BufferPlane::V:
      return 2;
    case gfx::BufferPlane::A:
      return format == gfx::BufferFormat::YUVA_420_TRIPLANAR ? 2 : 3;
  }
}

gfx::Size GetPlaneSize(gfx::BufferPlane plane, const gfx::Size& size) {
  switch (plane) {
    case gfx::BufferPlane::DEFAULT:
    case gfx::BufferPlane::Y:
    case gfx::BufferPlane::A:
      return size;
    case gfx::BufferPlane::U:
    case gfx::BufferPlane::V:
    case gfx::BufferPlane::UV:
      return gfx::ScaleToCeiledSize(size, 0.5);
  }
}

uint32_t GetPlatformSpecificTextureTarget() {
#if BUILDFLAG(IS_MAC)
  return macos_specific_texture_target;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  return GL_TEXTURE_EXTERNAL_OES;
#elif BUILDFLAG(IS_IOS)
  return GL_TEXTURE_2D;
#elif BUILDFLAG(IS_FUCHSIA)
  // Fuchsia uses Vulkan.
  return 0;
#elif BUILDFLAG(IS_NACL)
  NOTREACHED();
  return 0;
#else
#error Unsupported OS
#endif
}

#if BUILDFLAG(IS_MAC)
GPU_EXPORT void SetMacOSSpecificTextureTarget(uint32_t texture_target) {
  DCHECK(texture_target == GL_TEXTURE_2D ||
         texture_target == GL_TEXTURE_RECTANGLE_ARB);
  macos_specific_texture_target = texture_target;
}
#endif  // BUILDFLAG(IS_MAC)

GPU_EXPORT uint32_t GetBufferTextureTarget(gfx::BufferUsage usage,
                                           gfx::BufferFormat format,
                                           const Capabilities& capabilities) {
  bool found = base::Contains(capabilities.texture_target_exception_list,
                              gfx::BufferUsageAndFormat(usage, format));
  return found ? gpu::GetPlatformSpecificTextureTarget() : GL_TEXTURE_2D;
}

GPU_EXPORT bool NativeBufferNeedsPlatformSpecificTextureTarget(
    gfx::BufferFormat format,
    gfx::BufferPlane plane) {
#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_WIN)
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
#if BUILDFLAG(IS_CHROMEOS)
  // Use GL_TEXTURE_2D when importing the NV12 DMA-buf as two GL textures, Y
  // plane as gfx::BufferFormat::R_8, UV plane as gfx::BufferFormat::RG_88, then
  // we can sample and write to NV12 DMA-buf through the two GL textures.
  if (format == gfx::BufferFormat::YUV_420_BIPLANAR &&
      (plane == gfx::BufferPlane::Y || plane == gfx::BufferPlane::UV)) {
    return false;
  }
#endif
#elif BUILDFLAG(IS_ANDROID)
  if (format == gfx::BufferFormat::BGR_565 ||
      format == gfx::BufferFormat::RGBA_8888) {
    return false;
  }
#endif
  return true;
}

}  // namespace gpu
