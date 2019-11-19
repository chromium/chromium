// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows/d3d11_device_helpers.h"

#include <D3D11_1.h>
#include <dxgi.h>
#include <wrl.h>

#include "base/stl_util.h"

namespace vr {

void GetD3D11_1Adapter(int32_t* adapter_index, IDXGIAdapter** adapter) {
  // Enumerate devices until we find one that supports 11.1.
  *adapter_index = -1;
  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  bool success = SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory)));
  DCHECK(success);
  for (int i = 0; SUCCEEDED(dxgi_factory->EnumAdapters(i, adapter)); ++i) {
    D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1};
    UINT flags = 0;
    D3D_FEATURE_LEVEL feature_level_out = D3D_FEATURE_LEVEL_11_1;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
    if (SUCCEEDED(D3D11CreateDevice(
            *adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, feature_levels,
            base::size(feature_levels), D3D11_SDK_VERSION, &d3d11_device,
            &feature_level_out, &d3d11_device_context))) {
      *adapter_index = i;
      return;
    }
  }
}

void GetD3D11_1AdapterIndex(int32_t* adapter_index) {
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  GetD3D11_1Adapter(adapter_index, &adapter);
}

}  // namespace vr
