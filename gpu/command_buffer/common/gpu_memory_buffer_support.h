// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

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

// Returns true if |size| is valid for |format|.
GPU_EXPORT bool IsImageSizeValidForGpuMemoryBufferFormat(
    const gfx::Size& size,
    gfx::BufferFormat format);

// Returns the texture target to use with native GpuMemoryBuffers.
GPU_EXPORT uint32_t GetPlatformSpecificTextureTarget();

// Returns the texture target to be used for the given |usage| and |format|
// based on |capabilities|.
GPU_EXPORT uint32_t GetBufferTextureTarget(gfx::BufferUsage usage,
                                           gfx::BufferFormat format,
                                           const Capabilities& capabilities);

// Returns whether a native GMB with the given format needs to be bound to the
// platform-specfic texture target or GL_TEXTURE_2D.
GPU_EXPORT bool NativeBufferNeedsPlatformSpecificTextureTarget(
    gfx::BufferFormat format);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
