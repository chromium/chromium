// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac.h"
#include "services/ml/mpscnn_context.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

API_AVAILABLE(macosx(10.13)) @interface ConvDataSource : NSObject<MPSCNNConvolutionDataSource> {}
@property(nonatomic, assign) float* weights_;
@property(nonatomic, assign) float* bias_;
@property(nonatomic, assign) MPSCNNConvolutionDescriptor* desc_;
@end

@implementation ConvDataSource
@synthesize weights_;
@synthesize bias_;
@synthesize desc_;
- (id)initWithWeight:(float*)weights
                bias:(float*)bias
                desc:(MPSCNNConvolutionDescriptor*)desc {
  self = [super init];
  self.weights_ = weights;
  self.bias_ = bias;
  self.desc_ = desc;
  return self;
}
- (float*)biasTerms {
  return self.bias_;
}
- (MPSDataType)dataType {
  return MPSDataTypeFloat32;
}
- (MPSCNNConvolutionDescriptor*)descriptor {
  return self.desc_;
}
- (NSString*)label {
  return nullptr;
}
- (BOOL)load {
  return true;
}
- (float*)lookupTableForUInt8Kernel {
  return nullptr;
}
- (void)purge {
  return;
}
- (vector_float2*)rangesForUInt8Kernel {
  return nullptr;
}
- (void*)weights {
  return self.weights_;
}
@end

namespace ml {

OperandMac::OperandMac() = default;
OperandMac::OperandMac(const OperandMac& operand) = default;
OperandMac::OperandMac(const Operand& operand) : Operand(operand), read_count(0) {}
OperandMac::~OperandMac() = default;

OperationMac::OperationMac() = default;
OperationMac::OperationMac(const OperationMac& operation) = default;
OperationMac::OperationMac(const Operation& operation) : Operation(operation) {}
OperationMac::~OperationMac() = default;

MPSCNNNeuron* API_AVAILABLE(macosx(10.13)) CreateMPSCNNNeuron(int32_t fuse_code) {
  MPSCNNNeuron* relu = nullptr;
  if (fuse_code == mojom::FUSED_NONE) {
    relu = nullptr;
  } else if (fuse_code == mojom::FUSED_RELU) {
    relu = [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device a:0];
  } else if (fuse_code == mojom::FUSED_RELU1) {
    DLOG(INFO) << "Emulate RELU1 with RELU";
    relu = [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device a:0];
  } else if (fuse_code == mojom::FUSED_RELU6) {
    DLOG(INFO) << "Emulate RELU6 with RELU";
    relu = [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device a:0];
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

  auto data_source = [[ConvDataSource alloc]
                       initWithWeight:(float*)weights
                       bias:(float*)bias
                       desc:(MPSCNNConvolutionDescriptor*)desc];
  MPSCNNConvolution* conv = 
      [[MPSCNNConvolution alloc]
        initWithDevice:GetMPSCNNContext().device
        weights:data_source];
  return conv;
}

void API_AVAILABLE(macosx(10.13)) ComputeMPSOffsetForImplictPadding(
    bool same_padding, MPSOffset& offset,
    int32_t input_height, int32_t input_width, int32_t output_height, int32_t output_width,
    int32_t filter_height, int32_t filter_width, int32_t stride_height, int32_t stride_width) {
  if (same_padding) {
    int pad_along_height = ((output_height - 1) * stride_height + filter_height - input_height);
    int pad_along_width  = ((output_width - 1) * stride_width + filter_width - input_width);
    int pad_top = (int)(pad_along_height / 2);
    int pad_left = (int)(pad_along_width / 2);

    offset.x = (int)(filter_width / 2) - pad_left;
    offset.y = (int)(filter_height / 2) - pad_top;
    offset.z = 0;
  } else {
    offset.x = (int)(filter_width / 2);
    offset.y = (int)(filter_height / 2);
    offset.z = 0;
  }
}

CompilationImplMac::CompilationImplMac(ModelImplMac* model) {
  for (uint32_t i = 0; i < model->operands_.size(); ++i) {
    OperandMac operand(model->operands_[i]);
    operands_.push_back(operand);
  }
  for (uint32_t i = 0; i < model->operations_.size(); ++i) {
    OperationMac operation(model->operations_[i]);
    operations_.push_back(operation);
  }
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

  if (@available(macOS 10.13, *)) {
    id<MTLDevice> device = GetMPSCNNContext().device;
    if (device == nil) {
      DLOG(ERROR) << "Cannot create MTLDevice";
      std::move(callback).Run(mojom::BAD_STATE);
      return;
    } else {
      DLOG(INFO) << "Created MTLDevice: " << device.name.UTF8String;
    }
  }

  DLOG(INFO) << "Compile operations(" << operations_.size() << ")";
  bool success = true;
  for (size_t i = 0; i < operations_.size(); ++i ) {
    OperationMac& operation = operations_[i];
    uint32_t type = operation.type;
    DLOG(INFO) << "  operation[" << i << "]";
    DLOG(INFO) << "    type: " << type;
    std::vector<uint32_t>& inputs = operation.inputs;
    std::vector<uint32_t>& outputs = operation.outputs;
    DLOG(INFO) << "    inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
    DLOG(INFO) << "    outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
    // Adjust the read count
    for (size_t j = 0; j < inputs.size(); ++j) {
      OperandMac& operand = operands_[inputs[j]];
      operand.read_count += 1;
    }
    if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D) {
      success = CompileConv2DOrDepthwiseConv2D(operation);
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      success = CompileAverageOrMaxPool2D(operation);
    } else if (type == mojom::SOFTMAX) {
      success = CompileSoftmax(operation);
    } else if (type == mojom::RESHAPE) {
      success = CompileReshape(operation);
    } else if (type == mojom::CONCATENATION) {
      success = CompileConcatenation(operation);
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

bool CompilationImplMac::CompileConv2DOrDepthwiseConv2D(OperationMac& operation) {
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
  uint32_t output_idx = outputs[0];
  OperandMac& output = operands_[output_idx];
  output_height = output.dimensions[1];
  output_width = output.dimensions[2];
  int32_t i = 0;
  int32_t input_idx = inputs[i++];
  OperandMac& input = operands_[input_idx];
  input_height = input.dimensions[1];
  input_width = input.dimensions[2];

  DLOG(INFO) << "  input_height: " << input_height << " input_width: " << input_width;
  DLOG(INFO) << "  output_height: " << output_height << " output_width: " << output_width;

  OperandMac& filter = operands_[inputs[i++]];
  if (depthwise) {
    depth_out = filter.dimensions[3];
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

  OperandMac& bias = operands_[inputs[i++]];
  DLOG(INFO) << "  bias length: " << bias.dimensions[0];

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
    depth_in = depth_out / depthwise_multiplier;
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
    //uint32_t size = weights_value_info.length / 4;
    //DLOG(INFO) << "  " << "weights(" << size << "): " << VectorToString(weights, size);
    ValueInfo bias_value_info = values_.at(inputs[2]);
    const float* bias = reinterpret_cast<const float*>(memory_.get() + bias_value_info.offset);
    //size = bias_value_info.length / 4;
    //DLOG(INFO) << "  " << "bias(" << size << "): " << VectorToString(bias, size);

    MPSCNNConvolution* conv;
    if (depthwise) {
      // Convert from WebML weights shape [1, filter_height, filter_width, depth_out]
      // to MPSCNNConvlution weight[ outputChannels ][ kernelHeight ][ kernelWidth ][ inputChannels / groups ]
      const uint32_t depthwise_weights_length = 1 * filter_height * filter_width * depth_out;
      std::vector<float> depthwise_weights(depthwise_weights_length);
      DLOG_IF(FATAL, depthwise_weights.size() * sizeof(float) != weights_value_info.length) <<
          "depthwise weigths length is incorrect";
      for (auto h = 0; h < filter_height; ++h) {
        for (auto w = 0; w < filter_width; ++w) {
          for (auto c = 0; c < depth_out; ++c) {
            depthwise_weights[c * filter_height * filter_width + h * filter_width + w] =
                weights[h * filter_width * depth_out  + w * depth_out + c];
          }
        }
      }
      //DLOG(INFO) << "  " << "depthwise_weights(" << depthwise_weights.size() << "): " << VectorToString(depthwise_weights.data(), depthwise_weights.size());
      conv = CreateMPSCNNConvolution(
          filter_width, filter_height, depth_in, depth_out,
          stride_width, stride_height, depthwise_weights.data(), bias, relu, depthwise);
    } else {
      conv = CreateMPSCNNConvolution(
        filter_width, filter_height, depth_in, depth_out,
        stride_width, stride_height, weights, bias, relu, depthwise);
    }
    
    MPSOffset offset;
    if (implicit_padding) {
      ComputeMPSOffsetForImplictPadding(
          padding_code == mojom::PADDING_SAME,
          offset, input_height, input_width, output_height, output_width,
          filter_height, filter_width, stride_height, stride_width);
    } else {
      offset.x = (int)(filter_width / 2) - padding_left;
      offset.y = (int)(filter_height / 2) - padding_top;
      offset.z = 0;
    }
    [conv setOffset:offset];
    [conv setEdgeMode:MPSImageEdgeModeZero];
    DLOG(INFO) << "  Create MPSCNNConvolution: " << conv;
    DLOG(INFO) << "    strideInPixelsY: " << conv.strideInPixelsY;
    DLOG(INFO) << "    strideInPixelsX: " << conv.strideInPixelsX;
    DLOG(INFO) << "    inputFeatureChannels: " << conv.inputFeatureChannels;
    DLOG(INFO) << "    outputFeatureChannels: " << conv.outputFeatureChannels;
    DLOG(INFO) << "    kernelWidth: " << conv.kernelWidth;
    DLOG(INFO) << "    kernelHeight: " << conv.kernelHeight;
    DLOG(INFO) << "    offset MPSOffset(x: " << offset.x << " y: " << offset.y << ")";
    operation.mpscnn_kernel.reset(conv);
  }

  return true;
}

bool CompilationImplMac::CompileAverageOrMaxPool2D(OperationMac& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileAverageOrMaxPool2D";
  DLOG_IF(FATAL, operation.type != mojom::AVERAGE_POOL_2D && operation.type != mojom::MAX_POOL_2D);
  int32_t input_width, input_height, output_width, output_height;
  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom;
  int32_t stride_width, stride_height;
  int32_t padding_code, fuse_code;
  int32_t filter_height, filter_width;

  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;
  uint32_t output_idx = outputs[0];
  OperandMac& output = operands_[output_idx];
  output_height = output.dimensions[1];
  output_width = output.dimensions[2];
  int32_t i = 0;
  int32_t input_idx = inputs[i++];
  OperandMac& input = operands_[input_idx];
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
    MPSCNNPooling* pool;
    if (operation.type == mojom::AVERAGE_POOL_2D) {
      pool = [[MPSCNNPoolingAverage alloc]
               initWithDevice:GetMPSCNNContext().device
               kernelWidth:filter_width
               kernelHeight:filter_height
               strideInPixelsX:stride_width
               strideInPixelsY:stride_height];
    } else if (operation.type == mojom::MAX_POOL_2D) {
      pool = [[MPSCNNPoolingMax alloc]
               initWithDevice:GetMPSCNNContext().device
               kernelWidth:filter_width
               kernelHeight:filter_height
               strideInPixelsX:stride_width
               strideInPixelsY:stride_height];
    } else {
      DLOG(ERROR) << "Operation " << operation.type << " is not supported";
      return false;
    }
    MPSOffset offset;
    if (implicit_padding) {
      ComputeMPSOffsetForImplictPadding(
          padding_code == mojom::PADDING_SAME,
          offset, input_height, input_width, output_height, output_width,
          filter_height, filter_width, stride_height, stride_width);
    } else {
      offset.x = (int)(filter_width / 2) - padding_left;
      offset.y = (int)(filter_height / 2) - padding_top;
      offset.z = 0;
    }
    DLOG(INFO) << "  MPSOffset x: " << offset.x << " y: " << offset.y;
    [pool setOffset:offset];
    [pool setEdgeMode:MPSImageEdgeModeClamp];
    DLOG(INFO) << "  Create MPSCNNPoolingAverage: " << pool;
    operation.mpscnn_kernel.reset(pool);
  }
  return true;
}

bool CompilationImplMac::CompileSoftmax(OperationMac& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileSoftmax";
  DLOG_IF(FATAL, operation.type != mojom::SOFTMAX);
  float beta = getScalarFloat(values_[operation.inputs[1]], memory_.get());
  DLOG(INFO) << "  beta: " << beta;
  if (beta != 1.0) {
    DLOG(ERROR) << "  beta " << beta << " is not supported.";
    return false;
  }
  if (@available(macOS 10.13, *)) {
    MPSCNNSoftMax* softmax =
        [[MPSCNNSoftMax alloc] initWithDevice:GetMPSCNNContext().device];
    DLOG(INFO) << "  Create MPSCNNSoftMax: " << softmax;
    operation.mpscnn_kernel.reset(softmax);
  }
  return true;
}

bool CompilationImplMac::CompileReshape(OperationMac& reshape) {
  DLOG(INFO) << "CompilationImplMac::CompileReshape";
  DLOG_IF(FATAL, reshape.type != mojom::RESHAPE);

  DLOG(INFO) << "  Reshape is compiled to no-op";
  uint32_t reshape_input_idx = reshape.inputs[0];
  uint32_t reshape_output_idx = reshape.outputs[0];
  for (size_t i = 0; i < operations_.size(); ++i) {
    OperationMac& operation = operations_[i];
    if (operation.inputs[0] == reshape_output_idx) {
      DLOG(INFO) << "  Connect op " << i << " type " << operation.type <<
          " input from " << operation.inputs[0] << " to " << reshape_input_idx;
      operation.inputs[0] = reshape_input_idx;
    }
  }
  return true;
}

bool CompilationImplMac::CompileConcatenation(OperationMac& concat) {
  DLOG(INFO) << "CompilationImplMac::CompileConcatenation";
  DLOG_IF(FATAL, concat.type != mojom::CONCATENATION);

  std::vector<uint32_t> inputs = concat.inputs;
  std::vector<uint32_t> outputs = concat.outputs;

  uint32_t axis = getScalarInt32(values_[inputs[inputs.size() - 1]], memory_.get());

  if (axis != 3) {
    DLOG(ERROR) << "Only axis == 3 is supported";
    return false;
  }

  if (@available(macOS 10.13, *)) {
    DLOG(INFO) << "  Concatenation is compiled to no-op";
    uint32_t concat_output_idx = concat.outputs[0];
    uint32_t channelOffset = 0;
    for (size_t i = 0; i < inputs.size() - 1; ++i) {
      uint32_t concat_input_idx = inputs[i];
      for (size_t j = 0; j < operations_.size(); ++j) {
        OperationMac& operation = operations_[j];
        if (operation.outputs[0] == concat_input_idx) {
          DLOG(INFO) << "  Rewrite op " << j << " type " << operation.type
                     << " output from " << operation.outputs[0] << " to " << concat_output_idx;
          operation.outputs[0] = concat_output_idx;
          MPSCNNKernel* kernel = operation.mpscnn_kernel.get();
          if (!kernel) {
            DLOG(ERROR) << "MPSKernel of operation " << j << " type " << operation.type
                        << " is not found";
            return false;
          }
          if (channelOffset % 4 != 0) {
            DLOG(ERROR) << "Invalid channelOffset " << channelOffset << ". It must be multiple of 4";
            return false;
          }
          DLOG(INFO) << "  Set destinationFeatureChannelOffset to " << channelOffset;
          [kernel setDestinationFeatureChannelOffset:channelOffset];
          OperandMac& operand = operands_[concat_input_idx];
          if (operand.dimensions.size() < 4) {
            DLOG(ERROR) << "Invalid dimensions of operand " << concat_input_idx << " length is " << operand.dimensions.size();
            return false;
          }
          channelOffset += operand.dimensions[axis];
        }
      }
    }
  }

  return true;
}

void CompilationImplMac::createExecution(createExecutionCallback callback) {
  DLOG(INFO) << "CompilationImplMac::createExecution";
  auto init_params = mojom::ExecutionInitParams::New();

  uint32_t input_memory_size = 0;
  for (size_t i = 0; i < inputs_.size(); ++i) {
    OperandMac& operand = operands_[inputs_[i]];
    input_memory_size += operand.requiredSize();
    init_params->inputs.push_back(
        mojom::OperandInfo::New(operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required input memory size: " << input_memory_size;

  uint32_t output_memory_size = 0;
  for (size_t i = 0; i < outputs_.size(); ++i) {
    OperandMac& operand = operands_[outputs_[i]];
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
