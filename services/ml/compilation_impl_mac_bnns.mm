// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac_bnns.h"

#import <Accelerate/Accelerate.h>

#include "base/logging.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

void ResamplingkernelFunc(const float* xArray,
                          float* yArray,
                          unsigned long count,
                          void* userData) {
  float sum = 0;
  for (size_t i = 0; i < count; ++i) {
    yArray[i] = 1 - fabs(xArray[i]);
    sum += yArray[i];
  }

  sum = float(1 / sum);
  for (size_t i = 0; i < count; ++i) {
    yArray[i] *= sum;
  }
}

void SetExtendInputs(const uint32_t extend_input_idx,
                     const OperandMac& input,
                     OperationMac& operation,
                     float* input_memory) {
  if (input.dimensions.size() == 4) {
    const int32_t input_batch = input.dimensions[0];
    const int32_t input_height = input.dimensions[1];
    const int32_t input_width = input.dimensions[2];
    const int32_t input_depth = input.dimensions[3];
    float* output_memory =
        (float*)malloc(sizeof(float) * product(input.dimensions));
    for (int b = 0; b < input_batch; b++) {
      for (int h = 0; h < input_height; h++) {
        for (int w = 0; w < input_width; w++) {
          for (int d = 0; d < input_depth; d++) {
            const int new_batch_offset =
                b * input_height * input_width * input_depth;
            const int ori_index = new_batch_offset +
                                  h * input_width * input_depth +
                                  w * input_depth + d;
            const int new_index = new_batch_offset +
                                  d * input_height * input_width +
                                  h * input_width + w;
            *(output_memory + new_index) = *(input_memory + ori_index);
          }
        }
      }
    }
    operation.extend_input.push_back(output_memory);
  } else {
    operation.extend_input.push_back(input_memory);
  }
}

API_AVAILABLE(macosx(10.13))
void ComputeBNNSOffsetForImplicitPadding(bool same_padding,
                                         OperationMac& operation,
                                         int32_t& padding_top,
                                         int32_t& padding_left,
                                         int32_t output_height,
                                         int32_t stride_height,
                                         int32_t filter_height,
                                         int32_t input_height,
                                         int32_t output_width,
                                         int32_t stride_width,
                                         int32_t filter_width,
                                         int32_t input_width) {
  if (same_padding) {
    int32_t top_base_val =
        (output_height - 1) * stride_height + filter_height - input_height;
    if (top_base_val % 2 == 1) {
      operation.offset_y = 1;
      padding_top = (top_base_val - 1) / 2;
    } else {
      padding_top = top_base_val / 2;
    }
    int32_t left_base_val =
        (output_width - 1) * stride_width + filter_width - input_width;
    if (left_base_val % 2 == 1) {
      operation.offset_x = 1;
      padding_left = (left_base_val - 1) / 2;
    } else {
      padding_left = left_base_val / 2;
    }
  } else {
    padding_top = 0;
    padding_left = 0;
  }
}

API_AVAILABLE(macosx(10.13))
bool CompileResizeBilinearBNNS(OperationMac& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileResizeBilinearBNNS";
  DLOG_IF(FATAL, operation.type != mojom::RESIZE_BILINEAR);

  operation.local_operation = KResize;
  operation.offset_x = 0;
  operation.offset_y = 0;
  operation.kernelFunc = ResamplingkernelFunc;
  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileArithmeticBNNS(OperationMac& operation,
                           const std::map<uint32_t, ValueInfo>& values,
                           const std::unique_ptr<int8_t[]>& memory,
                           const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileArithmetic";
  DLOG_IF(FATAL, operation.type != mojom::ADD && operation.type != mojom::MUL);

  if (operation.type == mojom::ADD) {
    operation.local_operation = KAdd;
  } else if (operation.type == mojom::MUL) {
    operation.local_operation = KMul;
  }
  operation.offset_x = 0;
  operation.offset_y = 0;

  const std::vector<uint32_t> inputs = operation.inputs;
  if (operands[inputs[0]].dimensions != operands[inputs[1]].dimensions) {
    DLOG(ERROR) << "Broadcasting is not supported by now!";
    return false;
  }

  for (size_t i = 0; i < 2; i++) {
    uint32_t extend_input_idx = inputs[i];
    if (values.find(extend_input_idx) != values.end()) {
      float* input_memory = reinterpret_cast<float*>(
          memory.get() + values.at(extend_input_idx).offset);
      const OperandMac& input = operands[inputs[i]];
      SetExtendInputs(extend_input_idx, input, operation, input_memory);
    }
  }

  const int32_t fuse_code = getScalarInt32(values, inputs[2], memory.get());
  DLOG(INFO) << "FUSE_CODE:  " << fuse_code;

  BNNSVectorDescriptor in_desc, out_desc;
  const OperandMac& output = operands[operation.outputs[0]];
  int32_t size = product(output.dimensions);
  in_desc.size = size;
  in_desc.data_type = BNNSDataTypeFloat32;
  in_desc.data_scale = 0;
  in_desc.data_bias = 0;

  out_desc.size = size;
  out_desc.data_type = BNNSDataTypeFloat32;
  out_desc.data_scale = 0;
  out_desc.data_bias = 0;
  BNNSActivation activation;
  bzero(&activation, sizeof(activation));
  if (fuse_code == mojom::FUSED_RELU) {
    activation.function = BNNSActivationFunctionRectifiedLinear;
  } else if (fuse_code == mojom::FUSED_RELU1) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = -1;
    activation.beta = 1;
  } else if (fuse_code == mojom::FUSED_RELU6) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = 0;
    activation.beta = 6;
  }
  BNNSFilterParameters filter_params;
  bzero(&filter_params, sizeof(filter_params));
  operation.filter = BNNSFilterCreateVectorActivationLayer(
      &in_desc, &out_desc, &activation, &filter_params);
  if (operation.filter == nullptr) {
    LOG(ERROR) << "BNNS Fail to Create activation function!";
    return false;
  }

  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileConv2DBNNS(OperationMac& operation,
                       const std::map<uint32_t, ValueInfo>& values,
                       const std::unique_ptr<int8_t[]>& memory,
                       const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileConv2D";
  DLOG_IF(FATAL, operation.type != mojom::CONV_2D);
  int32_t input_batch_size, input_width, input_height, output_width,
      output_height;
  bool implicit_padding = false;
  int32_t padding_left, padding_right, padding_top, padding_bottom;
  int32_t stride_width, stride_height;
  int32_t padding_code, fuse_code;
  int32_t depth_out, filter_height, filter_width, depth_in;
  int32_t depthwise_multiplier;

  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;

  if (inputs.size() != 7 && inputs.size() != 10) {
    DLOG(ERROR) << "Unsupported input size!";
    return false;
  }

  ParameterExtracterForConv(
      operation, inputs, outputs, values, memory, operands, input_batch_size,
      input_width, input_height, output_width, output_height, implicit_padding,
      padding_left, padding_right, padding_top, padding_bottom, stride_width,
      stride_height, padding_code, fuse_code, depth_out, filter_height,
      filter_width, depth_in, depthwise_multiplier);

  DLOG(INFO) << "FILTER_HEIGHT: " << filter_height;
  DLOG(INFO) << "FILTER_WIDTH: " << filter_width;
  DLOG(INFO) << "IMPLICIT_PADDING: " << implicit_padding;
  BNNSActivation activation;
  bzero(&activation, sizeof(activation));
  if (fuse_code == mojom::FUSED_RELU6) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = 0;
    activation.beta = 6;
  } else if (fuse_code == mojom::FUSED_RELU) {
    activation.function = BNNSActivationFunctionRectifiedLinear;
  } else if (fuse_code == mojom::FUSED_RELU1) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = -1;
    activation.beta = 1;
  }

  DLOG(INFO) << "  stride_width: " << stride_width;
  DLOG(INFO) << "  stride_height: " << stride_height;
  DLOG(INFO) << "  fuse_code: " << fuse_code;

  operation.input_batch_size = input_batch_size;
  operation.fuse_code = fuse_code;

  // build conv weights BNNSLayerData structure
  BNNSConvolutionLayerParameters conv_params;
  BNNSFilterParameters filter_params;
  bzero(&filter_params, sizeof(filter_params));
  BNNSImageStackDescriptor in_desc, out_desc;

  ValueInfo weights_value_info = values.at(inputs[1]);
  const float* source_weights =
      reinterpret_cast<const float*>(memory.get() + weights_value_info.offset);
  ValueInfo bias_value_info = values.at(inputs[2]);
  const float* source_bias =
      reinterpret_cast<const float*>(memory.get() + bias_value_info.offset);

  // build conv_weights
  BNNSLayerData conv_weights;
  // The weights will be destroyed by BNNSFilterDestroy
  float* new_filter_weights = (float*)malloc(
      sizeof(float) * depth_in * depth_out * filter_height * filter_width);
  if (new_filter_weights == nullptr) {
    DLOG(ERROR) << "Fail to alloc memory!";
    return false;
  }

  for (auto o = 0; o < depth_out; ++o) {
    for (auto h = 0; h < filter_height; ++h) {
      for (auto w = 0; w < filter_width; ++w) {
        for (auto i = 0; i < depth_in; ++i) {
          auto old_idx = o * filter_height * filter_width * depth_in +
                         h * filter_width * depth_in + w * depth_in + i;
          auto new_idx =
              w + filter_width * (h + filter_height * (i + depth_in * o));
          new_filter_weights[new_idx] = source_weights[old_idx];
        }
      }
    }
  }

  conv_weights.data = new_filter_weights;
  conv_weights.data_type = BNNSDataTypeFloat32;
  // we can just ignore data_scale, data_bias and data_table
  // for the data type in float32
  conv_weights.data_scale = 0.0;
  conv_weights.data_bias = 0.0;
  conv_weights.data_table = nullptr;

  // build conv bias
  BNNSLayerData conv_bias;
  conv_bias.data = source_bias;
  conv_bias.data_type = BNNSDataTypeFloat32;
  // we can just ignore data_scale, data_bias and data_table
  // for the data type in float32
  conv_bias.data_scale = 0.0;
  conv_bias.data_bias = 0.0;
  conv_bias.data_table = nullptr;

  operation.offset_x = 0;
  operation.offset_y = 0;

  if (implicit_padding) {
    ComputeBNNSOffsetForImplicitPadding(
        padding_code == mojom::PADDING_SAME, operation, padding_top,
        padding_left, output_height, stride_height, filter_height, input_height,
        output_width, stride_width, filter_width, input_width);
  }
  DLOG(INFO) << "PADDING_LEFT: " << padding_left;
  DLOG(INFO) << "PADDING_TOP:" << padding_top;

  conv_params.x_stride = stride_width;
  conv_params.y_stride = stride_height;
  conv_params.x_padding = padding_left;
  conv_params.y_padding = padding_top;
  conv_params.k_width = filter_width;
  conv_params.k_height = filter_height;
  conv_params.in_channels = depth_in;
  conv_params.out_channels = depth_out;
  conv_params.weights = conv_weights;
  conv_params.bias = conv_bias;
  conv_params.activation = activation;
  // If 0, use the best number of threads for the current machine.
  // https://developer.apple.com/documentation/accelerate/bnnsfilterparameters/1642345-n_threads?language=objc
  filter_params.n_threads = 0;
  filter_params.alloc_memory = nullptr;
  filter_params.free_memory = nullptr;

  size_t fix_input_width = input_width + operation.offset_x;
  size_t fix_input_height = input_height + operation.offset_y;
  DLOG(INFO) << "FIX_INPUT_WIDTH: " << fix_input_width;
  DLOG(INFO) << "FIX_INPUT_HEIGHT: " << fix_input_height;
  in_desc.width = fix_input_width;
  in_desc.height = fix_input_height;
  in_desc.channels = depth_in;
  in_desc.row_stride = fix_input_width;
  in_desc.image_stride = fix_input_width * fix_input_height;
  in_desc.data_type = BNNSDataTypeFloat32;
  out_desc.width = output_width;
  out_desc.height = output_height;
  out_desc.channels = depth_out;
  out_desc.row_stride = output_width;
  out_desc.image_stride = output_width * output_height;
  out_desc.data_type = BNNSDataTypeFloat32;
  operation.filter = BNNSFilterCreateConvolutionLayer(
      &in_desc, &out_desc, &conv_params, &filter_params);
  if (operation.filter == nullptr) {
    DLOG(ERROR) << "BNNS Fail to Create ConvolutionLayer";
    return false;
  }

  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileAverageOrMaxPool2DBNNS(OperationMac& operation,
                                   const std::map<uint32_t, ValueInfo>& values,
                                   const std::unique_ptr<int8_t[]>& memory,
                                   const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileAverageOrMaxPool2DBnns";
  DLOG_IF(FATAL, operation.type != mojom::AVERAGE_POOL_2D &&
                     operation.type != mojom::MAX_POOL_2D);

  bool implicit_padding;
  int32_t x_padding, y_padding, padding_code;

  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;
  uint32_t output_idx = outputs[0];
  const OperandMac& output = operands[output_idx];
  const int32_t output_height = output.dimensions[1];
  const int32_t output_width = output.dimensions[2];
  const int32_t depth_out = output.dimensions[3];
  int32_t i = 0;
  int32_t input_idx = inputs[i++];
  const OperandMac& input = operands[input_idx];
  const int32_t input_batch_size = input.dimensions[0];
  const int32_t input_height = input.dimensions[1];
  const int32_t input_width = input.dimensions[2];
  const int32_t depth_in = input.dimensions[3];
  operation.input_batch_size = input_batch_size;

  DLOG(INFO) << "  input_height: " << input_height
             << " input_width: " << input_width;
  DLOG(INFO) << "  output_height: " << output_height
             << " output_width: " << output_width;

  if (inputs.size() == 10) {
    implicit_padding = false;
    const int32_t padding_left =
        getScalarInt32(values, inputs[i++], memory.get());
    const int32_t padding_right =
        getScalarInt32(values, inputs[i++], memory.get());
    const int32_t padding_top =
        getScalarInt32(values, inputs[i++], memory.get());
    const int32_t padding_bottom =
        getScalarInt32(values, inputs[i++], memory.get());

    // bnns only accept x_padding and y_padding
    x_padding = padding_left;
    y_padding = padding_top;
    DLOG(INFO) << "  padding_left: " << padding_left;
    DLOG(INFO) << "  padding_right: " << padding_right;
    DLOG(INFO) << "  padding_top: " << padding_top;
    DLOG(INFO) << "  padding_bottom: " << padding_bottom;
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

  operation.offset_x = 0;
  operation.offset_y = 0;

  if (implicit_padding) {
    ComputeBNNSOffsetForImplicitPadding(
        padding_code == mojom::PADDING_SAME, operation, y_padding, x_padding,
        output_height, stride_height, filter_height, input_height, output_width,
        stride_width, filter_width, input_width);
  }

  BNNSLayerData layer_data;
  BNNSFilterParameters filter_params;
  BNNSPoolingLayerParameters pool;
  BNNSImageStackDescriptor in_desc, out_desc;
  BNNSActivation activation;

  layer_data.data_type = BNNSDataTypeFloat32;
  bzero(&filter_params, sizeof(filter_params));
  bzero(&activation, sizeof(activation));
  filter_params.n_threads = 0;
  filter_params.alloc_memory = nullptr;
  filter_params.free_memory = nullptr;

  if (fuse_code == mojom::FUSED_RELU6) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = 0;
    activation.beta = 6;
  } else if (fuse_code == mojom::FUSED_RELU) {
    activation.function = BNNSActivationFunctionRectifiedLinear;
  } else if (fuse_code == mojom::FUSED_RELU1) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = -1;
    activation.beta = 1;
  }
  pool.x_stride = stride_width;
  pool.y_stride = stride_height;
  pool.x_padding = x_padding;
  pool.y_padding = y_padding;
  pool.k_width = filter_width;
  pool.k_height = filter_height;
  pool.in_channels = depth_in;
  pool.out_channels = depth_out;
  pool.activation = activation;

  // build pooling bias
  BNNSLayerData pooling_bias;
  float* pooling_bias_data = (float*)malloc(sizeof(float) * depth_out);
  if (pooling_bias_data == nullptr) {
    DLOG(ERROR) << "Fail to alloc memory!";
    return false;
  }
  bzero(pooling_bias_data, sizeof(float) * depth_out);
  pooling_bias.data = pooling_bias_data;
  pooling_bias.data_type = BNNSDataTypeFloat32;
  pooling_bias.data_scale = 0.0;
  pooling_bias.data_bias = 0.0;
  pooling_bias.data_table = nullptr;
  pool.bias = pooling_bias;

  if (operation.type == mojom::AVERAGE_POOL_2D) {
    pool.pooling_function = BNNSPoolingFunctionAverage;
  } else if (operation.type == mojom::MAX_POOL_2D) {
    pool.pooling_function = BNNSPoolingFunctionMax;
  } else {
    DLOG(ERROR) << "Operation " << operation.type << " is not supported";
    return false;
  }

  in_desc.width = input_width;
  in_desc.height = input_height;
  in_desc.channels = depth_in;
  in_desc.row_stride = input_width;
  in_desc.image_stride = input_width * input_height;
  in_desc.data_type = BNNSDataTypeFloat32;
  out_desc.width = output_width;
  out_desc.height = output_height;
  out_desc.channels = depth_out;
  out_desc.row_stride = output_width;
  out_desc.image_stride = output_width * output_height;
  out_desc.data_type = BNNSDataTypeFloat32;

  operation.filter =
      BNNSFilterCreatePoolingLayer(&in_desc, &out_desc, &pool, &filter_params);
  if (operation.filter == nullptr) {
    DLOG(ERROR) << "BNNS Fail to Create PoolingLayer";
    return false;
  }
  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileSoftmaxBNNS(OperationMac& operation,
                        const std::map<uint32_t, ValueInfo>& values,
                        const std::unique_ptr<int8_t[]>& memory,
                        const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileSoftmaxBNNS";
  DLOG_IF(FATAL, operation.type != mojom::SOFTMAX);

  std::vector<uint32_t> inputs = operation.inputs;
  std::vector<uint32_t> outputs = operation.outputs;
  const OperandMac& input = operands[inputs[0]];
  const OperandMac& output = operands[outputs[0]];
  const uint32_t beta = getScalarFloat(values, inputs[1], memory.get());
  if (beta != 1.0) {
    DLOG(ERROR) << "  beta " << beta << " is not supported.";
    return false;
  }
  operation.offset_x = 0;
  operation.offset_y = 0;

  operation.input_batch_size = input.dimensions[0];
  BNNSVectorDescriptor in_desc, out_desc;
  int32_t size = 1;
  for (size_t i = 1; i < input.dimensions.size(); i++) {
    size = size * input.dimensions[i];
  }
  in_desc.size = size;
  in_desc.data_type = BNNSDataTypeFloat32;
  in_desc.data_scale = 0;
  in_desc.data_bias = 0;
  size = 1;
  for (size_t i = 1; i < output.dimensions.size(); i++) {
    size = size * output.dimensions[i];
  }
  out_desc.size = size;
  out_desc.data_type = BNNSDataTypeFloat32;
  out_desc.data_scale = 0;
  out_desc.data_bias = 0;
  BNNSActivation activation;
  bzero(&activation, sizeof(activation));
  activation.function = BNNSActivationFunctionSoftmax;
  BNNSFilterParameters filter_params;
  bzero(&filter_params, sizeof(filter_params));
  operation.filter = BNNSFilterCreateVectorActivationLayer(
      &in_desc, &out_desc, &activation, &filter_params);
  if (operation.filter == nullptr) {
    DLOG(ERROR) << "BNNS Fail to Create SoftmaxLayer";
    return false;
  }
  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileReshapeBNNS(OperationMac& reshape) {
  DLOG(INFO) << "CompilationImplMac::CompileReshapeBNNS";
  DLOG_IF(FATAL, reshape.type != mojom::RESHAPE);

  reshape.local_operation = KReshape;
  reshape.offset_x = 0;
  reshape.offset_y = 0;
  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileConcatenationBNNS(OperationMac& concat,
                              const std::map<uint32_t, ValueInfo>& values,
                              const std::unique_ptr<int8_t[]>& memory,
                              const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileConcatenationBNNS";
  DLOG_IF(FATAL, concat.type != mojom::CONCATENATION);
  concat.local_operation = KConcatenation;
  concat.offset_x = 0;
  concat.offset_y = 0;

  std::vector<uint32_t> inputs = concat.inputs;
  std::vector<uint32_t> outputs = concat.outputs;

  for (size_t i = 0; i < inputs.size() - 1; ++i) {
    uint32_t extend_input_idx = inputs[i];
    if (values.find(extend_input_idx) != values.end()) {
      float* input_memory =
          reinterpret_cast<float*>(memory.get() + values.at(inputs[i]).offset);
      const OperandMac& input = operands[inputs[i]];
      SetExtendInputs(extend_input_idx, input, concat, input_memory);
    }
  }

  uint32_t axis =
      getScalarInt32(values, inputs[inputs.size() - 1], memory.get());
  if (axis != 3) {
    DLOG(ERROR) << "Only axis == 3 is supported";
    return false;
  }
  return true;
}

API_AVAILABLE(macosx(10.13))
bool CompileFullyConnectedBNNS(OperationMac& operation,
                               const std::map<uint32_t, ValueInfo>& values,
                               const std::unique_ptr<int8_t[]>& memory,
                               const std::vector<OperandMac>& operands) {
  DLOG(INFO) << "CompilationImplMac::CompileFullyConnectedBNNS";
  DLOG_IF(FATAL, operation.type != mojom::FULLY_CONNECTED);
  const std::vector<uint32_t> inputs = operation.inputs;
  const std::vector<uint32_t> outputs = operation.outputs;
  const OperandMac& output = operands[outputs[0]];
  const int32_t output_size = output.dimensions[1];

  int32_t i = 0;
  const OperandMac& input = operands[inputs[i++]];
  if (input.dimensions.size() < 2 || input.dimensions.size() > 4) {
    DLOG(ERROR) << "A tensor of least rank 2 and up to 4";
    return false;
  }
  const uint32_t weights_idx = inputs[i++];
  const Operand& weights = operands[weights_idx];
  const int32_t num_unit = weights.dimensions[0];
  const int32_t input_size = weights.dimensions[1];

  int32_t input_batch_size = 1;
  if (input.dimensions.size() > 2) {
    input_batch_size = product(input.dimensions) / input_size;
  } else {
    input_batch_size = input.dimensions[0];
  }
  operation.input_batch_size = input_batch_size;

  operation.offset_x = 0;
  operation.offset_y = 0;
  DLOG(INFO) << "  num_unit: " << num_unit;
  DLOG(INFO) << "  input_batch_size: " << input_batch_size;

  BNNSFilterParameters filter_params;
  bzero(&filter_params, sizeof(filter_params));

  ValueInfo weights_value_info = values.at(weights_idx);
  const float* source_weights =
      reinterpret_cast<const float*>(memory.get() + weights_value_info.offset);
  ValueInfo bias_value_info = values.at(inputs[i++]);
  const float* source_bias =
      reinterpret_cast<const float*>(memory.get() + bias_value_info.offset);

  int32_t fuse_code;
  fuse_code = getScalarInt32(values, inputs[i++], memory.get());
  DLOG(INFO) << "  Fuse_code: " << fuse_code;
  BNNSActivation activation;
  bzero(&activation, sizeof(activation));
  if (fuse_code == mojom::FUSED_RELU) {
    activation.function = BNNSActivationFunctionRectifiedLinear;
  } else if (fuse_code == mojom::FUSED_RELU1) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = -1;
    activation.beta = 1;
  } else if (fuse_code == mojom::FUSED_RELU6) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = 0;
    activation.beta = 6;
  }

  BNNSLayerData connected_weights;
  connected_weights.data = source_weights;
  connected_weights.data_type = BNNSDataTypeFloat32;
  // we can just ignore data_scale, data_bias and data_table
  // for the data type in float32
  connected_weights.data_scale = 0.0;
  connected_weights.data_bias = 0.0;
  connected_weights.data_table = nullptr;

  BNNSLayerData connected_bias;
  connected_bias.data = source_bias;
  connected_bias.data_type = BNNSDataTypeFloat32;
  // we can just ignore data_scale, data_bias and data_table
  // for the data type in float32
  connected_bias.data_scale = 0.0;
  connected_bias.data_bias = 0.0;
  connected_bias.data_table = nullptr;

  BNNSFullyConnectedLayerParameters connected_params;
  connected_params.in_size = input_size;
  connected_params.out_size = output_size;
  connected_params.weights = connected_weights;
  connected_params.bias = connected_bias;
  connected_params.activation = activation;

  BNNSVectorDescriptor in_desc, out_desc;
  in_desc.size = input_size;
  in_desc.data_type = BNNSDataTypeFloat32;
  in_desc.data_scale = 0;
  in_desc.data_bias = 0;

  out_desc.size = output_size;
  out_desc.data_type = BNNSDataTypeFloat32;
  out_desc.data_scale = 0;
  out_desc.data_bias = 0;

  operation.filter = BNNSFilterCreateFullyConnectedLayer(
      &in_desc, &out_desc, &connected_params, &filter_params);
  if (operation.filter == nullptr) {
    LOG(ERROR) << "BNNS Fail to Create FullyConnctedLayer";
    return false;
  }
  return true;
}
}  // namespace ml
