// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "services/ml/ml_utils_dml.h"

#include "base/logging.h"
#include "services/ml/common.h"
#include "services/ml/dml/float16_compressor.h"
#include "services/ml/dml/format_data.h"
#include "services/ml/dml_d3dx12_utils.h"

namespace ml {

OperationDML::OperationDML(ComPtr<IDMLCompiledOperator> compiled_operator,
                           size_t descriptor_index,
                           int32_t inputs_size,
                           std::vector<uint32_t> inputs,
                           std::vector<uint32_t> outputs,
                           ComPtr<ID3D12Resource> persistent_buffer,
                           uint64_t persistent_size)
    : descriptor_index_(descriptor_index),
      binding_table_(nullptr),
      compiled_operator_(std::move(compiled_operator)),
      persistent_buffer_(std::move(persistent_buffer)),
      persistent_size_(persistent_size),
      bind_inputs_size(inputs_size),
      inputs_(inputs),
      outputs_(outputs) {}

OperationDML::~OperationDML() = default;

OperandDML::OperandDML(const std::vector<uint32_t>& sizes,
                       bool depth_conv_weight)
    : operand_desc_({}),
      operand_resource_(nullptr),
      upload_resource_(nullptr),
      readback_resource_(nullptr) {
  // All buffer tensors must have a DimensionCount of either 4 or 5.
  // input data of NHWC to DirectML NCHW.
  switch (sizes.size()) {
    case 1:
      // One bias per output channel.
      dimensions_ = {1, sizes[0], 1, 1};
      break;
    case 2:
      dimensions_ = {1, 1, sizes[0], sizes[1]};
      break;
    case 3:
      dimensions_ = {1, sizes[2], sizes[0], sizes[1]};
      break;
    case 4:
      if (depth_conv_weight) {
        // depth_out, depth_in, h, w
        dimensions_ = {sizes[3], sizes[0], sizes[1], sizes[2]};
      } else {
        dimensions_ = {sizes[0], sizes[3], sizes[1], sizes[2]};
      }
      break;
    default: {
      NOTREACHED();
      LOG(ERROR) << "The dimension isn't supported.";
    }
  }
  strides_ = {
      dimensions_[1] * dimensions_[2] * dimensions_[3],
      dimensions_[2] * dimensions_[3],
      dimensions_[3],
      1,
  };

  operand_desc_.DataType = g_support_f16 ? DML_TENSOR_DATA_TYPE_FLOAT16
                                         : DML_TENSOR_DATA_TYPE_FLOAT32;
  operand_desc_.Flags = DML_TENSOR_FLAG_NONE;
  operand_desc_.DimensionCount = dimensions_.size();
  operand_desc_.Sizes = dimensions_.data();
  operand_desc_.Strides = strides_.data();
  operand_desc_.TotalTensorSizeInBytes = DMLCalcBufferTensorSize(
      operand_desc_.DataType, operand_desc_.DimensionCount, operand_desc_.Sizes,
      operand_desc_.Strides);
}
OperandDML::~OperandDML() = default;

CompiledModelDML::CompiledModelDML(std::vector<uint32_t> inputs,
                                   std::vector<uint32_t> outputs)
    : inputs_(inputs), outputs_(outputs) {}
CompiledModelDML::~CompiledModelDML() = default;

D3D12_GPU_DESCRIPTOR_HANDLE CompiledModelDML::GetGpuHandle(size_t index) const {
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc = descriptor_heap_->GetDesc();
  DCHECK(index < heap_desc.NumDescriptors);
  uint32_t increment_size =
      d3d12_device_->GetDescriptorHandleIncrementSize(heap_desc.Type);
  D3D12_GPU_DESCRIPTOR_HANDLE handle;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle =
      descriptor_heap_->GetGPUDescriptorHandleForHeapStart();
  handle.ptr = gpu_handle.ptr + UINT64(index) * UINT64(increment_size);
  return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE CompiledModelDML::GetCpuHandle(size_t index) const {
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc = descriptor_heap_->GetDesc();
  DCHECK(index < heap_desc.NumDescriptors);
  uint32_t increment_size =
      d3d12_device_->GetDescriptorHandleIncrementSize(heap_desc.Type);
  D3D12_CPU_DESCRIPTOR_HANDLE handle;
  D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle =
      descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
  handle.ptr = cpu_handle.ptr + UINT64(index) * UINT64(increment_size);
  return handle;
}

void CompiledModelDML::CreateFormatData() {
  format_data_ = std::make_unique<FormatData>(this);
}

void CompiledModelDML::FormatInputData() {
  format_data_->FormatInputData(this);
}

void CompiledModelDML::FormatOutputData() {
  format_data_->FormatOutputData(this);
}

UINT64 DMLCalcBufferTensorSize(DML_TENSOR_DATA_TYPE dataType,
                               UINT dimensionCount,
                               _In_reads_(dimensionCount) const UINT* sizes,
                               const UINT* strides) {
  UINT elementSizeInBytes = 0;
  switch (dataType) {
    case DML_TENSOR_DATA_TYPE_FLOAT32:
    case DML_TENSOR_DATA_TYPE_UINT32:
    case DML_TENSOR_DATA_TYPE_INT32:
      elementSizeInBytes = 4;
      break;

    case DML_TENSOR_DATA_TYPE_FLOAT16:
    case DML_TENSOR_DATA_TYPE_UINT16:
    case DML_TENSOR_DATA_TYPE_INT16:
      elementSizeInBytes = 2;
      break;

    case DML_TENSOR_DATA_TYPE_UINT8:
    case DML_TENSOR_DATA_TYPE_INT8:
      elementSizeInBytes = 1;
      break;

    default:
      return 0;  // Invalid data type
  }

  UINT64 minimumImpliedSizeInBytes = 0;
  if (!strides) {
    minimumImpliedSizeInBytes = sizes[0];
    for (UINT i = 1; i < dimensionCount; ++i) {
      minimumImpliedSizeInBytes *= sizes[i];
    }
    minimumImpliedSizeInBytes *= elementSizeInBytes;
  } else {
    UINT indexOfLastElement = 0;
    for (UINT i = 0; i < dimensionCount; ++i) {
      indexOfLastElement += (sizes[i] - 1) * strides[i];
    }

    minimumImpliedSizeInBytes = (indexOfLastElement + 1) * elementSizeInBytes;
  }

  // Round up to the nearest 4 bytes.
  UINT64 roundUpSizeInBytes = (minimumImpliedSizeInBytes + 3) & ~3ui64;
  // DCHECK(roundUpSizeInBytes == minimumImpliedSizeInBytes);

  return roundUpSizeInBytes;
}

HRESULT CloseExecuteResetWait(ComPtr<ID3D12Device> d3D12_device,
                              ComPtr<ID3D12CommandQueue> command_queue,
                              ComPtr<ID3D12CommandAllocator> command_allocator,
                              ComPtr<ID3D12GraphicsCommandList> command_list) {
  HRESULT hr = command_list->Close();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed closing command list.";
    return hr;
  }

  ID3D12CommandList* command_lists[] = {command_list.Get()};
  command_queue->ExecuteCommandLists(ARRAYSIZE(command_lists), command_lists);

  ComPtr<ID3D12Fence> d3D12_fence;
  hr = d3D12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                 IID_PPV_ARGS(&d3D12_fence));

  HANDLE fence_event_handle = CreateEvent(nullptr, true, false, nullptr);
  if (!fence_event_handle) {
    LOG(ERROR) << "Failed creating event.";
    return hr;
  }

  hr = d3D12_fence->SetEventOnCompletion(1, fence_event_handle);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed setting event.";
    return hr;
  }

  hr = command_queue->Signal(d3D12_fence.Get(), 1);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed signal command queue.";
    return hr;
  }
  WaitForSingleObjectEx(fence_event_handle, INFINITE, FALSE);

  hr = command_allocator->Reset();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed resetting command allocator.";
    return hr;
  }
  hr = command_list->Reset(command_allocator.Get(), nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed resetting command list.";
    return hr;
  }

  return S_OK;
}

// Create common resource for temporary and persistent resource.
HRESULT CreateCommonResource(uint64_t size,
                             ComPtr<ID3D12Resource>& commom_resource,
                             ComPtr<ID3D12Device> d3D12_device) {
  CD3DX12_HEAP_PROPERTIES default_heap =
      CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(
      size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  HRESULT hr = d3D12_device->CreateCommittedResource(
      &default_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&commom_resource));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for temorary buffer.";
    return hr;
  }
  return S_OK;
}

HRESULT CreateOutputResource(uint64_t size,
                             ComPtr<ID3D12Resource>& intermediate_resource,
                             ComPtr<ID3D12Device> d3D12_device) {
  CD3DX12_HEAP_PROPERTIES default_heap =
      CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(
      size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  HRESULT hr = d3D12_device->CreateCommittedResource(
      &default_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
      IID_PPV_ARGS(&intermediate_resource));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for output.";
    return hr;
  }
  return S_OK;
}

HRESULT CreateReadbackResource(uint64_t size,
                               ComPtr<ID3D12Resource>& readback_resource,
                               ComPtr<ID3D12Resource>& formatted_resource,
                               ComPtr<ID3D12Device> d3D12_device) {
  CD3DX12_HEAP_PROPERTIES readback_heap =
      CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
  CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
  HRESULT hr = d3D12_device->CreateCommittedResource(
      &readback_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&readback_resource));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for output data.";
    return hr;
  }

  hr = CreateCommonResource(size, formatted_resource, d3D12_device);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating resource for formatting output data.";
    return hr;
  }

  return S_OK;
}

HRESULT CreateUploadResource(uint64_t size,
                             ComPtr<ID3D12Resource>& upload_resource,
                             ComPtr<ID3D12Resource>& input_resource,
                             ComPtr<ID3D12Device> d3D12_device) {
  CD3DX12_HEAP_PROPERTIES upload_heap =
      CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
  HRESULT hr = d3D12_device->CreateCommittedResource(
      &upload_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&upload_resource));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for reading";
    return hr;
  }

  CD3DX12_HEAP_PROPERTIES default_heap =
      CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  resource_desc = CD3DX12_RESOURCE_DESC::Buffer(
      size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  hr = d3D12_device->CreateCommittedResource(
      &default_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&input_resource));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for coping";
    return hr;
  }
  return S_OK;
}

HRESULT UploadTensorResource(const void* data,
                             uint64_t size,
                             ComPtr<ID3D12Resource>& upload_resource,
                             ComPtr<ID3D12Resource>& input_resource,
                             ComPtr<ID3D12GraphicsCommandList> command_list) {
  D3D12_SUBRESOURCE_DATA subresource_data = {};
  subresource_data.pData = data;
  subresource_data.RowPitch = size;
  subresource_data.SlicePitch = subresource_data.RowPitch;

  // Upload the input tensor to the GPU.
  UpdateSubresources(command_list.Get(), input_resource.Get(),
                     upload_resource.Get(), 0, 0, 1, &subresource_data);

  CD3DX12_RESOURCE_BARRIER resource_barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(
          input_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
          D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_list->ResourceBarrier(1, &resource_barrier);

  return S_OK;
}

HRESULT FormatAndUploadResource(void* data,
                                const std::vector<uint32_t>& dimension,
                                ComPtr<ID3D12Resource>& upload_resource,
                                ComPtr<ID3D12Resource>& input_resource,
                                ComPtr<ID3D12GraphicsCommandList> command_list,
                                bool depth_conv_weight = false) {
  // float32 -> float16
  float* original_data = static_cast<float*>(data);
  std::vector<float> float32_data;
  std::vector<uint16_t> float16_data;
  size_t data_length = product(dimension);
  g_support_f16 ? float16_data.resize(data_length)
                : float32_data.resize(data_length);

  if (depth_conv_weight) {
    // n = 1
    size_t chw_length = dimension[0] * dimension[2] * dimension[3];
    size_t size = dimension[2] * dimension[3];
    size_t channel = dimension[0];
    for (size_t i = 0; i < chw_length; ++i) {
      if (g_support_f16) {
        float16_data[i % channel * size + i / channel] =
            Float16Compressor::compress(original_data[i]);
      } else {
        float32_data[i % channel * size + i / channel] = original_data[i];
      }
    }
  } else {
    // NHWC -> NCHW
    for (size_t n = 0; n < dimension[0]; ++n) {
      size_t chw_length = dimension[1] * dimension[2] * dimension[3];
      size_t size = dimension[2] * dimension[3];
      size_t channel = dimension[1];
      for (size_t i = 0; i < chw_length; ++i) {
        if (g_support_f16) {
          float16_data[i % channel * size + i / channel + chw_length * n] =
              Float16Compressor::compress(original_data[i + chw_length * n]);
        } else {
          float32_data[i % channel * size + i / channel + chw_length * n] =
              original_data[i + chw_length * n];
        }
      }
    }
  }

  // Upload float16 constant resources.
  if (g_support_f16) {
    UploadTensorResource(float16_data.data(),
                         float16_data.size() * sizeof(uint16_t),
                         upload_resource, input_resource, command_list);
  } else {
    UploadTensorResource(float32_data.data(),
                         float32_data.size() * sizeof(float), upload_resource,
                         input_resource, command_list);
  }
  return S_OK;
}

}  // namespace ml
