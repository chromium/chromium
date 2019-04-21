// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "services/ml/compilation_delegate_dml.h"

#include <dxgi1_4.h>

#include "base/logging.h"
#include "services/ml/dml_d3dx12_utils.h"
#include "services/ml/dml_symbol_table.h"
#include "services/ml/ml_utils_dml.h"
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

    hr = D3D(D3D12CreateDevice)(dxgi_adapter.Get(), D3D_FEATURE_LEVEL_12_0,
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
    const CompilationImpl* compilation)
    : compilation_(compilation),
      execute_descriptor_count_(0),
      execute_temporary_resource_size_(0) {
  dml_ = std::make_unique<ExecutionData>();

  // Set up Direct3D 12.
  HRESULT hr =
      InitializeDirect3D12(dml_->d3D12_device_, dml_->command_queue_,
                           dml_->command_allocator_, dml_->command_list_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed initializing D3D12.";
    return;
  }
  DLOG(INFO) << "The combination views heap size is "
             << dml_->d3D12_device_->GetDescriptorHandleIncrementSize(
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  // Create the DirectML device.
  DML_CREATE_DEVICE_FLAGS dml_create_device_flags = DML_CREATE_DEVICE_FLAG_NONE;
  hr = DML(DMLCreateDevice)(dml_->d3D12_device_.Get(), dml_create_device_flags,
                            IID_PPV_ARGS(&dml_->dml_device_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating DirectML.";
    return;
  }
}

CompilationDelegateDML::~CompilationDelegateDML() = default;

int32_t CompilationDelegateDML::Compile() {
  HRESULT hr = S_OK;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  for (size_t i = 0; i < model->operations.size(); ++i) {
    const mojom::OperationPtr& operation = model->operations[i];
    DCHECK(operation->outputs.size() == 1);

    if (operation->type == mojom::ADD) {
      hr = CompileArithmetic(model, operation, dml_->constants_);
    } else {
      LOG(ERROR) << "Operation is not supported";
      hr = E_FAIL;
    }

    if (FAILED(hr)) {
      LOG(ERROR) << "Failed compiling model.";
      return mojom::OP_FAILED;
    }
  }

  hr = InitializeOperators();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed initializing operators.";
    return mojom::OP_FAILED;
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDML::CreateExecution(
    std::unique_ptr<mojom::Execution>& execution,
    mojom::ExecutionInitParamsPtr params) {
  execution = std::make_unique<ExecutionImplDML>(this, std::move(dml_),
                                                 std::move(params));

  return mojom::NOT_ERROR;
}

const mojom::ModelInfoPtr& CompilationDelegateDML::GetModel() const {
  return compilation_->GetModel();
}
mojo::ScopedSharedBufferMapping CompilationDelegateDML::MapMemory(
    uint32_t index) const {
  return compilation_->MapMemory(index);
}

HRESULT CompilationDelegateDML::InitializeOperators() {
  ComPtr<IDMLOperatorInitializer> operator_initializer;
  size_t size = dml_->compiled_operators_.size();
  IDMLCompiledOperator* compiled_operators[size];
  for (size_t i = 0; i < size; i++) {
    compiled_operators[i] = dml_->compiled_operators_[i].Get();
  }
  HRESULT hr = dml_->dml_device_->CreateOperatorInitializer(
      size, compiled_operators, IID_PPV_ARGS(&operator_initializer));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating operator initializer.";
    return hr;
  }
  // Query the operator for the required size in descriptors of its binding
  // table.
  DML_BINDING_PROPERTIES initialize_binding_properties =
      operator_initializer->GetBindingProperties();
  UINT descriptor_count =
      std::max(initialize_binding_properties.RequiredDescriptorCount,
               execute_descriptor_count_);

  // Create descriptor heaps.
  ComPtr<ID3D12DescriptorHeap> descriptor_heap;
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
  descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descriptor_heap_desc.NumDescriptors = descriptor_count;
  descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  hr = dml_->d3D12_device_->CreateDescriptorHeap(
      &descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating descriptor heap.";
    return hr;
  }

  // Set the descriptor heap(s).
  ID3D12DescriptorHeap* d3D12_descriptor_heaps[] = {descriptor_heap.Get()};
  dml_->command_list_->SetDescriptorHeaps(ARRAYSIZE(d3D12_descriptor_heaps),
                                          d3D12_descriptor_heaps);

  // Create a binding table over the descriptor heap we just created.
  dml_->binding_table_desc_ = {};
  dml_->binding_table_desc_.Dispatchable = operator_initializer.Get();
  dml_->binding_table_desc_.CPUDescriptorHandle =
      descriptor_heap->GetCPUDescriptorHandleForHeapStart();
  dml_->binding_table_desc_.GPUDescriptorHandle =
      descriptor_heap->GetGPUDescriptorHandleForHeapStart();
  dml_->binding_table_desc_.SizeInDescriptors = descriptor_count;

  hr = dml_->dml_device_->CreateBindingTable(
      &dml_->binding_table_desc_, IID_PPV_ARGS(&dml_->binding_table_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating binding table.";
    return hr;
  }

  // The command recorder is a stateless object that records Dispatches into an
  // existing Direct3D 12 command list.
  ComPtr<IDMLCommandRecorder> command_recorder;
  hr = dml_->dml_device_->CreateCommandRecorder(
      IID_PPV_ARGS(&dml_->command_recorder_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating command recorder.";
    return hr;
  }

  // Record execution of the operator initializer.
  dml_->command_recorder_->RecordDispatch(dml_->command_list_.Get(),
                                          operator_initializer.Get(),
                                          dml_->binding_table_.Get());

  // Close the Direct3D 12 command list, and submit it for execution.
  hr = CloseExecuteResetWait(dml_->d3D12_device_, dml_->command_queue_,
                             dml_->command_allocator_, dml_->command_list_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed executing command list for initializing operators.";
    return hr;
  }

  // Bind and execute the operator on the GPU.
  dml_->command_list_->SetDescriptorHeaps(ARRAYSIZE(d3D12_descriptor_heaps),
                                          d3D12_descriptor_heaps);

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileArithmetic(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation,
    std::vector<uint32_t>& constants) {
  DLOG(INFO) << "CompilationImplMac::CompileArithmetic";
  // Check constants for input 0 and 1
  for (size_t i = 0; i < 2; ++i) {
    size_t input_index = operation->inputs[i];
    std::string index_id(base::NumberToString(input_index));
    if (model->values.find(index_id) != model->values.end()) {
      constants.push_back(input_index);
    }
  }

  // Assume there are the same dimensions.
  const std::vector<uint32_t>& dimensions =
      model->operands[operation->inputs[0]]->dimensions;
  DML_BUFFER_TENSOR_DESC buffer_tensor_desc = {};
  buffer_tensor_desc.DataType = DML_TENSOR_DATA_TYPE_FLOAT32;
  buffer_tensor_desc.Flags = DML_TENSOR_FLAG_NONE;
  buffer_tensor_desc.DimensionCount = dimensions.size();
  buffer_tensor_desc.Sizes = dimensions.data();
  buffer_tensor_desc.Strides = nullptr;
  buffer_tensor_desc.TotalTensorSizeInBytes = DMLCalcBufferTensorSize(
      buffer_tensor_desc.DataType, buffer_tensor_desc.DimensionCount,
      buffer_tensor_desc.Sizes, buffer_tensor_desc.Strides);

  DML_TENSOR_DESC tensor_desc = {DML_TENSOR_TYPE_BUFFER, &buffer_tensor_desc};
  DML_ELEMENT_WISE_ADD_OPERATOR_DESC add_operator_desc = {
      &tensor_desc, &tensor_desc, &tensor_desc};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_ELEMENT_WISE_ADD,
                                     &add_operator_desc};
  ComPtr<IDMLOperator> dml_operator;
  HRESULT hr = dml_->dml_device_->CreateOperator(&operator_desc,
                                                 IID_PPV_ARGS(&dml_operator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating add operator";
    return hr;
  }

  // Compile the operator into an object that can be dispatched to the GPU.
  ComPtr<IDMLCompiledOperator> compiled_operator;
  hr = dml_->dml_device_->CompileOperator(dml_operator.Get(),
                                          DML_EXECUTION_FLAG_NONE,
                                          IID_PPV_ARGS(&compiled_operator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling operator.";
    return hr;
  }

  DML_BINDING_PROPERTIES execute_binding_properties =
      compiled_operator->GetBindingProperties();
  execute_descriptor_count_ +=
      execute_binding_properties.RequiredDescriptorCount;
  execute_temporary_resource_size_ +=
      execute_binding_properties.TemporaryResourceSize;

  dml_->compiled_operators_.push_back(std::move(compiled_operator));

  return hr;
}

}  // namespace ml
