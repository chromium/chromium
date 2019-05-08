// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "services/ml/execution_impl_dml.h"

#include <utility>

#include "services/ml/common.h"
#include "services/ml/dml_d3dx12_utils.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

namespace {

HRESULT ResetBindingTable(scoped_refptr<CompiledModelDML> dml,
                          size_t operation_index) {
  if (operation_index > 0) {
    static size_t increment_size =
        dml->d3D12_device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    size_t offset_in_descriptors =
        dml->operations_[operation_index - 1]->descriptor_count_;
    size_t offset_ptr = INT64(offset_in_descriptors) * UINT64(increment_size);
    dml->binding_table_desc_.CPUDescriptorHandle.ptr += offset_ptr;
    dml->binding_table_desc_.GPUDescriptorHandle.ptr += offset_ptr;
  }
  dml->binding_table_desc_.SizeInDescriptors =
      dml->operations_[operation_index]->descriptor_count_;
  dml->binding_table_desc_.Dispatchable =
      dml->operations_[operation_index]->compiled_operator_.Get();

  HRESULT hr = dml->binding_table_->Reset(&dml->binding_table_desc_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed resetting binding table.";
    return hr;
  }
  return S_OK;
}

}  // namespace

ExecutionImplDML::ExecutionImplDML(scoped_refptr<CompiledModelDML> dml,
                                   mojom::ExecutionInitParamsPtr params)
    : params_(std::move(params)), dml_(dml) {}

ExecutionImplDML::~ExecutionImplDML() = default;

void ExecutionImplDML::StartCompute(StartComputeCallback callback) {
  // Bind and execute the operator on the GPU.
  ID3D12DescriptorHeap* d3D12_descriptor_heaps[] = {
      dml_->descriptor_heap_.Get()};
  dml_->command_list_->SetDescriptorHeaps(ARRAYSIZE(d3D12_descriptor_heaps),
                                          d3D12_descriptor_heaps);

  uint32_t memory_offset = 0;
  HRESULT hr = S_OK;
  for (size_t i = 0; i < params_->inputs.size(); ++i) {
    const mojom::OperandInfoPtr& operand = params_->inputs[i];
    uint32_t offset = memory_offset;
    uint32_t length = GetRequiredSize(operand);
    memory_offset += length;
    auto mapping = params_->memory->MapAtOffset(length, offset);
    ComPtr<ID3D12Resource> upload_resource =
        dml_->operand_map_[operand->index]->upload_resource_;
    ComPtr<ID3D12Resource> operand_resource =
        dml_->operand_map_[operand->index]->operand_resource_;
    hr = UploadTensorResource(static_cast<void*>(mapping.get()), length,
                              upload_resource, operand_resource,
                              dml_->command_list_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed uploading tensor resource for inputs data.";
      std::move(callback).Run(mojom::BAD_DATA);
      return;
    }
  }

  for (size_t i = 0; i < dml_->operations_.size(); ++i) {
    hr = ExecuteCompiledOperator(dml_->operations_[i]->compiled_operator_.Get(),
                                 dml_->operations_[i], i);
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
    const std::unique_ptr<OperationDML>& operation,
    uint32_t operation_index) {
  // Reset the binding table to bind for the operator, or create
  // a new table for executing operators.
  HRESULT hr = ResetBindingTable(dml_, operation_index);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed resetting binding table on executing operator.";
    return hr;
  }

  DML_BINDING_PROPERTIES binding_properties =
      compiled_operator->GetBindingProperties();
  if (binding_properties.TemporaryResourceSize != 0) {
    DML_BUFFER_BINDING buffer_binding = {dml_->temporary_buffer_.Get(), 0,
                                         dml_->temporary_resource_size_};
    DML_BINDING_DESC binding_desc = {DML_BINDING_TYPE_BUFFER, &buffer_binding};
    dml_->binding_table_->BindTemporaryResource(&binding_desc);
  }

  size_t input_size = operation->bind_inputs_size;
  DML_BUFFER_BINDING input_buffer_binding[input_size];
  DML_BINDING_DESC input_binding_array[input_size];
  DCHECK(input_size != 0);
  for (size_t i = 0; i < input_size; ++i) {
    size_t input_index = operation->inputs_[i];
    UINT64 input_buffer_size = dml_->operand_map_[input_index]->SizeInBytes();
    ComPtr<ID3D12Resource> input_resource =
        dml_->operand_map_[input_index]->operand_resource_;
    input_buffer_binding[i] = {input_resource.Get(), 0, input_buffer_size};
    input_binding_array[i] = {DML_BINDING_TYPE_BUFFER,
                              &input_buffer_binding[i]};
  }
  dml_->binding_table_->BindInputs(input_size, input_binding_array);

  DCHECK(operation->outputs_.size() == 1);
  size_t output_index = operation->outputs_[0];
  UINT64 output_buffer_size = dml_->operand_map_[output_index]->SizeInBytes();
  ComPtr<ID3D12Resource> output_resource =
      dml_->operand_map_[output_index]->operand_resource_;
  DML_BUFFER_BINDING output_buffer_binding = {output_resource.Get(), 0,
                                              output_buffer_size};
  DML_BINDING_DESC output_binding_desc{DML_BINDING_TYPE_BUFFER,
                                       &output_buffer_binding};
  dml_->binding_table_->BindOutputs(1, &output_binding_desc);

  CD3DX12_RESOURCE_BARRIER resource_barrier =
      CD3DX12_RESOURCE_BARRIER::UAV(output_resource.Get());
  dml_->command_list_->ResourceBarrier(1, &resource_barrier);

  // Record execution of the compiled operator.
  dml_->command_recorder_->RecordDispatch(
      dml_->command_list_.Get(), compiled_operator, dml_->binding_table_.Get());

  return S_OK;
}

HRESULT ExecutionImplDML::ReadResultBack(uint32_t memory_offset) {
  // The output buffer now contains the result of the identity operator,
  // so read it back if you want the CPU to access it.
  HRESULT hr = S_OK;
  for (size_t i = 0; i < params_->outputs.size(); ++i) {
    size_t output_index = params_->outputs[i]->index;
    ComPtr<ID3D12Resource> output_resource =
        dml_->operand_map_[output_index]->operand_resource_;
    ComPtr<ID3D12Resource> readback_buffer =
        dml_->operand_map_[output_index]->readback_resource_;
    CD3DX12_RESOURCE_BARRIER resource_barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            output_resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
    dml_->command_list_->ResourceBarrier(1, &resource_barrier);

    dml_->command_list_->CopyResource(readback_buffer.Get(),
                                      output_resource.Get());
  }

  CloseExecuteResetWait(dml_->d3D12_device_, dml_->command_queue_,
                        dml_->command_allocator_, dml_->command_list_);

  for (size_t i = 0; i < params_->outputs.size(); ++i) {
    size_t output_index = params_->outputs[i]->index;
    ComPtr<ID3D12Resource> readback_buffer =
        dml_->operand_map_[output_index]->readback_resource_;
    const mojom::OperandInfoPtr& operand = params_->outputs[i];
    const uint32_t offset = memory_offset;
    const uint32_t output_buffer_size = GetRequiredSize(operand);
    memory_offset += output_buffer_size;
    auto mapping = params_->memory->MapAtOffset(output_buffer_size, offset);
    D3D12_RANGE tensor_buffer_range = {0, output_buffer_size};
    void* output_buffer_data = nullptr;
    hr = readback_buffer->Map(0, &tensor_buffer_range, &output_buffer_data);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed map buffer for reading result.";
      return hr;
    }
    memcpy(mapping.get(), output_buffer_data, output_buffer_size);

    D3D12_RANGE empty_range{0, 0};
    readback_buffer->Unmap(0, &empty_range);
  }

  return S_OK;
}

}  // namespace ml
