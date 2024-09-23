// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/ipc/common/dxgi_helpers.h"

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace {

constexpr char kStagingTextureLabel[] = "DxgiGmb_Map_StagingTexture";

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
  // Add debug label to the long lived texture.
  staging_texture->SetPrivateData(WKPDID_D3DDebugObjectName,
                                  strlen(kStagingTextureLabel),
                                  kStagingTextureLabel);

  return staging_texture;
}

}  // namespace

namespace gpu {

D3D11ScopedTextureUnmap::D3D11ScopedTextureUnmap(
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    : context_(std::move(context)), texture_(std::move(texture)) {}

D3D11ScopedTextureUnmap::~D3D11ScopedTextureUnmap() {
  context_->Unmap(texture_.Get(), 0);
}

DXGIScopedReleaseKeyedMutex::DXGIScopedReleaseKeyedMutex(
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
    UINT64 key)
    : keyed_mutex_(std::move(keyed_mutex)), key_(key) {
  DCHECK(keyed_mutex_);
}

DXGIScopedReleaseKeyedMutex::~DXGIScopedReleaseKeyedMutex() {
  HRESULT hr = keyed_mutex_->ReleaseSync(key_);
  DCHECK(SUCCEEDED(hr));
}

bool CopyDXGIBufferToShMem(
    HANDLE dxgi_handle,
    base::span<uint8_t> shared_memory,
    ID3D11Device* d3d11_device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* staging_texture) {
  DCHECK(d3d11_device);

  uint8_t* dest_buffer = shared_memory.data();
  size_t dst_buffer_size = shared_memory.size_bytes();

  Microsoft::WRL::ComPtr<ID3D11Device1> device1;
  HRESULT hr = d3d11_device->QueryInterface(IID_PPV_ARGS(&device1));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open D3D11_1 device. hr=" << std::hex << hr;
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

  // Open texture on device using shared handle
  hr = device1->OpenSharedResource1(dxgi_handle, IID_PPV_ARGS(&texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open shared texture. hr=" << std::hex << hr;
    return false;
  }

  return CopyD3D11TexToMem(texture.Get(), dest_buffer, dst_buffer_size,
                           d3d11_device, staging_texture);
}

bool CopyD3D11TexToMem(
    ID3D11Texture2D* src_texture,
    uint8_t* dst_buffer,
    size_t buffer_size,
    ID3D11Device* d3d11_device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* staging_texture) {
  DCHECK(d3d11_device);
  DCHECK(staging_texture);
  DCHECK(dst_buffer);
  DCHECK(src_texture);

  D3D11_TEXTURE2D_DESC texture_desc = {};
  src_texture->GetDesc(&texture_desc);

  if (texture_desc.Format != DXGI_FORMAT_NV12) {
    DLOG(ERROR) << "Can't copy non-NV12 texture. format="
                << static_cast<int>(texture_desc.Format);
    return false;
  }
  size_t copy_size = texture_desc.Height * texture_desc.Width * 3 / 2;
  if (buffer_size < copy_size) {
    DLOG(ERROR) << "Invalid buffer size for copy.";
    return false;
  }

  // The texture isn't accessible for CPU reads, thus a staging texture is used.
  bool create_staging_texture = !*staging_texture;
  if (*staging_texture) {
    D3D11_TEXTURE2D_DESC staging_texture_desc;
    (*staging_texture)->GetDesc(&staging_texture_desc);
    create_staging_texture =
        (staging_texture_desc.Width != texture_desc.Width ||
         staging_texture_desc.Height != texture_desc.Height ||
         staging_texture_desc.Format != texture_desc.Format);
  }
  if (create_staging_texture) {
    *staging_texture = CreateStagingTexture(d3d11_device, texture_desc);
    if (!*staging_texture)
      return false;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);
  HRESULT hr = S_OK;

  if (texture_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;

    hr = src_texture->QueryInterface(IID_PPV_ARGS(&keyed_mutex));

    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get keyed mutex. Error msg: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }

    // Key equal to 0 is also used by the producer. Therefore, this keyed
    // mutex acts purely as a regular mutex.
    // 300ms is long enough to get the mutex in 99.999% of cases. Yet we
    // don't want to stall the callee indefinitely if the mutex is held by
    // e.g. GpuMain thread while it's blocked on driver waiting for shader
    // compilation.
    // It's better to drop a frame in this case.
    hr = keyed_mutex->AcquireSync(0, 300);

    // Can't check FAILED(hr), because AcquireSync may return e.g. WAIT_TIMEOUT
    // value.
    if (hr != S_OK) {
      DLOG(ERROR) << "Failed to acquire keyed mutex. Error msg: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }
    DXGIScopedReleaseKeyedMutex release_keyed_mutex(keyed_mutex, 0);

    device_context->CopySubresourceRegion(staging_texture->Get(), 0, 0, 0, 0,
                                          src_texture, 0, nullptr);
  } else {
    device_context->CopySubresourceRegion(staging_texture->Get(), 0, 0, 0, 0,
                                          src_texture, 0, nullptr);
  }

  D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
  hr = device_context->Map(staging_texture->Get(), 0, D3D11_MAP_READ, 0,
                           &mapped_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to map texture for read. Error msg: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }
  D3D11ScopedTextureUnmap scoped_unmap(device_context, *staging_texture);

  const uint8_t* source_buffer = static_cast<uint8_t*>(mapped_resource.pData);
  const uint32_t source_stride = mapped_resource.RowPitch;
  const uint32_t dest_stride = texture_desc.Width;

  return libyuv::NV12Copy(source_buffer, source_stride,
                          source_buffer + texture_desc.Height * source_stride,
                          source_stride, dst_buffer, dest_stride,
                          dst_buffer + texture_desc.Height * dest_stride,
                          dest_stride, texture_desc.Width,
                          texture_desc.Height) == 0;
}

GPU_EXPORT bool CopyShMemToDXGIBuffer(base::span<uint8_t> shared_memory,
                                      HANDLE dxgi_handle,
                                      ID3D11Device* d3d11_device) {
  CHECK(d3d11_device);

  uint8_t* src_buffer = shared_memory.data();
  size_t src_buffer_size = shared_memory.size_bytes();

  Microsoft::WRL::ComPtr<ID3D11Device1> device1;
  HRESULT hr = d3d11_device->QueryInterface(IID_PPV_ARGS(&device1));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open D3D11_1 device. hr=" << std::hex << hr;
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

  // Open texture on device using shared handle
  hr = device1->OpenSharedResource1(dxgi_handle, IID_PPV_ARGS(&texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open shared texture. hr=" << std::hex << hr;
    return false;
  }

  return CopyMemToD3D11Tex(src_buffer, src_buffer_size, texture.Get(),
                           d3d11_device);
}

GPU_EXPORT bool CopyMemToD3D11Tex(uint8_t* src_buffer,
                                  size_t buffer_size,
                                  ID3D11Texture2D* output_texture,
                                  ID3D11Device* d3d11_device) {
  CHECK(d3d11_device);
  CHECK(src_buffer);
  CHECK(output_texture);

  D3D11_TEXTURE2D_DESC texture_desc = {};
  output_texture->GetDesc(&texture_desc);

  if (texture_desc.Format != DXGI_FORMAT_NV12) {
    DLOG(ERROR) << "Can't copy non-NV12 texture. format="
                << static_cast<int>(texture_desc.Format);
    return false;
  }
  size_t copy_size = texture_desc.Height * texture_desc.Width * 3 / 2;
  if (buffer_size < copy_size) {
    DLOG(ERROR) << "Invalid buffer size for copy.";
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);
  HRESULT hr = S_OK;

  if (texture_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;

    hr = output_texture->QueryInterface(IID_PPV_ARGS(&keyed_mutex));

    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get keyed mutex. Error msg: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }

    // Key equal to 0 is also used by the producer. Therefore, this keyed
    // mutex acts purely as a regular mutex.
    hr = keyed_mutex->AcquireSync(0, INFINITE);
    // Can't check FAILED(hr), because AcquireSync may return e.g. WAIT_TIMEOUT
    // value.
    if (hr != S_OK) {
      DLOG(ERROR) << "Failed to acquire keyed mutex. Error msg: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }
    DXGIScopedReleaseKeyedMutex release_keyed_mutex(keyed_mutex, 0);

    device_context->UpdateSubresource(output_texture, 0, nullptr, src_buffer,
                                      texture_desc.Width, copy_size);
  } else {
    device_context->UpdateSubresource(output_texture, 0, nullptr, src_buffer,
                                      texture_desc.Width, copy_size);
  }

  return true;
}

}  // namespace gpu