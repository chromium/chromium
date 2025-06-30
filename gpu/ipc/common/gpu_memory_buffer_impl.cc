// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl.h"

namespace gpu {

GpuMemoryBufferImpl::GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                                         const gfx::Size& size,
                                         gfx::BufferFormat format,
                                         DestructionCallback callback)
    : id_(id), size_(size), format_(format), callback_(std::move(callback)) {}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
#if DCHECK_IS_ON()
  {
    base::AutoLock auto_lock(map_lock_);
    DCHECK_EQ(map_count_, 0u);
  }
#endif
  if (!callback_.is_null())
    std::move(callback_).Run();
}

gfx::Size GpuMemoryBufferImpl::GetSize() const {
  return size_;
}

gfx::BufferFormat GpuMemoryBufferImpl::GetFormat() const {
  return format_;
}

gfx::GpuMemoryBufferId GpuMemoryBufferImpl::GetId() const {
  return id_;
}

void GpuMemoryBufferImpl::AssertMapped() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
}

}  // namespace gpu
