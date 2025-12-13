// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d11_image_same_adapter_copy_strategy.h"

#include <d3d11_1.h>
#include <dxgi.h>

#include "base/check.h"
#include "base/win/scoped_handle.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace gpu {

namespace {

// TODO(crbug.com/434215964) : Implement this logic later in follow up CLs.
bool IsOnSameAdapter(ID3D11Device* device1, ID3D11Device* device2) {
  return true;
}

}  // namespace

D3D11ImageSameAdapterCopyStrategy::D3D11ImageSameAdapterCopyStrategy() =
    default;
D3D11ImageSameAdapterCopyStrategy::~D3D11ImageSameAdapterCopyStrategy() =
    default;

// static
bool D3D11ImageSameAdapterCopyStrategy::CopyD3D11TextureOnSameAdapter(
    D3D11TextureAndArrayIndex source_texture,
    ID3D11Texture2D* dest_texture) {
  if (!source_texture.texture || !dest_texture) {
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> src_device;
  source_texture.texture->GetDevice(&src_device);

  Microsoft::WRL::ComPtr<ID3D11Device> dest_device;
  dest_texture->GetDevice(&dest_device);

  CHECK(IsOnSameAdapter(src_device.Get(), dest_device.Get()));

  // Get a shared handle to the destination texture.
  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  HRESULT hr = dest_texture->QueryInterface(IID_PPV_ARGS(&dxgi_resource));
  CHECK_EQ(hr, S_OK);
  HANDLE shared_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &shared_handle);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create shared handle. hr=" << std::hex << hr;
    return false;
  }
  base::win::ScopedHandle scoped_shared_handle(shared_handle);

  // Open the shared handle on the source device.
  Microsoft::WRL::ComPtr<ID3D11Device1> src_device1;
  hr = src_device.As(&src_device1);
  CHECK_EQ(hr, S_OK);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> opened_texture_on_src;
  hr = src_device1->OpenSharedResource1(shared_handle,
                                        IID_PPV_ARGS(&opened_texture_on_src));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to open shared resource on source device. hr="
               << std::hex << hr;
    return false;
  }

  // Get the keyed mutex for synchronization.
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  hr = opened_texture_on_src.As(&keyed_mutex);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get keyed mutex. hr=" << std::hex << hr;
    return false;
  }

  // Copy from the source texture to the shared texture on the source device.
  // The keyed mutex ensures that the copy operation is visible to the
  // destination device.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> src_context;
  src_device->GetImmediateContext(&src_context);
  hr = keyed_mutex->AcquireSync(0, INFINITE);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to acquire keyed mutex sync. hr=" << std::hex << hr;
    return false;
  }
  {
    DXGIScopedReleaseKeyedMutex scoped_keyed_mutex(keyed_mutex, 0);
    src_context->CopySubresourceRegion(opened_texture_on_src.Get(), 0, 0, 0, 0,
                                       source_texture.texture.Get(),
                                       source_texture.array_index, nullptr);
  }

  return true;
}

bool D3D11ImageSameAdapterCopyStrategy::CanCopy(
    SharedImageBacking* source_backing,
    SharedImageBacking* dest_backing) {
  if (source_backing->GetType() != SharedImageBackingType::kD3D ||
      dest_backing->GetType() != SharedImageBackingType::kD3D) {
    return false;
  }

  auto* d3d_source_backing = static_cast<D3DImageBacking*>(source_backing);
  auto* d3d_dest_backing = static_cast<D3DImageBacking*>(dest_backing);

  auto* src_device = d3d_source_backing->texture_d3d11_device_.Get();
  auto* dst_device = d3d_dest_backing->texture_d3d11_device_.Get();

  // This strategy is for different devices on the same adapter.
  return (src_device != dst_device) && IsOnSameAdapter(src_device, dst_device);
}

bool D3D11ImageSameAdapterCopyStrategy::Copy(SharedImageBacking* source_backing,
                                             SharedImageBacking* dest_backing) {
  auto* d3d_source_backing = static_cast<D3DImageBacking*>(source_backing);
  auto* d3d_dest_backing = static_cast<D3DImageBacking*>(dest_backing);

  if (!d3d_source_backing->BeginAccessD3D11(
          d3d_source_backing->texture_d3d11_device_, /*write_access=*/false)) {
    return false;
  }
  absl::Cleanup src_end_access = [&] {
    d3d_source_backing->EndAccessD3D11(
        d3d_source_backing->texture_d3d11_device_);
  };

  if (!d3d_dest_backing->BeginAccessD3D11(
          d3d_dest_backing->texture_d3d11_device_, /*write_access=*/true)) {
    return false;
  }
  absl::Cleanup dest_end_access = [&] {
    d3d_dest_backing->EndAccessD3D11(d3d_dest_backing->texture_d3d11_device_);
  };

  D3D11TextureAndArrayIndex source_texture(d3d_source_backing->d3d11_texture_,
                                           d3d_source_backing->array_slice_);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> dest_texture =
      d3d_dest_backing->d3d11_texture_;

  if (!source_texture.texture || !dest_texture) {
    return false;
  }

  return CopyD3D11TextureOnSameAdapter(source_texture, dest_texture.Get());
}

}  // namespace gpu
