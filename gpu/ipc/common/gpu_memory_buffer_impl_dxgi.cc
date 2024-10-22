// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

GpuMemoryBufferImplDXGI::~GpuMemoryBufferImplDXGI() {
  base::AutoLock auto_lock(map_lock_);
  CHECK(!async_mapping_in_progress_);
}

std::unique_ptr<GpuMemoryBufferImplDXGI>
GpuMemoryBufferImplDXGI::CreateFromHandle(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    DestructionCallback callback,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool,
    base::span<uint8_t> premapped_memory) {
  DCHECK(handle.dxgi_handle.IsValid());
  DCHECK(handle.dxgi_token.has_value());
  return base::WrapUnique(new GpuMemoryBufferImplDXGI(
      handle.id, size, format, std::move(callback),
      std::move(handle.dxgi_handle), std::move(handle.dxgi_token.value()),
      gpu_memory_buffer_manager, std::move(pool), premapped_memory));
}

base::OnceClosure GpuMemoryBufferImplDXGI::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  // This test only works with hardware rendering.
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseGpuInTests));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();

  DCHECK(format == gfx::BufferFormat::RGBA_8888 ||
         format == gfx::BufferFormat::RGBX_8888);
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

  gfx::GpuMemoryBufferId kBufferId(1);
  handle->dxgi_handle.Set(texture_handle);
  handle->type = gfx::DXGI_SHARED_HANDLE;
  handle->id = kBufferId;
  return base::DoNothing();
}

bool GpuMemoryBufferImplDXGI::Map() {
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

void GpuMemoryBufferImplDXGI::MapAsync(
    base::OnceCallback<void(bool)> result_cb) {
  std::optional<base::OnceCallback<void(void)>> early_result;
  early_result = DoMapAsync(std::move(result_cb));
  // Can't run the callback inside DoMapAsync because it grabs the lock.
  if (early_result.has_value()) {
    std::move(*early_result).Run();
  }
}

std::optional<base::OnceCallback<void(void)>>
GpuMemoryBufferImplDXGI::DoMapAsync(base::OnceCallback<void(bool)> result_cb) {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_ > 0) {
    ++map_count_;
    return base::BindOnce(std::move(result_cb), true);
  }

  if (premapped_memory_.data()) {
    ++map_count_;
    return base::BindOnce(std::move(result_cb), true);
  }

  CHECK(gpu_memory_buffer_manager_);
  CHECK(shared_memory_pool_);

  if (!shared_memory_handle_) {
    shared_memory_handle_ = shared_memory_pool_->MaybeAllocateBuffer(
        gfx::BufferSizeForBufferFormat(size_, format_));
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
  gpu_memory_buffer_manager_->CopyGpuMemoryBufferAsync(
      CloneHandle(), shared_memory_handle_->GetRegion().Duplicate(),
      base::BindOnce(&GpuMemoryBufferImplDXGI::CheckAsyncMapResult,
                     base::Unretained(this)));

  return std::nullopt;
}

void GpuMemoryBufferImplDXGI::CheckAsyncMapResult(
    bool result) {
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

bool GpuMemoryBufferImplDXGI::AsyncMappingIsNonBlocking() const {
  return true;
}

void* GpuMemoryBufferImplDXGI::memory(size_t plane) {
  AssertMapped();

  if (plane > gfx::NumberOfPlanesForLinearBufferFormat(format_) ||
      (!shared_memory_handle_ && !premapped_memory_.data())) {
    return nullptr;
  }

  uint8_t* plane_addr =
      (premapped_memory_.data() ? premapped_memory_.data()
                                : shared_memory_handle_->GetMapping()
                                      .GetMemoryAsSpan<uint8_t>()
                                      .data());
  // This is safe, since we already checked that the requested plane is
  // valid for current buffer format.
  plane_addr += gfx::BufferOffsetForBufferFormat(size_, format_, plane);
  return plane_addr;
}

void GpuMemoryBufferImplDXGI::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  CHECK(!async_mapping_in_progress_);
  if (--map_count_)
    return;

  if (shared_memory_handle_) {
    shared_memory_handle_.reset();
  }
}

int GpuMemoryBufferImplDXGI::stride(size_t plane) const {
  return gfx::RowSizeForBufferFormat(size_.width(), format_, plane);
}

gfx::GpuMemoryBufferType GpuMemoryBufferImplDXGI::GetType() const {
  return gfx::DXGI_SHARED_HANDLE;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplDXGI::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::DXGI_SHARED_HANDLE;
  handle.id = id_;
  handle.offset = 0;
  handle.stride = stride(0);
  base::ProcessHandle process = ::GetCurrentProcess();
  HANDLE duplicated_handle;
  BOOL result =
      ::DuplicateHandle(process, dxgi_handle_.Get(), process,
                        &duplicated_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
  if (!result)
    DPLOG(ERROR) << "Failed to duplicate DXGI resource handle.";
  handle.dxgi_handle.Set(duplicated_handle);
  handle.dxgi_token = dxgi_token_;

  return handle;
}

HANDLE GpuMemoryBufferImplDXGI::GetHandle() const {
  return dxgi_handle_.Get();
}

const gfx::DXGIHandleToken& GpuMemoryBufferImplDXGI::GetToken() const {
  return dxgi_token_;
}

GpuMemoryBufferImplDXGI::GpuMemoryBufferImplDXGI(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    DestructionCallback callback,
    base::win::ScopedHandle dxgi_handle,
    gfx::DXGIHandleToken dxgi_token,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool,
    base::span<uint8_t> premapped_memory)
    : GpuMemoryBufferImpl(id, size, format, std::move(callback)),
      dxgi_handle_(std::move(dxgi_handle)),
      dxgi_token_(std::move(dxgi_token)),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      shared_memory_pool_(std::move(pool)),
      premapped_memory_(premapped_memory) {
  if (premapped_memory_.data() &&
      premapped_memory_.size() <
          gfx::BufferSizeForBufferFormat(size_, format_)) {
    LOG(ERROR)
        << "GpuMemoryBufferImplDXGI: Premapped memory has insufficient size.";
    premapped_memory_ = base::span<uint8_t>();
  }
}
}  // namespace gpu
