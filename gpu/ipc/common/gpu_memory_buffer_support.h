// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

#include <memory>
#include <unordered_set>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

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

// Provides a common factory for GPU memory buffer implementations.
class GPU_IPC_COMMON_EXPORT GpuMemoryBufferSupport {
 public:
  GpuMemoryBufferSupport();

  GpuMemoryBufferSupport(const GpuMemoryBufferSupport&) = delete;
  GpuMemoryBufferSupport& operator=(const GpuMemoryBufferSupport&) = delete;

  virtual ~GpuMemoryBufferSupport();

  // Returns the set of supported configurations.
  static GpuMemoryBufferConfigurationSet
  GetNativeGpuMemoryBufferConfigurations();

  // Returns whether the provided buffer format is supported.
  static bool IsNativeGpuMemoryBufferConfigurationSupportedForTesting(
      gfx::BufferFormat format,
      gfx::BufferUsage usage) {
    return IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
  }

 private:
  // Returns whether the provided buffer format is supported.
  static bool IsNativeGpuMemoryBufferConfigurationSupported(
      gfx::BufferFormat format,
      gfx::BufferUsage usage);
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
