// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_mkl_dnn.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_delegate_mkl_dnn.h"
#include "services/ml/mkl_dnn_symbol_table.h"

namespace ml {

ExecutionImplMklDnn::ExecutionImplMklDnn(
    std::shared_ptr<CompiledModelMklDnn> compiled_model,
    mojom::ExecutionInitParamsPtr params)
    : params_(std::move(params)),
      compiled_model_(std::move(compiled_model)) {}

ExecutionImplMklDnn::~ExecutionImplMklDnn() {}

void ExecutionImplMklDnn::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMklDnn::StartCompute";
  mkldnn_status_t status;
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
    auto mapping = params_->memory->MapAtOffset(length, offset);
    DLOG(INFO) << "Mapping " << mapping.get() << " for input " << i
               << " offset " << offset << " length " << length;
    std::string input_id = base::NumberToString(operand->index);
    mkldnn_primitive_t memory = compiled_model_->memories[input_id];
    void* buffer;
    status = LATE(mkldnn_memory_get_data_handle)(memory, &buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to get memory handle " << status;
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    memcpy(buffer, mapping.get(), length);
    DLOG(INFO) << "Copy data to input memory primitive buffer for " << input_id;
  }

  int32_t result;
  std::vector<mkldnn_primitive_t> net;
  for (size_t i = 0; i < compiled_model_->operations.size(); ++i) {
    const OperationMklDnn& operation = compiled_model_->operations[i];
    if (operation.primitive) {
      net.push_back(operation.primitive);
    } else {
      // Execute previous net first
      if (net.size() > 0) {
        result = MkldnnExecuteNet(net);
        if (result != mojom::NOT_ERROR) {
          std::move(callback).Run(mojom::BAD_DATA);
          return;
        }
      }

      // Execute the custom operation
      result = MkldnnExecuteCustomOperation(operation);
      if (result != mojom::NOT_ERROR) {
        std::move(callback).Run(mojom::BAD_DATA);
        return;
      }
    }
  }
  if (net.size() > 0) {
    result = MkldnnExecuteNet(net);
    if (result != mojom::NOT_ERROR) {
      std::move(callback).Run(mojom::BAD_DATA);
      return;
    }
  }

  for (size_t i = 0; i < params_->outputs.size(); ++i) {
    const mojom::OperandInfoPtr& operand = params_->outputs[i];
    const uint32_t offset = total_length;
    const uint32_t length = GetRequiredSize(operand);
    total_length += length;
    auto mapping = params_->memory->MapAtOffset(length, offset);
    DLOG(INFO) << "Mapping " << mapping.get() << " for output " << i
               << " offset " << offset << " length " << length;
    // Use the reordered outputs.
    std::string output_id = base::NumberToString(operand->index);
    mkldnn_primitive_t memory = compiled_model_->memories[output_id];
    void* buffer;
    status = LATE(mkldnn_memory_get_data_handle)(memory, &buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to get memory handle " << status;
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    memcpy(mapping.get(), buffer, length);
    DLOG(INFO) << "Copy data from output memory primitive buffer for " << output_id;
  }

  DLOG(INFO) << "ExecutionImplMklDnn::StartCompute succeeds";
  std::move(callback).Run(mojom::NOT_ERROR);
}

int32_t ExecutionImplMklDnn::MkldnnExecuteNet(std::vector<mkldnn_primitive_t>& net) {
  mkldnn_stream_t stream;
  mkldnn_status_t status = LATE(mkldnn_stream_create)(&stream, mkldnn_eager);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create stream " << status;
    return mojom::OP_FAILED;
  }
  status = LATE(mkldnn_stream_submit)(stream, net.size(), net.data(), NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to submit stream " << status;
    return mojom::OP_FAILED;
  }
  status = LATE(mkldnn_stream_wait)(stream, net.size(), NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to wait stream " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to execute " << net.size() << " primitives";
  LATE(mkldnn_stream_destroy)(stream);
  net.clear();
  return mojom::NOT_ERROR;
}

int32_t ExecutionImplMklDnn::MkldnnExecuteCustomOperation(const OperationMklDnn& operation) {
  LOG(ERROR) << "Operation type " << operation.type << " is not supported";
  return mojom::BAD_DATA;
}

}  // namespace ml
