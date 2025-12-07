// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_HANDLE_INFO_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_HANDLE_INFO_H_

#include "components/viz/common/resources/shared_image_format.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {
struct GpuMemoryBufferHandleInfo {
  GpuMemoryBufferHandleInfo() = default;
  GpuMemoryBufferHandleInfo(gfx::GpuMemoryBufferHandle handle,
                            gfx::BufferUsage buffer_usage)
      : handle(std::move(handle)),
        buffer_usage(buffer_usage) {}
  ~GpuMemoryBufferHandleInfo() = default;

  GpuMemoryBufferHandleInfo(const GpuMemoryBufferHandleInfo& other) {
    handle = other.handle.Clone();
    buffer_usage = other.buffer_usage;
  }

  GpuMemoryBufferHandleInfo& operator=(const GpuMemoryBufferHandleInfo& other) {
    handle = other.handle.Clone();
    buffer_usage = other.buffer_usage;
    return *this;
  }

  gfx::GpuMemoryBufferHandle handle;
  gfx::BufferUsage buffer_usage;
};
}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_HANDLE_INFO_H_
