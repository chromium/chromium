// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl.h"

#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/process_memory_dump.h"

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

void GpuMemoryBufferImpl::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    uint64_t tracing_process_id,
    int importance) const {
  auto shared_buffer_guid =
      gfx::GetGenericSharedGpuMemoryGUIDForTracing(tracing_process_id, GetId());
  pmd->CreateSharedGlobalAllocatorDump(shared_buffer_guid);
  pmd->AddOwnershipEdge(buffer_dump_guid, shared_buffer_guid, importance);
}

void GpuMemoryBufferImpl::AssertMapped() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
}

}  // namespace gpu
