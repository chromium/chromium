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
#include "services/ml/common.h"
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

// Create committed resources for inputs and outputs of Model.
HRESULT CreateCommittedResources(scoped_refptr<CompiledModelDML> dml,
                                 const mojom::ModelInfoPtr& model) {
  HRESULT hr;
  // Create committed resource for graphic inputs to upload CPU resource to GPU.
  const std::vector<uint32_t>& inputs = model->inputs;
  for (size_t i = 0; i < inputs.size(); ++i) {
    size_t index = inputs[i];
    dml->operand_map_[index] =
        std::make_unique<OperandDML>(model->operands[index]->dimensions);
    // The input data will be formatted with hsls, so the size of input
    // data is Float32 * product(dimensions).
    size_t input_data_size =
        product(model->operands[index]->dimensions) * sizeof(float);
    hr = CreateUploadResource(
        input_data_size, dml->operand_map_[index]->upload_resource_,
        dml->operand_map_[index]->format_resource_, dml->d3d12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating upload committed resource for inputs.";
      return hr;
    }
    // Create common resource for formatting input data.
    hr = CreateCommonResource(dml->operand_map_[index]->SizeInBytes(),
                              dml->operand_map_[index]->operand_resource_,
                              dml->d3d12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating resource for formatting input data.";
      return hr;
    }
  }

  // Create readback resource for graphic outputs.
  const std::vector<uint32_t>& outputs = model->outputs;
  for (size_t i = 0; i < outputs.size(); ++i) {
    size_t index = outputs[i];
    dml->operand_map_[index] =
        std::make_unique<OperandDML>(model->operands[index]->dimensions);
    hr = CreateOutputResource(dml->operand_map_[index]->SizeInBytes(),
                              dml->operand_map_[index]->operand_resource_,
                              dml->d3d12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating committed resource for output data.";
      return hr;
    }

    // The input data will be formatted with hsls, so the size of input
    // data is Float32 * product(dimensions).
    size_t output_data_size =
        product(model->operands[index]->dimensions) * sizeof(float);
    hr = CreateReadbackResource(
        output_data_size, dml->operand_map_[index]->readback_resource_,
        dml->operand_map_[index]->format_resource_, dml->d3d12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating readback committed resource for inputs.";
      return hr;
    }
  }

  return S_OK;
}

HRESULT UploadConstantResource(scoped_refptr<CompiledModelDML> dml,
                               const mojom::ModelInfoPtr& model,
                               uint32_t index,
                               mojo::ScopedSharedBufferMapping mapping,
                               bool depth_conv_weight = false) {
  // Create OperandDML if it doesn't exist.
  if (dml->operand_map_.find(index) == dml->operand_map_.end()) {
    dml->operand_map_[index] = std::make_unique<OperandDML>(
        model->operands[index]->dimensions, depth_conv_weight);
  }

  // Upload constants_ that hold the value of setting with setOperandValue js
  // API, and maybe it has been uploaded.
  if (dml->operand_map_[index]->operand_resource_)
    return S_OK;

  HRESULT hr = CreateUploadResource(dml->operand_map_[index]->SizeInBytes(),
                                    dml->operand_map_[index]->upload_resource_,
                                    dml->operand_map_[index]->operand_resource_,
                                    dml->d3d12_device_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for constants.";
    return hr;
  }

  hr = UploadFloat16Resource(static_cast<void*>(mapping.get()),
                             dml->operand_map_[index]->dimensions_,
                             dml->operand_map_[index]->upload_resource_,
                             dml->operand_map_[index]->operand_resource_,
                             dml->command_list_, depth_conv_weight);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed uploading tensor resource for inputs data.";
    return hr;
  }
  return S_OK;
}

HRESULT CreateIntermediateResource(scoped_refptr<CompiledModelDML> dml,
                                   const mojom::ModelInfoPtr& model,
                                   uint32_t index) {
  const std::vector<uint32_t>& outputs = model->outputs;
  for (size_t i = 0; i < outputs.size(); ++i) {
    // It's not intermediate output.
    if (index == outputs[i])
      return S_OK;
  }

  if (dml->operand_map_.find(index) == dml->operand_map_.end()) {
    dml->operand_map_[index] =
        std::make_unique<OperandDML>(model->operands[index]->dimensions);
  }

  if (dml->operand_map_[index]->operand_resource_)
    return S_OK;

  HRESULT hr = CreateOutputResource(dml->operand_map_[index]->SizeInBytes(),
                                    dml->operand_map_[index]->operand_resource_,
                                    dml->d3d12_device_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for intermediate.";
    return hr;
  }
  return S_OK;
}

DML_BUFFER_BINDING* InputBuffers(CompiledModelDML* dml,
                                 OperationDML* operation) {
  // It's to output persistent resources.
  size_t input_size = operation->bind_inputs_size;
  DML_BUFFER_BINDING* input_buffers = new DML_BUFFER_BINDING[input_size];
  for (size_t i = 0; i < input_size; ++i) {
    size_t input_index = operation->inputs_[i];
    OperandDML* operand = dml->operand_map_[input_index].get();
    if (operand->operand_desc_.Flags == DML_TENSOR_FLAG_OWNED_BY_DML) {
      input_buffers[i] = {operand->operand_resource_.Get(), 0,
                          operand->SizeInBytes()};
    } else {
      // Empty buffer binding.
      input_buffers[i] = {nullptr, 0, 0};
    }
  }
  return input_buffers;
}

void FreeUnusedResources(CompiledModelDML* dml,
                         DML_BUFFER_ARRAY_BINDING* buffer_array,
                         size_t size) {
  for (size_t i = 0; i < size; ++i) {
    delete buffer_array[i].Bindings;
  }
  // Release those have been copied to persistent resources.
  for (auto& iter : dml->operand_map_) {
    OperandDML* operand = iter.second.get();
    if (operand->operand_desc_.Flags == DML_TENSOR_FLAG_OWNED_BY_DML) {
      operand->operand_resource_.Reset();
      operand->upload_resource_.Reset();
    }
  }
}

HRESULT InitializeOperators(scoped_refptr<CompiledModelDML> dml,
                            uint32_t execute_descriptor_count,
                            uint64_t execute_temporary_resource_size) {
  size_t size = dml->operations_.size();
  DML_BUFFER_ARRAY_BINDING init_buffer_array[size];
  DML_BINDING_DESC init_binding_array[size];
  DML_BUFFER_BINDING persistent_buffers[size];
  DML_BINDING_DESC persistent_bindings[size];
  IDMLCompiledOperator* compiled_operators[size];
  for (size_t i = 0; i < size; i++) {
    OperationDML* operation = dml->operations_[i].get();
    // Inputs binding desc for initializeing.
    init_buffer_array[i] = {operation->bind_inputs_size,
                            InputBuffers(dml.get(), operation)};
    init_binding_array[i] = {DML_BINDING_TYPE_BUFFER_ARRAY,
                             &init_buffer_array[i]};

    // Output binding desc for persistent resources.
    if (operation->persistent_size_ != 0) {
      persistent_buffers[i] = {operation->persistent_buffer_.Get(), 0,
                               operation->persistent_size_};
      persistent_bindings[i] = {DML_BINDING_TYPE_BUFFER,
                                &persistent_buffers[i]};
    } else {
      persistent_bindings[i] = {DML_BINDING_TYPE_NONE, nullptr};
    }

    // Compiled operators for initializeing.
    compiled_operators[i] = dml->operations_[i]->compiled_operator_.Get();
  }
  ComPtr<IDMLOperatorInitializer> operator_initializer;
  HRESULT hr = dml->dml_device_->CreateOperatorInitializer(
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
               execute_descriptor_count);

  // Create descriptor heaps.
  ComPtr<ID3D12DescriptorHeap> descriptor_heap;
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
  descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descriptor_heap_desc.NumDescriptors = descriptor_count;
  descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  hr = dml->d3d12_device_->CreateDescriptorHeap(&descriptor_heap_desc,
                                                IID_PPV_ARGS(&descriptor_heap));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating descriptor heap.";
    return hr;
  }

  // Set the descriptor heap(s).
  ID3D12DescriptorHeap* d3D12_descriptor_heaps[] = {descriptor_heap.Get()};
  dml->command_list_->SetDescriptorHeaps(ARRAYSIZE(d3D12_descriptor_heaps),
                                         d3D12_descriptor_heaps);

  // Create a binding table over the descriptor heap we just created.
  DML_BINDING_TABLE_DESC binding_table_desc = {};
  binding_table_desc.Dispatchable = operator_initializer.Get();
  binding_table_desc.CPUDescriptorHandle =
      descriptor_heap->GetCPUDescriptorHandleForHeapStart();
  binding_table_desc.GPUDescriptorHandle =
      descriptor_heap->GetGPUDescriptorHandleForHeapStart();
  binding_table_desc.SizeInDescriptors = descriptor_count;
  ComPtr<IDMLBindingTable> binding_table;
  hr = dml->dml_device_->CreateBindingTable(&binding_table_desc,
                                            IID_PPV_ARGS(&binding_table));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating binding table.";
    return hr;
  }
  binding_table->BindInputs(size, init_binding_array);
  binding_table->BindOutputs(size, persistent_bindings);

  dml->descriptor_heap_ = std::move(descriptor_heap);

  dml->temporary_resource_size_ =
      std::max(initialize_binding_properties.TemporaryResourceSize,
               execute_temporary_resource_size);
  if (dml->temporary_resource_size_ != 0) {
    hr = CreateCommonResource(dml->temporary_resource_size_,
                              dml->temporary_buffer_, dml->d3d12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating committed resource for temorary buffer.";
      return hr;
    }
  }

  // The command recorder is a stateless object that records Dispatches into an
  // existing Direct3D 12 command list.
  ComPtr<IDMLCommandRecorder> command_recorder;
  hr = dml->dml_device_->CreateCommandRecorder(
      IID_PPV_ARGS(&dml->command_recorder_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating command recorder.";
    return hr;
  }

  // Record execution of the operator initializer.
  dml->command_recorder_->RecordDispatch(dml->command_list_.Get(),
                                         operator_initializer.Get(),
                                         binding_table.Get());

  // Close the Direct3D 12 command list, and submit it for execution.
  hr = CloseExecuteResetWait(dml->d3d12_device_, dml->command_queue_,
                             dml->command_allocator_, dml->command_list_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed executing command list for initializing operators.";
    return hr;
  }

  // Free unused resources.
  FreeUnusedResources(dml.get(), init_buffer_array, size);

  return S_OK;
}

HRESULT BindingTableForExecution(IDMLDevice* dml_device,
                              OperationDML* operation,
                              CompiledModelDML* dml) {
  // Create a table per executed operator.
  auto binding_props = operation->compiled_operator_->GetBindingProperties();
  DML_BINDING_TABLE_DESC table_desc = {
      operation->compiled_operator_.Get(),
      dml->GetCpuHandle(operation->descriptor_index_),
      dml->GetGpuHandle(operation->descriptor_index_),
      binding_props.RequiredDescriptorCount};
  HRESULT hr = dml_device->CreateBindingTable(
      &table_desc, IID_PPV_ARGS(&operation->binding_table_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating binding table for compiled operator.";
    return hr;
  }

  if (binding_props.TemporaryResourceSize != 0) {
    DML_BUFFER_BINDING buffer_binding = {dml->temporary_buffer_.Get(), 0,
                                         dml->temporary_resource_size_};
    DML_BINDING_DESC binding_desc = {DML_BINDING_TYPE_BUFFER, &buffer_binding};
    operation->binding_table_->BindTemporaryResource(&binding_desc);
  }

  if (operation->persistent_size_ != 0) {
    DML_BUFFER_BINDING persistent_buffer = {operation->persistent_buffer_.Get(),
                                            0, operation->persistent_size_};
    DML_BINDING_DESC persistent_binding = {DML_BINDING_TYPE_BUFFER,
                                           &persistent_buffer};
    operation->binding_table_->BindPersistentResource(&persistent_binding);
  }

  size_t input_size = operation->bind_inputs_size;
  DML_BUFFER_BINDING input_buffer_array[input_size];
  DML_BINDING_DESC input_binding_array[input_size];
  DCHECK(input_size != 0);
  for (size_t i = 0; i < input_size; ++i) {
    size_t input_index = operation->inputs_[i];
    OperandDML* operand = dml->operand_map_[input_index].get();
    if (operand->operand_desc_.Flags == DML_TENSOR_FLAG_OWNED_BY_DML) {
      input_binding_array[i] = {DML_BINDING_TYPE_NONE, nullptr};
    } else {
      input_buffer_array[i] = {operand->operand_resource_.Get(), 0,
                                 operand->SizeInBytes()};
      input_binding_array[i] = {DML_BINDING_TYPE_BUFFER,
                                &input_buffer_array[i]};
    }
  }
  operation->binding_table_->BindInputs(input_size, input_binding_array);

  DCHECK(operation->outputs_.size() == 1);
  size_t output_index = operation->outputs_[0];
  UINT64 output_buffer_size = dml->operand_map_[output_index]->SizeInBytes();
  ComPtr<ID3D12Resource> output_resource =
      dml->operand_map_[output_index]->operand_resource_;
  DML_BUFFER_BINDING output_buffer_binding = {output_resource.Get(), 0,
                                              output_buffer_size};
  DML_BINDING_DESC output_binding_desc{DML_BINDING_TYPE_BUFFER,
                                       &output_buffer_binding};
  operation->binding_table_->BindOutputs(1, &output_binding_desc);

  return S_OK;
}

}  // namespace

CompilationDelegateDML::CompilationDelegateDML(
    const CompilationImpl* compilation)
    : compilation_(compilation),
      execute_descriptor_count_(0),
      execute_temporary_resource_size_(0) {
  const mojom::ModelInfoPtr& model = compilation->GetModel();
  dml_ = base::MakeRefCounted<CompiledModelDML>(model->inputs, model->outputs);

  // Set up Direct3D 12.
  HRESULT hr =
      InitializeDirect3D12(dml_->d3d12_device_, dml_->command_queue_,
                           dml_->command_allocator_, dml_->command_list_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed initializing D3D12.";
    return;
  }
  DLOG(INFO) << "The combination views heap size is "
             << dml_->d3d12_device_->GetDescriptorHandleIncrementSize(
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  // Create the DirectML device.
  DML_CREATE_DEVICE_FLAGS dml_create_device_flags = DML_CREATE_DEVICE_FLAG_NONE;
#if DEBUG_DIRECT_ML
  // Enable debugging via DirectML debug layers with this flag.
  dml_create_device_flags |= DML_CREATE_DEVICE_FLAG_DEBUG;
#endif
  hr = DML(DMLCreateDevice)(dml_->d3d12_device_.Get(), dml_create_device_flags,
                            IID_PPV_ARGS(&dml_->dml_device_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating DirectML.";
    return;
  }
}

CompilationDelegateDML::~CompilationDelegateDML() = default;

int32_t CompilationDelegateDML::Compile() {
  if (!dml_->d3d12_device_ || !dml_->dml_device_) {
    LOG(ERROR) << "Failed enabling DirectML backend";
    return mojom::OP_FAILED;
  }

  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  HRESULT hr = CreateCommittedResources(dml_, model);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resources for model's inputs and "
                  "outputs.";
    return mojom::OP_FAILED;
  }

  // Compile operations of model.
  for (size_t i = 0; i < model->operations.size(); ++i) {
    const mojom::OperationPtr& operation = model->operations[i];
    DCHECK(operation->outputs.size() == 1);

    if (operation->type == mojom::ADD || operation->type == mojom::MUL) {
      hr = CompileArithmetic(model, operation);
    } else if (operation->type == mojom::CONV_2D ||
               operation->type == mojom::DEPTHWISE_CONV_2D ||
               operation->type == mojom::ATROUS_CONV_2D ||
               operation->type == mojom::ATROUS_DEPTHWISE_CONV_2D) {
      hr = CompileConvolution(model, operation);
    } else if (operation->type == mojom::AVERAGE_POOL_2D ||
               operation->type == mojom::MAX_POOL_2D) {
      hr = CompilePooling(model, operation);
    } else if (operation->type == mojom::SOFTMAX) {
      hr = CompileSoftmax(model, operation);
    } else if (operation->type == mojom::RESHAPE) {
      hr = CompileReshape(model, operation);
    } else if (operation->type == mojom::CONCATENATION) {
      hr = CompileConcatenation(model, operation);
    } else if (operation->type == mojom::FULLY_CONNECTED) {
      hr = CompileFullyConnected(model, operation);
    } else if (operation->type == mojom::RESIZE_BILINEAR) {
      hr = CompileBilinearScale(model, operation);
    } else {
      LOG(ERROR) << "Operation is not supported";
      hr = E_FAIL;
    }

    if (FAILED(hr)) {
      LOG(ERROR) << "Failed compiling model.";
      return mojom::OP_FAILED;
    }
  }

  hr = InitializeOperators(dml_, execute_descriptor_count_,
                           execute_temporary_resource_size_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed initializing operators.";
    return mojom::OP_FAILED;
  }

  for (size_t i = 0; i < dml_->operations_.size(); ++i) {
    hr = BindingTableForExecution(dml_->dml_device_.Get(),
                               dml_->operations_[i].get(), dml_.get());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed binding table for execution.";
    }
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDML::CreateExecution(
    std::unique_ptr<mojom::Execution>& execution,
    mojom::ExecutionInitParamsPtr params) {
  execution = std::make_unique<ExecutionImplDML>(dml_, std::move(params));

  return mojom::NOT_ERROR;
}

HRESULT CompilationDelegateDML::CompileOperator(
    DML_OPERATOR_DESC& operator_desc,
    size_t bind_input_size,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
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
                              dml_->d3d12_device_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed creating committed resource for persistent.";
      return hr;
    }
  }
  dml_->operations_.push_back(std::make_unique<OperationDML>(
      compiled_operator, execute_descriptor_count_, bind_input_size, inputs,
      outputs, persistent_buffer, persistent_size));

  execute_descriptor_count_ += descriptor_count;
  execute_temporary_resource_size_ +=
      execute_binding_properties.TemporaryResourceSize;

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileActivation(
    int32_t fuse_code,
    std::vector<uint32_t> outputs) {
  if (fuse_code == mojom::FUSED_NONE)
    return S_OK;

  DLOG(INFO) << "CompilationImplMac::CompileActivation";
  // The inputs and outputs are the same operand.
  DML_BUFFER_TENSOR_DESC buffer_tensor_desc =
      dml_->operand_map_[outputs[0]]->operand_desc_;
  DML_TENSOR_DESC tensor_desc = {DML_TENSOR_TYPE_BUFFER, &buffer_tensor_desc};
  DML_ACTIVATION_RELU_OPERATOR_DESC relu_operator_desc;
  DML_ELEMENT_WISE_CLIP_OPERATOR_DESC clip_operator_desc;
  DML_SCALE_BIAS scale = {1.0, 0};
  DML_OPERATOR_DESC operator_desc;
  switch (fuse_code) {
    case mojom::FUSED_RELU:
      relu_operator_desc = {&tensor_desc, &tensor_desc};
      operator_desc = {DML_OPERATOR_ACTIVATION_RELU, &relu_operator_desc};
      break;
    case mojom::FUSED_RELU1:
      clip_operator_desc = {&tensor_desc, &tensor_desc, &scale, -1.0, 1.0};
      operator_desc = {DML_OPERATOR_ELEMENT_WISE_CLIP, &clip_operator_desc};
      break;
    case mojom::FUSED_RELU6:
      clip_operator_desc = {&tensor_desc, &tensor_desc, &scale, 0, 6.0};
      operator_desc = {DML_OPERATOR_ELEMENT_WISE_CLIP, &clip_operator_desc};
      break;
    default:
      LOG(ERROR) << "Fuse code " << fuse_code << "isn't supported.";
      return E_FAIL;
  }

  HRESULT hr = CompileOperator(operator_desc, 1, outputs, outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling add operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileArithmetic(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileArithmetic";
  // TODO:: Create persistent resources for constants
  // https://github.com/intel/webml-polyfill/issues/758.
  // Check constants for input 0 and 1.
  HRESULT hr = S_OK;
  for (size_t i = 0; i < 2; ++i) {
    size_t input_index = operation->inputs[i];
    std::string index_id(base::NumberToString(input_index));
    if (model->values.find(index_id) != model->values.end()) {
      hr = UploadConstantResource(dml_, model, input_index,
                                  compilation_->MapMemory(input_index));
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed uploading constant resource.";
        return hr;
      }
    }
  }
  hr = CreateIntermediateResource(dml_, model, operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  // Update strides to support broadcasting.
  size_t primary_index = operation->inputs[0];
  size_t secondary_index = operation->inputs[1];
  // Copy new dimensions so that keep original dimensions.
  std::vector<uint32_t> primary_dimensions =
      dml_->operand_map_[primary_index]->dimensions_;
  std::vector<uint32_t> primary_strides =
      dml_->operand_map_[primary_index]->strides_;
  std::vector<uint32_t> secondary_dimensions =
      dml_->operand_map_[secondary_index]->dimensions_;
  std::vector<uint32_t> secondary_strides =
      dml_->operand_map_[secondary_index]->strides_;
  DCHECK(primary_dimensions.size() == 4);
  for (size_t i = 0; i < 4; ++i) {
    if (primary_dimensions[i] > secondary_dimensions[i]) {
      secondary_dimensions[i] = primary_dimensions[i];
      secondary_strides[i] = 0;
    } else if (primary_dimensions[i] < secondary_dimensions[i]) {
      primary_dimensions[i] = secondary_dimensions[i];
      primary_strides[i] = 0;
    }
  }

  DML_BUFFER_TENSOR_DESC primary_buffer_desc =
      dml_->operand_map_[primary_index]->operand_desc_;
  primary_buffer_desc.Sizes = primary_dimensions.data();
  primary_buffer_desc.Strides = primary_strides.data();
  DML_TENSOR_DESC primary_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                         &primary_buffer_desc};

  DML_BUFFER_TENSOR_DESC secondary_buffer_desc =
      dml_->operand_map_[secondary_index]->operand_desc_;
  secondary_buffer_desc.Sizes = secondary_dimensions.data();
  secondary_buffer_desc.Strides = secondary_strides.data();
  DML_TENSOR_DESC secondary_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                           &secondary_buffer_desc};

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  DML_OPERATOR_DESC operator_desc;
  DML_ELEMENT_WISE_ADD_OPERATOR_DESC add_operator_desc = {
      &primary_tensor_desc, &secondary_tensor_desc, &output_tensor_desc};
  DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC multiply_operator_desc = {
      &primary_tensor_desc, &secondary_tensor_desc, &output_tensor_desc};
  if (operation->type == mojom::ADD) {
    operator_desc = {DML_OPERATOR_ELEMENT_WISE_ADD, &add_operator_desc};
  } else if (operation->type == mojom::MUL) {
    operator_desc = {DML_OPERATOR_ELEMENT_WISE_MULTIPLY,
                     &multiply_operator_desc};
  }

  hr = CompileOperator(operator_desc, 2, operation->inputs, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling add operator.";
    return hr;
  }

  ElementWiseParams params;
  int32_t result = compilation_->GetElementWiseParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return E_FAIL;
  hr = CompileActivation(params.fuse_code, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling activation operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileConvolution(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileConvolution";
  ConvParams params;
  int32_t result = compilation_->GetConvParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return E_FAIL;
  if (params.depthwise && params.depthwise_multiplier != 1) {
    LOG(ERROR) << "depthwise_multiplier " << params.depthwise_multiplier
               << " is not supported.";
    return E_FAIL;
  }

  HRESULT hr = S_OK;
  // Create committed resource for weights and bias.
  for (size_t i = 1; i < 3; ++i) {
    size_t index = operation->inputs[i];
    std::string index_id(base::NumberToString(index));
    if (model->values.find(index_id) != model->values.end()) {
      hr = UploadConstantResource(dml_, model, index,
                                  compilation_->MapMemory(index),
                                  params.depthwise && i == 1 ? true : false);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed uploading for weights and bias.";
        return hr;
      }
    }
  }
  hr = CreateIntermediateResource(dml_, model, operation->outputs[0]);
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
  DML_BUFFER_TENSOR_DESC& weights_buffer_desc =
      dml_->operand_map_[weights_index]->operand_desc_;
  weights_buffer_desc.Flags = DML_TENSOR_FLAG_OWNED_BY_DML;
  DML_TENSOR_DESC weights_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                         &weights_buffer_desc};

  size_t bias_index = operation->inputs[2];
  DML_BUFFER_TENSOR_DESC& bias_buffer_desc =
      dml_->operand_map_[bias_index]->operand_desc_;
  bias_buffer_desc.Flags = DML_TENSOR_FLAG_OWNED_BY_DML;
  DML_TENSOR_DESC bias_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                      &bias_buffer_desc};

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  const uint32_t strides[2] = {params.stride_width, params.stride_height};
  const uint32_t dilations[2] = {params.dilation_width, params.dilation_height};
  const uint32_t start_padding[2] = {params.padding_top, params.padding_left};
  const uint32_t end_padding[2] = {params.padding_bottom, params.padding_right};
  const uint32_t output_padding[2] = {0, 0};

  DML_CONVOLUTION_OPERATOR_DESC conv_operator_desc = {
      &input_tensor_desc,
      &weights_tensor_desc,
      &bias_tensor_desc,
      &output_tensor_desc,
      DML_CONVOLUTION_MODE_CROSS_CORRELATION,
      DML_CONVOLUTION_DIRECTION_FORWARD,
      2,
      strides,
      dilations,
      start_padding,
      end_padding,
      output_padding,
      params.depthwise ? params.depth_in : 1,
      nullptr};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_CONVOLUTION,
                                     &conv_operator_desc};
  hr = CompileOperator(operator_desc, 3, operation->inputs, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling convolution operator.";
    return hr;
  }
  hr = CompileActivation(params.fuse_code, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling activation operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompilePooling(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompilePooling";
  HRESULT hr = CreateIntermediateResource(dml_, model, operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  size_t input_index = operation->inputs[0];
  DML_BUFFER_TENSOR_DESC input_buffer_desc =
      dml_->operand_map_[input_index]->operand_desc_;
  DML_TENSOR_DESC input_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                       &input_buffer_desc};

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  PoolingParams params;
  int32_t result = compilation_->GetPoolingParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return E_FAIL;

  const uint32_t strides[2] = {params.stride_width, params.stride_height};
  const uint32_t windows_size[2] = {params.filter_width, params.filter_height};
  const uint32_t start_padding[2] = {params.padding_left, params.padding_top};
  const uint32_t end_padding[2] = {params.padding_right, params.padding_bottom};

  DML_OPERATOR_DESC operator_desc;
  DML_AVERAGE_POOLING_OPERATOR_DESC average_pooling_desc;
  DML_MAX_POOLING_OPERATOR_DESC max_pooling_desc;
  if (operation->type == mojom::AVERAGE_POOL_2D) {
    average_pooling_desc = {
        &input_tensor_desc, &output_tensor_desc, 2,           strides,
        windows_size,       start_padding,       end_padding, false};
    operator_desc = {DML_OPERATOR_AVERAGE_POOLING, &average_pooling_desc};
  } else if (operation->type == mojom::MAX_POOL_2D) {
    max_pooling_desc = {
        &input_tensor_desc, &output_tensor_desc, 2,          strides,
        windows_size,       start_padding,       end_padding};
    operator_desc = {DML_OPERATOR_MAX_POOLING, &max_pooling_desc};
  }
  hr = CompileOperator(operator_desc, 1, operation->inputs, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling pooling operator.";
    return hr;
  }
  hr = CompileActivation(params.fuse_code, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling activation operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileSoftmax(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileSoftmax";
  SoftmaxParams params;
  int32_t result = compilation_->GetSoftmaxParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return E_FAIL;
  if (params.beta != 1.0) {
    LOG(ERROR) << "beta " << params.beta << " is not supported.";
    return E_FAIL;
  }

  HRESULT hr = CreateIntermediateResource(dml_, model, operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  size_t input_index = operation->inputs[0];
  DML_BUFFER_TENSOR_DESC input_buffer_desc =
      dml_->operand_map_[input_index]->operand_desc_;
  DML_TENSOR_DESC input_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                       &input_buffer_desc};

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  DML_ACTIVATION_SOFTMAX_OPERATOR_DESC softmax_operator_desc = {
      &input_tensor_desc, &output_tensor_desc};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_ACTIVATION_SOFTMAX,
                                     &softmax_operator_desc};
  hr = CompileOperator(operator_desc, 1, operation->inputs, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling softmax operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileReshape(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileReshape";
  HRESULT hr = CreateIntermediateResource(dml_, model, operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  size_t input_index = operation->inputs[0];
  DML_BUFFER_TENSOR_DESC input_buffer_desc =
      dml_->operand_map_[input_index]->operand_desc_;
  DML_TENSOR_DESC input_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                       &input_buffer_desc};

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  DML_CAST_OPERATOR_DESC cast_operator_desc = {&input_tensor_desc,
                                               &output_tensor_desc};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_CAST, &cast_operator_desc};
  hr = CompileOperator(operator_desc, 1, operation->inputs, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling reshape operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileConcatenation(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileConcatenation";
  ConcatParams params;
  int32_t result = compilation_->GetConcatParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return E_FAIL;

  uint32_t axis = 0;
  // Original rank has convert to 4 for NCHW.
  switch (model->operands[operation->inputs[0]]->dimensions.size()) {
    case 1:
      axis = 3;
      break;
    case 2:
      axis = params.axis + 2;
      break;
    case 3:
      if (params.axis == 2) {
        axis = 1;
      } else {
        axis = params.axis + 2;
      }
      break;
    case 4:
      if (params.axis == 0) {
        axis = 0;
      } else if (params.axis == 1) {
        axis = 2;
      } else if (params.axis == 2) {
        axis = 3;
      } else {
        axis = 1;
      }
      break;
    default:
      LOG(ERROR) << "The rank isn't supported.";
      return E_FAIL;
  }

  // Check constants for inputs tensor.
  HRESULT hr = S_OK;
  size_t inputs_size = operation->inputs.size() - 1;
  DML_TENSOR_DESC inputs_tensor_desc[inputs_size];
  for (size_t i = 0; i < inputs_size; ++i) {
    size_t input_index = operation->inputs[i];
    std::string index_id(base::NumberToString(input_index));
    if (model->values.find(index_id) != model->values.end()) {
      hr = UploadConstantResource(dml_, model, input_index,
                                  compilation_->MapMemory(input_index));
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed uploading constant resource.";
        return hr;
      }
    }
    inputs_tensor_desc[i] = {DML_TENSOR_TYPE_BUFFER,
                             &dml_->operand_map_[input_index]->operand_desc_};
  }
  hr = CreateIntermediateResource(dml_, model, operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  DML_JOIN_OPERATOR_DESC join_operator_desc = {inputs_size, inputs_tensor_desc,
                                               &output_tensor_desc, axis};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_JOIN, &join_operator_desc};

  hr = CompileOperator(operator_desc, inputs_size, operation->inputs,
                       operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling gather operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileFullyConnected(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileFullyConnected";
  FullyConnectedParams params;
  int32_t result = compilation_->GetFullyConnectedParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return E_FAIL;

  HRESULT hr = S_OK;
  // Create committed resource for weights and bias.
  for (size_t i = 1; i < 3; ++i) {
    size_t index = operation->inputs[i];
    std::string index_id(base::NumberToString(index));
    if (model->values.find(index_id) != model->values.end()) {
      hr = UploadConstantResource(dml_, model, index,
                                  compilation_->MapMemory(index));
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed uploading for weights and bias.";
        return hr;
      }
    }
  }
  hr = CreateIntermediateResource(dml_, model, operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  // Update tensor's dimensions to [batch_size, input_size]
  size_t input_index = operation->inputs[0];
  OperandDML* operand = dml_->operand_map_[input_index].get();
  // Update the dimensions to be used in FormatData class.
  operand->dimensions_ = {1, 1, params.input_batch_size, params.input_size};
  std::vector<uint32_t> input_strides = {
      operand->dimensions_[1] * operand->dimensions_[2] *
          operand->dimensions_[3],
      operand->dimensions_[2] * operand->dimensions_[3],
      operand->dimensions_[3],
      1,
  };
  DML_BUFFER_TENSOR_DESC input_buffer_desc = operand->operand_desc_;
  input_buffer_desc.Strides = input_strides.data();
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
  // Update strides to support broadcasting for bias.
  std::vector<uint32_t> bias_dimensions = {1, 1, params.output_batch_size,
                                           params.output_num_units};
  std::vector<uint32_t> bias_strides = {1, 1, 0, 1};
  bias_buffer_desc.Sizes = bias_dimensions.data();
  bias_buffer_desc.Strides = bias_strides.data();
  DML_TENSOR_DESC bias_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                      &bias_buffer_desc};

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  DML_GEMM_OPERATOR_DESC gemm_operator_desc = {&input_tensor_desc,
                                               &weights_tensor_desc,
                                               &bias_tensor_desc,
                                               &output_tensor_desc,
                                               DML_MATRIX_TRANSFORM_NONE,
                                               DML_MATRIX_TRANSFORM_TRANSPOSE,
                                               1.0f,
                                               1.0f,
                                               nullptr};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_GEMM, &gemm_operator_desc};
  hr = CompileOperator(operator_desc, 3, operation->inputs, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling fully connected operator.";
    return hr;
  }
  hr = CompileActivation(params.fuse_code, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling activation operator.";
    return hr;
  }

  return S_OK;
}

HRESULT CompilationDelegateDML::CompileBilinearScale(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileBilinearScale";
  ResizeBilinearParams params;
  int32_t result = compilation_->GetResizeBilinearParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return E_FAIL;

  if (params.new_height % params.height != 0 ||
      params.new_width % params.width != 0) {
    LOG(ERROR) << "The upsampling factor for the x/y must be integer.";
    return E_FAIL;
  }

  HRESULT hr = CreateIntermediateResource(dml_, model, operation->outputs[0]);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating intermediate resource for output.";
    return hr;
  }

  size_t input_index = operation->inputs[0];
  DML_BUFFER_TENSOR_DESC input_buffer_desc =
      dml_->operand_map_[input_index]->operand_desc_;
  DML_TENSOR_DESC input_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                       &input_buffer_desc};

  size_t output_index = operation->outputs[0];
  DML_BUFFER_TENSOR_DESC output_buffer_desc =
      dml_->operand_map_[output_index]->operand_desc_;
  DML_TENSOR_DESC output_tensor_desc = {DML_TENSOR_TYPE_BUFFER,
                                        &output_buffer_desc};

  DML_SIZE_2D scale_size = {params.x_scale, params.y_scale};
  DML_UPSAMPLE_2D_OPERATOR_DESC upsample_operator_desc = {
      &input_tensor_desc, &output_tensor_desc, scale_size,
      DML_INTERPOLATION_MODE_LINEAR};
  DML_OPERATOR_DESC operator_desc = {DML_OPERATOR_UPSAMPLE_2D,
                                     &upsample_operator_desc};
  hr = CompileOperator(operator_desc, 1, operation->inputs, operation->outputs);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed compiling resize bilinear operator.";
    return hr;
  }

  return S_OK;
}

}  // namespace ml
