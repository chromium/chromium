// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_DXGI_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_DXGI_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ui/gfx/color_space.h"

namespace gpu {

class GpuMemoryBufferManager;

// Implementation of GPU memory buffer based on dxgi textures.
class GPU_EXPORT GpuMemoryBufferImplDXGI : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplDXGI(const GpuMemoryBufferImplDXGI&) = delete;
  GpuMemoryBufferImplDXGI& operator=(const GpuMemoryBufferImplDXGI&) = delete;

  ~GpuMemoryBufferImplDXGI() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType =
      gfx::DXGI_SHARED_HANDLE;

  static std::unique_ptr<GpuMemoryBufferImplDXGI> CreateFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      DestructionCallback callback,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      scoped_refptr<base::UnsafeSharedMemoryPool> pool,
      base::span<uint8_t> premapped_memory = base::span<uint8_t>());

  static base::OnceClosure AllocateForTesting(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gfx::GpuMemoryBufferHandle* handle);

  bool Map() override;
  void MapAsync(base::OnceCallback<void(bool)> result_cb) override;
  bool AsyncMappingIsNonBlocking() const override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;

  HANDLE GetHandle() const;
  const gfx::DXGIHandleToken& GetToken() const;

 private:
  GpuMemoryBufferImplDXGI(gfx::GpuMemoryBufferId id,
                          const gfx::Size& size,
                          gfx::BufferFormat format,
                          DestructionCallback callback,
                          base::win::ScopedHandle dxgi_handle,
                          gfx::DXGIHandleToken dxgi_token,
                          GpuMemoryBufferManager* gpu_memory_buffer_manager,
                          scoped_refptr<base::UnsafeSharedMemoryPool> pool,
                          base::span<uint8_t> premapped_memory);

  std::optional<bool> PrepareToMap(bool is_async)
      EXCLUSIVE_LOCKS_REQUIRED(map_lock_);
  void CheckAsyncMapResult(base::OnceCallback<void(bool)> result_cb,
                           bool result);

  base::win::ScopedHandle dxgi_handle_;
  gfx::DXGIHandleToken dxgi_token_;
  raw_ptr<GpuMemoryBufferManager> gpu_memory_buffer_manager_;

  // Used to create and store shared memory for data, copied via request to
  // gpu process.
  scoped_refptr<base::UnsafeSharedMemoryPool> shared_memory_pool_;
  std::unique_ptr<base::UnsafeSharedMemoryPool::Handle> shared_memory_handle_;

  // Used to store shared memory passed from the capturer.
  base::raw_span<uint8_t> premapped_memory_;
  bool async_mapping_in_progress_ GUARDED_BY(map_lock_) = false;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_DXGI_H_
