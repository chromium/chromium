// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef SERVICES_ML_ML_UTILS_DML_H_
#define SERVICES_ML_ML_UTILS_DML_H_

#include <wrl/client.h>
#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "d3d12.h"
// TODO: Window sdk should be upgraded to 10.0.18361.0 in VS
// seeing https://chromium-review.googlesource.com/c/chromium/src/+/1054027
#include "services/ml/direct_ml.h"

using Microsoft::WRL::ComPtr;

namespace ml {

struct OperationDML {
 public:
  OperationDML(ComPtr<IDMLCompiledOperator> compiled_operator,
               uint32_t descriptor_count,
               int32_t inputs_size,
               std::vector<uint32_t> inputs,
               std::vector<uint32_t> outputs,
               ComPtr<ID3D12Resource> persistent_buffer,
               uint64_t persistent_size);
  ~OperationDML();

  ComPtr<IDMLCompiledOperator> compiled_operator_;
  uint32_t descriptor_count_;
  ComPtr<ID3D12Resource> persistent_buffer_;
  uint64_t persistent_size_;
  std::vector<uint32_t> persistent_index_;

  int32_t bind_inputs_size;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OperationDML);
};

struct OperandDML {
 public:
  OperandDML(const std::vector<uint32_t>&, bool depth_conv_weight = false);
  ~OperandDML();

  size_t SizeInBytes() { return operand_desc_.TotalTensorSizeInBytes; }

  std::vector<uint32_t> dimensions_;
  UINT strides_[4];
  DML_BUFFER_TENSOR_DESC operand_desc_;

  ComPtr<ID3D12Resource> operand_resource_;
  ComPtr<ID3D12Resource> upload_resource_;
  ComPtr<ID3D12Resource> readback_resource_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OperandDML);
};

class CompiledModelDML : public base::RefCounted<CompiledModelDML> {
 public:
  CompiledModelDML();

  ComPtr<ID3D12Device> d3D12_device_;
  ComPtr<IDMLDevice> dml_device_;
  ComPtr<IDMLCommandRecorder> command_recorder_;
  ComPtr<ID3D12GraphicsCommandList> command_list_;
  ComPtr<ID3D12CommandQueue> command_queue_;
  ComPtr<ID3D12CommandAllocator> command_allocator_;

  ComPtr<ID3D12DescriptorHeap> descriptor_heap_;
  DML_BINDING_TABLE_DESC binding_table_desc_;
  ComPtr<IDMLBindingTable> binding_table_;

  std::vector<std::unique_ptr<OperationDML>> operations_;
  std::map<uint32_t, std::unique_ptr<OperandDML>> operand_map_;

  ComPtr<ID3D12Resource> temporary_buffer_;
  uint64_t temporary_resource_size_;

 private:
  friend class base::RefCounted<CompiledModelDML>;
  ~CompiledModelDML();

  DISALLOW_COPY_AND_ASSIGN(CompiledModelDML);
};

UINT64 DMLCalcBufferTensorSize(DML_TENSOR_DATA_TYPE dataType,
                               UINT dimensionCount,
                               _In_reads_(dimensionCount) const UINT* sizes,
                               const UINT* strides);

HRESULT CloseExecuteResetWait(ComPtr<ID3D12Device> d3D12_device,
                              ComPtr<ID3D12CommandQueue> command_queue,
                              ComPtr<ID3D12CommandAllocator> command_allocator,
                              ComPtr<ID3D12GraphicsCommandList> command_list);

HRESULT CreateCommonResource(uint64_t size,
                             ComPtr<ID3D12Resource>& commom_resource,
                             ComPtr<ID3D12Device> d3D12_device);

HRESULT CreateOutputResource(uint64_t size,
                             ComPtr<ID3D12Resource>& intermediate_resource,
                             ComPtr<ID3D12Device> d3D12_device);

HRESULT CreateReadbackResource(uint64_t size,
                               ComPtr<ID3D12Resource>& readback_resource,
                               ComPtr<ID3D12Resource>& operand_resource,
                               ComPtr<ID3D12Device> d3D12_device);

HRESULT CreateUploadResource(uint64_t size,
                             ComPtr<ID3D12Resource>& upload_resource,
                             ComPtr<ID3D12Resource>& input_resource,
                             ComPtr<ID3D12Device> d3D12_device);

HRESULT UploadTensorResource(const void* data,
                             uint64_t size,
                             ComPtr<ID3D12Resource>& upload_resource,
                             ComPtr<ID3D12Resource>& input_resource,
                             ComPtr<ID3D12GraphicsCommandList> command_list);
}  // namespace ml

#endif  // SERVICES_ML_ML_UTILS_DML_H_