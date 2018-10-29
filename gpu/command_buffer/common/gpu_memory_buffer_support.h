// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

#include "gpu/gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

struct Capabilities;

// Returns the GL internalformat that is compatible with |format|.
GPU_EXPORT unsigned InternalFormatForGpuMemoryBufferFormat(
    gfx::BufferFormat format);

// Returns true if |internalformat| is compatible with |format|.
GPU_EXPORT bool IsImageFormatCompatibleWithGpuMemoryBufferFormat(
    unsigned internalformat,
    gfx::BufferFormat format);

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

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
