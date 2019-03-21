// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_cl_dnn.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/cl_dnn_symbol_table.h"
#include "services/ml/compilation_delegate_cl_dnn.h"

namespace ml {

ExecutionImplClDnn::ExecutionImplClDnn(
    const CompilationDelegateClDnn* compilation,
    mojom::ExecutionInitParamsPtr params)
    : network_(nullptr) {
  params_ = std::move(params);

  cldnn_status status;
  network_ = LATE(cldnn_allocate_network)(compilation->program_, &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to allocate network " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    network_ = nullptr;
    return;
  }

  DLOG(INFO) << "[clDNN] succeed to allocate network";
}

ExecutionImplClDnn::~ExecutionImplClDnn() {
  cldnn_status status;
  if (network_) {
    LATE(cldnn_release_network)(network_, &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to release network " << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to release network";
  }
  for (size_t i = 0; i < input_memories_.size(); ++i) {
    LATE(cldnn_release_memory)(input_memories_[i], &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to release memory " << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to release memory for input " << i;
  }
}

void ExecutionImplClDnn::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplClDnn::StartCompute";

  if (network_ == nullptr) {
    LOG(ERROR) << "Execution is not initialized successfully";
    std::move(callback).Run(mojom::BAD_STATE);
    return;
  }

  cldnn_status status;
  if (!network_) {
    std::move(callback).Run(mojom::BAD_STATE);
    return;
  }

  uint32_t total_length = 0;
  for (size_t i = 0; i < params_->inputs.size(); ++i) {
    const mojom::OperandInfoPtr& operand = params_->inputs[i];
    const uint32_t offset = total_length;
    const uint32_t length = GetRequiredSize(operand);
    total_length += length;
    if (operand->type != mojom::TENSOR_FLOAT32) {
      LOG(ERROR) << "Only TENSOR_FLOAT32 operand type is supported";
      std::move(callback).Run(mojom::BAD_DATA);
      return;
    }
    cldnn_layout layout;
    int32_t result = CompilationDelegateClDnn::CldnnGetLayout(
        operand->type, operand->dimensions, layout, cldnn_format_byxf);
    if (result != mojom::NOT_ERROR) {
      std::move(callback).Run(mojom::BAD_DATA);
      return;
    }
    auto mapping = params_->memory->MapAtOffset(length, offset);
    DLOG(INFO) << "Mapping " << mapping.get() << " for input " << i
               << " offset " << offset << " length " << length;
    cldnn_memory memory = LATE(cldnn_attach_memory)(
        layout, static_cast<void*>(mapping.get()), length, &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to attach memory " << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
      return;
    }
    std::string input_id_str = base::NumberToString(operand->index);
    LATE(cldnn_set_network_input)
    (network_, input_id_str.c_str(), memory, &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to set network input " << i << " " << status
                 << " " << std::string(LATE(cldnn_get_last_error_message)());
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to set network input " << i;
  }

  LATE(cldnn_execute_network)(network_, nullptr, 0, &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to execute network " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    std::move(callback).Run(mojom::OP_FAILED);
    return;
  }
  DLOG(INFO) << "[clDNN] succeed to execute network";

  for (size_t i = 0; i < params_->outputs.size(); ++i) {
    // Use the reordered outputs (byxf).
    const mojom::OperandInfoPtr& operand = params_->outputs[i];
    const uint32_t offset = total_length;
    const uint32_t length = GetRequiredSize(operand);
    total_length += length;
    std::string output_id_str =
        base::NumberToString(operand->index) + std::string("-reordered");
    cldnn_memory memory = LATE(cldnn_get_network_output_memory)(
        network_, output_id_str.c_str(), &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to get network output " << i << " "
                 << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    void* output_ptr = LATE(cldnn_lock_memory)(memory, &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    auto mapping = params_->memory->MapAtOffset(length, offset);
    memcpy(static_cast<void*>(mapping.get()), output_ptr, length);
    LATE(cldnn_unlock_memory)(memory, &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to get network output " << i;
  }

  std::move(callback).Run(mojom::NOT_ERROR);
}

}  // namespace ml
