// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/adapter.h"

#include <d3d11.h>

#include "base/logging.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/platform_functions.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {

// static
scoped_refptr<Adapter> Adapter::GetInstance() {
  // If the `Adapter` instance is created, add a reference and return it.
  if (instance_) {
    return base::WrapRefCounted(instance_);
  }

  // Otherwise, create a new one with the adapter queried from ANGLE.
  ComPtr<ID3D11Device> d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Failed to query ID3D11Device from ANGLE.";
    return nullptr;
  }
  // A ID3D11Device is always QueryInteface-able to a IDXGIDevice.
  ComPtr<IDXGIDevice> dxgi_device;
  CHECK_EQ(d3d11_device.As(&dxgi_device), S_OK);
  // All DXGI devices should have adapters.
  ComPtr<IDXGIAdapter> dxgi_adapter;
  CHECK_EQ(dxgi_device->GetAdapter(&dxgi_adapter), S_OK);
  return Adapter::Create(std::move(dxgi_adapter));
}

// static
scoped_refptr<Adapter> Adapter::Create(ComPtr<IDXGIAdapter> dxgi_adapter) {
  PlatformFunctions* platformFunctions = PlatformFunctions::GetInstance();
  if (!platformFunctions) {
    return nullptr;
  }

  // Create d3d12 device.
  ComPtr<ID3D12Device> d3d12_device;
  auto d3d12_create_device_proc = platformFunctions->d3d12_create_device_proc();
  HRESULT hr = d3d12_create_device_proc(
      dxgi_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12_device));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create d3d12 device : "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  };

  // Create dml device.
  ComPtr<IDMLDevice> dml_device;
  auto dml_create_device_proc = platformFunctions->dml_create_device_proc();
  hr = dml_create_device_proc(d3d12_device.Get(), DML_CREATE_DEVICE_FLAG_NONE,
                              IID_PPV_ARGS(&dml_device));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create dml device : "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  };

  // Create command queue.
  scoped_refptr<CommandQueue> command_queue =
      CommandQueue::Create(d3d12_device.Get());
  if (!command_queue) {
    DLOG(ERROR) << "Failed to create command queue.";
    return nullptr;
  }

  return WrapRefCounted(
      new Adapter(std::move(dxgi_adapter), std::move(d3d12_device),
                  std::move(dml_device), std::move(command_queue)));
}

Adapter::Adapter(ComPtr<IDXGIAdapter> dxgi_adapter,
                 ComPtr<ID3D12Device> d3d12_device,
                 ComPtr<IDMLDevice> dml_device,
                 scoped_refptr<CommandQueue> command_queue)
    : dxgi_adapter_(std::move(dxgi_adapter)),
      d3d12_device_(std::move(d3d12_device)),
      dml_device_(std::move(dml_device)),
      command_queue_(std::move(command_queue)) {
  CHECK_EQ(instance_, nullptr);
  instance_ = this;
}

Adapter::~Adapter() {
  CHECK_EQ(instance_, this);
  instance_ = nullptr;
}

Adapter* Adapter::instance_ = nullptr;

}  // namespace webnn::dml
