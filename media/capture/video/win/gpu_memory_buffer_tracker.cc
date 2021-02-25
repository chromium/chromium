// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/gpu_memory_buffer_tracker.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/win/scoped_handle.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "ui/gfx/geometry/size.h"

#include <dxgi1_2.h>

namespace media {

namespace {

base::win::ScopedHandle CreateNV12Texture(ID3D11Device* d3d11_device,
                                          const gfx::Size& size) {
  const DXGI_FORMAT dxgi_format = DXGI_FORMAT_NV12;
  D3D11_TEXTURE2D_DESC desc = {
      .Width = size.width(),
      .Height = size.height(),
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = dxgi_format,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = 0,
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                   D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;

  HRESULT hr = d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create D3D11 texture: "
                << logging::SystemErrorCodeToString(hr);
    return base::win::ScopedHandle();
  }

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  CHECK(SUCCEEDED(hr));

  HANDLE texture_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &texture_handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create shared D3D11 texture handle: "
                << logging::SystemErrorCodeToString(hr);
    return base::win::ScopedHandle();
  }
  return base::win::ScopedHandle(texture_handle);
}

}  // namespace

GpuMemoryBufferTracker::GpuMemoryBufferTracker(
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager)
    : dxgi_device_manager_(std::move(dxgi_device_manager)),
      d3d_device_(dxgi_device_manager_->GetDevice()) {}

GpuMemoryBufferTracker::~GpuMemoryBufferTracker() = default;

bool GpuMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                  VideoPixelFormat format,
                                  const mojom::PlaneStridesPtr& strides) {
  // Only support NV12
  if (format != PIXEL_FORMAT_NV12) {
    NOTREACHED() << "Unsupported VideoPixelFormat " << format;
    return false;
  }

  buffer_size_ = dimensions;

  return CreateBufferInternal();
}

bool GpuMemoryBufferTracker::CreateBufferInternal() {
  gfx::GpuMemoryBufferHandle buffer_handle;
  buffer_handle.dxgi_handle =
      CreateNV12Texture(d3d_device_.Get(), buffer_size_);

  buffer_ = gpu::GpuMemoryBufferImplDXGI::CreateFromHandle(
      std::move(buffer_handle), buffer_size_,
      gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferUsage::GPU_READ,
      gpu::GpuMemoryBufferImpl::DestructionCallback());
  if (!buffer_) {
    NOTREACHED() << "Failed to create GPU memory buffer";
    return false;
  }
  return true;
}

bool GpuMemoryBufferTracker::EnsureD3DDevice() {
  // Check for and handle device loss by recreating the texture
  if (FAILED(d3d_device_->GetDeviceRemovedReason())) {
    DVLOG(1) << "Detected device loss.";
    dxgi_device_manager_->ResetDevice();
    d3d_device_ = dxgi_device_manager_->GetDevice();
    if (!d3d_device_) {
      return false;
    }

    return CreateBufferInternal();
  }
  return true;
}

bool GpuMemoryBufferTracker::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  return (format == PIXEL_FORMAT_NV12) && (dimensions == buffer_->GetSize());
}

std::unique_ptr<VideoCaptureBufferHandle>
GpuMemoryBufferTracker::GetMemoryMappedAccess() {
  NOTREACHED() << "Unsupported operation";
  return std::make_unique<NullHandle>();
}

base::UnsafeSharedMemoryRegion
GpuMemoryBufferTracker::DuplicateAsUnsafeRegion() {
  NOTREACHED() << "Unsupported operation";
  return base::UnsafeSharedMemoryRegion();
}

mojo::ScopedSharedBufferHandle GpuMemoryBufferTracker::DuplicateAsMojoBuffer() {
  NOTREACHED() << "Unsupported operation";
  return mojo::ScopedSharedBufferHandle();
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferTracker::GetGpuMemoryBufferHandle() {
  if (!EnsureD3DDevice()) {
    return gfx::GpuMemoryBufferHandle();
  }
  return buffer_->CloneHandle();
}

uint32_t GpuMemoryBufferTracker::GetMemorySizeInBytes() {
  DCHECK(buffer_);
  return (buffer_->GetSize().width() * buffer_->GetSize().height() * 3) / 2;
}

}  // namespace media