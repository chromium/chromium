// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_dml.h"

#include <dxgi1_4.h>
#include <wrl/client.h>

#include "base/logging.h"
#include "d3d12.h"
#include "services/ml/dml_symbol_table.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

namespace {

using Microsoft::WRL::ComPtr;

HRESULT InitializeDirect3D12(ComPtr<ID3D12Device>& d3D12_device,
                             ComPtr<ID3D12CommandQueue>& command_queue,
                             ComPtr<ID3D12CommandAllocator>& command_allocator,
                             ComPtr<ID3D12GraphicsCommandList>& command_list) {
  Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory;
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating DXGI factory.";
    return hr;
  }

  ComPtr<IDXGIAdapter> dxgi_adapter;
  size_t adapter_index = 0;
  do {
    dxgi_adapter = nullptr;
    hr = dxgi_factory->EnumAdapters(adapter_index, &dxgi_adapter);
    if (FAILED(hr))
      return hr;
    ++adapter_index;

    hr = LATE(D3D12CreateDevice)(dxgi_adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                 IID_PPV_ARGS(&d3D12_device));
    if (hr == DXGI_ERROR_UNSUPPORTED)
      continue;
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating d3d12 device.";
      return hr;
    }
  } while (hr != S_OK);

  D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
  command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  hr = d3D12_device->CreateCommandQueue(&command_queue_desc,
                                        IID_PPV_ARGS(&command_queue));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating command queue.";
    return hr;
  }

  hr = d3D12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_PPV_ARGS(&command_allocator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating command allocator.";
    return hr;
  }

  hr = d3D12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       command_allocator.Get(), nullptr,
                                       IID_PPV_ARGS(&command_list));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating command allocator.";
    return hr;
  }
  return S_OK;
}

}  // namespace

CompilationDelegateDML::CompilationDelegateDML(
    const CompilationImpl* compilation) {
  ComPtr<ID3D12Device> d3D12_device;
  ComPtr<ID3D12CommandQueue> command_queue;
  ComPtr<ID3D12CommandAllocator> command_allocator;
  ComPtr<ID3D12GraphicsCommandList> command_list;
  // Set up Direct3D 12.
  HRESULT hr = InitializeDirect3D12(d3D12_device, command_queue,
                                    command_allocator, command_list);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed initializing D3D12.";
    return;
  }
  DLOG(INFO) << "The combination views heap size is "
            << d3D12_device->GetDescriptorHandleIncrementSize(
                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
            << "\n The render-target view heap size is "
            << d3D12_device->GetDescriptorHandleIncrementSize(
                   D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
            << "\n The depth-stencil view heap size is "
            << d3D12_device->GetDescriptorHandleIncrementSize(
                   D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

CompilationDelegateDML::~CompilationDelegateDML() {}

int32_t CompilationDelegateDML::Compile() {
  LOG(ERROR) << "Compilition of DML need to be supported.";

  return mojom::INCOMPLETE;
}

int32_t CompilationDelegateDML::CreateExecution(
    std::unique_ptr<mojom::Execution>& execution,
    mojom::ExecutionInitParamsPtr params) {
  LOG(ERROR) << "Execution of DML need to be supported";

  return mojom::INCOMPLETE;
}

}  // namespace ml
