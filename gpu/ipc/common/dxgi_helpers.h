// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_DXGI_HELPERS_H_
#define GPU_IPC_COMMON_DXGI_HELPERS_H_

#include <d3d11_1.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "base/containers/span.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "gpu/gpu_export.h"

namespace gpu {

// This helper class can be used to securely unmap texture.
// Create one after successful ID3D11DeviceContext::Map() with the
// mapped texture and device context.
// Upon destruction the texture will be automatically unmapped.
class GPU_EXPORT D3D11ScopedTextureUnmap {
 public:
  D3D11ScopedTextureUnmap(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                          Microsoft::WRL::ComPtr<ID3D11Texture2D> texture);

  D3D11ScopedTextureUnmap(const D3D11ScopedTextureUnmap&) = delete;

  D3D11ScopedTextureUnmap& operator=(const D3D11ScopedTextureUnmap&) = delete;

  ~D3D11ScopedTextureUnmap();

 private:
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
};

// This helper class can be used to securely release KeyedMutex.
// Create one after successful acquisition of the keyed mutex.
// Upon destruction the mutex would be automatically released.
class GPU_EXPORT DXGIScopedReleaseKeyedMutex {
 public:
  DXGIScopedReleaseKeyedMutex(
      Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
      UINT64 key);

  DXGIScopedReleaseKeyedMutex(const DXGIScopedReleaseKeyedMutex&) = delete;

  DXGIScopedReleaseKeyedMutex& operator=(const DXGIScopedReleaseKeyedMutex&) =
      delete;

  ~DXGIScopedReleaseKeyedMutex();

 private:
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  UINT64 key_ = 0;
};

// Copies |dxgi_handle| data to |shared_memory| using provided D3D11 device, and
// a staging texture. The texture may be recreated if it has wrong size or
// format. Returns true if succeeded.
GPU_EXPORT bool CopyDXGIBufferToShMem(
    HANDLE dxgi_handle,
    base::span<uint8_t> shared_memory,
    ID3D11Device* d3d11_device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* staging_texture);

// Copies from |input_texture| to |dst_buffer| using provided D3D11 device, and
// a staging texture. The staging texture may be recreated if it does not match
// input texture size or format. Returns true if succeeded.
GPU_EXPORT bool CopyD3D11TexToMem(
    ID3D11Texture2D* input_texture,
    uint8_t* dst_buffer,
    size_t buffer_size,
    ID3D11Device* d3d11_device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* staging_texture);

GPU_EXPORT bool CopyShMemToDXGIBuffer(base::span<uint8_t> shared_memory,
                                      HANDLE dxgi_handle,
                                      ID3D11Device* d3d11_device);

GPU_EXPORT bool CopyMemToD3D11Tex(uint8_t* src_buffer,
                                  size_t buffer_size,
                                  ID3D11Texture2D* output_texture,
                                  ID3D11Device* d3d11_device);
}  // namespace gpu

#endif  // GPU_IPC_COMMON_DXGI_HELPERS_H_
