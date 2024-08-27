// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "gpu/ipc/common/android/android_hardware_buffer_utils.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

GpuMemoryBufferImplAndroidHardwareBuffer::
    GpuMemoryBufferImplAndroidHardwareBuffer(
        gfx::GpuMemoryBufferId id,
        const gfx::Size& size,
        gfx::BufferFormat format,
        DestructionCallback callback,
        base::android::ScopedHardwareBufferHandle handle)
    : GpuMemoryBufferImpl(id, size, format, std::move(callback)),
      hardware_buffer_handle_(std::move(handle)) {}

GpuMemoryBufferImplAndroidHardwareBuffer::
    ~GpuMemoryBufferImplAndroidHardwareBuffer() {}

bool GpuMemoryBufferImplAndroidHardwareBuffer::Map() {
  return false;
}

void* GpuMemoryBufferImplAndroidHardwareBuffer::memory(size_t plane) {
  return nullptr;
}

void GpuMemoryBufferImplAndroidHardwareBuffer::Unmap() {}

int GpuMemoryBufferImplAndroidHardwareBuffer::stride(size_t plane) const {
  return 0;
}

gfx::GpuMemoryBufferType GpuMemoryBufferImplAndroidHardwareBuffer::GetType()
    const {
  return gfx::ANDROID_HARDWARE_BUFFER;
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferImplAndroidHardwareBuffer::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::ANDROID_HARDWARE_BUFFER;
  handle.id = id_;
  handle.android_hardware_buffer = hardware_buffer_handle_.Clone();
  return handle;
}

}  // namespace gpu
