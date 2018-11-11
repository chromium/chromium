// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac.h"

#include "services/ml/compilation_impl_mac_bnns.h"
#include "services/ml/compilation_impl_mac_mps.h"
#include "services/ml/execution_impl_mac.h"
#include "services/ml/mpscnn_context.h"

namespace ml {

CompilationImplMac::CompilationImplMac(ModelImplMac* model)
    : compilation_factory_(this) {
  operands_.reserve(model->operands_.size());
  for (uint32_t i = 0; i < model->operands_.size(); ++i) {
    OperandMac operand(model->operands_[i]);
    operands_.push_back(operand);
  }
  operations_.reserve(model->operations_.size());
  for (uint32_t i = 0; i < model->operations_.size(); ++i) {
    OperationMac operation(model->operations_[i]);
    operation.filter = nullptr;
    operations_.push_back(operation);
  }
  values_ = model->values_;
  inputs_ = model->inputs_;
  outputs_ = model->outputs_;
  memory_size_ = model->memory_size_;
  memory_.reset(new int8_t[memory_size_]);
  memcpy(memory_.get(), model->memory_.get(), memory_size_);
  is_bnns_ = true;
}

CompilationImplMac::~CompilationImplMac() {
  if (is_bnns_) {
    for (size_t i = 0; i < this->operations_.size(); i++) {
      const OperationMac& operation = this->operations_[i];
      if (operation.local_operation == KBNNSFilter &&
          operation.filter != nullptr) {
        if (@available(macOS 10.12, *)) {
          BNNSFilterDestroy(operation.filter);
        }
      }
    }
  }
}

void CompilationImplMac::Finish(int32_t preference, FinishCallback callback) {
  DLOG(INFO) << "CompilationImplMac::Finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;
  is_bnns_ = (preference == mojom::PREFER_FAST_SINGLE_ANSWER) ? true : false;
  if (@available(macOS 10.13, *)) {
    if (is_bnns_ == false) {
      if (!GetMPSCNNContext().IsValid()) {
        std::move(callback).Run(mojom::BAD_STATE);
        return;
      }
    }
  }

  DLOG(INFO) << "Compile operations(" << operations_.size() << ")";
  bool success = true;
  for (size_t i = 0; i < operations_.size(); ++i) {
    OperationMac& operation = operations_[i];
    uint32_t type = operation.type;
    std::vector<uint32_t>& inputs = operation.inputs;
    std::vector<uint32_t>& outputs = operation.outputs;
    DLOG(INFO) << "    inputs(" << inputs.size()
               << "): " << VectorToString(inputs.data(), inputs.size());
    DLOG(INFO) << "    outputs(" << outputs.size()
               << "): " << VectorToString(outputs.data(), outputs.size());
    // Adjust the read count
    for (size_t j = 0; j < inputs.size(); ++j) {
      OperandMac& operand = operands_[inputs[j]];
      operand.read_count += 1;
    }

    if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D) {
      if (is_bnns_) {
        if (type == mojom::CONV_2D) {
          success = CompileConv2DBNNS(operation, values_, memory_, operands_);
        } else {
          DLOG(ERROR) << "Operation is not supported";
          success = false;
        }
      } else {
        success = CompileConv2DOrDepthwiseConv2D(operation, values_, memory_,
                                                 operands_);
      }
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      if (is_bnns_) {
        success = CompileAverageOrMaxPool2DBNNS(operation, values_, memory_,
                                                operands_);
      } else {
        success =
            CompileAverageOrMaxPool2D(operation, values_, memory_, operands_);
      }
    } else if (type == mojom::SOFTMAX) {
      if (is_bnns_) {
        success = CompileSoftmaxBNNS(operation, values_, memory_, operands_);
      } else {
        success = CompileSoftmax(operation, values_, memory_);
      }
    } else if (type == mojom::RESHAPE) {
      if (is_bnns_) {
        success = CompileReshapeBNNS(operation);
      } else {
        success = CompileReshape(operation, operations_);
      }
    } else if (type == mojom::CONCATENATION) {
      if (is_bnns_) {
        success = CompileConcatenationBNNS(operation, values_, memory_,
                                           i == 0 ? true : false);
      } else {
        success = CompileConcatenation(operation, values_, memory_, operands_,
                                       operations_);
      }
    } else if (type == mojom::ADD || type == mojom::MUL) {
      if (is_bnns_) {
        DLOG(ERROR) << "Operation is not supported";
        success = false;
      } else {
        success = CompileArithmetic(operation, values_, constants_);
      }
    } else {
      DLOG(ERROR) << "Operation is not supported";
      success = false;
    }

    if (!success) {
      break;
    }
  }

  if (success) {
    std::move(callback).Run(mojom::NOT_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

void CompilationImplMac::CreateExecution(CreateExecutionCallback callback) {
  DLOG(INFO) << "CompilationImplMac::CreateExecution";
  auto init_params = mojom::ExecutionInitParams::New();

  uint32_t input_memory_size = 0;
  init_params->inputs.reserve(inputs_.size());
  for (size_t i = 0; i < inputs_.size(); ++i) {
    OperandMac& operand = operands_[inputs_[i]];
    input_memory_size += operand.requiredSize();
    init_params->inputs.push_back(
        mojom::OperandInfo::New(operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required input memory size: " << input_memory_size;

  uint32_t output_memory_size = 0;
  init_params->outputs.reserve(outputs_.size());
  for (size_t i = 0; i < outputs_.size(); ++i) {
    OperandMac& operand = operands_[outputs_[i]];
    output_memory_size += operand.requiredSize();
    init_params->outputs.push_back(
        mojom::OperandInfo::New(operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required output memory size: " << output_memory_size;

  mojo::ScopedSharedBufferHandle memory_handle =
      mojo::SharedBufferHandle::Create(input_memory_size + output_memory_size);

  init_params->memory =
      memory_handle->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE);

  auto impl = std::make_unique<ExecutionImplMac>(
      compilation_factory_.GetWeakPtr(), std::move(memory_handle));
  if (!impl->IsValid()) {
    std::move(callback).Run(mojom::BAD_DATA, nullptr);
    return;
  }
  mojom::ExecutionPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl), mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml

