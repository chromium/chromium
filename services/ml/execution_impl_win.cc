// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_win.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_impl_win.h"

namespace ml {

ExecutionImplWin::ExecutionImplWin(const CompilationImplWin* compilation,
                                   mojo::ScopedSharedBufferHandle memory)
    : network_(nullptr) {
  operands_ = compilation->operands_;
  operations_ = compilation->operations_;
  inputs_ = compilation->inputs_;
  outputs_ = compilation->outputs_;
  memory_ = std::move(memory);
  uint32_t total_length = 0;
  inputs_info_.reserve(inputs_.size());
  for (size_t i = 0; i < inputs_.size(); ++i) {
    const uint32_t offset = total_length;
    const uint32_t length = operands_[inputs_[i]].requiredSize();
    inputs_info_.push_back(std::make_unique<OperandInfo>(
        offset, length, memory_->MapAtOffset(length, offset)));
    total_length += length;
  }
  outputs_info_.reserve(outputs_.size());
  for (size_t i = 0; i < outputs_.size(); ++i) {
    const uint32_t offset = total_length;
    const uint32_t length = operands_[outputs_[i]].requiredSize();
    outputs_info_.push_back(std::make_unique<OperandInfo>(
        offset, length, memory_->MapAtOffset(length, offset)));
    total_length += length;
  }

  cldnn_status status;
  input_memories_.resize(inputs_.size());
  for (size_t i = 0; i < inputs_.size(); ++i) {
    const Operand& operand = operands_[inputs_[i]];
    std::unique_ptr<OperandInfo>& info = inputs_info_[i];
    if (operand.type != mojom::TENSOR_FLOAT32) {
      DLOG(ERROR) << "Only TENSOR_FLOAT32 operand type is supported";
      return;
    }
    cldnn_layout layout = {
        .data_type = cldnn_f32, .format = cldnn_format_byxf, .padding = {}};
    if (operand.dimensions.size() == 1) {
      layout.size = {1, 1, 2, {1, 1, operand.dimensions[0], 1, 1, 1, 1, 1}};
    } else if (operand.dimensions.size() == 2) {
      // HW -> {batch, feature, width, height}
      layout.size = {
          1,
          1,
          2,
          {1, 1, operand.dimensions[1], operand.dimensions[0], 1, 1, 1, 1}};
    } else if (operand.dimensions.size() == 3) {
      // HWC -> {batch, feature, width, height}
      layout.size = {1,
                     1,
                     2,
                     {1, operand.dimensions[2], operand.dimensions[1],
                      operand.dimensions[0], 1, 1, 1, 1}};
    } else if (operand.dimensions.size() == 4) {
      // NHWC -> {batch, feature, width, height}
      layout.size = {
          1,
          1,
          2,
          {operand.dimensions[0], operand.dimensions[3], operand.dimensions[2],
           operand.dimensions[1], 1, 1, 1, 1}};
    } else {
      DLOG(ERROR) << "Operand dimensions size " << operand.dimensions.size()
                  << " is not supported.";
      return;
    }

    cldnn_memory memory = cldnn_attach_memory(
        layout, static_cast<void*>(info->mapping.get()), info->length, &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to attach memory " << status << " "
                  << std::string(cldnn_get_last_error_message());
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to attach memory for input " << i;
    input_memories_[i] = memory;
  }

  network_ = cldnn_allocate_network(compilation->program_, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to allocate network " << status << " "
                << std::string(cldnn_get_last_error_message());
    network_ = nullptr;
    return;
  }

  DLOG(INFO) << "[clDNN] succeed to allocate network";
}

ExecutionImplWin::~ExecutionImplWin() {
  cldnn_status status;
  if (network_) {
    cldnn_release_network(network_, &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to release network " << status << " "
                  << std::string(cldnn_get_last_error_message());
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to release network";
  }
  for (size_t i = 0; i < input_memories_.size(); ++i) {
    cldnn_release_memory(input_memories_[i], &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to release memory " << status << " "
                  << std::string(cldnn_get_last_error_message());
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to release memory for input " << i;
  }
}

void ExecutionImplWin::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplWin::StartCompute";

  if (network_ == nullptr) {
    DLOG(ERROR) << "Execution is not initialized successfully";
    std::move(callback).Run(mojom::BAD_STATE);
    return;
  }

  for (size_t i = 0; i < inputs_.size(); ++i) {
    DLOG(INFO) << "inputs[" << i << "]:";
    PrintOperand(operands_[inputs_[i]], inputs_info_[i]);
  }

  cldnn_status status;
  if (!network_) {
    std::move(callback).Run(mojom::BAD_STATE);
    return;
  }

  for (size_t i = 0; i < inputs_.size(); ++i) {
    std::string input_id_str = base::NumberToString(inputs_[i]);
    cldnn_set_network_input(network_, input_id_str.c_str(), input_memories_[i],
                            &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to set network input " << i << " "
                  << status << " "
                  << std::string(cldnn_get_last_error_message());
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to set network input " << i;
  }

  cldnn_execute_network(network_, nullptr, 0, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to execute network " << status << " "
                << std::string(cldnn_get_last_error_message());
    std::move(callback).Run(mojom::OP_FAILED);
    return;
  }
  DLOG(INFO) << "[clDNN] succeed to execute network";

  for (size_t i = 0; i < outputs_.size(); ++i) {
    // Use the reordered outputs (byxf).
    std::string output_id_str =
        base::NumberToString(outputs_[i]) + std::string("-reordered");
    cldnn_memory memory = cldnn_get_network_output_memory(
        network_, output_id_str.c_str(), &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to get network output " << i << " "
                  << status << " "
                  << std::string(cldnn_get_last_error_message());
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    void* output_ptr = cldnn_lock_memory(memory, &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
                  << std::string(cldnn_get_last_error_message());
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    std::unique_ptr<OperandInfo>& info = outputs_info_[i];
    memcpy(static_cast<void*>(info->mapping.get()), output_ptr, info->length);
    cldnn_unlock_memory(memory, &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
                  << std::string(cldnn_get_last_error_message());
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    DLOG(INFO) << "[clDNN] succeed to get network output " << i;
  }

  for (size_t i = 0; i < outputs_.size(); ++i) {
    DLOG(INFO) << "outputs[" << i << "]:";
    PrintOperand(operands_[outputs_[i]], outputs_info_[i]);
  }

  std::move(callback).Run(mojom::NOT_ERROR);
}

}  // namespace ml
