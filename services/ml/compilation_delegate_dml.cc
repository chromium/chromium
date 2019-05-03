// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "services/ml/compilation_delegate_dml.h"

#include <dxgi1_4.h>

#include "base/logging.h"
// TODO: Window sdk should be upgraded to 10.0.18361.0 in VS
// seeing https://chromium-review.googlesource.com/c/chromium/src/+/1054027
#include "services/ml/direct_ml.h"
#include "services/ml/dml_d3dx12_utils.h"
#include "services/ml/dml_symbol_table.h"
#include "services/ml/ml_utils_dml.h"
#include "services/ml/public/mojom/constants.mojom.h"

#define DEBUG_DIRECT_ML 0

namespace ml {

namespace {

using Microsoft::WRL::ComPtr;

HRESULT InitializeDirect3D12(ComPtr<ID3D12Device>& d3D12_device,
                             ComPtr<ID3D12CommandQueue>& command_queue,
                             ComPtr<ID3D12CommandAllocator>& command_allocator,
                             ComPtr<ID3D12GraphicsCommandList>& command_list) {
#if DEBUG_DIRECT_ML
  ComPtr<ID3D12Debug> d3D12_debug;
  if (FAILED(D3D(D3D12GetDebugInterface)(IID_PPV_ARGS(&d3D12_debug)))) {
    LOG(ERROR) << "Failed getting debug interface because of missing the "
                  "Graphics Tools optional feature.";
  } else {
    d3D12_debug->EnableDebugLayer();
  }
#endif

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
  dml_ = base::MakeRefCounted<CompiledModelDML>();

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
#if DEBUG_DIRECT_ML
  // Enable debugging via DirectML debug layers with this flag.
  dml_create_device_flags |= DML_CREATE_DEVICE_FLAG_DEBUG;
#endif
  hr = DML(DMLCreateDevice)(dml_->d3D12_device_.Get(), dml_create_device_flags,
                            IID_PPV_ARGS(&dml_->dml_device_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating DirectML.";
    return;
  }
}

CompilationDelegateDML::~CompilationDelegateDML() = default;

int32_t CompilationDelegateDML::Compile() {
  if (!dml_->d3D12_device_ || !dml_->dml_device_) {
    LOG(ERROR) << "Failed enabling DirectML backend";
    return mojom::OP_FAILED;
  }

  HRESULT hr = CreateCommittedResources();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resources for all of operands.";
    return mojom::OP_FAILED;
  }

  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  for (size_t i = 0; i < model->operations.size(); ++i) {
    const mojom::OperationPtr& operation = model->operations[i];
    DCHECK(operation->outputs.size() == 1);

    if (operation->type == mojom::ADD) {
      hr = CompileArithmetic(model, operation);
    } else if (operation->type == mojom::CONV_2D) {
      hr = CompileConvolution(model, operation);
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
  execution = std::make_unique<ExecutionImplDML>(dml_, std::move(params));

  return mojom::NOT_ERROR;
}

HRESULT CompilationDelegateDML::InitializeOperators() {
  ComPtr<IDMLOperatorInitializer> operator_initializer;
  size_t size = dml_->operations_.size();
  IDMLCompiledOperator* compiled_operators[size];
  for (size_t i = 0; i < size; i++) {
    compiled_operators[i] = dml_->operations_[i]->compiled_operator_.Get();
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
  dml_->descriptor_heap_ = std::move(descriptor_heap);

  dml_->temporary_resource_size_ =
      std::max(initialize_binding_properties.TemporaryResourceSize,
               execute_temporary_resource_size_);
  if (dml_->temporary_resource_size_ != 0) {
    hr = CreateCommonResource(dml_->temporary_resource_size_,
                              dml_->temporary_buffer_, dml_->d3D12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating committed resource for temorary buffer.";
      return hr;
    }

    DML_BUFFER_BINDING buffer_binding = {dml_->temporary_buffer_.Get(), 0,
                                         dml_->temporary_resource_size_};
    DML_BINDING_DESC binding_desc = {DML_BINDING_TYPE_BUFFER, &buffer_binding};
    dml_->binding_table_->BindTemporaryResource(&binding_desc);
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

  return S_OK;
}

HRESULT CompilationDelegateDML::UploadConstantResource(uint32_t index) {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  if (dml_->operand_map_.find(index) == dml_->operand_map_.end()) {
    dml_->operand_map_[index] =
        std::make_unique<OperandDML>(model->operands[index]->dimensions);
  }

  // Upload constants_ that hold the value of setting with setOperandValue js
  // API, and maybe it has been uploaded.
  if (dml_->operand_map_[index]->operand_resource_)
    return S_OK;

  HRESULT hr = CreateUploadResource(
      dml_->operand_map_[index]->SizeInBytes(),
      dml_->operand_map_[index]->upload_resource_,
      dml_->operand_map_[index]->operand_resource_, dml_->d3D12_device_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for inputs.";
    return hr;
  }

  const mojom::OperandValueInfoPtr& input_info =
      model->values[base::NumberToString(index)];
  auto mapping = compilation_->MapMemory(index);
  hr = UploadTensorResource(
      static_cast<void*>(mapping.get()), input_info->length,
      dml_->operand_map_[index]->upload_resource_,
      dml_->operand_map_[index]->operand_resource_, dml_->command_list_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed uploading tensor resource for inputs data.";
    return hr;
  }
  return S_OK;
}

// Create committed resources for inputs and outputs of Model.
HRESULT CompilationDelegateDML::CreateCommittedResources() {
  HRESULT hr;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  // Create committed resource for graphic inputs to upload CPU resource to GPU.
  const std::vector<uint32_t>& inputs = model->inputs;
  for (size_t i = 0; i < inputs.size(); ++i) {
    size_t index = inputs[i];
    dml_->operand_map_[index] =
        std::make_unique<OperandDML>(model->operands[index]->dimensions);
    hr = CreateUploadResource(dml_->operand_map_[index]->SizeInBytes(),
                              dml_->operand_map_[index]->upload_resource_,
                              dml_->operand_map_[index]->operand_resource_,
                              dml_->d3D12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating committed resource for inputs.";
      return hr;
    }
  }

  // Create readback resource for graphic outputs.
  const std::vector<uint32_t>& outputs = model->outputs;
  for (size_t i = 0; i < outputs.size(); ++i) {
    size_t index = outputs[i];
    dml_->operand_map_[index] =
        std::make_unique<OperandDML>(model->operands[index]->dimensions);
    hr = CreateReadbackResource(dml_->operand_map_[index]->SizeInBytes(),
                                dml_->operand_map_[index]->readback_resource_,
                                dml_->operand_map_[index]->operand_resource_,
                                dml_->d3D12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating committed resource for inputs.";
      return hr;
    }
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CreateIntermediateResource(uint32_t index) {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const std::vector<uint32_t>& outputs = model->outputs;
  for (size_t i = 0; i < outputs.size(); ++i) {
    // It's not intermediate output.
    if (index == outputs[i])
      return S_OK;
  }

  if (dml_->operand_map_.find(index) == dml_->operand_map_.end()) {
    dml_->operand_map_[index] =
        std::make_unique<OperandDML>(model->operands[index]->dimensions);
  }

  if (dml_->operand_map_[index]->operand_resource_)
    return S_OK;

  HRESULT hr = CreateOutputResource(
      dml_->operand_map_[index]->SizeInBytes(),
      dml_->operand_map_[index]->operand_resource_, dml_->d3D12_device_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for intermediate.";
    return hr;
  }
  return S_OK;
}

HRESULT CompilationDelegateDML::CompileOperator(
    DML_OPERATOR_DESC& operator_desc,
    size_t bind_input_size,
    const mojom::OperationPtr& operation) {
  ComPtr<IDMLOperator> dml_operator;
  HRESULT hr = dml_->dml_device_->CreateOperator(&operator_desc,
                                                 IID_PPV_ARGS(&dml_operator));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating operator.";
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
  uint32_t descriptor_count =
      execute_binding_properties.RequiredDescriptorCount;
  uint64_t persistent_size = execute_binding_properties.PersistentResourceSize;
  ComPtr<ID3D12Resource> persistent_buffer = nullptr;
  if (persistent_size != 0) {
    hr = CreateCommonResource(persistent_size, persistent_buffer,
                              dml_->d3D12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating committed resource for persistent.";
      return hr;
    }
  }
  dml_->operations_.push_back(std::make_unique<OperationDML>(
      compiled_operator, descriptor_count, bind_input_size, operation->inputs,
      operation->outputs, persistent_buffer, persistent_size));

  execute_descriptor_count_ += descriptor_count;
  execute_temporary_resource_size_ +=
      execute_binding_properties.TemporaryResourceSize;

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileArithmetic(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  // TODO: Support Activation for Add operation in
  // https://github.com/intel/webml-polyfill/issues/757.
  DLOG(INFO) << "CompilationImplMac::CompileArithmetic";
  // TODO:: Create persistent resources for constants
  // https://github.com/intel/webml-polyfill/issues/758.
  // Check constants for input 0 and 1.
  HRESULT hr = S_OK;
  for (size_t i = 0; i < 2; ++i) {
    size_t input_index = operation->inputs[i];
    std::string index_id(base::NumberToString(input_index));
    if (model->values.find(index_id) != model->values.end()) {
      hr = UploadConstantResource(input_index);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed uploading constant resource.";
        return hr;
      }
    }
  }
  hr = CreateIntermediateResource(operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  size_t operand_index = operation->inputs[0];
  DML_BUFFER_TENSOR_DESC buffer_tensor_desc =
      dml_->operand_map_[operand_index]->operand_desc_;
  DML_TENSOR_DESC tensor_desc = {DML_TENSOR_TYPE_BUFFER, &buffer_tensor_desc};
  DML_ELEMENT_WISE_ADD_OPERATOR_DESC add_operator_desc = {
      &tensor_desc, &tensor_desc, &tensor_desc};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_ELEMENT_WISE_ADD,
                                     &add_operator_desc};

  hr = CompileOperator(operator_desc, 2, operation);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling add operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileConvolution(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileConvolution";
  HRESULT hr = S_OK;
  // Create committed resource for weights and bias.
  for (size_t i = 1; i < 3; ++i) {
    size_t index = operation->inputs[i];
    std::string index_id(base::NumberToString(index));
    if (model->values.find(index_id) != model->values.end()) {
      hr = UploadConstantResource(index);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed uploading for weights and bias.";
        return hr;
      }
    }
  }
  hr = CreateIntermediateResource(operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  size_t input_index = operation->inputs[0];
  DML_BUFFER_TENSOR_DESC input_buffer_desc =
      dml_->operand_map_[input_index]->operand_desc_;
  DML_TENSOR_DESC input_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                       &input_buffer_desc};

  size_t weights_index = operation->inputs[1];
  DML_BUFFER_TENSOR_DESC weights_buffer_desc =
      dml_->operand_map_[weights_index]->operand_desc_;
  DML_TENSOR_DESC weights_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                         &weights_buffer_desc};

  size_t bias_index = operation->inputs[2];
  DML_BUFFER_TENSOR_DESC bias_buffer_desc =
      dml_->operand_map_[bias_index]->operand_desc_;
  DML_TENSOR_DESC bias_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                      &bias_buffer_desc};

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  ConvParams params;
  int32_t result = compilation_->GetConvParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return E_FAIL;

  const uint32_t strides[2] = {params.stride_width, params.stride_height};
  const uint32_t dilations[2] = {params.dilation_width, params.dilation_height};
  const uint32_t start_padding[2] = {params.padding_left, params.padding_top};
  const uint32_t end_padding[2] = {params.padding_right, params.padding_bottom};
  const uint32_t output_padding[2] = {0, 0};

  DML_CONVOLUTION_OPERATOR_DESC conv_operator_desc = {
      &input_tensor_desc,
      &weights_tensor_desc,
      &bias_tensor_desc,
      &output_tensor_desc,
      DML_CONVOLUTION_MODE_CONVOLUTION,
      DML_CONVOLUTION_DIRECTION_FORWARD,
      2,
      strides,
      dilations,
      start_padding,
      end_padding,
      output_padding,
      1,
      nullptr};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_CONVOLUTION,
                                     &conv_operator_desc};
  hr = CompileOperator(operator_desc, 3, operation);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling convolution operator.";
    return hr;
  }

  return S_OK;
}

}  // namespace ml
