// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/compilation_delegate_cl_dnn.h"
#if defined(OS_WIN)
#include "services/ml/compilation_delegate_dml.h"
#endif
#include "services/ml/compilation_delegate_ie.h"
#include "services/ml/compilation_delegate_mkl_dnn.h"
#include "services/ml/ml_switches.h"
#include "services/ml/model_impl.h"

namespace ml {

namespace {

inline void CalculateExplicitPadding(bool padding_same,
                                     uint32_t in_size,
                                     uint32_t stride,
                                     uint32_t filter_size,
                                     uint32_t& padding_head,
                                     uint32_t& padding_tail,
                                     uint32_t dilation = 1) {
  padding_head = 0;
  padding_tail = 0;

  if (padding_same) {
    uint32_t out_size = (in_size + stride - 1) / stride;
    uint32_t effective_filter_size = (filter_size - 1) * dilation + 1;
    uint32_t tmp = (out_size - 1) * stride + effective_filter_size;
    if (tmp > in_size) {
      padding_head = (tmp - in_size) / 2;
      padding_tail = (tmp - in_size) - padding_head;
    }
  }
}

}  // namespace

CompilationImpl::CompilationImpl(mojom::ModelInfoPtr model_info)
    : model_info_(std::move(model_info)) {}

CompilationImpl::~CompilationImpl() {}

void CompilationImpl::Finish(int32_t preference, FinishCallback callback) {
  DLOG(INFO) << "CompilationImpl::Finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;
  preference_ = preference;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseInferenceEngine)) {
#if defined(OS_LINUX)
    delegate_ = std::make_unique<CompilationDelegateIe>(this);
#endif
  } else if (preference == mojom::PREFER_SUSTAINED_SPEED) {
    if (command_line->HasSwitch(switches::kUseDirectML)) {
#if defined(OS_WIN)
      delegate_ = std::make_unique<CompilationDelegateDML>(this);
#endif
    } else {
#if defined(OS_WIN) || defined(OS_LINUX)
      delegate_ = std::make_unique<CompilationDelegateClDnn>(this);
#endif  // defined(OS_WIN) || defined(OS_LINUX)
    }
  } else if (preference == mojom::PREFER_FAST_SINGLE_ANSWER) {
    delegate_ = std::make_unique<CompilationDelegateMklDnn>(this);
  } else {
    LOG(ERROR) << "Preference: " << preference << " is not suppoted.";
    std::move(callback).Run(mojom::BAD_DATA);
    return;
  }
  int32_t result = delegate_->Compile();
  std::move(callback).Run(result);
}

void CompilationImpl::CreateExecution(CreateExecutionCallback callback) {
  DLOG(INFO) << "CompilationImpl::CreateExecution";
  auto init_params = mojom::ExecutionInitParams::New();
  auto remote_init_params = mojom::ExecutionInitParams::New();

  uint32_t input_memory_size = 0;
  for (size_t i = 0; i < model_info_->inputs.size(); ++i) {
    const mojom::OperandPtr& operand =
        model_info_->operands[model_info_->inputs[i]];
    input_memory_size += GetRequiredSize(operand);
    init_params->inputs.push_back(mojom::OperandInfo::New(
        model_info_->inputs[i], operand->type, operand->dimensions));
    remote_init_params->inputs.push_back(mojom::OperandInfo::New(
        model_info_->inputs[i], operand->type, operand->dimensions));
  }
  DLOG(INFO) << "Required input memory size: " << input_memory_size;

  uint32_t output_memory_size = 0;
  for (size_t i = 0; i < model_info_->outputs.size(); ++i) {
    const mojom::OperandPtr& operand =
        model_info_->operands[model_info_->outputs[i]];
    output_memory_size += GetRequiredSize(operand);
    init_params->outputs.push_back(mojom::OperandInfo::New(
        model_info_->outputs[i], operand->type, operand->dimensions));
    remote_init_params->outputs.push_back(mojom::OperandInfo::New(
        model_info_->outputs[i], operand->type, operand->dimensions));
  }
  DLOG(INFO) << "Required output memory size: " << output_memory_size;

  uint32_t total_memory_size = input_memory_size + output_memory_size;
  init_params->memory = mojo::SharedBufferHandle::Create(total_memory_size);

  remote_init_params->memory = init_params->memory->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_WRITE);

  mojom::ExecutionPtrInfo ptr_info;
  std::unique_ptr<mojom::Execution> execution;
  int32_t result =
      delegate_->CreateExecution(execution, std::move(init_params));
  if (result != mojom::NOT_ERROR) {
    std::move(callback).Run(result, nullptr);
    return;
  }
  mojo::MakeStrongBinding(std::move(execution), mojo::MakeRequest(&ptr_info));
  remote_init_params->execution = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(remote_init_params));
}

int32_t CompilationImpl::GetScalarInt32(uint32_t index) const {
  const mojom::OperandValueInfoPtr& info =
      model_info_->values[base::NumberToString(index)];
  auto mapping = model_info_->memory->MapAtOffset(info->length, info->offset);
  return reinterpret_cast<int32_t*>(mapping.get())[0];
}

float CompilationImpl::GetScalarFloat(uint32_t index) const {
  const mojom::OperandValueInfoPtr& info =
      model_info_->values[base::NumberToString(index)];
  auto mapping = model_info_->memory->MapAtOffset(info->length, info->offset);
  return reinterpret_cast<float*>(mapping.get())[0];
}

mojo::ScopedSharedBufferMapping CompilationImpl::MapMemory(
    uint32_t index) const {
  const mojom::OperandValueInfoPtr& info =
      model_info_->values[base::NumberToString(index)];
  return model_info_->memory->MapAtOffset(info->length, info->offset);
}

int32_t CompilationImpl::GetElementWiseParams(
    const mojom::OperationPtr& operation,
    ElementWiseParams& params) const {
  const int32_t type = operation->type;
  if (!(type == mojom::ADD || type == mojom::MUL)) {
    LOG(ERROR) << "Operation type " << type << " is not element-wise";
    return mojom::BAD_DATA;
  }
  params.fuse_code = GetScalarInt32(operation->inputs[2]);
  return mojom::NOT_ERROR;
}

int32_t CompilationImpl::GetConvParams(const mojom::OperationPtr& operation,
                                       ConvParams& params) const {
  const int32_t type = operation->type;
  if (!(type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D ||
        type == mojom::ATROUS_CONV_2D ||
        type == mojom::ATROUS_DEPTHWISE_CONV_2D)) {
    LOG(ERROR) << "Operation type " << type << " is not convolution";
    return mojom::BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation->inputs;
  const std::vector<uint32_t>& outputs = operation->outputs;
  params.depthwise = (type == mojom::DEPTHWISE_CONV_2D ||
                      type == mojom::ATROUS_DEPTHWISE_CONV_2D)
                         ? true
                         : false;
  params.atrous =
      (type == mojom::ATROUS_CONV_2D || type == mojom::ATROUS_DEPTHWISE_CONV_2D)
          ? true
          : false;
  const uint32_t output_index = outputs[0];
  const mojom::OperandPtr& output = model_info_->operands[output_index];
  params.output_batch = output->dimensions[0];
  params.output_height = output->dimensions[1];
  params.output_width = output->dimensions[2];
  params.output_channel = output->dimensions[3];
  uint32_t index = 0;
  const uint32_t input_index = inputs[index++];
  const mojom::OperandPtr& input = model_info_->operands[input_index];
  params.input_batch = input->dimensions[0];
  params.input_height = input->dimensions[1];
  params.input_width = input->dimensions[2];
  params.input_channel = input->dimensions[3];

  const uint32_t filter_idx = inputs[index++];
  mojom::OperandPtr& filter = model_info_->operands[filter_idx];
  if (params.depthwise) {
    params.depth_out = filter->dimensions[3];
  } else {
    params.depth_out = filter->dimensions[0];
    params.depth_in = filter->dimensions[3];
  }
  params.filter_height = filter->dimensions[1];
  params.filter_width = filter->dimensions[2];

  const uint32_t bias_idx = inputs[index++];
  mojom::OperandPtr& bias = model_info_->operands[bias_idx];
  params.bias_length = bias->dimensions[0];

  bool implicit_padding;
  int32_t padding_code;
  if ((!params.depthwise && inputs.size() == 10) ||
      (params.depthwise && inputs.size() == 11)) {
    implicit_padding = false;
    params.padding_left = GetScalarInt32(inputs[index++]);
    params.padding_right = GetScalarInt32(inputs[index++]);
    params.padding_top = GetScalarInt32(inputs[index++]);
    params.padding_bottom = GetScalarInt32(inputs[index++]);
  } else if ((!params.depthwise && inputs.size() == 7) ||
             (params.depthwise && inputs.size() == 8)) {
    implicit_padding = true;
    padding_code = GetScalarInt32(inputs[index++]);
  } else {
    LOG(ERROR) << "Inputs size is incorrect";
    return mojom::BAD_DATA;
  }
  if (!params.atrous) {
    params.stride_width = GetScalarInt32(inputs[index++]);
    params.stride_height = GetScalarInt32(inputs[index++]);
    params.dilation_width = 1;
    params.dilation_height = 1;
  } else {
    params.dilation_width = GetScalarInt32(inputs[index++]);
    params.dilation_height = GetScalarInt32(inputs[index++]);
    params.stride_width = 1;
    params.stride_height = 1;
  }
  if (params.depthwise) {
    params.depthwise_multiplier = GetScalarInt32(inputs[index++]);
    params.depth_in = params.depth_out / params.depthwise_multiplier;
  }
  params.fuse_code = GetScalarInt32(inputs[index++]);

  DLOG(INFO) << "  input_batch: " << params.input_batch;
  DLOG(INFO) << "  input_height: " << params.input_height;
  DLOG(INFO) << "  input_width: " << params.input_width;
  DLOG(INFO) << "  input_channel: " << params.input_channel;
  DLOG(INFO) << "  output_batch: " << params.output_batch;
  DLOG(INFO) << "  output_height: " << params.output_height;
  DLOG(INFO) << "  output_width: " << params.output_width;
  DLOG(INFO) << "  output_channel: " << params.output_channel;
  DLOG(INFO) << "  filter_height: " << params.filter_height;
  DLOG(INFO) << "  filter_width: " << params.filter_width;
  DLOG(INFO) << "  bias_length: " << params.bias_length;
  DLOG(INFO) << "  depth_in: " << params.depth_in;
  DLOG(INFO) << "  depth_out: " << params.depth_out;
  DLOG(INFO) << "  implicit_padding: " << implicit_padding;
  if (implicit_padding) {
    DLOG(INFO) << "  padding_code: " << padding_code;
  } else {
    DLOG(INFO) << "  padding_left: " << params.padding_left;
    DLOG(INFO) << "  padding_right: " << params.padding_right;
    DLOG(INFO) << "  padding_top: " << params.padding_top;
    DLOG(INFO) << "  padding_bottom: " << params.padding_bottom;
  }
  DLOG(INFO) << "  stride_width: " << params.stride_width;
  DLOG(INFO) << "  stride_height: " << params.stride_height;
  DLOG(INFO) << "  dilation_width: " << params.dilation_width;
  DLOG(INFO) << "  dilation_height: " << params.dilation_height;
  if (params.depthwise) {
    DLOG(INFO) << "  depthwise_multiplier: " << params.depthwise_multiplier;
  }
  DLOG(INFO) << "  fuse_code: " << params.fuse_code;

  if (implicit_padding) {
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME,
                             params.input_width, params.stride_width,
                             params.filter_width, params.padding_left,
                             params.padding_right, params.dilation_width);
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME,
                             params.input_height, params.stride_height,
                             params.filter_height, params.padding_top,
                             params.padding_bottom, params.dilation_height);
    DLOG(INFO) << "  padding_left: " << params.padding_left;
    DLOG(INFO) << "  padding_right: " << params.padding_right;
    DLOG(INFO) << "  padding_top: " << params.padding_top;
    DLOG(INFO) << "  padding_bottom: " << params.padding_bottom;
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationImpl::GetPoolingParams(const mojom::OperationPtr& operation,
                                          PoolingParams& params) const {
  const int32_t type = operation->type;
  if (!(type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D)) {
    LOG(ERROR) << "Operation type " << type << " is not pooling";
    return mojom::BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation->inputs;
  const std::vector<uint32_t>& outputs = operation->outputs;
  const uint32_t output_index = outputs[0];
  const mojom::OperandPtr& output = model_info_->operands[output_index];
  params.output_batch = output->dimensions[0];
  params.output_height = output->dimensions[1];
  params.output_width = output->dimensions[2];
  params.output_channel = output->dimensions[3];
  int32_t i = 0;
  const int32_t input_index = inputs[i++];
  const mojom::OperandPtr& input = model_info_->operands[input_index];
  params.input_batch = input->dimensions[0];
  params.input_height = input->dimensions[1];
  params.input_width = input->dimensions[2];
  params.input_channel = input->dimensions[3];

  DLOG(INFO) << "  input_batch: " << params.input_batch
             << "  input_height: " << params.input_height
             << "  input_width: " << params.input_width
             << "  input_channel: " << params.input_channel;
  DLOG(INFO) << "  output_batch: " << params.output_batch
             << "  output_height: " << params.output_height
             << "  output_width: " << params.output_width
             << "  output_channel: " << params.output_channel;

  bool implicit_padding;
  int32_t padding_code;
  if (inputs.size() == 10) {
    implicit_padding = false;
    params.padding_left = GetScalarInt32(inputs[i++]);
    params.padding_right = GetScalarInt32(inputs[i++]);
    params.padding_top = GetScalarInt32(inputs[i++]);
    params.padding_bottom = GetScalarInt32(inputs[i++]);
  } else if (inputs.size() == 7) {
    implicit_padding = true;
    padding_code = GetScalarInt32(inputs[i++]);
  } else {
    LOG(ERROR) << "  inputs size is incorrect";
    return mojom::BAD_DATA;
  }
  params.stride_width = GetScalarInt32(inputs[i++]);
  params.stride_height = GetScalarInt32(inputs[i++]);
  params.filter_width = GetScalarInt32(inputs[i++]);
  params.filter_height = GetScalarInt32(inputs[i++]);
  params.fuse_code = GetScalarInt32(inputs[i++]);

  DLOG(INFO) << "  implicit_padding: " << implicit_padding;
  if (implicit_padding) {
    DLOG(INFO) << "  padding_code: " << padding_code;
  } else {
    DLOG(INFO) << "  padding_left: " << params.padding_left;
    DLOG(INFO) << "  padding_right: " << params.padding_right;
    DLOG(INFO) << "  padding_top: " << params.padding_top;
    DLOG(INFO) << "  padding_bottom: " << params.padding_bottom;
  }
  DLOG(INFO) << "  stride_width: " << params.stride_width;
  DLOG(INFO) << "  stride_height: " << params.stride_height;
  DLOG(INFO) << "  filter_height: " << params.filter_height;
  DLOG(INFO) << "  filter_width: " << params.filter_width;
  DLOG(INFO) << "  fuse_code: " << params.fuse_code;

  // Setup paddings.
  if (implicit_padding) {
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME,
                             params.input_width, params.stride_width,
                             params.filter_width, params.padding_left,
                             params.padding_right);
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME,
                             params.input_height, params.stride_height,
                             params.filter_height, params.padding_top,
                             params.padding_bottom);
    DLOG(INFO) << "  padding_left: " << params.padding_left;
    DLOG(INFO) << "  padding_right: " << params.padding_right;
    DLOG(INFO) << "  padding_top: " << params.padding_top;
    DLOG(INFO) << "  padding_bottom: " << params.padding_bottom;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationImpl::GetSoftmaxParams(const mojom::OperationPtr& operation,
                                          SoftmaxParams& params) const {
  const int32_t type = operation->type;
  if (type != mojom::SOFTMAX) {
    LOG(ERROR) << "Operation type " << type << " is not softmax";
    return mojom::BAD_DATA;
  }
  params.beta = GetScalarFloat(operation->inputs[1]);
  return mojom::NOT_ERROR;
}

int32_t CompilationImpl::GetConcatParams(const mojom::OperationPtr& operation,
                                         ConcatParams& params) const {
  const int32_t type = operation->type;
  if (type != mojom::CONCATENATION) {
    LOG(ERROR) << "Operation type " << type << " is not concatenation";
    return mojom::BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation->inputs;
  params.axis = GetScalarInt32(inputs[inputs.size() - 1]);
  return mojom::NOT_ERROR;
}

int32_t CompilationImpl::GetFullyConnectedParams(
    const mojom::OperationPtr& operation,
    FullyConnectedParams& params) const {
  const int32_t type = operation->type;
  if (type != mojom::FULLY_CONNECTED) {
    LOG(ERROR) << "Operation type " << type << " is not fully connected";
    return mojom::BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation->inputs;
  const std::vector<uint32_t>& outputs = operation->outputs;
  // The output tensor, of shape [batch_size, num_units]
  const mojom::ModelInfoPtr& model = GetModel();
  const uint32_t output_index = outputs[0];
  const mojom::OperandPtr& output = model_info_->operands[output_index];
  params.output_batch_size = output->dimensions[0];
  params.output_num_units = output->dimensions[1];

  uint32_t index = 0;
  const uint32_t input_index = inputs[index++];
  const mojom::OperandPtr& input = model_info_->operands[input_index];
  // A tensor of at least rank 2, specifying the input.
  if (input->dimensions.size() < 2) {
    LOG(ERROR) << "Input tenosr must have least rank 2.";
    return mojom::BAD_DATA;
  }

  const uint32_t weights_idx = inputs[index++];
  const mojom::OperandPtr& weights = model->operands[weights_idx];
  params.num_units = weights->dimensions[0];
  params.input_size = weights->dimensions[1];

  // According to Android NN API doc:
  // If rank is greater than 2, then it gets flattened to a 2-D Tensor.
  // The (flattened) 2-D Tensor is reshaped (if necessary) to
  // [batch_size, input_size], where "input_size" corresponds to the number of
  // inputs to the layer, matching the second dimension of weights, and
  // "batch_size" is calculated by dividing the number of elements by
  // "input_size".
  if (input->dimensions.size() > 2) {
    params.input_batch_size = product(input->dimensions) / params.input_size;
  } else {
    if (input->dimensions[1] != params.input_size) {
      LOG(ERROR) << "input.dimensions[1] (" << input->dimensions[1] << ") "
                 << "!= input_size (" << params.input_size << ")";
      return mojom::BAD_DATA;
    }
    params.input_batch_size = input->dimensions[0];
  }

  // A 1-D tensor, of shape [num_units]
  const uint32_t bias_idx = inputs[index++];
  const mojom::OperandPtr& bias = model->operands[bias_idx];
  params.bias_num_units = bias->dimensions[0];
  if (params.bias_num_units != params.num_units) {
    LOG(ERROR) << "bias_num_units (" << params.bias_num_units << ") != "
               << "num_units (" << params.num_units << ")";
    return mojom::BAD_DATA;
  }

  params.fuse_code = GetScalarInt32(inputs[index++]);

  DLOG(INFO) << "  input_batch_size: " << params.input_batch_size;
  DLOG(INFO) << "  num_units: " << params.num_units;
  DLOG(INFO) << "  input_size: " << params.input_size;
  DLOG(INFO) << "  bias_num_units: " << params.bias_num_units;
  DLOG(INFO) << "  output_batch_size: " << params.output_batch_size;
  DLOG(INFO) << "  output_num_units: " << params.output_num_units;
  DLOG(INFO) << "  fuse_code: " << params.fuse_code;
  return mojom::NOT_ERROR;
}

int32_t CompilationImpl::GetResizeBilinearParams(
    const mojom::OperationPtr& operation,
    ResizeBilinearParams& params) const {
  const int32_t type = operation->type;
  if (operation->type != mojom::RESIZE_BILINEAR) {
    LOG(ERROR) << "Operation type " << type << " is not resize bilinear";
    return mojom::BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation->inputs;
  if (inputs.size() != 3 && inputs.size() != 4) {
    LOG(ERROR) << "Inputs size is wrong " << inputs.size();
    return mojom::BAD_DATA;
  }
  const mojom::OperandPtr& input = model_info_->operands[inputs[0]];
  if (input->dimensions.size() != 4) {
    LOG(ERROR) << "Input must be a 4-D tensor";
    return mojom::BAD_DATA;
  }
  params.height = input->dimensions[1];
  params.width = input->dimensions[2];
  params.new_height = GetScalarInt32(inputs[1]);
  params.new_width = GetScalarInt32(inputs[2]);
  params.y_scale = params.new_height / params.height;
  params.x_scale = params.new_width / params.width;

  params.align_corners = false;
  if (inputs.size() == 4) {
    params.align_corners = GetScalarInt32(inputs[3]) == 0 ? false : true;
  }

  DLOG(INFO) << "  height: " << params.height;
  DLOG(INFO) << "  width: " << params.width;
  DLOG(INFO) << "  new_height: " << params.new_height;
  DLOG(INFO) << "  new_width: " << params.new_width;
  DLOG(INFO) << "  y_scale: " << params.y_scale;
  DLOG(INFO) << "  x_scale: " << params.x_scale;
  DLOG(INFO) << "  align_corners: " << params.align_corners;
  return mojom::NOT_ERROR;
}

}  // namespace ml
