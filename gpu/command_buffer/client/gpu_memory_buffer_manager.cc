// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

#include "base/functional/callback.h"
#include "base/notreached.h"

namespace gpu {

GpuMemoryBufferManager::GpuMemoryBufferManager() = default;

GpuMemoryBufferManager::~GpuMemoryBufferManager() = default;

void GpuMemoryBufferManager::CopyGpuMemoryBufferAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED();
}

}  // namespace gpu
