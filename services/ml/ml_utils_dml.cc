// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "services/ml/ml_utils_dml.h"

#include "base/logging.h"
#include "services/ml/dml_d3dx12_utils.h"

namespace ml {

ExecutionData::ExecutionData() = default;
ExecutionData::~ExecutionData() = default;

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
  DCHECK(roundUpSizeInBytes == minimumImpliedSizeInBytes);

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

  hr = command_list->Reset(command_allocator.Get(), nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed resetting command list.";
    return hr;
  }

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

  return S_OK;
}

HRESULT UploadTensorResource(const void* data,
                             uint64_t size,
                             ComPtr<ID3D12Resource>& upload_resource,
                             ComPtr<ID3D12Resource>& input_resource,
                             std::unique_ptr<ExecutionData>& dml) {
  // const mojom::ModelInfoPtr& model = compilation_->GetModel();
  // const mojom::OperandValueInfoPtr& weights_info =
  //     model->values[base::NumberToString(weights_index)];
  // auto mapping = compilation_->MapMemory(weights_index);
  // memcpy(weights_memory_ptr, mapping.get(), weights_info->length);

  // std::string index_id(base::NumberToString(input_index));
  // // The vaule input with setOperandValue Javascript API.
  // if (model->values.find(index_id) != model->values.end()) {
  // }
  CD3DX12_HEAP_PROPERTIES upload_heap =
      CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
  HRESULT hr = dml->d3D12_device_->CreateCommittedResource(
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
  hr = dml->d3D12_device_->CreateCommittedResource(
      &default_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&input_resource));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed creating committed resource for coping";
    return hr;
  }

  D3D12_SUBRESOURCE_DATA subresource_data = {};
  subresource_data.pData = data;
  subresource_data.RowPitch = size;
  subresource_data.SlicePitch = subresource_data.RowPitch;

  // Upload the input tensor to the GPU.
  UpdateSubresources(dml->command_list_.Get(), input_resource.Get(),
                     upload_resource.Get(), 0, 0, 1, &subresource_data);

  CD3DX12_RESOURCE_BARRIER resource_barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(
          input_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
          D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  dml->command_list_->ResourceBarrier(1, &resource_barrier);

  return S_OK;
}

}  // namespace ml
