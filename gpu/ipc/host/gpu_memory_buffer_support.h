// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_HOST_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_IPC_HOST_GPU_MEMORY_BUFFER_SUPPORT_H_

#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "ui/gfx/buffer_types.h"

namespace gpu {

using GpuMemoryBufferConfigurationKey = gfx::BufferUsageAndFormat;
using GpuMemoryBufferConfigurationSet =
    std::unordered_set<GpuMemoryBufferConfigurationKey>;

}  // namespace gpu

namespace std {

template <>
struct hash<gpu::GpuMemoryBufferConfigurationKey> {
  size_t operator()(const gpu::GpuMemoryBufferConfigurationKey& key) const {
    return base::HashInts(static_cast<int>(key.format),
                          static_cast<int>(key.usage));
  }
};

}  // namespace std

namespace gpu {

class GpuMemoryBufferSupport;

// Returns the set of supported configurations.
GpuMemoryBufferConfigurationSet GetNativeGpuMemoryBufferConfigurations(
    GpuMemoryBufferSupport* support);

// Returns true of the OpenGL target to use for the combination of format/usage
// is not GL_TEXTURE_2D but a platform specific texture target.
bool GetImageNeedsPlatformSpecificTextureTarget(gfx::BufferFormat format,
                                                gfx::BufferUsage usage);

// Populate a list of buffer usage/format for which a per platform specific
// texture target must be used instead of GL_TEXTURE_2D.
std::vector<gfx::BufferUsageAndFormat>
CreateBufferUsageAndFormatExceptionList();

}  // namespace gpu

#endif  // GPU_IPC_HOST_GPU_MEMORY_BUFFER_SUPPORT_H_
