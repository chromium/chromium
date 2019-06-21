// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "services/ml/dml/format_data.h"

#include "base/logging.h"
#include "services/ml/common.h"
#include "services/ml/dml/format_input_shader.h"
#include "services/ml/dml/format_output_shader.h"
#include "services/ml/dml/half_input_shader.h"
#include "services/ml/dml/half_output_shader.h"
#include "services/ml/dml_d3dx12_utils.h"
#include "services/ml/dml_symbol_table.h"
#include "services/ml/ml_utils_dml.h"

namespace ml {

namespace {

enum ROOT_PARAMETERS {
  CONSTANT_BUFFER = 0,
  FIRST_UNORDERED_VIEW,
  SECOND_UNORDERED_VIEW
};

struct Dimension {
  UINT num;
  UINT channel;
  UINT size;
};

D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(ID3D12Device* d3d12_device,
                                         ID3D12DescriptorHeap* descriptor_heap,
                                         size_t index) {
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc = descriptor_heap->GetDesc();
  uint32_t increment_size =
      d3d12_device->GetDescriptorHandleIncrementSize(heap_desc.Type);
  D3D12_GPU_DESCRIPTOR_HANDLE handle;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle =
      descriptor_heap->GetGPUDescriptorHandleForHeapStart();
  handle.ptr = gpu_handle.ptr + UINT64(index) * UINT64(increment_size);
  return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(ID3D12Device* d3d12_device,
                                         ID3D12DescriptorHeap* descriptor_heap,
                                         size_t index) {
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc = descriptor_heap->GetDesc();
  uint32_t increment_size =
      d3d12_device->GetDescriptorHandleIncrementSize(heap_desc.Type);
  D3D12_CPU_DESCRIPTOR_HANDLE handle;
  D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle =
      descriptor_heap->GetCPUDescriptorHandleForHeapStart();
  handle.ptr = cpu_handle.ptr + UINT64(index) * UINT64(increment_size);
  return handle;
}

void CreateUnorderedAccessView(CompiledModelDML* dml,
                               ID3D12Resource* resource,
                               size_t element_count,
                               DXGI_FORMAT format,
                               ID3D12DescriptorHeap* descriptor_heap,
                               size_t descriptor_index) {
  // Describe a UAV for the original input tensor.
  D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
  uav_desc.Format = format;
  uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uav_desc.Buffer.FirstElement = 0;
  uav_desc.Buffer.NumElements = element_count;
  uav_desc.Buffer.StructureByteStride = 0;
  uav_desc.Buffer.CounterOffsetInBytes = 0;
  uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
  // Create UAV in slot index.
  dml->d3d12_device_->CreateUnorderedAccessView(
      resource, nullptr, &uav_desc,
      GetCpuHandle(dml->d3d12_device_.Get(), descriptor_heap,
                   descriptor_index));
}

}  // namespace

FormatData::FormatData(CompiledModelDML* dml) {
  // Create descriptor heaps.
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
  descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descriptor_heap_desc.NumDescriptors =
      2 * (dml->inputs_.size() + dml->outputs_.size());
  descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  HRESULT hr = dml->d3d12_device_->CreateDescriptorHeap(
      &descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating descriptor heap.";
    return;
  }

  const std::vector<uint32_t>& inputs = dml->inputs_;
  for (size_t i = 0; i < inputs.size(); ++i) {
    const OperandDML* operand = dml->operand_map_[inputs[i]].get();
    size_t element_count = product(operand->dimensions_);
    CreateUnorderedAccessView(dml, operand->format_resource_.Get(),
                              element_count, DXGI_FORMAT_R32_FLOAT,
                              descriptor_heap_.Get(), 2 * i);
    CreateUnorderedAccessView(
        dml, operand->operand_resource_.Get(), element_count,
        g_support_f16 ? DXGI_FORMAT_R16_FLOAT : DXGI_FORMAT_R32_FLOAT,
        descriptor_heap_.Get(), 2 * i + 1);
  }

  size_t output_heap_index = inputs.size() * 2;
  const std::vector<uint32_t>& outputs = dml->outputs_;
  for (size_t i = 0; i < outputs.size(); ++i) {
    const OperandDML* operand = dml->operand_map_[outputs[i]].get();
    size_t element_count = product(operand->dimensions_);
    CreateUnorderedAccessView(
        dml, operand->operand_resource_.Get(), element_count,
        g_support_f16 ? DXGI_FORMAT_R16_FLOAT : DXGI_FORMAT_R32_FLOAT,
        descriptor_heap_.Get(), 2 * i + output_heap_index);
    CreateUnorderedAccessView(dml, operand->format_resource_.Get(),
                              element_count, DXGI_FORMAT_R32_FLOAT,
                              descriptor_heap_.Get(),
                              2 * i + 1 + output_heap_index);
  }

  // Define root table layout.
  CD3DX12_DESCRIPTOR_RANGE descriptor_range[2];
  descriptor_range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // u0
  descriptor_range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);  // u1

  CD3DX12_ROOT_PARAMETER root_parameters[3];
  root_parameters[CONSTANT_BUFFER].InitAsConstants(3, 0);
  root_parameters[FIRST_UNORDERED_VIEW].InitAsDescriptorTable(
      1, &descriptor_range[0], D3D12_SHADER_VISIBILITY_ALL);
  root_parameters[SECOND_UNORDERED_VIEW].InitAsDescriptorTable(
      1, &descriptor_range[1], D3D12_SHADER_VISIBILITY_ALL);

  CD3DX12_ROOT_SIGNATURE_DESC root_signature(_countof(root_parameters),
                                             root_parameters);
  ComPtr<ID3DBlob> serialized_signature;
  hr = D3D(D3D12SerializeRootSignature)(&root_signature,
                                        D3D_ROOT_SIGNATURE_VERSION_1,
                                        &serialized_signature, nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed serializeing root signature.";
    return;
  }
  // Create the root signature
  hr = dml->d3d12_device_->CreateRootSignature(
      0, serialized_signature->GetBufferPointer(),
      serialized_signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating root signature.";
    return;
  }

  // Create compute pipeline state
  D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_state_desc = {};
  pipeline_state_desc.pRootSignature = root_signature_.Get();
  pipeline_state_desc.CS.pShaderBytecode =
      g_support_f16 ? g_format_half_input : g_format_input;
  pipeline_state_desc.CS.BytecodeLength =
      _countof(g_support_f16 ? g_format_half_input : g_format_input);

  hr = dml->d3d12_device_->CreateComputePipelineState(
      &pipeline_state_desc, IID_PPV_ARGS(&input_pipline_state_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating input compute pipeline state.";
    return;
  }

  pipeline_state_desc.pRootSignature = root_signature_.Get();
  // byte_code = g_support_f16 ? g_format_half_output : g_format_output;
  pipeline_state_desc.CS.pShaderBytecode =
      g_support_f16 ? g_format_half_output : g_format_output;
  pipeline_state_desc.CS.BytecodeLength =
      _countof(g_support_f16 ? g_format_half_output : g_format_output);

  hr = dml->d3d12_device_->CreateComputePipelineState(
      &pipeline_state_desc, IID_PPV_ARGS(&output_pipline_state_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating output compute pipeline state.";
    return;
  }
}

FormatData::~FormatData() = default;

HRESULT FormatData::FormatInputData(CompiledModelDML* dml) {
  if (!input_pipline_state_.Get())
    return E_FAIL;

  ID3D12DescriptorHeap* descriptor_heaps[] = {descriptor_heap_.Get()};
  dml->command_list_->SetDescriptorHeaps(_countof(descriptor_heaps),
                                         descriptor_heaps);

  const std::vector<uint32_t>& inputs = dml->inputs_;
  for (size_t i = 0; i < inputs.size(); ++i) {
    dml->command_list_->SetComputeRootSignature(root_signature_.Get());
    const OperandDML* operand = dml->operand_map_[inputs[i]].get();
    Dimension dimension = {};
    dimension.num = operand->dimensions_[0];
    dimension.channel = operand->dimensions_[1];
    dimension.size = operand->dimensions_[2] * operand->dimensions_[3];

    // 3 is the number of constants to set in the root signature.
    // 0 is the offset, in 32-bit values, to set the first constant of the group
    // in the root signature.
    dml->command_list_->SetComputeRoot32BitConstants(CONSTANT_BUFFER, 3,
                                                     &dimension, 0);
    // FIRST_UNORDERED_VIEW is the slot number for binding.
    // A GPU_descriptor_handle object for the base descriptor to set.
    dml->command_list_->SetComputeRootDescriptorTable(
        FIRST_UNORDERED_VIEW,
        GetGpuHandle(dml->d3d12_device_.Get(), descriptor_heap_.Get(), 2 * i));
    dml->command_list_->SetComputeRootDescriptorTable(
        SECOND_UNORDERED_VIEW, GetGpuHandle(dml->d3d12_device_.Get(),
                                            descriptor_heap_.Get(), 2 * i + 1));

    dml->command_list_->SetPipelineState(input_pipline_state_.Get());
    // 512 is the number of threads to be executed in a single group.
    // dimensions / 512 is the number of groups dispatched in the x direction.
    dml->command_list_->Dispatch(product(operand->dimensions_) / 512 + 1, 1, 1);
  }
  CD3DX12_RESOURCE_BARRIER uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
  dml->command_list_->ResourceBarrier(1, &uav_barrier);
  return S_OK;
}

HRESULT FormatData::FormatOutputData(CompiledModelDML* dml) {
  if (!output_pipline_state_.Get())
    return E_FAIL;

  ID3D12DescriptorHeap* descriptor_heaps[] = {descriptor_heap_.Get()};
  dml->command_list_->SetDescriptorHeaps(_countof(descriptor_heaps),
                                         descriptor_heaps);

  size_t output_heap_index = dml->inputs_.size() * 2;
  const std::vector<uint32_t>& outputs = dml->outputs_;
  for (size_t i = 0; i < outputs.size(); ++i) {
    dml->command_list_->SetComputeRootSignature(root_signature_.Get());
    const OperandDML* operand = dml->operand_map_[outputs[i]].get();
    Dimension dimension = {};
    dimension.num = operand->dimensions_[0];
    dimension.channel = operand->dimensions_[1];
    dimension.size = operand->dimensions_[2] * operand->dimensions_[3];

    // 3 is the number of constants to set in the root signature.
    // 0 is the offset, in 32-bit values, to set the first constant of the group
    // in the root signature.
    dml->command_list_->SetComputeRoot32BitConstants(CONSTANT_BUFFER, 3,
                                                     &dimension, 0);
    // dml->command_list_->SetComputeRoot32BitConstants(CONSTANT_BUFFER, 1,
    //                                                  &dimension, 12);
    // FIRST_UNORDERED_VIEW is the slot number for binding.
    // A GPU_descriptor_handle object for the base descriptor to set.
    dml->command_list_->SetComputeRootDescriptorTable(
        FIRST_UNORDERED_VIEW,
        GetGpuHandle(dml->d3d12_device_.Get(), descriptor_heap_.Get(),
                     2 * i + output_heap_index));
    dml->command_list_->SetComputeRootDescriptorTable(
        SECOND_UNORDERED_VIEW,
        GetGpuHandle(dml->d3d12_device_.Get(), descriptor_heap_.Get(),
                     2 * i + 1 + output_heap_index));

    dml->command_list_->SetPipelineState(output_pipline_state_.Get());
    // 512 is the number of threads to be executed in a single group.
    // dimensions / 512 is the number of groups dispatched in the x direction.
    dml->command_list_->Dispatch(product(operand->dimensions_) / 512 + 1, 1, 1);
  }
  CD3DX12_RESOURCE_BARRIER uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
  dml->command_list_->ResourceBarrier(1, &uav_barrier);
  return S_OK;
}

}  // namespace ml
