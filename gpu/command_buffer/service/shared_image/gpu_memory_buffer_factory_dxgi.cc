// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory_dxgi.h"

#include "base/logging.h"
#include "ui/gl/gl_angle_util_win.h"

namespace gpu {

GpuMemoryBufferFactoryDXGI::GpuMemoryBufferFactoryDXGI() {
  DETACH_FROM_THREAD(thread_checker_);
}

GpuMemoryBufferFactoryDXGI::~GpuMemoryBufferFactoryDXGI() = default;

// TODO(crbug.com/40774668): Avoid the need for a separate D3D device here by
// sharing keyed mutex state between DXGI GMBs and D3D shared image backings.
Microsoft::WRL::ComPtr<ID3D11Device>
GpuMemoryBufferFactoryDXGI::GetOrCreateD3D11Device() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!d3d11_device_ || FAILED(d3d11_device_->GetDeviceRemovedReason())) {
    // Reset device if it was removed.
    d3d11_device_ = nullptr;
    // Use same adapter as ANGLE device.
    auto angle_d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
    if (!angle_d3d11_device) {
      DLOG(ERROR) << "Failed to get ANGLE D3D11 device";
      return nullptr;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> angle_dxgi_device;
    HRESULT hr = angle_d3d11_device.As(&angle_dxgi_device);
    CHECK(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter = nullptr;
    hr = FAILED(angle_dxgi_device->GetAdapter(&dxgi_adapter));
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetAdapter failed with error 0x" << std::hex << hr;
      return nullptr;
    }

    // If adapter is not null, driver type must be D3D_DRIVER_TYPE_UNKNOWN
    // otherwise D3D11CreateDevice will return E_INVALIDARG.
    // See
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice#return-value
    const D3D_DRIVER_TYPE driver_type =
        dxgi_adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

    // It's ok to use D3D11_CREATE_DEVICE_SINGLETHREADED because this device is
    // only ever used on the IO thread (verified by |thread_checker_|).
    const UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

    // Using D3D_FEATURE_LEVEL_11_1 is ok since we only support D3D11 when the
    // platform update containing DXGI 1.2 is present on Win7.
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1};

    hr = D3D11CreateDevice(dxgi_adapter.Get(), driver_type,
                           /*Software=*/nullptr, flags, feature_levels,
                           std::size(feature_levels), D3D11_SDK_VERSION,
                           &d3d11_device_, /*pFeatureLevel=*/nullptr,
                           /*ppImmediateContext=*/nullptr);
    if (FAILED(hr)) {
      DLOG(ERROR) << "D3D11CreateDevice failed with error 0x" << std::hex << hr;
      return nullptr;
    }

    const char* kDebugName = "GPUIPC_GpuMemoryBufferFactoryDXGI";
    d3d11_device_->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(kDebugName),
                                  kDebugName);
  }
  DCHECK(d3d11_device_);
  return d3d11_device_;
}

}  // namespace gpu
