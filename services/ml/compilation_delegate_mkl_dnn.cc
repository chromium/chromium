// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_mkl_dnn.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/cl_dnn_custom_kernels.h"
#include "services/ml/execution_impl_mkl_dnn.h"
#include "services/ml/public/interfaces/constants.mojom.h"

namespace ml {

namespace {

inline void CalculateExplicitPadding(bool padding_same,
                                     int32_t in_size,
                                     int32_t stride,
                                     int32_t filter_size,
                                     int32_t& padding_head,
                                     int32_t& padding_tail,
                                     int32_t dilation = 1) {
  padding_head = 0;
  padding_tail = 0;

  if (padding_same) {
    int32_t out_size = (in_size + stride - 1) / stride;
    int32_t tmp = (out_size - 1) * stride + filter_size;
    if (tmp > in_size) {
      padding_head = ((tmp - in_size) / 2) * dilation;
      padding_tail = ((tmp - in_size) - padding_head) * dilation;
    }
  }
}

}  // namespace

CompilationDelegateMklDnn::CompilationDelegateMklDnn(
    const CompilationImpl* compilation)
    : CompilationDelegate(),
      compilation_(compilation) {}

CompilationDelegateMklDnn::~CompilationDelegateMklDnn() {
}

int32_t CompilationDelegateMklDnn::Compile() {
  DLOG(INFO) << "CompilationDelegateMklDnn::Compile";

  int32_t result = MkldnnInit();
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  return mojom::NOT_ERROR;
}

std::unique_ptr<mojom::Execution> CompilationDelegateMklDnn::CreateExecution(
    mojom::ExecutionInitParamsPtr params) {
  return std::make_unique<ExecutionImplMklDnn>(this, std::move(params));
}

int32_t CompilationDelegateMklDnn::MkldnnInit() {
#if defined(OS_LINUX)
  if (!GetMklDnnSymbolTable()->Load()) {
    LOG(ERROR) << "[MKLDNN] failed to load MKLDNN library";
    return mojom::OP_FAILED;
  }
#endif

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnCreateTopology() {
  int32_t result;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  for (size_t i = 0; i < model->inputs.size(); ++i) {
    result = MkldnnAddInput(model->inputs[i]);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }
  for (size_t i = 0; i < model->outputs.size(); ++i) {
  }

  for (size_t i = 0; i < model->operations.size(); ++i) {
    const mojom::OperationPtr& operation = model->operations[i];
    const int32_t type = operation->type;
    const std::vector<uint32_t>& inputs = operation->inputs;
    const std::vector<uint32_t>& outputs = operation->outputs;

    if (outputs.size() != 1) {
      DLOG(ERROR) << "Only 1 output is supported";
      return mojom::BAD_DATA;
    }

    int32_t result = mojom::NOT_ERROR;
    if (type == mojom::ADD || type == mojom::MUL) {
      result = MkldnnAddElementwise(type, inputs, outputs);
    } else if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D ||
               type == mojom::ATROUS_CONV_2D ||
               type == mojom::ATROUS_DEPTHWISE_CONV_2D) {
      result = MkldnnAddConvolution(type, inputs, outputs);
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      result = MkldnnAddPooling(type, inputs, outputs);
    } else if (type == mojom::SOFTMAX) {
      result = MkldnnAddSoftmax(type, inputs, outputs);
    } else if (type == mojom::RESHAPE) {
      result = MkldnnAddReshape(type, inputs, outputs);
    } else if (type == mojom::CONCATENATION) {
      result = MkldnnAddConcatenation(type, inputs, outputs);
    } else if (type == mojom::FULLY_CONNECTED) {
      result = MkldnnAddFullyConnected(type, inputs, outputs);
    } else if (type == mojom::RESIZE_BILINEAR) {
      result = MkldnnAddResizeBilinear(type, inputs, outputs);
    } else {
      DLOG(ERROR) << "Operation type " << type << " is not supported.";
      return mojom::BAD_DATA;
    }
  }

  if (result != mojom::NOT_ERROR) {
    return result;
  }

  DLOG(INFO) << "[MKLDNN] succeed to create topology";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnCreateProgram() {
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddInput(uint32_t index) {
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddReorder(
    const std::string& input_name,
    const std::string& output_name,
    int32_t target_format) {
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddData(uint32_t index) {
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddActivationByFusedCode(
    const std::string& input,
    const std::string& id,
    int32_t fuse_code) {
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddElementwise(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddConvolution(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  const bool depthwise = (type == mojom::DEPTHWISE_CONV_2D ||
                          type == mojom::ATROUS_DEPTHWISE_CONV_2D)
                             ? true
                             : false;
  const bool atrous =
      (type == mojom::ATROUS_CONV_2D || type == mojom::ATROUS_DEPTHWISE_CONV_2D)
          ? true
          : false;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const uint32_t output_index = outputs[0];
  const mojom::OperandPtr& output = model->operands[output_index];
  const int32_t output_batch = output->dimensions[0];
  const int32_t output_height = output->dimensions[1];
  const int32_t output_width = output->dimensions[2];
  const int32_t output_channel = output->dimensions[3];
  uint32_t index = 0;
  const uint32_t input_index = inputs[index++];
  const mojom::OperandPtr& input = model->operands[input_index];
  const int32_t input_height = input->dimensions[1];
  const int32_t input_width = input->dimensions[2];

  const uint32_t filter_idx = inputs[index++];
  mojom::OperandPtr& filter = model->operands[filter_idx];
  int32_t depth_out, depth_in;
  if (depthwise) {
    depth_out = filter->dimensions[3];
  } else {
    depth_out = filter->dimensions[0];
    depth_in = filter->dimensions[3];
  }
  const int32_t filter_height = filter->dimensions[1];
  const int32_t filter_width = filter->dimensions[2];

  const uint32_t bias_idx = inputs[index++];
  mojom::OperandPtr& bias = model->operands[bias_idx];
  const int32_t bias_length = bias->dimensions[0];

  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom,
      padding_code;
  if ((!depthwise && inputs.size() == 10) ||
      (depthwise && inputs.size() == 11)) {
    implicit_padding = false;
    padding_left = compilation_->GetScalarInt32(inputs[index++]);
    padding_right = compilation_->GetScalarInt32(inputs[index++]);
    padding_top = compilation_->GetScalarInt32(inputs[index++]);
    padding_bottom = compilation_->GetScalarInt32(inputs[index++]);
  } else if ((!depthwise && inputs.size() == 7) ||
             (depthwise && inputs.size() == 8)) {
    implicit_padding = true;
    padding_code = compilation_->GetScalarInt32(inputs[index++]);
  } else {
    DLOG(ERROR) << "Inputs size is incorrect";
    return mojom::BAD_DATA;
  }
  int32_t stride_width, stride_height;
  int32_t dilation_width, dilation_height;
  if (!atrous) {
    stride_width = compilation_->GetScalarInt32(inputs[index++]);
    stride_height = compilation_->GetScalarInt32(inputs[index++]);
    dilation_width = 1;
    dilation_height = 1;
  } else {
    dilation_width = compilation_->GetScalarInt32(inputs[index++]);
    dilation_height = compilation_->GetScalarInt32(inputs[index++]);
    stride_width = 1;
    stride_height = 1;
  }
  int32_t depthwise_multiplier;
  if (depthwise) {
    depthwise_multiplier = compilation_->GetScalarInt32(inputs[index++]);
    if (depthwise_multiplier != 1) {
      DLOG(ERROR) << "  depthwise_multiplier " << depthwise_multiplier
                  << " is not supported.";
      return mojom::BAD_DATA;
    }
    depth_in = depth_out / depthwise_multiplier;
  }
  const int32_t fuse_code = compilation_->GetScalarInt32(inputs[index++]);

  DLOG(INFO) << "  input_height: " << input_height;
  DLOG(INFO) << "  input_width: " << input_width;
  DLOG(INFO) << "  output_batch: " << output_batch;
  DLOG(INFO) << "  output_height: " << output_height;
  DLOG(INFO) << "  output_width: " << output_width;
  DLOG(INFO) << "  output_channel: " << output_channel;
  DLOG(INFO) << "  filter_height: " << filter_height;
  DLOG(INFO) << "  filter_width: " << filter_width;
  DLOG(INFO) << "  bias_length: " << bias_length;
  DLOG(INFO) << "  depth_in: " << depth_in;
  DLOG(INFO) << "  depth_out: " << depth_out;
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
  DLOG(INFO) << "  dilation_width: " << dilation_width;
  DLOG(INFO) << "  dilation_height: " << dilation_height;
  if (depthwise) {
    DLOG(INFO) << "  depthwise_multiplier: " << depthwise_multiplier;
  }
  DLOG(INFO) << "  fuse_code: " << fuse_code;

  if (implicit_padding) {
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME, input_width,
                             stride_width, filter_width, padding_left,
                             padding_right, dilation_width);
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME, input_height,
                             stride_height, filter_height, padding_top,
                             padding_bottom, dilation_height);
    DLOG(INFO) << "  padding_left: " << padding_left;
    DLOG(INFO) << "  padding_right: " << padding_right;
    DLOG(INFO) << "  padding_top: " << padding_top;
    DLOG(INFO) << "  padding_bottom: " << padding_bottom;
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddPooling(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const uint32_t output_index = outputs[0];
  const mojom::OperandPtr& output = model->operands[output_index];
  const int32_t output_batch = output->dimensions[0];
  const int32_t output_height = output->dimensions[1];
  const int32_t output_width = output->dimensions[2];
  const int32_t output_channel = output->dimensions[3];
  int32_t i = 0;
  const int32_t input_index = inputs[i++];
  const mojom::OperandPtr& input = model->operands[input_index];
  const int32_t input_height = input->dimensions[1];
  const int32_t input_width = input->dimensions[2];

  DLOG(INFO) << "  input_height: " << input_height
             << "  input_width: " << input_width;
  DLOG(INFO) << "  output_batch: " << output_batch
             << "  output_height: " << output_height
             << "  output_width: " << output_width
             << "  output_channel: " << output_channel;

  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom,
      padding_code;
  if (inputs.size() == 10) {
    implicit_padding = false;
    padding_left = compilation_->GetScalarInt32(inputs[i++]);
    padding_right = compilation_->GetScalarInt32(inputs[i++]);
    padding_top = compilation_->GetScalarInt32(inputs[i++]);
    padding_bottom = compilation_->GetScalarInt32(inputs[i++]);
  } else if (inputs.size() == 7) {
    implicit_padding = true;
    padding_code = compilation_->GetScalarInt32(inputs[i++]);
  } else {
    DLOG(ERROR) << "  inputs size is incorrect";
    return mojom::BAD_DATA;
  }
  const int32_t stride_width = compilation_->GetScalarInt32(inputs[i++]);
  const int32_t stride_height = compilation_->GetScalarInt32(inputs[i++]);
  const int32_t filter_width = compilation_->GetScalarInt32(inputs[i++]);
  const int32_t filter_height = compilation_->GetScalarInt32(inputs[i++]);
  const int32_t fuse_code = compilation_->GetScalarInt32(inputs[i++]);

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

  // Setup paddings.
  if (implicit_padding) {
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME, input_width,
                             stride_width, filter_width, padding_left,
                             padding_right);
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME, input_height,
                             stride_height, filter_height, padding_top,
                             padding_bottom);
    DLOG(INFO) << "  padding_left: " << padding_left;
    DLOG(INFO) << "  padding_right: " << padding_right;
    DLOG(INFO) << "  padding_top: " << padding_top;
    DLOG(INFO) << "  padding_bottom: " << padding_bottom;
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddSoftmax(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  // Check beta.
  const float beta = compilation_->GetScalarFloat(inputs[1]);
  DLOG(INFO) << "  beta: " << beta;
  if (beta != 1.0) {
    DLOG(ERROR) << "beta " << beta << " is not supported.";
    return mojom::BAD_DATA;
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddReshape(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddConcatenation(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddFullyConnected(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  // The output tensor, of shape [batch_size, num_units]
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const uint32_t output_index = outputs[0];
  const mojom::OperandPtr& output = model->operands[output_index];
  const int32_t output_batch_size = output->dimensions[0];
  const int32_t output_num_units = output->dimensions[1];

  uint32_t index = 0;
  const uint32_t input_index = inputs[index++];
  const mojom::OperandPtr& input = model->operands[input_index];
  // A tensor of at least rank 2, specifying the input.
  if (input->dimensions.size() < 2) {
    DLOG(ERROR) << "A tenosr of least rank 2.";
    return mojom::BAD_DATA;
  }

  const uint32_t weights_idx = inputs[index++];
  const mojom::OperandPtr& weights = model->operands[weights_idx];
  const uint32_t num_units = weights->dimensions[0];
  const uint32_t input_size = weights->dimensions[1];

  // According to Android NN API doc:
  // If rank is greater than 2, then it gets flattened to a 2-D Tensor.
  // The (flattened) 2-D Tensor is reshaped (if necessary) to
  // [batch_size, input_size], where "input_size" corresponds to the number of
  // inputs to the layer, matching the second dimension of weights, and
  // "batch_size" is calculated by dividing the number of elements by
  // "input_size".
  uint32_t input_batch_size;
  if (input->dimensions.size() > 2) {
    input_batch_size = product(input->dimensions) / input_size;
  } else {
    if (input->dimensions[1] != input_size) {
      DLOG(ERROR) << "input.dimensions[1] (" << input->dimensions[1] << ") "
                  << "!= input_size (" << input_size << ")";
      return mojom::BAD_DATA;
    }
    input_batch_size = input->dimensions[0];
  }

  // A 1-D tensor, of shape [num_units]
  const uint32_t bias_idx = inputs[index++];
  const mojom::OperandPtr& bias = model->operands[bias_idx];
  const uint32_t bias_num_units = bias->dimensions[0];
  if (bias_num_units != num_units) {
    DLOG(ERROR) << "bias_num_units (" << bias_num_units << ") != "
                << "num_units (" << num_units << ")";
    return mojom::BAD_DATA;
  }

  const int32_t fuse_code = compilation_->GetScalarInt32(inputs[index++]);

  DLOG(INFO) << "  input_batch_size: " << input_batch_size;
  DLOG(INFO) << "  num_units: " << num_units;
  DLOG(INFO) << "  input_size: " << input_size;
  DLOG(INFO) << "  bias_num_units: " << bias_num_units;
  DLOG(INFO) << "  output_batch_size: " << output_batch_size;
  DLOG(INFO) << "  output_num_units: " << output_num_units;
  DLOG(INFO) << "  fuse_code: " << fuse_code;

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddResizeBilinear(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const mojom::OperandPtr& input = model->operands[inputs[0]];
  if (input->dimensions.size() != 4) {
    DLOG(ERROR) << "Input must be a 4-D tensor";
    return mojom::BAD_DATA;
  }
  const uint32_t height = input->dimensions[1];
  const uint32_t width = input->dimensions[2];
  const uint32_t channel = input->dimensions[3];
  const uint32_t new_height = compilation_->GetScalarInt32(inputs[1]);
  const uint32_t new_width = compilation_->GetScalarInt32(inputs[2]);
  const float y_scale = new_height / height;
  const float x_scale = new_width / width;
  const uint32_t scale = std::floor(x_scale);

  DLOG(INFO) << "  height: " << height;
  DLOG(INFO) << "  width: " << width;
  DLOG(INFO) << "  channel: " << channel;
  DLOG(INFO) << "  new_height: " << new_height;
  DLOG(INFO) << "  new_width: " << new_width;
  DLOG(INFO) << "  y_scale: " << y_scale;
  DLOG(INFO) << "  x_scale: " << x_scale;
  DLOG(INFO) << "  scale: " << scale;

  return mojom::NOT_ERROR;
}

}  // namespace ml
