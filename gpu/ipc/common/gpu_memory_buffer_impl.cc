// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl.h"

#include "ui/gfx/buffer_format_util.h"

namespace gpu {

GpuMemoryBufferImpl::GpuMemoryBufferImpl(const gfx::Size& size,
                                         gfx::BufferFormat format)
    : size_(size), format_(format) {}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
#if DCHECK_IS_ON()
  {
    base::AutoLock auto_lock(map_lock_);
    DCHECK_EQ(map_count_, 0u);
  }
#endif
}

void GpuMemoryBufferImpl::AssertMapped() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
}

void GpuMemoryBufferImpl::MapAsync(base::OnceCallback<void(bool)> result_cb) {
  std::move(result_cb).Run(Map());
}

bool GpuMemoryBufferImpl::AsyncMappingIsNonBlocking() const {
  return false;
}

base::span<uint8_t> GpuMemoryBufferImpl::memory_span(size_t plane) {
  uint8_t* data = static_cast<uint8_t*>(memory(plane));
  if (!data) {
    return {};
  }
  size_t size = 0;
  if (!PlaneSizeForBufferFormatChecked(size_, format_, plane, &size)) {
    return {};
  }

  // SAFETY: The safety is ensured by the contract of the `GpuMemoryBuffer`.
  // `data` is a pointer to memory that has been mapped by `Map()` and
  // the `size` is calculated using the buffer utility method used by all
  // `GpuMemoryBuffer` clients already.
  return UNSAFE_BUFFERS(base::span<uint8_t>(data, size));
}

}  // namespace gpu
