// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

// A struct that represents a set of gfx::BufferFormat in a compact way.
struct GpuMemoryBufferFormatSet {
  constexpr GpuMemoryBufferFormatSet() = default;
  constexpr GpuMemoryBufferFormatSet(
      std::initializer_list<gfx::BufferFormat> formats) {
    for (auto format : formats)
      bitfield |= 1u << static_cast<int>(format);
  }

  constexpr bool Has(gfx::BufferFormat format) const {
    return !!(bitfield & (1u << static_cast<int>(format)));
  }

  void Add(gfx::BufferFormat format) {
    bitfield |= 1u << static_cast<int>(format);
  }

  void Remove(gfx::BufferFormat format) {
    bitfield &= ~(1u << static_cast<int>(format));
  }

  static_assert(static_cast<int>(gfx::BufferFormat::LAST) < 32,
                "GpuMemoryBufferFormatSet only supports 32 formats");
  uint32_t bitfield = 0;
};

struct Capabilities;

// Returns true if creating an image for a GpuMemoryBuffer with |format| is
// supported by |capabilities|.
GPU_EXPORT bool IsImageFromGpuMemoryBufferFormatSupported(
    gfx::BufferFormat format,
    const Capabilities& capabilities);

// Returns true if |size| is valid for plane |plane| of |format|.
GPU_EXPORT bool IsImageSizeValidForGpuMemoryBufferFormat(
    const gfx::Size& size,
    gfx::BufferFormat format);

// Returns true if |plane| is a valid plane index for |format|.
GPU_EXPORT bool IsPlaneValidForGpuMemoryBufferFormat(gfx::BufferPlane plane,
                                                     gfx::BufferFormat format);

// Return the buffer format for |plane| of |format|. E.g, for the Y plane of
// YUV_420_BIPLANAR, return R_8. Assumes IsPlaneValidForGpuMemoryBufferFormat
// returns true for the provided arguments.
GPU_EXPORT gfx::BufferFormat GetPlaneBufferFormat(gfx::BufferPlane plane,
                                                  gfx::BufferFormat format);

// Return the index for |plane| of |format|. E.g, for the Y plane of
// YUV_420_BIPLANAR, return 0. for the A plane of YUVA_420_TRIPLANAR return 2.
GPU_EXPORT int32_t GetPlaneIndex(gfx::BufferPlane plane,
                                 gfx::BufferFormat format);

// Return the size for |plane| with image |size|. E.g, for the Y plane of
// YUV_420_BIPLANAR, return size subsampled by a factor of 2. Assumes
// IsPlaneValidForGpuMemoryBufferFormat returns true for the provided arguments.
GPU_EXPORT gfx::Size GetPlaneSize(gfx::BufferPlane plane,
                                  const gfx::Size& size);

// Returns the texture target to use with native GpuMemoryBuffers.
GPU_EXPORT uint32_t GetPlatformSpecificTextureTarget();

#if BUILDFLAG(IS_MAC)
// Set the texture target to use with MacOS native GpuMemoryBuffers.
GPU_EXPORT void SetMacOSSpecificTextureTarget(uint32_t texture_target);
#endif  // BUILDFLAG(IS_MAC)

// Returns the texture target to be used for the given |usage| and |format|
// based on |capabilities|.
GPU_EXPORT uint32_t GetBufferTextureTarget(gfx::BufferUsage usage,
                                           gfx::BufferFormat format,
                                           const Capabilities& capabilities);

// Returns whether a native GMB with the given format and plane needs to be
// bound to the platform-specfic texture target or GL_TEXTURE_2D.
GPU_EXPORT bool NativeBufferNeedsPlatformSpecificTextureTarget(
    gfx::BufferFormat format,
    gfx::BufferPlane plane = gfx::BufferPlane::DEFAULT);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
