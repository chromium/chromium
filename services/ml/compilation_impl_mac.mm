// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac.h"
#include "services/ml/mpscnn_context.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

namespace ml {

MPSCNNNeuron* API_AVAILABLE(macosx(10.13)) CreateMPSCNNNeuron(int32_t fuse_code) {
  MPSCNNNeuron* relu = nullptr;
  if (fuse_code == mojom::FUSED_NONE) {
    relu = nullptr;
  } else if (fuse_code == mojom::FUSED_RELU) {
    relu = [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device a:0];
  } else if (fuse_code == mojom::FUSED_RELU1) {
    relu = [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device a:1];
  } else if (fuse_code == mojom::FUSED_RELU6) {
    relu = [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device a:6];
  }
  return relu;
}

MPSCNNConvolution* API_AVAILABLE(macosx(10.13)) CreateMPSCNNConvolution(
    int32_t filter_width, int32_t filter_height, int32_t depth_in, int32_t depth_out,
    int32_t stride_width, int32_t stride_height,
    const float* weights, const float* bias,
    MPSCNNNeuron* relu,
    bool depthwise = false) {
  const MPSCNNConvolutionDescriptor* desc;
  if (depthwise) {
    desc = [MPSCNNDepthWiseConvolutionDescriptor
      cnnConvolutionDescriptorWithKernelWidth:filter_width
      kernelHeight:filter_height
      inputFeatureChannels:depth_in
      outputFeatureChannels:depth_out
      neuronFilter:relu];
  } else {
    desc = [MPSCNNConvolutionDescriptor
      cnnConvolutionDescriptorWithKernelWidth:filter_width
      kernelHeight:filter_height
      inputFeatureChannels:depth_in
      outputFeatureChannels:depth_out
      neuronFilter:relu];
  }
  desc.strideInPixelsX = stride_width;
  desc.strideInPixelsY = stride_height;
  desc.groups = 1;

  MPSCNNConvolution* conv = 
      [[MPSCNNConvolution alloc]
        initWithDevice:GetMPSCNNContext().device
        convolutionDescriptor:desc
        kernelWeights:weights
        biasTerms:bias
        flags:MPSCNNConvolutionFlagsNone];
  return conv;
}

void API_AVAILABLE(macosx(10.13)) ComputeMPSOffsetForImplictPadding(
    MPSOffset& offset,
    int32_t input_height, int32_t input_width, int32_t output_height, int32_t output_width,
    int32_t filter_height, int32_t filter_width, int32_t stride_height, int32_t stride_width) {
  int pad_along_height = ((output_height - 1) * stride_height + filter_height - input_height);
  int pad_along_width  = ((output_width - 1) * stride_width + filter_width - input_width);
  int pad_top = (int)(pad_along_height / 2);
  int pad_left = (int)(pad_along_width / 2);
  offset.x = (int)(filter_width / 2) - pad_left;
  offset.y = (int)(filter_height / 2) - pad_top;
  offset.z = 0;
}

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

  if (@available(macOS 10.11, *)) {
    id<MTLDevice> device = GetMPSCNNContext().device;
    if (device == nil) {
      DLOG(ERROR) << "Cannot create MTLDevice";
      std::move(callback).Run(mojom::BAD_STATE);
      return;
    } else {
      DLOG(INFO) << "Created MTLDevice: " << device.name.UTF8String;
    }
  }

  DLOG(INFO) << "operations(" << operations_.size() << ")";
  bool success = true;
  for (size_t i = 0; i < operations_.size(); ++i ) {
    Operation operation = operations_[i];
    uint32_t type = operation.type;
    DLOG(INFO) << "  operation[" << i << "]";
    DLOG(INFO) << "    type: " << type;
    if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D) {
      success = CompileConv2DOrDepthwiseConv2D(operation);
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

bool CompilationImplMac::CompileConv2DOrDepthwiseConv2D(const Operation& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileConv2DOrDepthwiseConv2D";
  DLOG_IF(FATAL, operation.type != mojom::CONV_2D && operation.type != mojom::DEPTHWISE_CONV_2D);
  int32_t input_width, input_height, output_width, output_height;
  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom;
  int32_t stride_width, stride_height;
  int32_t padding_code, fuse_code;
  int32_t depth_out, filter_height, filter_width, depth_in;
  bool depthwise = (operation.type == mojom::DEPTHWISE_CONV_2D);
  int32_t depthwise_multiplier;

  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;
  Operand output = operands_[outputs[0]];
  output_height = output.dimensions[1];
  output_width = output.dimensions[2];
  int32_t i = 0;
  Operand input = operands_[inputs[i++]];
  input_height = input.dimensions[1];
  input_width = input.dimensions[2];

  DLOG(INFO) << "  input_height: " << input_height << " input_width: " << input_width;
  DLOG(INFO) << "  output_height: " << output_height << " output_width: " << output_width;

  Operand filter = operands_[inputs[i++]];
  if (depthwise) {
    depth_out = filter.dimensions[3];
    depth_in = depth_out;  
  } else {
    depth_out = filter.dimensions[0];
    depth_in = filter.dimensions[3];
  }
  filter_height = filter.dimensions[1];
  filter_width = filter.dimensions[2];

  DLOG(INFO) << "  depth_out: " << depth_out;
  DLOG(INFO) << "  filter_height: " << filter_height;
  DLOG(INFO) << "  filter_width: " << filter_width;
  DLOG(INFO) << "  depth_in: " << depth_in;

  Operand bias = operands_[inputs[i++]];
  if ((!depthwise && inputs.size() == 10) ||
      (depthwise && inputs.size() == 11)) {
    implicit_padding = false;
    padding_left = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_right = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_top = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_bottom = getScalarInt32(values_[inputs[i++]], memory_.get());
  } else if ((!depthwise && inputs.size() == 7) ||
             (depthwise && inputs.size() == 8)) {
    implicit_padding = true;
    padding_code = getScalarInt32(values_[inputs[i++]], memory_.get());
  } else {
    DLOG(ERROR) << "  inputs size is incorrect";
    return false;
  }
  stride_width = getScalarInt32(values_[inputs[i++]], memory_.get());
  stride_height = getScalarInt32(values_[inputs[i++]], memory_.get());
  if (depthwise == true) {
    depthwise_multiplier = getScalarInt32(values_[inputs[i++]], memory_.get());
    if (depthwise_multiplier != 1) {
      DLOG(ERROR) << "  depthwise_multiplier " << depthwise_multiplier << " is not supported.";
      return false;
    }
  }
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
  if (depthwise) {
    DLOG(INFO) << "  depthwise_multiplier: " << depthwise_multiplier;
  }
  DLOG(INFO) << "  fuse_code: " << fuse_code;

  if (@available(macOS 10.13, *)) {
    MPSCNNNeuron* relu = CreateMPSCNNNeuron(fuse_code);
    
    ValueInfo weights_value_info = values_.at(inputs[1]);
    const float* weights = reinterpret_cast<const float*>(memory_.get() + weights_value_info.offset);
    ValueInfo bias_value_info = values_.at(inputs[2]);
    const float* bias = reinterpret_cast<const float*>(memory_.get() + bias_value_info.offset);

    MPSCNNConvolution* conv = CreateMPSCNNConvolution(
        filter_width, filter_height, depth_in, depth_out,
        stride_width, stride_height, weights, bias, relu, depthwise);
    
    if (implicit_padding) {
      if (padding_code == mojom::PADDING_SAME) {
        MPSOffset offset;
        ComputeMPSOffsetForImplictPadding(
            offset, input_height, input_width, output_height, output_width,
            filter_height, filter_width, stride_height, stride_width);
        DLOG(INFO) << "  MPSOffset x: " << offset.x << " y: " << offset.y;
        [conv setOffset:offset];
      }
    } else {
      if (padding_left != padding_right || padding_top != padding_bottom) {
        DLOG(ERROR) << "padding_left != padding_right || padding_top != padding_bottom";
        return false;
      } else {
        MPSOffset offset;
        offset.x = padding_left;
        offset.y = padding_top;
        offset.z = 0;
        DLOG(INFO) << "  MPSOffset x: " << offset.x << " y: " << offset.y;
        [conv setOffset:offset];
      }
    }

    [conv setEdgeMode:MPSImageEdgeModeZero];
    
    DLOG(INFO) << "  Create MPSCNNConvolution: " << conv;

    base::scoped_nsobject<MPSCNNKernel> kernel;
    kernel.reset(conv);
    mpscnn_kernels_.push_back(kernel);
  }

  return true;
}

bool CompilationImplMac::CompileAveragePool2D(const Operation& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileAveragePool2D";
  DLOG_IF(FATAL, operation.type != mojom::AVERAGE_POOL_2D);
  int32_t input_width, input_height, output_width, output_height;
  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom;
  int32_t stride_width, stride_height;
  int32_t padding_code, fuse_code;
  int32_t filter_height, filter_width;

  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;
  Operand output = operands_[outputs[0]];
  output_height = output.dimensions[1];
  output_width = output.dimensions[2];
  int32_t i = 0;
  Operand input = operands_[inputs[i++]];
  input_height = input.dimensions[1];
  input_width = input.dimensions[2];

  DLOG(INFO) << "  input_height: " << input_height << " input_width: " << input_width;
  DLOG(INFO) << "  output_height: " << output_height << " output_width: " << output_width;

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
  filter_width = getScalarInt32(values_[inputs[i++]], memory_.get());
  filter_height = getScalarInt32(values_[inputs[i++]], memory_.get());
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
  DLOG(INFO) << "  filter_height: " << filter_height;
  DLOG(INFO) << "  filter_width: " << filter_width;
  DLOG(INFO) << "  fuse_code: " << fuse_code;

  if (fuse_code != mojom::FUSED_NONE) {
    DLOG(ERROR) << "  fuse_code " << fuse_code << " is not supproted.";
    return false;
  }

  if (@available(macOS 10.13, *)) {
    MPSCNNPoolingAverage* pool =
        [[MPSCNNPoolingAverage alloc]
            initWithDevice:GetMPSCNNContext().device
            kernelWidth:filter_width
            kernelHeight:filter_height
            strideInPixelsX:stride_width
            strideInPixelsY:stride_height];
    if (implicit_padding) {
      if (padding_code == mojom::PADDING_SAME) {
        MPSOffset offset;
        ComputeMPSOffsetForImplictPadding(
            offset, input_height, input_width, output_height, output_width,
            filter_height, filter_width, stride_height, stride_width);
        DLOG(INFO) << "  MPSOffset x: " << offset.x << " y: " << offset.y;
        [pool setOffset:offset];
      }
    } else {
      if (padding_left != padding_right || padding_top != padding_bottom) {
        DLOG(ERROR) << "padding_left != padding_right || padding_top != padding_bottom";
        return false;
      } else {
        MPSOffset offset;
        offset.x = padding_left;
        offset.y = padding_top;
        offset.z = 0;
        DLOG(INFO) << "  MPSOffset x: " << offset.x << " y: " << offset.y;
        [pool setOffset:offset];
      }
    }

    [pool setEdgeMode:MPSImageEdgeModeClamp];

    DLOG(INFO) << "  Create MPSCNNPoolingAverage: " << pool;

    base::scoped_nsobject<MPSCNNKernel> kernel;
    kernel.reset(pool);
    mpscnn_kernels_.push_back(kernel);
  }
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
