// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac_mps.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "base/logging.h"
#include "base/mac/availability.h"
#include "base/mac/sdk_forward_declarations.h"
#include "services/ml/mpscnn_context.h"
#include "services/ml/public/interfaces/constants.mojom.h"

API_AVAILABLE(macosx(10.13))
@interface ConvDataSource : NSObject<MPSCNNConvolutionDataSource> {
}
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
- (id)copyWithZone:(struct _NSZone*)zone {
  ConvDataSource* source = [[ConvDataSource allocWithZone:zone] init];
  source.weights_ = self.weights_;
  source.bias_ = self.bias_;
  source.desc_ = self.desc_;
  return source;
}
@end

namespace ml {

MPSCNNNeuron* API_AVAILABLE(macosx(10.13))
    CreateMPSCNNNeuron(int32_t fuse_code) {
  MPSCNNNeuron* relu = nullptr;
  if (fuse_code == mojom::FUSED_NONE) {
    relu = nullptr;
  } else if (fuse_code == mojom::FUSED_RELU) {
    relu =
        [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device a:0];
  } else if (fuse_code == mojom::FUSED_RELU1) {
    relu = [[MPSCNNNeuronReLUN alloc] initWithDevice:GetMPSCNNContext().device
                                                   a:0
                                                   b:1];
  } else if (fuse_code == mojom::FUSED_RELU6) {
    relu = [[MPSCNNNeuronReLUN alloc] initWithDevice:GetMPSCNNContext().device
                                                   a:0
                                                   b:6];
  } else {
    DLOG(INFO) << "Fuse code " << fuse_code
               << " is not supported by MPSCNNNeuron";
  }
  return relu;
}

MPSCNNConvolution* API_AVAILABLE(macosx(10.13))
    CreateMPSCNNConvolution(int32_t filter_width,
                            int32_t filter_height,
                            int32_t depth_in,
                            int32_t depth_out,
                            int32_t stride_width,
                            int32_t stride_height,
                            const float* weights,
                            const float* bias,
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
      [[MPSCNNConvolution alloc] initWithDevice:GetMPSCNNContext().device
                                        weights:data_source];
  return conv;
}

void API_AVAILABLE(macosx(10.13))
    ComputeMPSOffsetForImplictPadding(bool same_padding,
                                      MPSOffset& offset,
                                      int32_t input_height,
                                      int32_t input_width,
                                      int32_t output_height,
                                      int32_t output_width,
                                      int32_t filter_height,
                                      int32_t filter_width,
                                      int32_t stride_height,
                                      int32_t stride_width) {
  if (same_padding) {
    int pad_along_height =
        ((output_height - 1) * stride_height + filter_height - input_height);
    int pad_along_width =
        ((output_width - 1) * stride_width + filter_width - input_width);
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

bool CompileConv2DOrDepthwiseConv2D(OperationMac& operation,
                                    std::map<uint32_t, ValueInfo>& values,
                                    std::unique_ptr<int8_t[]>& memory,
                                    std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileConv2DOrDepthwiseConv2D";
  DLOG_IF(FATAL, operation.type != mojom::CONV_2D &&
                     operation.type != mojom::DEPTHWISE_CONV_2D);
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
  int i = 0;
  ParameterExtracterForConv(
      operation, inputs, outputs, values, memory, operands, input_width,
      input_height, output_width, output_height, implicit_padding, padding_left,
      padding_right, padding_top, padding_bottom, stride_width, stride_height,
      padding_code, fuse_code, depth_out, filter_height, filter_width, depth_in,
      i, depthwise_multiplier, depthwise);

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
    operation.fuse_code = fuse_code;

    ValueInfo weights_value_info = values.at(inputs[1]);
    const float* weights = reinterpret_cast<const float*>(
        memory.get() + weights_value_info.offset);
    ValueInfo bias_value_info = values.at(inputs[2]);
    const float* bias =
        reinterpret_cast<const float*>(memory.get() + bias_value_info.offset);

    MPSCNNConvolution* conv;
    if (depthwise) {
      if (depth_in != depth_out) {
        DLOG(ERROR) << @"for depth wise convolution, currently only channel"
                        " multiplier of 1 is supported";
        return false;
      }
      // Convert from WebML weights shape [1, filter_height, filter_width,
      // depth_out] to MPSCNNConvlution weight[ outputChannels ][ kernelHeight
      // ][ kernelWidth ][ inputChannels / groups ]
      const uint32_t depthwise_weights_length =
          1 * filter_height * filter_width * depth_out;
      std::vector<float> depthwise_weights(depthwise_weights_length);
      DLOG_IF(FATAL, depthwise_weights.size() * sizeof(float) !=
                         weights_value_info.length)
          << "depthwise weigths length is incorrect";
      for (auto h = 0; h < filter_height; ++h) {
        for (auto w = 0; w < filter_width; ++w) {
          for (auto c = 0; c < depth_out; ++c) {
            depthwise_weights[c * filter_height * filter_width +
                              h * filter_width + w] =
                weights[h * filter_width * depth_out + w * depth_out + c];
          }
        }
      }
      conv = CreateMPSCNNConvolution(
          filter_width, filter_height, depth_in, depth_out, stride_width,
          stride_height, depthwise_weights.data(), bias, relu, depthwise);
    } else {
      conv = CreateMPSCNNConvolution(filter_width, filter_height, depth_in,
                                     depth_out, stride_width, stride_height,
                                     weights, bias, relu, depthwise);
    }

    MPSOffset offset;
    if (implicit_padding) {
      ComputeMPSOffsetForImplictPadding(
          padding_code == mojom::PADDING_SAME, offset, input_height,
          input_width, output_height, output_width, filter_height, filter_width,
          stride_height, stride_width);
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
    DLOG(INFO) << "    offset MPSOffset(x: " << offset.x << " y: " << offset.y
               << ")";
    operation.mpscnn_kernel.reset(conv);
  }

  return true;
}

bool CompileAverageOrMaxPool2D(OperationMac& operation,
                               std::map<uint32_t, ValueInfo>& values,
                               std::unique_ptr<int8_t[]>& memory,
                               std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileAverageOrMaxPool2D";
  DLOG_IF(FATAL, operation.type != mojom::AVERAGE_POOL_2D &&
                     operation.type != mojom::MAX_POOL_2D);
  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom;
  int32_t padding_code;
  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;
  uint32_t output_idx = outputs[0];
  OperandMac& output = operands[output_idx];
  const int32_t output_height = output.dimensions[1];
  const int32_t output_width = output.dimensions[2];
  int32_t i = 0;
  int32_t input_idx = inputs[i++];
  OperandMac& input = operands[input_idx];
  const int32_t input_height = input.dimensions[1];
  const int32_t input_width = input.dimensions[2];

  DLOG(INFO) << "  input_height: " << input_height
             << " input_width: " << input_width;
  DLOG(INFO) << "  output_height: " << output_height
             << " output_width: " << output_width;

  if (inputs.size() == 10) {
    implicit_padding = false;
    padding_left = getScalarInt32(values[inputs[i++]], memory.get());
    padding_right = getScalarInt32(values[inputs[i++]], memory.get());
    padding_top = getScalarInt32(values[inputs[i++]], memory.get());
    padding_bottom = getScalarInt32(values[inputs[i++]], memory.get());
  } else if (inputs.size() == 7) {
    implicit_padding = true;
    padding_code = getScalarInt32(values[inputs[i++]], memory.get());
  } else {
    DLOG(ERROR) << "  inputs size is incorrect";
    return false;
  }
  const int32_t stride_width =
      getScalarInt32(values[inputs[i++]], memory.get());
  const int32_t stride_height =
      getScalarInt32(values[inputs[i++]], memory.get());
  const int32_t filter_width =
      getScalarInt32(values[inputs[i++]], memory.get());
  const int32_t filter_height =
      getScalarInt32(values[inputs[i++]], memory.get());
  const int32_t fuse_code = getScalarInt32(values[inputs[i++]], memory.get());

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
      pool =
          [[MPSCNNPoolingAverage alloc] initWithDevice:GetMPSCNNContext().device
                                           kernelWidth:filter_width
                                          kernelHeight:filter_height
                                       strideInPixelsX:stride_width
                                       strideInPixelsY:stride_height];
    } else if (operation.type == mojom::MAX_POOL_2D) {
      pool = [[MPSCNNPoolingMax alloc] initWithDevice:GetMPSCNNContext().device
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
          padding_code == mojom::PADDING_SAME, offset, input_height,
          input_width, output_height, output_width, filter_height, filter_width,
          stride_height, stride_width);
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

bool CompileSoftmax(OperationMac& operation,
                    std::map<uint32_t, ValueInfo>& values,
                    std::unique_ptr<int8_t[]>& memory) {
  DLOG(INFO) << "CompilationImplMac::CompileSoftmax";
  DLOG_IF(FATAL, operation.type != mojom::SOFTMAX);
  float beta = getScalarFloat(values[operation.inputs[1]], memory.get());
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

bool CompileReshape(OperationMac& reshape,
                    std::vector<OperationMac>& operations) {
  DLOG(INFO) << "CompilationImplMac::CompileReshape";
  DLOG_IF(FATAL, reshape.type != mojom::RESHAPE);

  DLOG(INFO) << "  Reshape is compiled to no-op";
  uint32_t reshape_input_idx = reshape.inputs[0];
  uint32_t reshape_output_idx = reshape.outputs[0];
  for (size_t i = 0; i < operations.size(); ++i) {
    OperationMac& operation = operations[i];
    if (operation.inputs[0] == reshape_output_idx) {
      DLOG(INFO) << "  Connect op " << i << " type " << operation.type
                 << " input from " << operation.inputs[0] << " to "
                 << reshape_input_idx;
      operation.inputs[0] = reshape_input_idx;
    }
  }
  return true;
}

bool CompileConcatenation(OperationMac& concat,
                          std::map<uint32_t, ValueInfo>& values,
                          std::unique_ptr<int8_t[]>& memory,
                          std::vector<OperandMac>& operands,
                          std::vector<OperationMac>& operations) {
  DLOG(INFO) << "CompilationImplMac::CompileConcatenation";
  DLOG_IF(FATAL, concat.type != mojom::CONCATENATION);

  std::vector<uint32_t> inputs = concat.inputs;
  std::vector<uint32_t> outputs = concat.outputs;

  uint32_t axis =
      getScalarInt32(values[inputs[inputs.size() - 1]], memory.get());
  DLOG(INFO) << "axis: " << axis;

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
      OperandMac& operand = operands[concat_input_idx];
      for (size_t j = 0; j < operations.size(); ++j) {
        OperationMac& operation = operations[j];
        if (operation.outputs[0] == concat_input_idx) {
          DLOG(INFO) << "  Rewrite op " << j << " type " << operation.type
                     << " output from " << operation.outputs[0] << " to "
                     << concat_output_idx;
          operation.outputs[0] = concat_output_idx;
          MPSCNNKernel* kernel = operation.mpscnn_kernel.get();
          if (!kernel) {
            DLOG(INFO) << "MPSKernel of operation " << j << " type "
                       << operation.type << " is not found";
            // Concatenation op has no kernel, continue to search
            continue;
          }
          if (channelOffset % 4 != 0) {
            DLOG(ERROR) << "Invalid channelOffset " << channelOffset
                        << ". It must be multiple of 4";
            return false;
          }
          // Accumulate the previous offset
          const uint32_t offset =
              [kernel destinationFeatureChannelOffset] + channelOffset;
          DLOG(INFO) << "  Set destinationFeatureChannelOffset to " << offset;
          [kernel setDestinationFeatureChannelOffset:offset];
          DLOG(INFO) << "OPERATION.DIMENSIONS.SIZE: "
                     << operand.dimensions.size();
          for (size_t i = 0; i < operand.dimensions.size(); ++i) {
            DLOG(INFO) << "OPERAND[" << i << "]: " << operand.dimensions[i];
          }
          if (operand.dimensions.size() < 4) {
            DLOG(ERROR) << "Invalid dimensions of operand " << concat_input_idx
                        << " length is " << operand.dimensions.size();
            return false;
          }
        }
      }
      channelOffset += operand.dimensions[axis];
    }
  }

  return true;
}

bool CompileArithmetic(OperationMac& operation,
                       std::map<uint32_t, ValueInfo>& values,
                       std::vector<uint32_t>& constants) {
  DLOG(INFO) << "CompilationImplMac::CompileArithmetic";
  DLOG_IF(FATAL, operation.type != mojom::ADD && operation.type != mojom::MUL);

  if (@available(macOS 10.13.4, *)) {
    MPSCNNBinaryKernel* arithmetic = nullptr;
    if (operation.type == mojom::ADD) {
      Class mpscnn_add_class = NSClassFromString(@"MPSCNNAdd");
      if (!mpscnn_add_class) {
        DLOG(ERROR) << "Failed to load MPSCNNAdd class";
        return false;
      }
      arithmetic =
          [[mpscnn_add_class alloc] initWithDevice:GetMPSCNNContext().device];
    } else if (operation.type == mojom::MUL) {
      Class mpscnn_multiply_class = NSClassFromString(@"MPSCNNMultiply");
      if (!mpscnn_multiply_class) {
        DLOG(ERROR) << "Failed to load MPSCNNMultiply class";
        return false;
      }
      arithmetic = [[mpscnn_multiply_class alloc]
          initWithDevice:GetMPSCNNContext().device];
    }

    if (!arithmetic)
      return false;

    operation.mpscnn_binary_kernel.reset(arithmetic);

    // Check constants for input 0 and 1
    for (size_t i = 0; i < operation.inputs.size() - 1; ++i) {
      if (values.find(operation.inputs[i]) != values.end()) {
        constants.push_back(operation.inputs[i]);
      }
    }
  }

  return true;
}

}  // namespace ml
