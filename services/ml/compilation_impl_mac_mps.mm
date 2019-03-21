// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac_mps.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "base/logging.h"
#include "base/mac/availability.h"
#include "base/mac/sdk_forward_declarations.h"
#include "services/ml/mps_protocols_impl.h"
#include "services/ml/mpscnn_context.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

API_AVAILABLE(macosx(10.13))
MPSCNNNeuron* CreateMPSCNNNeuron(int32_t fuse_code) {
  MPSCNNNeuron* relu = nullptr;
  if (fuse_code == mojom::FUSED_NONE) {
    relu = nullptr;
  } else if (fuse_code == mojom::FUSED_RELU) {
    relu = [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device
                                                  a:0];
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

API_AVAILABLE(macosx(10.13))
MPSCNNConvolutionNode* CreateMPSCNNConvolutionNode(MPSNNImageNode* image_node,
                                                   int32_t filter_width,
                                                   int32_t filter_height,
                                                   int32_t depth_in,
                                                   int32_t depth_out,
                                                   int32_t stride_width,
                                                   int32_t stride_height,
                                                   const float* weights,
                                                   const float* bias,
                                                   MPSCNNNeuron* relu,
                                                   int32_t type,
                                                   int32_t dilation_x = 1,
                                                   int32_t dilation_y = 1) {
  Class descriptor_class =
      (type == mojom::DEPTHWISE_CONV_2D ||
       type == mojom::ATROUS_DEPTHWISE_CONV_2D)
          ? NSClassFromString(@"MPSCNNDepthWiseConvolutionDescriptor")
          : NSClassFromString(@"MPSCNNConvolutionDescriptor");
  const MPSCNNConvolutionDescriptor* desc =
      [descriptor_class cnnConvolutionDescriptorWithKernelWidth:filter_width
                                                   kernelHeight:filter_height
                                           inputFeatureChannels:depth_in
                                          outputFeatureChannels:depth_out
                                                   neuronFilter:relu];
  desc.strideInPixelsX = stride_width;
  desc.strideInPixelsY = stride_height;
  desc.dilationRateX = dilation_x;
  desc.dilationRateY = dilation_y;
  desc.groups = 1;

  auto data_source = [[ConvDataSource alloc]
      initWithWeight:(float*)weights
                bias:(float*)bias
                desc:(MPSCNNConvolutionDescriptor*)desc];
  Class convolution_class = type == mojom::FULLY_CONNECTED
                                ? NSClassFromString(@"MPSCNNFullyConnectedNode")
                                : NSClassFromString(@"MPSCNNConvolutionNode");
  return [[convolution_class alloc] initWithSource:image_node
                                           weights:data_source];
}

API_AVAILABLE(macosx(10.13))
void ComputeMPSOffsetForImplictPadding(bool same_padding,
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

API_AVAILABLE(macosx(10.13))
bool CompileConv2DOrDepthwiseConv2D(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const OperationMac& operation,
    const std::map<uint32_t, ValueInfo>& values,
    std::unique_ptr<int8_t[]>& memory,
    const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileConv2DOrDepthwiseConv2D "
             << operation.type;
  int32_t input_batch_size, input_width, input_height, output_width,
      output_height;
  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom;
  int32_t stride_width, stride_height;
  int32_t padding_code, fuse_code;
  int32_t depth_out, filter_height, filter_width, depth_in;
  bool depthwise = (operation.type == mojom::DEPTHWISE_CONV_2D ||
                    operation.type == mojom::ATROUS_DEPTHWISE_CONV_2D);
  int32_t depthwise_multiplier = 1;

  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;
  DCHECK(outputs.size() == 1);
  if (!ParameterExtracterForConv(
          operation, inputs, outputs, values, memory, operands,
          input_batch_size, input_width, input_height, output_width,
          output_height, implicit_padding, padding_left, padding_right,
          padding_top, padding_bottom, stride_width, stride_height,
          padding_code, fuse_code, depth_out, filter_height, filter_width,
          depth_in, depthwise_multiplier, depthwise))
    return false;

  // TODO(junwei.fu):Use ConvParams to refactor code.
  uint32_t dilation_x = 1, dilation_y = 1;
  if (operation.type == mojom::ATROUS_DEPTHWISE_CONV_2D ||
      operation.type == mojom::ATROUS_CONV_2D) {
    dilation_x = stride_width;
    dilation_y = stride_height;
    stride_width = 1;
    stride_height = 1;
  }

  MPSCNNNeuron* relu = CreateMPSCNNNeuron(fuse_code);

  ValueInfo weights_value_info = values.at(inputs[1]);
  float* weights = (float*)(memory.get() + weights_value_info.offset);
  ValueInfo bias_value_info = values.at(inputs[2]);
  const float* bias =
      reinterpret_cast<const float*>(memory.get() + bias_value_info.offset);

  MPSNNImageNode* input_image = image_nodes[inputs[0]];
  if (depthwise) {
    if (depth_out != depth_in * depthwise_multiplier) {
      DLOG(ERROR)
          << "Failed assertion: outputFeatureChannels " << depth_out
          << " in MPS depthwise convolution descriptor must be multiplie of "
             "inFeatureChannels "
          << depth_in;
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
    memcpy(weights, depthwise_weights.data(), weights_value_info.length);
  }
  MPSCNNConvolutionNode* conv_node = CreateMPSCNNConvolutionNode(
      input_image, filter_width, filter_height, depth_in, depth_out,
      stride_width, stride_height, weights, bias, relu, operation.type,
      dilation_x, dilation_y);

  MPSOffset offset;
  if (implicit_padding) {
    ComputeMPSOffsetForImplictPadding(
        padding_code == mojom::PADDING_SAME, offset, input_height, input_width,
        output_height, output_width, filter_height, filter_width, stride_height,
        stride_width);
  } else {
    offset.x = (int)(filter_width / 2) - padding_left;
    offset.y = (int)(filter_height / 2) - padding_top;
    offset.z = 0;
  }
  DLOG(INFO) << "    offset MPSOffset(x: " << offset.x << " y: " << offset.y
             << ")";

  uint32_t n, width, height, channels;
  // operands[outputs[0]] is output operand.
  if (!GetMPSImageInfo(operands[outputs[0]], n, width, height, channels))
    return false;
  [conv_node setPaddingPolicy:[[CustomPadding alloc]
                                  initWithOffset:offset
                                        edgeMode:MPSImageEdgeModeZero
                                             num:n
                                           width:width
                                          height:height
                                        channels:channels]];
  image_nodes[outputs[0]] = conv_node.resultImage;

  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileAverageOrMaxPool2D(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                               const OperationMac& operation,
                               const std::map<uint32_t, ValueInfo>& values,
                               const std::unique_ptr<int8_t[]>& memory,
                               const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileAverageOrMaxPool2D";
  DLOG_IF(FATAL, operation.type != mojom::AVERAGE_POOL_2D &&
                     operation.type != mojom::MAX_POOL_2D);
  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom;
  int32_t padding_code;
  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;
  uint32_t output_idx = outputs[0];
  const OperandMac& output = operands[output_idx];
  const int32_t output_height = output.dimensions[1];
  const int32_t output_width = output.dimensions[2];
  int32_t i = 0;
  int32_t input_idx = inputs[i++];
  const OperandMac& input = operands[input_idx];
  const int32_t input_height = input.dimensions[1];
  const int32_t input_width = input.dimensions[2];

  DLOG(INFO) << "  input_height: " << input_height
             << " input_width: " << input_width;
  DLOG(INFO) << "  output_height: " << output_height
             << " output_width: " << output_width;

  if (inputs.size() == 10) {
    implicit_padding = false;
    padding_left = getScalarInt32(values, inputs[i++], memory.get());
    padding_right = getScalarInt32(values, inputs[i++], memory.get());
    padding_top = getScalarInt32(values, inputs[i++], memory.get());
    padding_bottom = getScalarInt32(values, inputs[i++], memory.get());
  } else if (inputs.size() == 7) {
    implicit_padding = true;
    padding_code = getScalarInt32(values, inputs[i++], memory.get());
  } else {
    DLOG(ERROR) << "  inputs size is incorrect";
    return false;
  }
  const int32_t stride_width =
      getScalarInt32(values, inputs[i++], memory.get());
  const int32_t stride_height =
      getScalarInt32(values, inputs[i++], memory.get());
  const int32_t filter_width =
      getScalarInt32(values, inputs[i++], memory.get());
  const int32_t filter_height =
      getScalarInt32(values, inputs[i++], memory.get());
  const int32_t fuse_code = getScalarInt32(values, inputs[i++], memory.get());

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

  MPSCNNPoolingNode* pool_node;
  MPSNNImageNode* input_image = image_nodes[inputs[0]];
  if (operation.type == mojom::AVERAGE_POOL_2D) {
    pool_node = [[MPSCNNPoolingAverageNode alloc] initWithSource:input_image
                                                     kernelWidth:filter_width
                                                    kernelHeight:filter_height
                                                 strideInPixelsX:stride_width
                                                 strideInPixelsY:stride_height];
  } else if (operation.type == mojom::MAX_POOL_2D) {
    pool_node = [[MPSCNNPoolingMaxNode alloc] initWithSource:input_image
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
        padding_code == mojom::PADDING_SAME, offset, input_height, input_width,
        output_height, output_width, filter_height, filter_width, stride_height,
        stride_width);
  } else {
    offset.x = (int)(filter_width / 2) - padding_left;
    offset.y = (int)(filter_height / 2) - padding_top;
    offset.z = 0;
  }
  DLOG(INFO) << "  MPSOffset x: " << offset.x << " y: " << offset.y;

  uint32_t n, width, height, channels;
  if (!GetMPSImageInfo(output, n, width, height, channels))
    return false;
  [pool_node setPaddingPolicy:[[CustomPadding alloc]
                                  initWithOffset:offset
                                        edgeMode:MPSImageEdgeModeClamp
                                             num:n
                                           width:width
                                          height:height
                                        channels:channels]];
  image_nodes[outputs[0]] = pool_node.resultImage;

  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileSoftmax(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                    const OperationMac& operation,
                    const std::map<uint32_t, ValueInfo>& values,
                    const std::unique_ptr<int8_t[]>& memory) {
  DLOG(INFO) << "CompilationImplMac::CompileSoftmax";
  DLOG_IF(FATAL, operation.type != mojom::SOFTMAX);
  float beta = getScalarFloat(values, operation.inputs[1], memory.get());
  DLOG(INFO) << "  beta: " << beta;
  if (beta != 1.0) {
    DLOG(ERROR) << "  beta " << beta << " is not supported.";
    return false;
  }

  MPSCNNSoftMaxNode* softmax_node = [[MPSCNNSoftMaxNode alloc]
      initWithSource:image_nodes[operation.inputs[0]]];
  image_nodes[operation.outputs[0]] = softmax_node.resultImage;

  return true;
}

bool CompileReshape(std::vector<OperationMac>& operations,
                    const OperationMac& reshape) {
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

API_AVAILABLE(macosx(10.13))
bool CompileConcatenation(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                          std::vector<OperationMac>& operations,
                          const OperationMac& concat,
                          const std::map<uint32_t, ValueInfo>& values,
                          const std::unique_ptr<int8_t[]>& memory,
                          const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileConcatenation";
  DLOG_IF(FATAL, concat.type != mojom::CONCATENATION);

  std::vector<uint32_t> inputs = concat.inputs;
  std::vector<uint32_t> outputs = concat.outputs;

  uint32_t axis =
      getScalarInt32(values, inputs[inputs.size() - 1], memory.get());
  DLOG(INFO) << "axis: " << axis;

  if (axis != 3) {
    DLOG(ERROR) << "Only axis == 3 is supported";
    return false;
  }

  NSMutableArray<MPSNNImageNode*>* image_array =
      [NSMutableArray arrayWithCapacity:1];
  for (size_t i = 0; i < inputs.size() - 1; ++i) {
    uint32_t concat_input_idx = inputs[i];
    const OperandMac& operand = operands[concat_input_idx];
    if (operand.dimensions.size() < 4) {
      DLOG(ERROR) << "Invalid dimensions of operand " << concat_input_idx
                  << " length is " << operand.dimensions.size();
      return false;
    }

    [image_array addObject:image_nodes[concat_input_idx]];
  }

  MPSNNConcatenationNode* concat_node =
      [[MPSNNConcatenationNode alloc] initWithSources:image_array];
  // concat.outputs[0] is index of output.
  image_nodes[concat.outputs[0]] = concat_node.resultImage;

  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileArithmetic(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                       const OperationMac& operation,
                       const std::vector<OperandMac>& operands,
                       std::vector<uint32_t>& constants,
                       const std::map<uint32_t, ValueInfo>& values,
                       const std::unique_ptr<int8_t[]>& memory) {
  DLOG(INFO) << "CompilationImplMac::CompileArithmetic";
  const std::vector<uint32_t>& primary_dimension =
      operands[operation.inputs[0]].dimensions;
  const std::vector<uint32_t>& secondary_dimension =
      operands[operation.inputs[1]].dimensions;
  size_t primary_batch_size =
      primary_dimension.size() == 4 ? primary_dimension[0] : 1;
  size_t secondary_batch_size =
      secondary_dimension.size() == 4 ? secondary_dimension[0] : 1;
  if (primary_batch_size != secondary_batch_size) {
    LOG(ERROR) << "Different batch size for arithmetic isn't supported.";
    return false;
  }

  // Check constants for input 0 and 1
  NSMutableArray<MPSNNImageNode*>* image_array =
      [NSMutableArray arrayWithCapacity:1];
  for (size_t i = 0; i < 2; ++i) {
    size_t input_index = operation.inputs[i];
    if (values.find(input_index) != values.end()) {
      constants.push_back(input_index);

      // Create a placeholder for input constant image.
      MPSNNImageNode* image_node =
          [[MPSNNImageNode alloc] initWithHandle:nullptr];
      [image_array addObject:image_node];
    } else {
      [image_array addObject:image_nodes[input_index]];
    }
  }

  MPSNNBinaryArithmeticNode* arithmetic_node = nullptr;
  if (operation.type == mojom::ADD) {
    arithmetic_node = [[MPSNNAdditionNode alloc] initWithSources:image_array];
  } else if (operation.type == mojom::MUL) {
    arithmetic_node =
        [[MPSNNMultiplicationNode alloc] initWithSources:image_array];
  }

  // TODO(junwei): the activation function must be configured in index 2.
  int32_t fuse_code = getScalarInt32(values, operation.inputs[2], memory.get());
  MPSCNNNeuronNode* relu_node = nullptr;
  switch (fuse_code) {
    case mojom::FUSED_NONE:
      break;
    case mojom::FUSED_RELU:
      [arithmetic_node setMinimumValue:0];
      relu_node = [[MPSCNNNeuronReLUNode alloc]
          initWithSource:arithmetic_node.resultImage
                       a:0];
      break;
    case mojom::FUSED_RELU1:
      [arithmetic_node setMinimumValue:-1];
      [arithmetic_node setMaximumValue:1];
      relu_node = [[MPSCNNNeuronReLUNNode alloc]
          initWithSource:arithmetic_node.resultImage
                       a:0
                       b:1];
      break;
    case mojom::FUSED_RELU6:
      [arithmetic_node setMinimumValue:0];
      [arithmetic_node setMaximumValue:6];
      relu_node = [[MPSCNNNeuronReLUNNode alloc]
          initWithSource:arithmetic_node.resultImage
                       a:0
                       b:6];
      break;
    default:
      NOTREACHED();
  }

  image_nodes[operation.outputs[0]] =
      relu_node ? relu_node.resultImage : arithmetic_node.resultImage;

  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileFullyConnected(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                           OperationMac& operation,
                           std::vector<OperandMac>& operands,
                           const std::map<uint32_t, ValueInfo>& values,
                           const std::unique_ptr<int8_t[]>& memory) {
  // operation.inputs[0] is the index of input in operands_.
  OperandMac& input = operands[operation.inputs[0]];
  if (input.dimensions.size() < 2) {
    DLOG(ERROR) << "A tenosr of least rank 2.";
    return false;
  }

  // If rank is greater than 2, then it gets flattened to a 2-D Tensor.
  // input_size corresponds to the number of inputs to the layer, matching
  // the second dimension of weights.
  // It is the same as input.dimensions[1] in 2-d inputs.
  // inputs[1] is index of weights.
  // operands_[inputs[1]].dimensions[1] is the second dimension of weights.
  const std::vector<uint32_t>& inputs = operation.inputs;
  // batch_size is calculated by dividing the number of elements by input_size.
  int32_t input_size = operands[inputs[1]].dimensions[1];
  input.dimensions = std::vector<uint32_t>(
      {product(input.dimensions) / input_size, input_size});

  int32_t fuse_code = getScalarInt32(values, inputs[3], memory.get());
  MPSCNNNeuron* relu = CreateMPSCNNNeuron(fuse_code);

  // inputs[1] is index of weights, values_.at(inputs[1]) is value info
  // of weights.
  const float* source_weights = reinterpret_cast<const float*>(
      memory.get() + values.at(inputs[1]).offset);
  // inputs[2] is index of bias, values_.at(inputs[2]) is value info of
  // bias.
  const float* source_bias = reinterpret_cast<const float*>(
      memory.get() + values.at(inputs[2]).offset);

  // the output_size is the same as first dimension of weights.
  int32_t output_size = operands[operation.outputs[0]].dimensions[1];
  MPSCNNConvolutionNode* fully_connected_node = CreateMPSCNNConvolutionNode(
      image_nodes[inputs[0]], 1, 1, input_size, output_size, 1, 1,
      source_weights, source_bias, relu, operation.type);

  image_nodes[operation.outputs[0]] = fully_connected_node.resultImage;
  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileBilinearScale(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                          OperationMac& operation,
                          const std::vector<OperandMac>& operands,
                          const std::map<uint32_t, ValueInfo>& values,
                          const std::unique_ptr<int8_t[]>& memory) {
  DLOG(INFO) << "Compile resize bilinear operation.";
  bool align_corners = false;
  switch (operation.inputs.size()) {
    case 3:
      break;
    case 4:
      align_corners =
          getScalarInt32(values, operation.inputs[3], memory.get()) == 0 ? false
                                                                         : true;
      break;
    default:
      LOG(ERROR) << "Inputs size is wrong " << operation.inputs.size();
      return false;
  }

  const OperandMac& output_operand = operands[operation.outputs[0]];
  if (output_operand.dimensions.size() != 4) {
    LOG(ERROR) << "Input and output must be 4-D tensor.";
    return false;
  }

  const OperandMac& input_operand = operands[operation.inputs[0]];
  if (output_operand.dimensions[2] % input_operand.dimensions[2] != 0 ||
      output_operand.dimensions[1] % input_operand.dimensions[1] != 0) {
    LOG(ERROR) << "The upsampling factor for the x/y must be integer.";
    return false;
  }

  // output_operand.dimensions[2] is width for "NHWC" data layout.
  NSUInteger scale_factorX =
      output_operand.dimensions[2] / input_operand.dimensions[2];
  NSUInteger scale_factorY =
      output_operand.dimensions[1] / input_operand.dimensions[1];

  MPSCNNUpsamplingBilinearNode* bilinear_scale_node;
  if (@available(macOS 10.14, *)) {
    bilinear_scale_node = [[MPSCNNUpsamplingBilinearNode alloc]
             initWithSource:image_nodes[operation.inputs[0]]
        integerScaleFactorX:scale_factorX
        integerScaleFactorY:scale_factorY
               alignCorners:align_corners];
    image_nodes[operation.outputs[0]] = bilinear_scale_node.resultImage;
    return true;
  }

  if (@available(macOS 10.13, *)) {
    LOG(WARNING) << "Only support false alignCorners on 10.13.";
    bilinear_scale_node = [[MPSCNNUpsamplingBilinearNode alloc]
             initWithSource:image_nodes[operation.inputs[0]]
        integerScaleFactorX:scale_factorX
        integerScaleFactorY:scale_factorY];
    image_nodes[operation.outputs[0]] = bilinear_scale_node.resultImage;
  }

  return true;
}

}  // namespace ml
