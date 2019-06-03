// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_bnns.h"

#include "services/ml/ml_utils_mac.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

API_AVAILABLE(macosx(10.13))
void ComputeBNNSOffsetForImplicitPadding(bool same_padding,
                                         OperationMac& operation,
                                         uint32_t& padding_top,
                                         uint32_t& padding_left,
                                         uint32_t output_height,
                                         uint32_t stride_height,
                                         uint32_t filter_height,
                                         uint32_t input_height,
                                         uint32_t output_width,
                                         uint32_t stride_width,
                                         uint32_t filter_width,
                                         uint32_t input_width) {
  if (same_padding) {
    uint32_t top_base_val =
        (output_height - 1) * stride_height + filter_height - input_height;
    if (top_base_val % 2 == 1) {
      operation.offset_y = 1;
      padding_top = (top_base_val - 1) / 2;
    } else {
      padding_top = top_base_val / 2;
    }
    uint32_t left_base_val =
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

CompiledModelBnns::CompiledModelBnns() = default;

CompiledModelBnns::~CompiledModelBnns() = default;

CompilationDelegateBnns::CompilationDelegateBnns(
    const CompilationImpl* compilation)
    : CompilationDelegate(), compilation_(compilation) {
  compiled_model_ = base::MakeRefCounted<CompiledModelBnns>();
}

CompilationDelegateBnns::~CompilationDelegateBnns() = default;

int32_t CompilationDelegateBnns::Compile() {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  CompileForModel(model, compiled_model_.get());
  bool success = true;
  for (size_t i = 0; i < model->operations.size(); ++i) {
    const mojom::OperationPtr& operation = model->operations[i];
	  OperationMac& operation_mac = compiled_model_->operations_[i];
          if (operation->type == mojom::ADD || operation->type == mojom::MUL) {
            success = CompileArithmetic(model, operation, operation_mac);
          } else if (operation->type == mojom::CONV_2D) {
            success = CompileConvolution(model, operation, operation_mac);
          } else if (operation->type == mojom::AVERAGE_POOL_2D ||
                     operation->type == mojom::MAX_POOL_2D) {
            success = CompilePooling(model, operation, operation_mac);
          } else if (operation->type == mojom::SOFTMAX) {
            success = CompileSoftmax(model, operation, operation_mac);
          } else if (operation->type == mojom::RESHAPE) {
            success = CompileReshape(model, operation, operation_mac);
          } else if (operation->type == mojom::CONCATENATION) {
            success = CompileConcatenation(model, operation, operation_mac);
          } else if (operation->type == mojom::FULLY_CONNECTED) {
            success = CompileFullyConnected(model, operation, operation_mac);
          } else {
            LOG(ERROR) << "Operation is not supported";
            success = false;
          }
  }
  if (!success) {
    LOG(ERROR) << "Failed compiling model.";
    return mojom::OP_FAILED;
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateBnns::CreateExecution(
    std::unique_ptr<mojom::Execution>& execution,
    mojom::ExecutionInitParamsPtr params) {
  execution = std::make_unique<ExecutionImplBnns>(std::move(compiled_model_),
                                                  std::move(params));
  return mojom::NOT_ERROR;
}

bool CompilationDelegateBnns::CompileConvolution(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation,
	  OperationMac& operation_bnns ) {
  DLOG(INFO) << "CompilationImplBnns::CompileConvolution";
  ConvParams params;
  int32_t result = compilation_->GetConvParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  if (params.depthwise) {
    LOG(ERROR) << "depthwise_multiplier is not supported.";
    return false;
  }

  BNNSActivation activation;
  bzero(&activation, sizeof(activation));
  if (params.fuse_code == mojom::FUSED_RELU6) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = 0;
    activation.beta = 6;
  } else if (params.fuse_code == mojom::FUSED_RELU) {
    activation.function = BNNSActivationFunctionRectifiedLinear;
  } else if (params.fuse_code == mojom::FUSED_RELU1) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = -1;
    activation.beta = 1;
  }

  // build conv weights BNNSLayerData structure
  BNNSConvolutionLayerParameters conv_params;
  BNNSFilterParameters filter_params;
  bzero(&filter_params, sizeof(filter_params));
  BNNSImageStackDescriptor in_desc, out_desc;

  float* source_weights = (float*)malloc(
      sizeof(float) * params.input_channel * params.output_channel *
      params.filter_height * params.filter_width);
  const mojom::OperandValueInfoPtr& weights_value_info =
      model->values[base::NumberToString(operation->inputs[1])];
  auto mapping = compilation_->MapMemory(operation->inputs[1]);
  memcpy(source_weights, mapping.get(), weights_value_info->length);

  float* source_bias = (float*)malloc(
      sizeof(float) * params.input_channel * params.output_channel *
      params.filter_height * params.filter_width);
  const mojom::OperandValueInfoPtr& bias_value_info =
      model->values[base::NumberToString(operation->inputs[2])];
  mapping = compilation_->MapMemory(operation->inputs[2]);
  memcpy(source_bias, mapping.get(), bias_value_info->length);

  // build conv_weights
  BNNSLayerData conv_weights;
  // The weights will be destroyed by BNNSFilterDestroy
  float* new_filter_weights = (float*)malloc(
      sizeof(float) * params.input_channel * params.output_channel *
      params.filter_height * params.filter_width);
  if (new_filter_weights == nullptr) {
    LOG(ERROR) << "Fail to alloc memory!";
    return false;
  }

  for (uint32_t o = 0; o < params.output_channel; ++o) {
    for (uint32_t h = 0; h < params.filter_height; ++h) {
      for (uint32_t w = 0; w < params.filter_width; ++w) {
        for (uint32_t i = 0; i < params.input_channel; ++i) {
          auto old_idx = o * params.filter_height * params.filter_width *
                             params.input_channel +
                         h * params.filter_width * params.input_channel +
                         w * params.input_channel + i;
          auto new_idx = w + params.filter_width *
                                 (h + params.filter_height *
                                          (i + params.input_channel * o));
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

  operation_bnns.offset_x = 0;
  operation_bnns.offset_y = 0;
  operation_bnns.input_batch_size = params.input_batch;

  bool implicit_padding = false ;
  int32_t padding_code;
  const std::vector<uint32_t>& inputs = operation->inputs;
  if ((!params.depthwise && inputs.size() == 10) ||
      (params.depthwise && inputs.size() == 11)) {
    implicit_padding = false ;
  } else if ((!params.depthwise && inputs.size() == 7) ||
             (params.depthwise && inputs.size() == 8)) {
    implicit_padding = true ;
    padding_code = compilation_->GetScalarInt32(inputs[3]);
  }

  if (implicit_padding) {
    ComputeBNNSOffsetForImplicitPadding(
        padding_code == mojom::PADDING_SAME, operation_bnns, params.padding_top,
        params.padding_left, params.output_height, params.stride_height, params.filter_height, params.input_height,
        params.output_width, params.stride_width, params.filter_width, params.input_width);
  }

  conv_params.x_stride = params.stride_width;
  conv_params.y_stride = params.stride_height;
  conv_params.x_padding = params.padding_left;
  conv_params.y_padding = params.padding_top;
  conv_params.k_width = params.filter_width;
  conv_params.k_height = params.filter_height;
  conv_params.in_channels = params.input_channel;
  conv_params.out_channels = params.output_channel;
  conv_params.weights = conv_weights;
  conv_params.bias = conv_bias;
  conv_params.activation = activation;
  // If 0, use the best number of threads for the current machine.
  // https://developer.apple.com/documentation/accelerate/bnnsfilterparameters/1642345-n_threads?language=objc
  filter_params.n_threads = 0;
  filter_params.alloc_memory = nullptr;
  filter_params.free_memory = nullptr;

  size_t fix_input_width = params.input_width + operation_bnns.offset_x;
  size_t fix_input_height = params.input_height + operation_bnns.offset_y;
  DLOG(INFO) << "FIX_INPUT_WIDTH: " << fix_input_width;
  DLOG(INFO) << "FIX_INPUT_HEIGHT: " << fix_input_height;
  in_desc.width = fix_input_width;
  in_desc.height = fix_input_height;
  in_desc.channels = params.input_channel;
  in_desc.row_stride = fix_input_width;
  in_desc.image_stride = fix_input_width * fix_input_height;
  in_desc.data_type = BNNSDataTypeFloat32;
  out_desc.width = params.output_width;
  out_desc.height = params.output_height;
  out_desc.channels = params.output_channel;
  out_desc.row_stride = params.output_width;
  out_desc.image_stride = params.output_width * params.output_height;
  out_desc.data_type = BNNSDataTypeFloat32;
  operation_bnns.filter = BNNSFilterCreateConvolutionLayer(
      &in_desc, &out_desc, &conv_params, &filter_params);
  if (operation_bnns.filter == nullptr) {
    LOG(ERROR) << "BNNS Fail to Create ConvLayer";
    return false;
  }
  return true;
}

bool CompilationDelegateBnns::CompileArithmetic(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation,
	  OperationMac& operation_bnns ) {
  DLOG(INFO) << "CompilationDelegateBnns::CompileArithmetic";

  ElementWiseParams params;
  int32_t result = compilation_->GetElementWiseParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  if (operation->type == mojom::ADD) {
    operation_bnns.local_operation = KAdd;
  } else if (operation->type == mojom::MUL) {
    operation_bnns.local_operation = KMul;
  }
  operation_bnns.offset_x = 0;
  operation_bnns.offset_y = 0;

  const std::vector<uint32_t>& inputs = operation->inputs;
  if (model->operands[inputs[0]]->dimensions != model->operands[inputs[1]]->dimensions) {
    DLOG(ERROR) << "Broadcasting is not supported by now!";
    return false;
  }

  DLOG(INFO) << "inputs size " << inputs.size();
  for (size_t i = 0; i < 2; ++i) {
    std::string input_id(base::NumberToString(inputs[i]));
    uint32_t extend_input_idx = inputs[i];
    if (model->values.find(input_id) != model->values.end()) {
      const mojom::OperandValueInfoPtr& weights_value_info =
          model->values[input_id];
      auto mapping = compilation_->MapMemory(extend_input_idx);
      void* tmp_buffer = malloc(weights_value_info->length);
      memcpy(tmp_buffer, mapping.get(), weights_value_info->length);
      const OperandMac& input = OperandMac(model->operands[inputs[i]]);
      SetExtendInputs(extend_input_idx, input, operation_bnns,
                      (float*)tmp_buffer);
    }
  }

  BNNSVectorDescriptor in_desc, out_desc;
  const OperandMac& output = OperandMac(model->operands[operation->outputs[0]]);
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
  if (params.fuse_code == mojom::FUSED_RELU) {
    activation.function = BNNSActivationFunctionRectifiedLinear;
  } else if (params.fuse_code == mojom::FUSED_RELU1) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = -1;
    activation.beta = 1;
  } else if (params.fuse_code == mojom::FUSED_RELU6) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = 0;
    activation.beta = 6;
  }
  BNNSFilterParameters filter_params;
  bzero(&filter_params, sizeof(filter_params));
  operation_bnns.filter = BNNSFilterCreateVectorActivationLayer(
      &in_desc, &out_desc, &activation, &filter_params);
  if (operation_bnns.filter == nullptr) {
    LOG(ERROR) << "BNNS Fail to Create activation function!";
    return false;
  }

  return true;
}

bool CompilationDelegateBnns::CompilePooling(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation,
	  OperationMac& operation_bnns ) {
  DLOG(INFO) << "CompilationDelegateBnns::CompilePooling";
  PoolingParams params;
  int32_t result = compilation_->GetPoolingParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  operation_bnns.offset_x = 0;
  operation_bnns.offset_y = 0;
  operation_bnns.input_batch_size = params.input_batch;

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

  if (params.fuse_code == mojom::FUSED_RELU6) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = 0;
    activation.beta = 6;
  } else if (params.fuse_code == mojom::FUSED_RELU) {
    activation.function = BNNSActivationFunctionRectifiedLinear;
  } else if (params.fuse_code == mojom::FUSED_RELU1) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = -1;
    activation.beta = 1;
  }

  pool.x_stride = params.stride_width;
  pool.y_stride = params.stride_height;
  pool.x_padding = params.padding_left;
  pool.y_padding = params.padding_top;
  pool.k_width = params.filter_width;
  pool.k_height = params.filter_height;
  pool.in_channels = params.input_channel;
  pool.out_channels = params.output_channel;
  pool.activation = activation;

  BNNSLayerData pooling_bias;
  float* pooling_bias_data = (float*)malloc(sizeof(float) * params.output_channel);
  if (pooling_bias_data == nullptr) {
    DLOG(ERROR) << "Fail to alloc memory!";
    return false;
  }
  bzero(pooling_bias_data, sizeof(float) * params.output_channel);
  pooling_bias.data = pooling_bias_data;
  pooling_bias.data_type = BNNSDataTypeFloat32;
  pooling_bias.data_scale = 0.0;
  pooling_bias.data_bias = 0.0;
  pooling_bias.data_table = nullptr;
  pool.bias = pooling_bias;

  if (operation->type == mojom::AVERAGE_POOL_2D) {
    pool.pooling_function = BNNSPoolingFunctionAverage;
  } else if (operation->type == mojom::MAX_POOL_2D) {
    pool.pooling_function = BNNSPoolingFunctionMax;
  } else {
    LOG(ERROR) << "Operation " << operation->type << " is not supported";
    return false;
  }

  in_desc.width = params.input_width;
  in_desc.height = params.input_height;
  in_desc.channels = params.input_channel;
  in_desc.row_stride = params.input_width;
  in_desc.image_stride = params.input_width * params.input_height;
  in_desc.data_type = BNNSDataTypeFloat32;
  out_desc.width = params.output_width;
  out_desc.height = params.output_height;
  out_desc.channels = params.output_channel;
  out_desc.row_stride = params.output_width;
  out_desc.image_stride = params.output_width * params.output_height;
  out_desc.data_type = BNNSDataTypeFloat32;

  operation_bnns.filter =
      BNNSFilterCreatePoolingLayer(&in_desc, &out_desc, &pool, &filter_params);
  if (operation_bnns.filter == nullptr) {
    DLOG(ERROR) << "BNNS Fail to Create PoolingLayer";
    return false;
  }
  return true;
}

bool CompilationDelegateBnns::CompileSoftmax(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation,
	  OperationMac& operation_bnns ) {
  DLOG(INFO) << "CompilationDelegateBnns::CompileSoftmax";

  operation_bnns.offset_x = 0;
  operation_bnns.offset_y = 0;
  
  SoftmaxParams params;
  int32_t result = compilation_->GetSoftmaxParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  if (params.beta != 1.0) {
    DLOG(ERROR) << "  beta " << params.beta << " is not supported.";
    return false;
  }

  BNNSVectorDescriptor in_desc, out_desc;
  const std::vector<uint32_t>& inputs = operation->inputs;
  const std::vector<uint32_t>& outputs = operation->outputs;
  const mojom::OperandPtr& input = model->operands[inputs[0]];
  const mojom::OperandPtr& output = model->operands[outputs[0]];

  operation_bnns.input_batch_size = input->dimensions[0] ;

  int32_t size = 1;
  for (size_t i = 1; i < input->dimensions.size(); i++) {
    size = size * input->dimensions[i];
  }
  in_desc.size = size;
  in_desc.data_type = BNNSDataTypeFloat32;
  in_desc.data_scale = 0;
  in_desc.data_bias = 0;
  size = 1;
  for (size_t i = 1; i < output->dimensions.size(); i++) {
    size = size * output->dimensions[i];
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
  operation_bnns.filter = BNNSFilterCreateVectorActivationLayer(
      &in_desc, &out_desc, &activation, &filter_params);
  return true;
}

bool CompilationDelegateBnns::CompileReshape(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation, 
	  OperationMac& operation_bnns ) {
  DLOG(INFO) << "CompilationDelegateBnns::CompileReshape";
  operation_bnns.local_operation = KReshape ;
  operation_bnns.offset_x = 0 ;
  operation_bnns.offset_y = 0 ;
  operation_bnns.input_batch_size = 1;
  return true;
}

bool CompilationDelegateBnns::CompileConcatenation(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation, 
	  OperationMac& operation_bnns ) {
  DLOG(INFO) << "CompilationDelegateBnns::CompileConcatenation";
  
  operation_bnns.local_operation = KConcatenation;
  operation_bnns.offset_x = 0;
  operation_bnns.offset_y = 0;
  operation_bnns.input_batch_size = 1;

  ConcatParams params;
  int32_t result = compilation_->GetConcatParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  if (params.axis != 3) {
    DLOG(ERROR) << "Only axis == 3 is supported";
    return false;
  }

  const std::vector<uint32_t>& inputs = operation->inputs;
  // const std::vector<uint32_t>& outputs = operation->outputs;

  for (size_t i = 0; i < inputs.size() - 1; ++i) {
    std::string input_id(base::NumberToString(inputs[i]));
    uint32_t extend_input_idx = inputs[i];
    if (model->values.find(input_id) != model->values.end()) {
      const mojom::OperandValueInfoPtr& weights_value_info =
          model->values[input_id];
      auto mapping = compilation_->MapMemory(extend_input_idx);
      void* tmp_buffer = malloc(weights_value_info->length);
      memcpy(tmp_buffer, mapping.get(), weights_value_info->length);
      const OperandMac& input = OperandMac(model->operands[inputs[i]]);
      SetExtendInputs(extend_input_idx, input, operation_bnns,
                      (float*)tmp_buffer);
    }
  }

  return true;
}

bool CompilationDelegateBnns::CompileFullyConnected(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation, 
	  OperationMac& operation_bnns ) {
  DLOG(INFO) << "CompilationDelegateBnns::CompileFullyConnected";
  
  FullyConnectedParams params;
  int32_t result = compilation_->GetFullyConnectedParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  operation_bnns.offset_x = 0;
  operation_bnns.offset_y = 0;
  operation_bnns.input_batch_size = params.input_batch_size;

  BNNSFilterParameters filter_params;
  bzero(&filter_params, sizeof(filter_params));

  float* source_weights = (float*)malloc(
      sizeof(float) * params.num_units * params.input_size);
  const mojom::OperandValueInfoPtr& weights_value_info =
      model->values[base::NumberToString(operation->inputs[1])];
  auto mapping = compilation_->MapMemory(operation->inputs[1]);
  memcpy(source_weights, mapping.get(), weights_value_info->length);

  float* source_bias = (float*)malloc(
      sizeof(float) * params.bias_num_units * params.input_size);
  const mojom::OperandValueInfoPtr& bias_value_info =
      model->values[base::NumberToString(operation->inputs[2])];
  mapping = compilation_->MapMemory(operation->inputs[2]);
  memcpy(source_bias, mapping.get(), bias_value_info->length);

  BNNSActivation activation;
  bzero(&activation, sizeof(activation));
  if (params.fuse_code == mojom::FUSED_RELU) {
    activation.function = BNNSActivationFunctionRectifiedLinear;
  } else if (params.fuse_code == mojom::FUSED_RELU1) {
    activation.function = BNNSActivationFunctionClamp;
    activation.alpha = -1;
    activation.beta = 1;
  } else if (params.fuse_code == mojom::FUSED_RELU6) {
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
  connected_params.in_size = params.input_size;
  connected_params.out_size = params.output_num_units;
  connected_params.weights = connected_weights;
  connected_params.bias = connected_bias;
  connected_params.activation = activation;

  BNNSVectorDescriptor in_desc, out_desc;
  in_desc.size = params.input_size;
  in_desc.data_type = BNNSDataTypeFloat32;
  in_desc.data_scale = 0;
  in_desc.data_bias = 0;

  out_desc.size = params.output_num_units;
  out_desc.data_type = BNNSDataTypeFloat32;
  out_desc.data_scale = 0;
  out_desc.data_bias = 0;

  operation_bnns.filter = BNNSFilterCreateFullyConnectedLayer(
      &in_desc, &out_desc, &connected_params, &filter_params);
  return true;
}

bool CompilationDelegateBnns::CompileBilinearScale(
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation, 
	  OperationMac& operation_bnns ) {
  OperationMac operation_mac(operation) ;
  operation_mac.filter = nullptr ;
  return true;
}

}  // namespace ml