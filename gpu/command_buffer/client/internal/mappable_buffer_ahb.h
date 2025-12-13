// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_AHB_H_
#define GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_AHB_H_

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/gpu_command_buffer_client_export.h"
#include "gpu/command_buffer/client/internal/mappable_buffer.h"

namespace gpu {

class ClientSharedImage;

class GPU_COMMAND_BUFFER_CLIENT_EXPORT MappableBufferAHB
    : public MappableBuffer {
 public:
  MappableBufferAHB(const MappableBufferAHB&) = delete;
  MappableBufferAHB& operator=(const MappableBufferAHB&) = delete;

  ~MappableBufferAHB() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType =
      gfx::ANDROID_HARDWARE_BUFFER;

  static base::OnceClosure AllocateForTesting(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage,
      gfx::GpuMemoryBufferHandle* handle);

  static std::unique_ptr<MappableBufferAHB> CreateFromHandleForTesting(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format) {
    return CreateFromHandle(std::move(handle), size, format);
  }

  // MappableBuffer:
  bool Map() override;
  void MapAsync(base::OnceCallback<void(bool)> result_cb) override;
  bool AsyncMappingIsNonBlocking() const override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;

 private:
  friend ClientSharedImage;

  static std::unique_ptr<MappableBufferAHB> CreateFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      CopyNativeBufferToShMemCallback copy_native_buffer_to_shmem_callback =
          CopyNativeBufferToShMemCallback(),
      scoped_refptr<base::UnsafeSharedMemoryPool> pool = nullptr);

  MappableBufferAHB(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::GpuMemoryBufferHandle handle,
      CopyNativeBufferToShMemCallback copy_native_buffer_to_shmem_callback,
      scoped_refptr<base::UnsafeSharedMemoryPool> pool);

  void CheckAsyncMapResult(bool result);
  void AssertMapped();

  const gfx::Size size_;
  const viz::SharedImageFormat format_;
  gfx::GpuMemoryBufferHandle handle_;

  CopyNativeBufferToShMemCallback copy_native_buffer_to_shmem_callback_;

  base::Lock map_lock_;
  uint32_t map_count_ GUARDED_BY(map_lock_) = 0;
  bool async_mapping_in_progress_ GUARDED_BY(map_lock_) = false;
  std::vector<base::OnceCallback<void(bool)>> map_callbacks_
      GUARDED_BY(map_lock_);

  scoped_refptr<base::UnsafeSharedMemoryPool> shared_memory_pool_;
  std::unique_ptr<base::UnsafeSharedMemoryPool::Handle> shared_memory_handle_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_AHB_H_
