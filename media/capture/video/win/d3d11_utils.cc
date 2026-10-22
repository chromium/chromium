// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/d3d11_utils.h"

#include <dxgi1_2.h>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "media/base/win/mf_helpers.h"

namespace media {

base::win::ScopedHandle CreateNV12Texture(ID3D11Device* d3d11_device,
                                          const gfx::Size& size,
                                          const char* debug_name) {
  const DXGI_FORMAT dxgi_format = DXGI_FORMAT_NV12;
  D3D11_TEXTURE2D_DESC desc = {
      .Width = static_cast<UINT>(size.width()),
      .Height = static_cast<UINT>(size.height()),
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = dxgi_format,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = 0,
      .MiscFlags =
          D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;

  HRESULT hr = d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create D3D11 texture: "
                << logging::SystemErrorCodeToString(hr);
    return base::win::ScopedHandle();
  }
  hr = SetDebugName(d3d11_texture.Get(), debug_name);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to label D3D11 texture: "
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

bool HasD3DDeviceChanged(DXGIDeviceManager* dxgi_device_manager,
                         ID3D11Device* expected_device) {
  CHECK(dxgi_device_manager);
  Microsoft::WRL::ComPtr<ID3D11Device> recreated_d3d_device;
  HRESULT hr = dxgi_device_manager->CheckDeviceRemovedAndGetDevice(
      &recreated_d3d_device);
  if (FAILED(hr)) {
    LOG(ERROR) << "Detected device loss: "
               << logging::SystemErrorCodeToString(hr);
    base::UmaHistogramSparse("Media.VideoCapture.Win.D3DDeviceRemovedReason",
                             hr);
  }
  return recreated_d3d_device.Get() != expected_device;
}

}  // namespace media
