// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/adapter.h"

#include <dxgi.h>
#include <string.h>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/platform_functions.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"

namespace webnn::dml {

// static
base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr> Adapter::GetInstance(
    DML_FEATURE_LEVEL min_feature_level_required,
    ComPtr<IDXGIAdapter> dxgi_adapter) {
  // If the `Adapter` instance is created, add a reference and return it.
  if (instance_) {
    if (!instance_->IsDMLFeatureLevelSupported(min_feature_level_required)) {
      return base::unexpected(
          CreateError(mojom::Error::Code::kNotSupportedError,
                      "The DirectML feature level on this platform is "
                      "lower than the minimum required one."));
    }
    return base::WrapRefCounted(instance_);
  }

  return Adapter::Create(std::move(dxgi_adapter), min_feature_level_required);
}

// static
base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr>
Adapter::GetInstanceForTesting(DML_FEATURE_LEVEL min_feature_level_required) {
  CHECK_IS_TEST();

  ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
    return base::unexpected(
        CreateError(mojom::Error::Code::kNotSupportedError,
                    "Failed to create an IDXGIFactory1 for testing."));
  }
  ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(factory->EnumAdapters(0, &dxgi_adapter))) {
    return base::unexpected(CreateError(
        mojom::Error::Code::kNotSupportedError,
        "Failed to get an IDXGIAdapter from EnumAdapters for testing."));
  }

  return Adapter::GetInstance(
      /*min_feature_level_required=*/min_feature_level_required,
      std::move(dxgi_adapter));
}

// static
base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr> Adapter::Create(
    ComPtr<IDXGIAdapter> dxgi_adapter,
    DML_FEATURE_LEVEL min_feature_level_required) {
  PlatformFunctions* platform_functions = PlatformFunctions::GetInstance();
  if (!platform_functions) {
    return base::unexpected(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to load all required libraries or functions "
                    "on this platform."));
  }

  bool is_d3d12_debug_layer_enabled = false;
  // Enable the d3d12 debug layer mainly for services_unittests.exe.
  if (enable_d3d12_debug_layer_for_testing_) {
    // Enable the D3D12 debug layer.
    // Must be called before the D3D12 device is created.
    auto d3d12_get_debug_interface_proc =
        platform_functions->d3d12_get_debug_interface_proc();
    ComPtr<ID3D12Debug> d3d12_debug;
    if (SUCCEEDED(d3d12_get_debug_interface_proc(IID_PPV_ARGS(&d3d12_debug)))) {
      d3d12_debug->EnableDebugLayer();
      is_d3d12_debug_layer_enabled = true;
    }
  }

  // Create d3d12 device.
  ComPtr<ID3D12Device> d3d12_device;
  auto d3d12_create_device_proc =
      platform_functions->d3d12_create_device_proc();
  HRESULT hr = d3d12_create_device_proc(
      dxgi_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12_device));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create D3D12 device: " +
                       logging::SystemErrorCodeToString(hr);
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create D3D12 device."));
  };

  // The d3d12 debug layer can also be enabled via Microsoft (R) DirectX Control
  // Panel (dxcpl.exe) for any executable apps by users.
  if (!is_d3d12_debug_layer_enabled) {
    ComPtr<ID3D12DebugDevice> debug_device;
    // Ignore failure.
    d3d12_device->QueryInterface(IID_PPV_ARGS(&debug_device));
    is_d3d12_debug_layer_enabled = (debug_device != nullptr);
  }

  // Enable the DML debug layer if the D3D12 debug layer was enabled.
  DML_CREATE_DEVICE_FLAGS flags = DML_CREATE_DEVICE_FLAG_NONE;
  if (is_d3d12_debug_layer_enabled) {
    flags |= DML_CREATE_DEVICE_FLAG_DEBUG;
  }

  // Create dml device.
  ComPtr<IDMLDevice> dml_device;
  auto dml_create_device_proc = platform_functions->dml_create_device_proc();
  hr = dml_create_device_proc(d3d12_device.Get(), flags,
                              IID_PPV_ARGS(&dml_device));
  if (FAILED(hr)) {
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
      // DirectML debug layer can fail to load even when it has been installed
      // on the system. Try again without the debug flag and see if we're
      // successful.
      flags = flags & ~DML_CREATE_DEVICE_FLAG_DEBUG;
      hr = dml_create_device_proc(d3d12_device.Get(), flags,
                                  IID_PPV_ARGS(&dml_device));
      if (FAILED(hr)) {
        DLOG(ERROR) << "Failed to create DirectML device without debug flag: " +
                           logging::SystemErrorCodeToString(hr);
        return base::unexpected(
            CreateError(mojom::Error::Code::kUnknownError,
                        "Failed to create DirectML device."));
      }
    } else {
      DLOG(ERROR) << "Failed to create DirectML device: " +
                         logging::SystemErrorCodeToString(hr);
      return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                          "Failed to create DirectML device."));
    }
  };

  const DML_FEATURE_LEVEL max_feature_level_supported =
      GetMaxSupportedDMLFeatureLevel(dml_device.Get());
  if (min_feature_level_required > max_feature_level_supported) {
    return base::unexpected(
        CreateError(mojom::Error::Code::kNotSupportedError,
                    "The DirectML feature level on this platform is lower "
                    "than the minimum required one."));
  }

  // Create command queue.
  scoped_refptr<CommandQueue> command_queue =
      CommandQueue::Create(d3d12_device.Get());
  if (!command_queue) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create command queue."));
  }

  return WrapRefCounted(new Adapter(
      std::move(dxgi_adapter), std::move(d3d12_device), std::move(dml_device),
      std::move(command_queue), max_feature_level_supported));
}

// static
void Adapter::EnableDebugLayerForTesting() {
  CHECK_IS_TEST();
  enable_d3d12_debug_layer_for_testing_ = true;
}

Adapter::Adapter(ComPtr<IDXGIAdapter> dxgi_adapter,
                 ComPtr<ID3D12Device> d3d12_device,
                 ComPtr<IDMLDevice> dml_device,
                 scoped_refptr<CommandQueue> command_queue,
                 DML_FEATURE_LEVEL max_feature_level_supported)
    : dxgi_adapter_(std::move(dxgi_adapter)),
      d3d12_device_(std::move(d3d12_device)),
      dml_device_(std::move(dml_device)),
      command_queue_(std::move(command_queue)),
      max_feature_level_supported_(max_feature_level_supported) {
  CHECK_EQ(instance_, nullptr);
  instance_ = this;
}

Adapter::~Adapter() {
  CHECK_EQ(instance_, this);
  instance_ = nullptr;
}

bool Adapter::IsDMLFeatureLevelSupported(
    DML_FEATURE_LEVEL feature_level) const {
  return feature_level <= max_feature_level_supported_;
}

bool Adapter::IsDMLDeviceCompileGraphSupportedForTesting() const {
  CHECK_IS_TEST();
  // IDMLDevice1::CompileGraph was introduced in DirectML version 1.2.0 or
  // DML_FEATURE_LEVEL_2_1.
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-feature-level-history
  return IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_2_1);
}

Adapter* Adapter::instance_ = nullptr;

bool Adapter::enable_d3d12_debug_layer_for_testing_ = false;

}  // namespace webnn::dml
