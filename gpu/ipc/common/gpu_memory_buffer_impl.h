// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_H_

#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

// Provides common implementation of a GPU memory buffer.
//
// TODO(reveman): Rename to GpuMemoryBufferBase.
class GPU_EXPORT GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  using DestructionCallback = base::OnceCallback<void()>;

  GpuMemoryBufferImpl(const GpuMemoryBufferImpl&) = delete;
  GpuMemoryBufferImpl& operator=(const GpuMemoryBufferImpl&) = delete;

  ~GpuMemoryBufferImpl() override;

  // Overridden from gfx::GpuMemoryBuffer:
  gfx::Size GetSize() const override;
  gfx::BufferFormat GetFormat() const override;
  gfx::GpuMemoryBufferId GetId() const override;
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override;

 protected:
  GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                      const gfx::Size& size,
                      gfx::BufferFormat format,
                      DestructionCallback callback);

  void AssertMapped();

  const gfx::GpuMemoryBufferId id_;
  const gfx::Size size_;
  const gfx::BufferFormat format_;
  DestructionCallback callback_;

  // Note: This lock must be held throughout the entirety of the Map() and
  // Unmap() operations to avoid corrupt mutation across multiple threads.
  base::Lock map_lock_;
  uint32_t map_count_ GUARDED_BY(map_lock_) = 0u;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_H_
