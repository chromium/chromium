// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

GpuMemoryBufferImplDXGI::~GpuMemoryBufferImplDXGI() {}

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
  base::AutoLock auto_lock(map_lock_);
  if (map_count_++)
    return true;

  if (premapped_memory_.data()) {
    return true;
  }

  DCHECK(!shared_memory_handle_);
  CHECK(gpu_memory_buffer_manager_);
  CHECK(shared_memory_pool_);

  shared_memory_handle_ = shared_memory_pool_->MaybeAllocateBuffer(
      gfx::BufferSizeForBufferFormat(size_, format_));
  if (!shared_memory_handle_) {
    --map_count_;
    return false;
  }

  // Need to perform mapping in GPU process
  if (!gpu_memory_buffer_manager_->CopyGpuMemoryBufferSync(
          CloneHandle(), shared_memory_handle_->GetRegion().Duplicate())) {
    shared_memory_handle_.reset();
    --map_count_;
    return false;
  }

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
  base::AutoLock al(map_lock_);
  DCHECK_GT(map_count_, 0u);
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
