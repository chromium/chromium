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
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"

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

  // This method allows clients to explicitly specify that it wants to use the
  // |premapped_memory_| which is internally created by this class from the GMB
  // handle shared memory |region_| instead of client providing it.
  // This method is also used when the clients wants to specify that it no
  // longer should be using the pre mapped memory.
  // Windows media capture code will need to use this api in order to transition
  // to MappableSI. It is currently not used until the media capture code is
  // converted to MappableSI.
  void SetUsePreMappedMemory(bool use_premapped_memory) override;

  gfx::GpuMemoryBufferHandle CloneHandleWithRegion(
      base::UnsafeSharedMemoryRegion region) const;

  HANDLE GetHandle() const;
  const gfx::DXGIHandleToken& GetToken() const;

 private:
  GpuMemoryBufferImplDXGI(gfx::GpuMemoryBufferId id,
                          const gfx::Size& size,
                          gfx::BufferFormat format,
                          DestructionCallback callback,
                          gfx::DXGIHandle dxgi_handle,
                          GpuMemoryBufferManager* gpu_memory_buffer_manager,
                          scoped_refptr<base::UnsafeSharedMemoryPool> pool,
                          base::span<uint8_t> premapped_memory);

  // Returns callback for reporting early result.
  // `DoMapAsync` can't invoke it directly as it holds a mapping lock.
  std::optional<base::OnceCallback<void(void)>> DoMapAsync(
      base::OnceCallback<void(bool)>);
  void CheckAsyncMapResult(bool result);

  // This is currently always set to false until media capture code is converted
  // to use MappableSI.
  bool use_premapped_memory_ = false;

  // The DXGIHandle's |region_| is not used currently. It will be eventually be
  // used to pre-map the |region_| internally in this class instead of clients
  // doing the pre-map.
  gfx::DXGIHandle dxgi_handle_;

  // It is created when |region_| is mapped via Map(). It is to keep the
  // |premapped_memory_| alive till its being used. Destroying this makes the
  // |premapped_memory_| invalid in cases where |premapped_memory_| is created
  // from it.
  base::WritableSharedMemoryMapping region_mapping_;

  raw_ptr<GpuMemoryBufferManager> gpu_memory_buffer_manager_;

  std::vector<base::OnceCallback<void(bool)>> map_callbacks_
      GUARDED_BY(map_lock_);

  // Used to create and store shared memory for data, copied via request to
  // gpu process.
  scoped_refptr<base::UnsafeSharedMemoryPool> shared_memory_pool_;
  std::unique_ptr<base::UnsafeSharedMemoryPool::Handle> shared_memory_handle_;

  // When |use_premapped_memory_| is false, the caller(which is the video
  // capturer in the render process) maps a memory region and
  // |premapped_memory_| is a view of that region owned by the caller.
  // When |use_premapped_memory_| is true, caller provides |region_| which is a
  // handle to a memory region. This region is mapped via |region_|.Map()
  // which maps the memory region and returns the handle called
  // |region_mapping_| to that mapping. |premapped_memory_| is now the view of
  // |region_mapping_| in this case.
  base::raw_span<uint8_t> premapped_memory_;
  bool async_mapping_in_progress_ GUARDED_BY(map_lock_) = false;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_DXGI_H_
