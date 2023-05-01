// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/adapter.h"

#include "base/logging.h"
#include "services/webnn/dml/platform_functions.h"

namespace webnn::dml {

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
  ComPtr<ID3D12CommandQueue> command_queue;
  D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
  command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  hr = d3d12_device->CreateCommandQueue(&command_queue_desc,
                                        IID_PPV_ARGS(&command_queue));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create command queue : "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return WrapRefCounted(
      new Adapter(std::move(dxgi_adapter), std::move(d3d12_device),
                  std::move(dml_device), std::move(command_queue)));
}

Adapter::Adapter(ComPtr<IDXGIAdapter> dxgi_adapter,
                 ComPtr<ID3D12Device> d3d12_device,
                 ComPtr<IDMLDevice> dml_device,
                 ComPtr<ID3D12CommandQueue> command_queue)
    : dxgi_adapter_(std::move(dxgi_adapter)),
      d3d12_device_(std::move(d3d12_device)),
      dml_device_(std::move(dml_device)),
      command_queue_(std::move(command_queue)) {}

Adapter::~Adapter() = default;

}  // namespace webnn::dml
