// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef SERVICES_ML_ML_UTILS_DML_H_
#define SERVICES_ML_ML_UTILS_DML_H_

#include <DirectML.h>
#include <wrl/client.h>
#include <map>
#include <vector>

#include "base/macros.h"
#include "d3d12.h"

using Microsoft::WRL::ComPtr;

namespace ml {

struct ExecutionData {
  ExecutionData();
  ~ExecutionData();

  ComPtr<ID3D12Device> d3D12_device_;
  ComPtr<IDMLDevice> dml_device_;
  std::vector<ComPtr<IDMLCompiledOperator>> compiled_operators_;
  DML_BINDING_TABLE_DESC binding_table_desc_;
  ComPtr<IDMLBindingTable> binding_table_;
  ComPtr<ID3D12Resource> temporary_buffer_;
  UINT64 temporary_resource_size_;
  std::map<uint32_t, ComPtr<ID3D12Resource>> persistent_buffer_;
  std::map<uint32_t, uint64_t> persistent_resource_size_;

  ComPtr<IDMLCommandRecorder> command_recorder_;
  ComPtr<ID3D12GraphicsCommandList> command_list_;
  ComPtr<ID3D12CommandQueue> command_queue_;
  ComPtr<ID3D12CommandAllocator> command_allocator_;

  // Operation Data.
  std::vector<uint32_t> constants_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExecutionData);
};

UINT64 DMLCalcBufferTensorSize(DML_TENSOR_DATA_TYPE dataType,
                               UINT dimensionCount,
                               _In_reads_(dimensionCount) const UINT* sizes,
                               const UINT* strides);

HRESULT CloseExecuteResetWait(ComPtr<ID3D12Device> d3D12_device,
                              ComPtr<ID3D12CommandQueue> command_queue,
                              ComPtr<ID3D12CommandAllocator> command_allocator,
                              ComPtr<ID3D12GraphicsCommandList> command_list);

HRESULT UploadTensorResource(const void* data,
                             uint64_t size,
                             ComPtr<ID3D12Resource>& upload_resource,
                             ComPtr<ID3D12Resource>& input_resource,
                             std::unique_ptr<ExecutionData>& dml);
}  // namespace ml

#endif  // SERVICES_ML_ML_UTILS_DML_H_