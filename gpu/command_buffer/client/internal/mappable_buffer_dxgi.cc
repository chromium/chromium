// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/internal/mappable_buffer_dxgi.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

MappableBufferDXGI::~MappableBufferDXGI() {
  base::AutoLock auto_lock(map_lock_);
  CHECK(!async_mapping_in_progress_);
  CHECK_EQ(map_count_, 0u);
}

std::unique_ptr<MappableBufferDXGI> MappableBufferDXGI::CreateFromHandle(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    CopyNativeBufferToShMemCallback copy_native_buffer_to_shmem_callback,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool) {
  DCHECK(handle.dxgi_handle().IsValid());
  CHECK(viz::HasEquivalentBufferFormat(format));
  return base::WrapUnique(new MappableBufferDXGI(
      size, format, std::move(handle).dxgi_handle(),
      std::move(copy_native_buffer_to_shmem_callback), std::move(pool)));
}

base::OnceClosure MappableBufferDXGI::AllocateForTesting(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  // This test only works with hardware rendering.
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseGpuInTests));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  DCHECK(format == viz::SinglePlaneFormat::kRGBA_8888 ||
         format == viz::SinglePlaneFormat::kRGBX_8888);
  DCHECK(usage == gfx::BufferUsage::GPU_READ ||
         usage == gfx::BufferUsage::SCANOUT);

  D3D11_TEXTURE2D_DESC desc = {
      static_cast<UINT>(size.width()),
      static_cast<UINT>(size.height()),
      1,
      1,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      {1, 0},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      0,
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
          D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;

  HRESULT hr = d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  DCHECK(SUCCEEDED(hr));

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  DCHECK(SUCCEEDED(hr));

  HANDLE texture_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &texture_handle);
  DCHECK(SUCCEEDED(hr));

  *handle = gfx::GpuMemoryBufferHandle(
      gfx::DXGIHandle(base::win::ScopedHandle(texture_handle)));
  return base::DoNothing();
}

bool MappableBufferDXGI::Map() {
  base::WaitableEvent event;
  bool mapping_result = false;
  // Note: this can be called from multiple threads at the same time. Some of
  // those threads may not have a TaskRunner set.
  // One of such threads is a WebRTC encoder thread.
  // That thread is not owned by chromium and therefore doesn't have any
  // blocking scope machinery. But the workload there is supposed to happen
  // synchronously, because this is how the WebRTC architecture is designed.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow;
  MapAsync(base::BindOnce(
      [](base::WaitableEvent* event, bool* result_ptr, bool result) {
        *result_ptr = result;
        event->Signal();
      },
      &event, &mapping_result));
  event.Wait();
  return mapping_result;
}

void MappableBufferDXGI::MapAsync(base::OnceCallback<void(bool)> result_cb) {
  std::optional<base::OnceCallback<void(void)>> early_result;
  early_result = DoMapAsync(std::move(result_cb));
  // Can't run the callback inside DoMapAsync because it grabs the lock.
  if (early_result.has_value()) {
    std::move(*early_result).Run();
  }
}

std::optional<base::OnceCallback<void(void)>> MappableBufferDXGI::DoMapAsync(
    base::OnceCallback<void(bool)> result_cb) {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_ > 0) {
    ++map_count_;
    return base::BindOnce(std::move(result_cb), true);
  }

  // Only client which uses premapped_memory is media capture code.
  // If |use_premapped_memory_| is set to true, we will use shared memory
  // provided in GpuMemoryBufferHandle instead of allocating temporary one. In
  // this case we also don't need to do a copy, because client already put data
  // inside shmem.
  if (use_premapped_memory_) {
    if (!premapped_memory_.data()) {
      CHECK(dxgi_handle_.region().IsValid());
      region_mapping_ = dxgi_handle_.region().Map();
      CHECK(region_mapping_.IsValid());
      premapped_memory_ = region_mapping_.GetMemoryAsSpan<uint8_t>();

      size_t buffer_size =
          viz::SharedMemorySizeForSharedImageFormat(format_, size_).value();
      if (premapped_memory_.data() && premapped_memory_.size() < buffer_size) {
        LOG(ERROR) << "MappableBufferDXGI: Premapped memory has "
                      "insufficient size.";
        premapped_memory_ = base::span<uint8_t>();
        region_mapping_ = base::WritableSharedMemoryMapping();
        use_premapped_memory_ = false;
        return base::BindOnce(std::move(result_cb), false);
      }
    }
    CHECK(premapped_memory_.data());
    ++map_count_;
    return base::BindOnce(std::move(result_cb), true);
  }

  CHECK(copy_native_buffer_to_shmem_callback_);
  CHECK(shared_memory_pool_);

  size_t buffer_size =
      viz::SharedMemorySizeForSharedImageFormat(format_, size_).value();
  if (!shared_memory_handle_) {
    shared_memory_handle_ =
        shared_memory_pool_->MaybeAllocateBuffer(buffer_size);
    if (!shared_memory_handle_) {
      return base::BindOnce(std::move(result_cb), false);
    }
  }

  map_callbacks_.push_back(std::move(result_cb));
  if (async_mapping_in_progress_) {
    return std::nullopt;
  }
  async_mapping_in_progress_ = true;
  // Need to perform mapping in GPU process
  // Unretained is safe because of GMB isn't destroyed before the callback
  // executes. This is CHECKed in the destructor.
  copy_native_buffer_to_shmem_callback_.Run(
      CloneHandle(), shared_memory_handle_->GetRegion().Duplicate(),
      base::BindOnce(&MappableBufferDXGI::CheckAsyncMapResult,
                     base::Unretained(this)));

  return std::nullopt;
}

void MappableBufferDXGI::CheckAsyncMapResult(bool result) {
  std::vector<base::OnceCallback<void(bool)>> map_callbacks;
  {
    // Must not hold the lock during the callbacks calls.
    base::AutoLock auto_lock(map_lock_);
    CHECK_EQ(map_count_, 0u);
    CHECK(async_mapping_in_progress_);

    if (result) {
      map_count_ += map_callbacks_.size();
    }

    async_mapping_in_progress_ = false;
    swap(map_callbacks_, map_callbacks);
  }
  for (auto& cb : map_callbacks) {
    std::move(cb).Run(result);
  }
}

bool MappableBufferDXGI::AsyncMappingIsNonBlocking() const {
  return true;
}

void* MappableBufferDXGI::memory(size_t plane) {
  AssertMapped();

  if (static_cast<int>(plane) > format_.NumberOfPlanes() ||
      (!shared_memory_handle_ && !premapped_memory_.data())) {
    return nullptr;
  }

  uint8_t* plane_addr = (use_premapped_memory_ && premapped_memory_.data())
                            ? premapped_memory_.data()
                            : shared_memory_handle_->GetMapping()
                                  .GetMemoryAsSpan<uint8_t>()
                                  .data();
  // This is safe, since we already checked that the requested plane is
  // valid for current format.
  UNSAFE_TODO(plane_addr += viz::SharedMemoryOffsetForSharedImageFormat(
                  format_, plane, size_));
  return plane_addr;
}

void MappableBufferDXGI::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  CHECK(!async_mapping_in_progress_);
  if (--map_count_) {
    return;
  }

  if (shared_memory_handle_) {
    shared_memory_handle_.reset();
  }
}

int MappableBufferDXGI::stride(size_t plane) const {
  size_t stride = viz::SharedMemoryRowSizeForSharedImageFormat(format_, plane,
                                                               size_.width())
                      .value();
  return static_cast<int>(stride);
}

gfx::GpuMemoryBufferType MappableBufferDXGI::GetType() const {
  return gfx::DXGI_SHARED_HANDLE;
}

gfx::GpuMemoryBufferHandle MappableBufferDXGI::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle(dxgi_handle_.Clone());
  handle.offset = 0;
  handle.stride = stride(0);

  return handle;
}

void MappableBufferDXGI::SetUsePreMappedMemory(bool use_premapped_memory) {
  use_premapped_memory_ = use_premapped_memory;
}

gfx::GpuMemoryBufferHandle MappableBufferDXGI::CloneHandleWithRegion(
    base::UnsafeSharedMemoryRegion region) const {
  gfx::GpuMemoryBufferHandle handle(
      dxgi_handle_.CloneWithRegion(std::move(region)));
  handle.offset = 0;
  handle.stride = stride(0);
  return handle;
}

HANDLE MappableBufferDXGI::GetHandle() const {
  return dxgi_handle_.buffer_handle();
}

const gfx::DXGIHandleToken& MappableBufferDXGI::GetToken() const {
  return dxgi_handle_.token();
}

void MappableBufferDXGI::AssertMapped() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
}

MappableBufferDXGI::MappableBufferDXGI(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::DXGIHandle dxgi_handle,
    CopyNativeBufferToShMemCallback copy_native_buffer_to_shmem_callback,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool)
    : size_(size),
      format_(format),
      dxgi_handle_(std::move(dxgi_handle)),
      copy_native_buffer_to_shmem_callback_(
          std::move(copy_native_buffer_to_shmem_callback)),
      shared_memory_pool_(std::move(pool)) {}

}  // namespace gpu
