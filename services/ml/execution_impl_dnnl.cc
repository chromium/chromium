// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_dnnl.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_delegate_dnnl.h"
#include "services/ml/dnnl_symbol_table.h"

namespace ml {

ExecutionImplDnnl::ExecutionImplDnnl(
    std::shared_ptr<CompiledModelDnnl> compiled_model,
    mojom::ExecutionInitParamsPtr params)
    : params_(std::move(params)), compiled_model_(std::move(compiled_model)) {}

ExecutionImplDnnl::~ExecutionImplDnnl() {}

void ExecutionImplDnnl::StartCompute(StartComputeCallback callback) {
  dnnl_status_t status;
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
    dnnl_memory_t memory = compiled_model_->memories[input_id];
    void* buffer;
    status = LATE(dnnl_memory_get_data_handle)(memory, &buffer);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to get memory handle " << status;
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    memcpy(buffer, mapping.get(), length);
  }

  int32_t result;
  std::vector<dnnl_primitive_t> net;
  std::vector<args_t> net_args;
  for (size_t i = 0; i < compiled_model_->operations.size(); ++i) {
    const OperationDnnl& operation = compiled_model_->operations[i];
    if (operation.primitive) {
      net.push_back(operation.primitive);
      net_args.push_back(operation.primitive_args);
    } else {
      // Execute previous net first
      if (net.size() > 0) {
        result = DnnlExecuteNet(net, net_args);
        if (result != mojom::NOT_ERROR) {
          std::move(callback).Run(mojom::BAD_DATA);
          return;
        }
      }

      // Execute the custom operation
      result = DnnlExecuteCustomOperation(operation);
      if (result != mojom::NOT_ERROR) {
        std::move(callback).Run(mojom::BAD_DATA);
        return;
      }
    }
  }

  if (net.size() > 0) {
    result = DnnlExecuteNet(net, net_args);
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
    dnnl_memory_t memory = compiled_model_->memories[output_id];
    void* buffer;
    status = LATE(dnnl_memory_get_data_handle)(memory, &buffer);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to get memory handle " << status;
      std::move(callback).Run(mojom::OP_FAILED);
      return;
    }
    memcpy(mapping.get(), buffer, length);
    DLOG(INFO) << "Copy data from output memory buffer for " << output_id;
  }

  DLOG(INFO) << "ExecutionImplDnnl::StartCompute succeeds";
  std::move(callback).Run(mojom::NOT_ERROR);
}

int32_t ExecutionImplDnnl::DnnlExecuteNet(std::vector<dnnl_primitive_t>& net,
                                          std::vector<args_t>& net_args) {
  dnnl_stream_t stream;
  dnnl_status_t status = LATE(dnnl_stream_create)(
      &stream, compiled_model_->engine, dnnl_stream_default_flags);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create stream " << status;
    return mojom::OP_FAILED;
  }

  for (uint32_t i = 0; i < net.size(); ++i) {
    dnnl_primitive_t operation = net[i];
    status = LATE(dnnl_primitive_execute)(operation, stream, net_args[i].size(),
                                          net_args[i].data());
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to execute stream " << status;
      LATE(dnnl_stream_destroy)(stream);
      return mojom::OP_FAILED;
    }
  }

  status = LATE(dnnl_stream_wait)(stream);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to wait stream " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[DNNL] succeed to execute " << net.size() << " primitives";
  LATE(dnnl_stream_destroy)(stream);
  net.clear();
  net_args.clear();
  return mojom::NOT_ERROR;
}

int32_t ExecutionImplDnnl::DnnlExecuteCustomOperation(
    const OperationDnnl& operation) {
  int32_t result;
  if (operation.type == mojom::RESHAPE) {
    result = DnnlExecuteReshape(operation);
  } else {
    LOG(ERROR) << "Operation type " << operation.type << " is not supported";
    result = mojom::BAD_DATA;
  }
  return result;
}

int32_t ExecutionImplDnnl::DnnlExecuteReshape(const OperationDnnl& operation) {
  std::string input_id = operation.inputs[0];
  if (compiled_model_->memories.find(input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  dnnl_memory_t input_memory = compiled_model_->memories[input_id];
  void* input_buffer = nullptr;
  dnnl_status_t status =
      LATE(dnnl_memory_get_data_handle)(input_memory, &input_buffer);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get memory data handle " << status;
    return mojom::OP_FAILED;
  }
  std::string output_id = operation.outputs[0];
  if (compiled_model_->memories.find(output_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Ouput memory is not ready";
    return mojom::BAD_DATA;
  }
  dnnl_memory_t output_memory = compiled_model_->memories[output_id];
  void* output_buffer = nullptr;
  status = LATE(dnnl_memory_get_data_handle)(output_memory, &output_buffer);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get memory data handle " << status;
    return mojom::OP_FAILED;
  }
  const dnnl_memory_desc_t* output_desc;
  status = LATE(dnnl_memory_get_memory_desc)(output_memory, &output_desc);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  size_t buffer_size = LATE(dnnl_memory_desc_get_size)(output_desc);
  memcpy(output_buffer, input_buffer, buffer_size);
  DLOG(INFO) << "Copy memory from buffer " << input_id << " to buffer "
             << output_id << " with size " << buffer_size;
  return mojom::NOT_ERROR;
}

}  // namespace ml
