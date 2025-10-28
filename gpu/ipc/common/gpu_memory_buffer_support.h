// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {

// Provides a common factory for GPU memory buffer implementations.
class GPU_IPC_COMMON_EXPORT GpuMemoryBufferSupport {
 public:
  GpuMemoryBufferSupport();

  GpuMemoryBufferSupport(const GpuMemoryBufferSupport&) = delete;
  GpuMemoryBufferSupport& operator=(const GpuMemoryBufferSupport&) = delete;

  virtual ~GpuMemoryBufferSupport();

  // Returns whether the provided buffer format is supported.
  static bool IsNativeGpuMemoryBufferConfigurationSupportedForTesting(
      viz::SharedImageFormat format,
      gfx::BufferUsage usage) {
    return IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
  }

  // Returns whether the provided buffer format is supported.
  static bool IsNativeGpuMemoryBufferConfigurationSupported(
      viz::SharedImageFormat format,
      gfx::BufferUsage usage);
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
