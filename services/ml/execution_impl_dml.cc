// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "services/ml/execution_impl_dml.h"

#include <utility>

#include "services/ml/compilation_delegate_dml.h"
#include "services/ml/dml_d3dx12_utils.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

ExecutionImplDML::ExecutionImplDML(const CompilationDelegateDML* compilation,
                                   std::unique_ptr<ExecutionData> dml,
                                   mojom::ExecutionInitParamsPtr params)
    : compilation_(compilation),
      params_(std::move(params)),
      dml_(std::move(dml)) {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  // The constants_ hold the value of setting with setOperandValue js API.
  for (size_t i = 0; i < dml_->constants_.size(); ++i) {
    uint32_t index = dml_->constants_[i];
    const mojom::OperandValueInfoPtr& input_info =
        model->values[base::NumberToString(index)];
    auto mapping = compilation_->MapMemory(index);
    HRESULT hr = UploadTensorResource(
        static_cast<void*>(mapping.get()), input_info->length,
        upload_resources_[index], input_resources_[index], dml_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed uploading tensor resource for inputs data.";
      return;
    }
  }
}

ExecutionImplDML::~ExecutionImplDML() = default;

void ExecutionImplDML::StartCompute(StartComputeCallback callback) {
  uint32_t memory_offset = 0;
  HRESULT hr = S_OK;
  for (size_t i = 0; i < params_->inputs.size(); ++i) {
    const mojom::OperandInfoPtr& operand = params_->inputs[i];
    uint32_t offset = memory_offset;
    uint32_t length = GetRequiredSize(operand);
    memory_offset += length;
    auto mapping = params_->memory->MapAtOffset(length, offset);
    hr = UploadTensorResource(static_cast<void*>(mapping.get()), length,
                              upload_resources_[operand->index],
                              input_resources_[operand->index], dml_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed uploading tensor resource for inputs data.";
      std::move(callback).Run(mojom::BAD_DATA);
      return;
    }
  }

  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  for (size_t i = 0; i < model->operations.size(); ++i) {
    const mojom::OperationPtr& operation = model->operations[i];
    size_t inputs_size = 1;
    if (operation->type == mojom::ADD) {
      inputs_size = 2;
    }
    hr = ExecuteCompiledOperator(dml_->compiled_operators_[i].Get(),
                                 inputs_size, operation);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed executing operator.";
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
  }
  hr = CloseExecuteResetWait(dml_->d3D12_device_, dml_->command_queue_,
                             dml_->command_allocator_, dml_->command_list_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed executing command list for compiled operators.";
    return;
  }

  hr = ReadResultBack(memory_offset);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed reading result.";
    std::move(callback).Run(mojom::OP_FAILED);
    return;
  }

  std::move(callback).Run(mojom::NOT_ERROR);
}

HRESULT ExecutionImplDML::ExecuteCompiledOperator(
    IDMLCompiledOperator* compiled_operator,
    uint32_t input_size,
    const mojom::OperationPtr& operation) {
  // Reset the binding table to bind for the operator we want to execute that
  // was previously used to bind for the initializer.
  dml_->binding_table_desc_.Dispatchable = compiled_operator;

  HRESULT hr = dml_->binding_table_->Reset(&dml_->binding_table_desc_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed resetting binding table.";
    return hr;
  }

  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  DML_BUFFER_BINDING input_buffer_binding[input_size];
  DML_BINDING_DESC input_binding_array[input_size];
  DCHECK(input_size != 0);
  for (size_t i = 0; i < input_size; ++i) {
    size_t input_index = operation->inputs[i];
    const mojom::OperandPtr& operand = model->operands[input_index];
    input_buffer_binding[i] = {input_resources_[input_index].Get(), 0,
                               GetRequiredSize(operand)};
    input_binding_array[i] = {DML_BINDING_TYPE_BUFFER,
                              &input_buffer_binding[i]};
  }
  dml_->binding_table_->BindInputs(input_size, input_binding_array);

  DCHECK(operation->outputs.size() == 1);
  size_t output_index = operation->outputs[0];
  UINT64 output_buffer_size = GetRequiredSize(model->operands[output_index]);
  CD3DX12_HEAP_PROPERTIES default_heap =
      CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(
      output_buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  hr = dml_->d3D12_device_->CreateCommittedResource(
      &default_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
      IID_PPV_ARGS(&input_resources_[output_index]));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for output.";
    return hr;
  }

  DML_BUFFER_BINDING output_buffer_binding = {
      input_resources_[output_index].Get(), 0, output_buffer_size};
  DML_BINDING_DESC output_binding_desc{DML_BINDING_TYPE_BUFFER,
                                       &output_buffer_binding};
  dml_->binding_table_->BindOutputs(1, &output_binding_desc);

  // Record execution of the compiled operator.
  dml_->command_recorder_->RecordDispatch(
      dml_->command_list_.Get(), compiled_operator, dml_->binding_table_.Get());

  return S_OK;
}

HRESULT ExecutionImplDML::ReadResultBack(uint32_t memory_offset) {
  // The output buffer now contains the result of the identity operator,
  // so read it back if you want the CPU to access it.
  DCHECK(params_->outputs.size() == 1);
  const mojom::OperandInfoPtr& operand = params_->outputs[0];
  size_t output_index = operand->index;
  const uint32_t output_buffer_size = GetRequiredSize(operand);
  CD3DX12_HEAP_PROPERTIES readback_heap =
      CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
  CD3DX12_RESOURCE_DESC resource_desc =
      CD3DX12_RESOURCE_DESC::Buffer(output_buffer_size);
  ComPtr<ID3D12Resource> readback_buffer;
  HRESULT hr = dml_->d3D12_device_->CreateCommittedResource(
      &readback_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_buffer));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for output data.";
  }

  CD3DX12_RESOURCE_BARRIER resource_barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(
          input_resources_[output_index].Get(),
          D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
          D3D12_RESOURCE_STATE_COPY_SOURCE);
  dml_->command_list_->ResourceBarrier(1, &resource_barrier);

  dml_->command_list_->CopyResource(readback_buffer.Get(),
                                    input_resources_[output_index].Get());

  CloseExecuteResetWait(dml_->d3D12_device_, dml_->command_queue_,
                        dml_->command_allocator_, dml_->command_list_);

  auto mapping =
      params_->memory->MapAtOffset(output_buffer_size, memory_offset);
  D3D12_RANGE tensor_buffer_range = {0, output_buffer_size};
  void* output_buffer_data = nullptr;
  hr = readback_buffer->Map(0, &tensor_buffer_range, &output_buffer_data);
  memcpy(mapping.get(), output_buffer_data, output_buffer_size);

  D3D12_RANGE empty_range{0, 0};
  readback_buffer->Unmap(0, &empty_range);

  return S_OK;
}

}  // namespace ml
