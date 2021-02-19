// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_dxgi.h"
#include <vector>
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_dxgi.h"

namespace {

class ScopedTextureUnmap {
 public:
  ScopedTextureUnmap(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                     Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
      : context_(context), texture_(texture) {}

  ScopedTextureUnmap(const ScopedTextureUnmap&) = delete;

  ScopedTextureUnmap& operator=(const ScopedTextureUnmap&) = delete;

  ~ScopedTextureUnmap() { context_->Unmap(texture_.Get(), 0); }

 private:
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
};

class ScopedReleaseKeyedMutex {
 public:
  ScopedReleaseKeyedMutex(Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
                          UINT64 key)
      : keyed_mutex_(keyed_mutex), key_(key) {
    DCHECK(keyed_mutex);
  }

  ScopedReleaseKeyedMutex(const ScopedReleaseKeyedMutex&) = delete;

  ScopedReleaseKeyedMutex& operator=(const ScopedReleaseKeyedMutex&) = delete;

  ~ScopedReleaseKeyedMutex() {
    HRESULT hr = keyed_mutex_->ReleaseSync(key_);
    DCHECK(SUCCEEDED(hr));
  }

 private:
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  UINT64 key_ = 0;
};

Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateStagingTexture(
    ID3D11Device* d3d11_device,
    D3D11_TEXTURE2D_DESC input_desc) {
  D3D11_TEXTURE2D_DESC staging_desc = {};
  staging_desc.Width = input_desc.Width;
  staging_desc.Height = input_desc.Height;
  staging_desc.Format = input_desc.Format;
  staging_desc.MipLevels = 1;
  staging_desc.ArraySize = 1;
  staging_desc.SampleDesc.Count = 1;
  staging_desc.Usage = D3D11_USAGE_STAGING;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture;
  HRESULT hr =
      d3d11_device->CreateTexture2D(&staging_desc, nullptr, &staging_texture);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create staging texture. hr=" << std::hex << hr;
    return nullptr;
  }

  return staging_texture;
}

}  // namespace

namespace gpu {

GpuMemoryBufferFactoryDXGI::GpuMemoryBufferFactoryDXGI() {}

GpuMemoryBufferFactoryDXGI::~GpuMemoryBufferFactoryDXGI() {}

gfx::GpuMemoryBufferHandle GpuMemoryBufferFactoryDXGI::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    const gfx::Size& framebuffer_size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
  TRACE_EVENT0("gpu", "GpuMemoryBufferFactoryDXGI::CreateGpuMemoryBuffer");
  DCHECK_EQ(framebuffer_size, size);

  gfx::GpuMemoryBufferHandle handle;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device)
    return handle;

  DXGI_FORMAT dxgi_format;
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
      break;
    default:
      NOTREACHED();
      return handle;
  }

  // We are binding as a shader resource and render target regardless of usage,
  // so make sure that the usage is one that we support.
  DCHECK(usage == gfx::BufferUsage::GPU_READ ||
         usage == gfx::BufferUsage::SCANOUT);

  D3D11_TEXTURE2D_DESC desc = {
      size.width(),
      size.height(),
      1,
      1,
      dxgi_format,
      {1, 0},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      0,
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
          D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;

  if (FAILED(d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture)))
    return handle;

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  if (FAILED(d3d11_texture.As(&dxgi_resource)))
    return handle;

  HANDLE texture_handle;
  if (FAILED(dxgi_resource->CreateSharedHandle(
          nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
          nullptr, &texture_handle)))
    return handle;

  size_t buffer_size;
  if (!BufferSizeForBufferFormatChecked(size, format, &buffer_size))
    return handle;

  handle.dxgi_handle.Set(texture_handle);
  handle.type = gfx::DXGI_SHARED_HANDLE;
  handle.id = id;

  return handle;
}

void GpuMemoryBufferFactoryDXGI::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {}

bool GpuMemoryBufferFactoryDXGI::FillSharedMemoryRegionWithBufferContents(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory) {
  DCHECK_EQ(buffer_handle.type, gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device)
    return false;

  Microsoft::WRL::ComPtr<ID3D11Device1> device1;
  HRESULT hr = d3d11_device.As(&device1);
  CHECK(SUCCEEDED(hr));

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

  // Open texture on device using shared handle
  hr = device1->OpenSharedResource1(buffer_handle.dxgi_handle.Get(),
                                    IID_PPV_ARGS(&texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open shared texture. hr=" << std::hex << hr;
    return false;
  }

  // If texture isn't accessible for CPU reads, create a staging texture
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture->GetDesc(&texture_desc);

  if (texture_desc.Format != DXGI_FORMAT_NV12) {
    DLOG(ERROR) << "Can't copy non-NV12 texture. format="
                << static_cast<int>(texture_desc.Format);
    return false;
  }

  bool create_staging_texture = !staging_texture_;
  if (staging_texture_) {
    D3D11_TEXTURE2D_DESC staging_texture_desc;
    staging_texture_->GetDesc(&staging_texture_desc);
    create_staging_texture =
        (staging_texture_desc.Width != texture_desc.Width ||
         staging_texture_desc.Height != texture_desc.Height ||
         staging_texture_desc.Format != texture_desc.Format);
  }
  if (create_staging_texture) {
    staging_texture_ = CreateStagingTexture(d3d11_device.Get(), texture_desc);
    if (!staging_texture_)
      return false;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);

  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  texture.As(&keyed_mutex);

  // A keyed mutex will only exist for shared textures
  base::Optional<ScopedReleaseKeyedMutex> release_keyed_mutex;
  if (keyed_mutex) {
    const int kAcquireKeyedMutexTimeout = 1000;
    // Key equal to 0 is also used by the producer. Therefore, this keyed mutex
    // acts purely as a regular mutex.
    // TODO: Add a constant for the key value and use it both here and at the
    // producer.
    hr = keyed_mutex->AcquireSync(0, kAcquireKeyedMutexTimeout);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to acquire keyed mutex. hr=" << std::hex << hr;
      return false;
    }
    release_keyed_mutex.emplace(keyed_mutex, 0);
  }

  device_context->CopySubresourceRegion(staging_texture_.Get(), 0, 0, 0, 0,
                                        texture.Get(), 0, nullptr);

  D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
  hr = device_context->Map(staging_texture_.Get(), 0, D3D11_MAP_READ, 0,
                           &mapped_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to map texture for read. hr=" << std::hex << hr;
    return false;
  }
  ScopedTextureUnmap scoped_unmap(device_context, staging_texture_);

  // Copy mapped texture to shared memory region for client
  size_t buffer_size = texture_desc.Height * texture_desc.Width * 3 / 2;
  if (shared_memory.GetSize() < buffer_size)
    return false;

  base::WritableSharedMemoryMapping mapping = shared_memory.Map();
  const uint8_t* source_buffer =
      reinterpret_cast<uint8_t*>(mapped_resource.pData);
  uint8_t* dest_buffer = reinterpret_cast<uint8_t*>(mapping.memory());

  // Direct copy if rows don't have extra padding
  if (texture_desc.Width == mapped_resource.RowPitch) {
    std::copy(source_buffer, source_buffer + buffer_size, dest_buffer);
  } else {
    const uint32_t source_stride = mapped_resource.RowPitch;
    const uint32_t dest_stride = texture_desc.Width;
    for (size_t i = 0; i < texture_desc.Height * 3 / 2; i++) {
      std::copy(source_buffer, source_buffer + dest_stride, dest_buffer);
      source_buffer += source_stride;
      dest_buffer += dest_stride;
    }
  }

  return true;
}

ImageFactory* GpuMemoryBufferFactoryDXGI::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryDXGI::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int client_id,
    SurfaceHandle surface_handle) {
  if (handle.type != gfx::DXGI_SHARED_HANDLE)
    return nullptr;
  // Transfer ownership of handle to GLImageDXGI.
  auto image = base::MakeRefCounted<gl::GLImageDXGI>(size, nullptr);
  if (!image->InitializeHandle(std::move(handle.dxgi_handle), 0, format))
    return nullptr;
  return image;
}

unsigned GpuMemoryBufferFactoryDXGI::RequiredTextureType() {
  return GL_TEXTURE_2D;
}

bool GpuMemoryBufferFactoryDXGI::SupportsFormatRGB() {
  return true;
}
}  // namespace gpu
