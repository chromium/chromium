// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac.h"
#include "services/ml/mpscnn_context.h"

namespace ml {

CompilationImplMac::CompilationImplMac(ModelImplMac* model) {
  operands_ = model->operands_;
  operations_ = model->operations_;
  values_ = model->values_;
  inputs_ = model->inputs_;
  outputs_ = model->outputs_;
  memory_size_ = model->memory_size_;
  memory_.reset(new int8_t[memory_size_]);
  memcpy(memory_.get(), model->memory_.get(), memory_size_);
}

CompilationImplMac::~CompilationImplMac() {}

void CompilationImplMac::finish(int32_t preference, finishCallback callback) {
  DLOG(INFO) << "CompilationImplMac::finish";
  DLOG(INFO) << "  " << "preference: " << preference;

  DLOG(INFO) << "operations(" << operations_.size() << ")";
  bool success = true;
  for (size_t i = 0; i < operations_.size(); ++i ) {
    Operation operation = operations_[i];
    uint32_t type = operation.type;
    DLOG(INFO) << "  operation[" << i << "]";
    DLOG(INFO) << "    type: " << type;
    if (type == mojom::CONV_2D) {
      success = CompileConv2D(operation);
    } else if (type == mojom::DEPTHWISE_CONV_2D) {
      success = CompileDepthwiseConv2D(operation);
    } else if (type == mojom::AVERAGE_POOL_2D) {
      success = CompileAveragePool2D(operation);
    } else if (type == mojom::SOFTMAX) {
      success = CompileSoftmax(operation);
    } else if (type == mojom::RESHAPE) {
      success = CompileReshape(operation);
    } else {
      DLOG(ERROR) << "Operation is not supported";
      success = false;
    }

    if (!success) {
      break;
    }
  }

  if (success) {
    std::move(callback).Run(mojom::NO_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

bool CompilationImplMac::CompileConv2D(const Operation& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileConv2D";
  DLOG_IF(FATAL, operation.type != mojom::CONV_2D);
  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom;
  int32_t stride_width, stride_height;
  int32_t padding_code, fuse_code;
  int32_t depth_out, filter_height, filter_width, depth_in;
  std::vector<uint32_t> inputs = operation.inputs;
  int32_t i = 1;
  Operand filter = operands_[inputs[i++]];
  depth_out = filter.dimensions[0];
  filter_height = filter.dimensions[1];
  filter_width = filter.dimensions[2];
  depth_in = filter.dimensions[3];

  DLOG(INFO) << "  depth_out: " << depth_out;
  DLOG(INFO) << "  filter_height: " << filter_height;
  DLOG(INFO) << "  filter_width: " << filter_width;
  DLOG(INFO) << "  depth_in: " << depth_in;

  Operand bias = operands_[inputs[i++]];
  if (inputs.size() == 10) {
    implicit_padding = false;
    padding_left = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_right = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_top = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_bottom = getScalarInt32(values_[inputs[i++]], memory_.get());
  } else if (inputs.size() == 7) {
    implicit_padding = true;
    padding_code = getScalarInt32(values_[inputs[i++]], memory_.get());
  } else {
    DLOG(ERROR) << "  inputs size is incorrect";
    return false;
  }
  stride_width = getScalarInt32(values_[inputs[i++]], memory_.get());
  stride_height = getScalarInt32(values_[inputs[i++]], memory_.get());
  fuse_code = getScalarInt32(values_[inputs[i++]], memory_.get());

  DLOG(INFO) << "  implicit_padding: " << implicit_padding;
  if (implicit_padding) {
    DLOG(INFO) << "  padding_code: " << padding_code;
  } else {
    DLOG(INFO) << "  padding_left: " << padding_left;
    DLOG(INFO) << "  padding_right: " << padding_right;
    DLOG(INFO) << "  padding_top: " << padding_top;
    DLOG(INFO) << "  padding_bottom: " << padding_bottom;
  }
  DLOG(INFO) << "  stride_width: " << stride_width;
  DLOG(INFO) << "  stride_height: " << stride_height;
  DLOG(INFO) << "  fuse_code: " << fuse_code;

  if (@available(macOS 10.11, *)) {
    DLOG(INFO) << "  device: " << GetMPSCNNContext().device;
  }

  /*
  MPSCNNConvolutionDescriptor* desc = [MPSCNNConvolutionDescriptor
      cnnConvolutionDescriptorWithKernelWidth:kernel_width
      kernelHeight:kernel_height
      inputFeatureChannels:kernel_input
      outputFeatureChannels:kernel_output
      neuronFilter:Neuron
      ]
  */  
  return true;
}

bool CompilationImplMac::CompileDepthwiseConv2D(const Operation& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileDepthwiseConv2D";
  DLOG_IF(FATAL, operation.type != mojom::DEPTHWISE_CONV_2D);
  return true;
}

bool CompilationImplMac::CompileAveragePool2D(const Operation& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileAveragePool2D";
  DLOG_IF(FATAL, operation.type != mojom::AVERAGE_POOL_2D);
  return true;
}

bool CompilationImplMac::CompileSoftmax(const Operation& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileSoftmax";
  DLOG_IF(FATAL, operation.type != mojom::SOFTMAX);
  return true;
}

bool CompilationImplMac::CompileReshape(const Operation& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileReshape";
  DLOG_IF(FATAL, operation.type != mojom::RESHAPE);
  return true;
}

void CompilationImplMac::createExecution(createExecutionCallback callback) {
  DLOG(INFO) << "CompilationImplMac::createExecution";
  auto init_params = mojom::ExecutionInitParams::New();

  uint32_t input_memory_size = 0;
  for (size_t i = 0; i < inputs_.size(); ++i) {
    Operand operand = operands_[inputs_[i]];
    input_memory_size += operand.requiredSize();
    init_params->inputs.push_back(
        mojom::OperandInfo::New(operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required input memory size: " << input_memory_size;

  uint32_t output_memory_size = 0;
  for (size_t i = 0; i < outputs_.size(); ++i) {
    Operand operand = operands_[outputs_[i]];
    output_memory_size += operand.requiredSize();
    init_params->outputs.push_back(
        mojom::OperandInfo::New(operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required output memory size: " << output_memory_size;

  uint32_t total_memory_size = input_memory_size + output_memory_size;
  mojo::ScopedSharedBufferHandle memory_handle =
      mojo::SharedBufferHandle::Create(total_memory_size);
  
  init_params->memory = memory_handle->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_WRITE);

  auto impl = std::make_unique<ExecutionImplMac>(this, std::move(memory_handle));
  mojom::ExecutionPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl),
                          mojo::MakeRequest(&ptr_info));
  init_params->execution = std::move(ptr_info);
  
  std::move(callback).Run(mojom::NO_ERROR,
                          std::move(init_params));
}

}  // namespace ml
