// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_mkl_dnn.h"

#include <string>
#include <utility>

#include "base/memory/aligned_memory.h"
#include "base/strings/string_number_conversions.h"
#include "services/ml/cl_dnn_custom_kernels.h"
#include "services/ml/execution_impl_mkl_dnn.h"
#include "services/ml/public/interfaces/constants.mojom.h"

static const uint32_t ALIGNMENT = 64;

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

OperationMklDnn::OperationMklDnn(mkldnn_primitive_t mkldnn_primitive)
    : primitive(mkldnn_primitive),
      type (-1) {}
OperationMklDnn::~OperationMklDnn() = default;
OperationMklDnn::OperationMklDnn(const OperationMklDnn& rhs) {
  primitive = rhs.primitive;
  type = rhs.type;
  inputs = rhs.inputs;
  outputs = rhs.outputs;
};

CompiledModelMklDnn::CompiledModelMklDnn() {}
CompiledModelMklDnn::~CompiledModelMklDnn() {
  mkldnn_status_t status;
  for (std::vector<OperationMklDnn>::iterator itr = operations.begin();
       itr != operations.end(); ++itr) {
    if (itr->primitive) {
      status = LATE(mkldnn_primitive_destroy)(itr->primitive);
      if (status != mkldnn_success) {
        LOG(ERROR) << "[MKLDNN] failed to destroy operation primitive " << status;
      }
      DLOG(INFO) << "[MKLDNN] succeed to destroy operation primitive";
    }
  }
  for (std::map<std::string, MemoryMklDnn>::iterator itr = memories.begin();
       itr != memories.end(); ++itr) {
    status = LATE(mkldnn_primitive_destroy)(itr->second.primitive);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to destroy memory primitive " << status;
    }
    DLOG(INFO) << "[MKLDNN] succeed to destroy memory primitive";
    if (itr->second.buffer) {
      base::AlignedFree(itr->second.buffer);
      DLOG(INFO) << "succeed to free buffer";
    }
  }
  if (engine) {
    status = LATE(mkldnn_engine_destroy)(engine);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to destory engine " << status;
    }
    DLOG(INFO) << "[MKLDNN] succeed to destory engine";
  }
}

CompilationDelegateMklDnn::CompilationDelegateMklDnn(
    const CompilationImpl* compilation)
    : CompilationDelegate(),
      compilation_(compilation) {}

CompilationDelegateMklDnn::~CompilationDelegateMklDnn() {}

int32_t CompilationDelegateMklDnn::Compile() {
  DLOG(INFO) << "CompilationDelegateMklDnn::Compile";

  int32_t result = MkldnnInit();
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  result = MkldnnCompile();
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  DLOG(INFO) << "CompilationDelegateMklDnn::Compile succeeds";
  return mojom::NOT_ERROR;
}

std::unique_ptr<mojom::Execution> CompilationDelegateMklDnn::CreateExecution(
    mojom::ExecutionInitParamsPtr params) {
  return std::make_unique<ExecutionImplMklDnn>(
      std::move(compiled_model_),
      std::move(params));
}

int32_t CompilationDelegateMklDnn::MkldnnInit() {
#if defined(OS_LINUX)
  if (!GetMklDnnSymbolTable()->Load()) {
    LOG(ERROR) << "[MKLDNN] failed to load MKLDNN library";
    return mojom::OP_FAILED;
  }
#endif

  compiled_model_.reset(new CompiledModelMklDnn());

  mkldnn_status_t status;
  status = LATE(mkldnn_engine_create)
      (&compiled_model_->engine, mkldnn_cpu, 0);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create engine " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to create engine " << compiled_model_->engine;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnCompile() {
  int32_t result;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  for (size_t i = 0; i < model->inputs.size(); ++i) {
    result = MkldnnAddInput(model->inputs[i]);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
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

    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  for (size_t i = 0; i < model->outputs.size(); ++i) {
    result = MkldnnAddOutput(model->outputs[i]);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  DLOG(INFO) << "Succeed to compile";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnGetMemoryFormat(
    const std::vector<uint32_t>& dimensions,
    mkldnn_memory_format_t* format) {
  if (dimensions.size() == 1) {
    *format = mkldnn_x;
  } else if (dimensions.size() == 2) {
    *format = mkldnn_nc;
  } else if (dimensions.size() == 3) {
    *format = mkldnn_nwc;
  } else if (dimensions.size() == 4) {
    *format = mkldnn_nhwc;
  } else {
    LOG(ERROR) << "Tensor rank " << dimensions.size() << " is not supproted";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnGetDims(
    const std::vector<uint32_t>& dimensions,
    std::vector<int32_t>& mkldnn_dims) {
  mkldnn_dims.resize(dimensions.size());
  if (dimensions.size() == 1) {
    mkldnn_dims[0] = dimensions[0];
  } else if (dimensions.size() == 2) {
    mkldnn_dims[0] = dimensions[0];
    mkldnn_dims[1] = dimensions[1];
  } else if (dimensions.size() == 3) {
    // mkldnn logical dimensions come in the order: (n, c, w)
    // WebNN order is nwc
    mkldnn_dims[0] = dimensions[0];
    mkldnn_dims[1] = dimensions[2];
    mkldnn_dims[2] = dimensions[1];
  } else if (dimensions.size() == 4) {
    // mkldnn logical dimensions come in the order: (n, c, h, w)
    // WebNN order is nhwc
    mkldnn_dims[0] = dimensions[0];
    mkldnn_dims[1] = dimensions[3];
    mkldnn_dims[2] = dimensions[1];
    mkldnn_dims[3] = dimensions[2];
  } else {
    LOG(ERROR) << "Tensor rank " << dimensions.size() << " is not supproted";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnGetDataType(
    int32_t type, mkldnn_data_type_t* mkldnn_type) {
  if (type == mojom::TENSOR_FLOAT32) {
    *mkldnn_type = mkldnn_f32;
  } else if (type == mojom::TENSOR_INT32) {
    *mkldnn_type = mkldnn_s32;
  } else {
    LOG(ERROR) << "Type " << type << " is not supported";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddMemory(
    uint32_t index, mkldnn_memory_format_t* user_format) {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const mojom::OperandPtr& operand = model->operands[index];
  mkldnn_data_type_t data_type;
  int32_t result = MkldnnGetDataType(operand->type, &data_type);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  mkldnn_memory_format_t format;
  if (!user_format) {
    result = MkldnnGetMemoryFormat(operand->dimensions, &format);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  } else {
    format = *user_format;
  }
  std::vector<int32_t> dims;
  result = MkldnnGetDims(operand->dimensions, dims);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  mkldnn_memory_desc_t memory_desc;
  mkldnn_status_t status = LATE(mkldnn_memory_desc_init)
      (&memory_desc, dims.size(), dims.data(), data_type, format);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t memory_pd;
  status = LATE(mkldnn_memory_primitive_desc_create)
      (&memory_pd, &memory_desc, compiled_model_->engine);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create memory primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t memory;
  status = LATE(mkldnn_primitive_create)
    (&memory, memory_pd, NULL, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
    return mojom::OP_FAILED;
  }
  size_t buffer_size =
        LATE(mkldnn_memory_primitive_desc_get_size)(memory_pd);
  void* buffer = base::AlignedAlloc(buffer_size, ALIGNMENT);
  status = LATE(mkldnn_memory_set_data_handle)(memory, buffer);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to set memory data " << status;
    base::AlignedFree(buffer);
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to set memory data handle with size "
             << buffer_size;
  std::string index_id(base::NumberToString(index));
  if (model->values.find(index_id) != model->values.end()) {
    const mojom::OperandValueInfoPtr& value_info =
        model->values[index_id];
    auto mapping = compilation_->MapMemory(index);
    memcpy(buffer, mapping.get(), value_info->length);
    DLOG(INFO) << "[MKLDNN] copy user data with size " << value_info->length
               << " to memory primitive buffer with size " << buffer_size;
  }
  compiled_model_->memories[index_id] = {memory, buffer};
  status = LATE(mkldnn_primitive_desc_destroy)(memory_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to destroy memory primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for " << index_id;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddInput(uint32_t index) {
  return MkldnnAddMemory(index);
}

int32_t CompilationDelegateMklDnn::MkldnnAddOutput(uint32_t index) {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const mojom::OperandPtr& operand = model->operands[index];
  mkldnn_data_type_t data_type;
  int32_t result = MkldnnGetDataType(operand->type, &data_type);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  mkldnn_memory_format_t format;
  result = MkldnnGetMemoryFormat(operand->dimensions, &format);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  std::vector<int32_t> dims;
  result = MkldnnGetDims(operand->dimensions, dims);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  mkldnn_memory_desc_t output_desc;
  mkldnn_status_t status = LATE(mkldnn_memory_desc_init)
      (&output_desc, dims.size(), dims.data(), data_type, format);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t output_pd;
  status = LATE(mkldnn_memory_primitive_desc_create)
      (&output_pd, &output_desc, compiled_model_->engine);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create memory primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  std::string internal_output_id(base::NumberToString(index));
  if (compiled_model_->memories.find(internal_output_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Output memory is not ready";
    return mojom::BAD_DATA;
  } 
  mkldnn_primitive_t internal_output_memory =
      compiled_model_->memories[internal_output_id].primitive;
  const_mkldnn_primitive_desc_t internal_output_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)
      (internal_output_memory, &internal_output_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  std::string output_id = internal_output_id + "-reordered";
  if (!LATE(mkldnn_memory_primitive_desc_equal)(output_pd, internal_output_pd)) {
    DLOG(INFO) << "Reorder internal output to output";
    mkldnn_primitive_t output_memory;
    status = LATE(mkldnn_primitive_create)
        (&output_memory, output_pd, NULL, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
      LATE(mkldnn_primitive_desc_destroy(output_pd));
      return mojom::OP_FAILED;
    }
    size_t size = LATE(mkldnn_memory_primitive_desc_get_size)(output_pd);
    void* buffer = base::AlignedAlloc(size, ALIGNMENT);
    status = LATE(mkldnn_memory_set_data_handle)(output_memory, buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to set data handle " << status;
      LATE(mkldnn_primitive_desc_destroy(output_pd));
      LATE(mkldnn_primitive_destroy(output_memory));
      base::AlignedFree(buffer);
      return mojom::OP_FAILED;
    }
    DLOG(INFO) << "[MKLDNN] succeed to add memory data handle with size "
               << size;
    compiled_model_->memories[output_id] = {output_memory, buffer};
    result = MkldnnAddReorder(internal_output_id, output_id);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy(output_pd));
      return result;
    }
  } else {
    DLOG(INFO) << "No need to reorder internal output to output";
    compiled_model_->memories[output_id] = {internal_output_memory, nullptr};
  }
  LATE(mkldnn_primitive_desc_destroy(output_pd));
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for " << output_id;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddReorder(
    const std::string& input_name,
    const std::string& output_name,
    bool run) {
  if (compiled_model_->memories.find(input_name) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  if (compiled_model_->memories.find(output_name) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Output memory is not ready";
    return mojom::BAD_DATA;
  }
  mkldnn_status_t status;
  mkldnn_primitive_t input = compiled_model_->memories[input_name].primitive;
  const_mkldnn_primitive_desc_t input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(input, &input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t output = compiled_model_->memories[output_name].primitive;
  const_mkldnn_primitive_desc_t output_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(output, &output_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t reorder_pd;
  status = LATE(mkldnn_reorder_primitive_desc_create)
      (&reorder_pd, input_pd, output_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] falied to create reorder primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_at_t inputs = { input, 0 };
  const_mkldnn_primitive_t outputs[] = { output };
  mkldnn_primitive_t reorder;
  status = LATE(mkldnn_primitive_create)(&reorder, reorder_pd, &inputs, outputs);
  if (status != mkldnn_success) {
    LATE(mkldnn_primitive_desc_destroy)(reorder_pd);
    LOG(ERROR) << "[MKLDNN] failed to create reorder primitive " << status;
  }
  LATE(mkldnn_primitive_desc_destroy)(reorder_pd);
  DLOG(INFO) << "[MKLDNN] succeed to create reorder primitive from "
             << input_name << " to " << output_name;
  if (run) {
    // Execute reorder primitive right now.
    uint32_t n = 1;
    mkldnn_stream_t stream;
    mkldnn_primitive_t net[1] = {reorder};
    status = LATE(mkldnn_stream_create)(&stream, mkldnn_eager);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create stream " << status;
      LATE(mkldnn_primitive_destroy)(reorder);
      return mojom::OP_FAILED;
    }
    status = LATE(mkldnn_stream_submit)(stream, n, net, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to submit stream " << status;
      LATE(mkldnn_primitive_destroy)(reorder);
      LATE(mkldnn_stream_destroy)(stream);
      return mojom::OP_FAILED;
    }
    status = LATE(mkldnn_stream_wait)(stream, n, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to wait stream " << status;
      LATE(mkldnn_primitive_destroy)(reorder);
      LATE(mkldnn_stream_destroy)(stream);
      return mojom::OP_FAILED;
    }
    DLOG(INFO) << "[MKLDNN] succeed to execute reorder primitive";
    LATE(mkldnn_primitive_destroy)(reorder);
    LATE(mkldnn_stream_destroy)(stream);
    // Release memory primitive and buffer.
    void* input_buffer = compiled_model_->memories[input_name].buffer;
    LATE(mkldnn_primitive_destroy)(input);
    base::AlignedFree(input_buffer);
    compiled_model_->memories.erase(input_name);
    DLOG(INFO) << "[MKLDNN] succeed to destroy primitive for " << input_name;
  } else {
    OperationMklDnn operation(reorder);
    compiled_model_->operations.push_back(operation);
  }
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
  const int32_t input_batch = input->dimensions[0];
  const int32_t input_height = input->dimensions[1];
  const int32_t input_width = input->dimensions[2];
  const int32_t input_channel = input->dimensions[3];

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

  DLOG(INFO) << "  input_batch: " << input_batch;
  DLOG(INFO) << "  input_height: " << input_height;
  DLOG(INFO) << "  input_width: " << input_width;
  DLOG(INFO) << "  input_channel: " << input_channel;
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
  int32_t result;
  mkldnn_status_t status;
  mkldnn_memory_desc_t input_desc;
  // Input logical order is nchw
  int input_dims[4] =
      {input_batch, input_channel, input_height, input_width};
  status = LATE(mkldnn_memory_desc_init)
      (&input_desc, 4, input_dims, mkldnn_f32, mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t weights_desc;
  // Weights logical order is oihw
  int weights_dims[4] =
      {depth_out, depth_in, filter_height, filter_width};
  status = LATE(mkldnn_memory_desc_init)
      (&weights_desc, 4, weights_dims, mkldnn_f32, mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t bias_desc;
  int bias_dims[1] ={ bias_length };
  status = LATE(mkldnn_memory_desc_init)
      (&bias_desc, 1, bias_dims, mkldnn_f32, mkldnn_x);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t output_desc;
  // Output logical order is nchw
  int output_dims[4] =
      {output_batch, output_channel, output_height, output_width};
  status = LATE(mkldnn_memory_desc_init)
      (&output_desc, 4, output_dims, mkldnn_f32, mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }

  mkldnn_convolution_desc_t conv_desc;
  int strides[2] = { stride_width, stride_height };
  int pad_left[2] = { padding_top, padding_left };
  int pad_right[2] = { padding_bottom, padding_right };
  status = LATE(mkldnn_convolution_forward_desc_init)
      (&conv_desc, mkldnn_forward, mkldnn_convolution_direct, 
       &input_desc, &weights_desc, &bias_desc, &output_desc,
       strides, pad_left, pad_right, mkldnn_padding_zero);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init convolution descriptor " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to init convolution descriptor";

  mkldnn_primitive_desc_t conv_pd;
  status = LATE(mkldnn_primitive_desc_create)
      (&conv_pd, &conv_desc, compiled_model_->engine, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create convolution primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to create convolution primitive descriptor";

  DLOG(INFO) << "Add input memory";
  std::string external_input_id(base::NumberToString(input_index));
  if (compiled_model_->memories.find(external_input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    LATE(mkldnn_primitive_desc_destroy(conv_pd));
    return mojom::BAD_DATA;
  } 
  mkldnn_primitive_t external_input_memory =
      compiled_model_->memories[external_input_id].primitive;
  const_mkldnn_primitive_desc_t external_input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)
      (external_input_memory, &external_input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    LATE(mkldnn_primitive_desc_destroy(conv_pd));
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t input_memory;
  const_mkldnn_primitive_desc_t input_pd =
      LATE(mkldnn_primitive_desc_query_pd)(conv_pd, mkldnn_query_src_pd, 0);
  if (!LATE(mkldnn_memory_primitive_desc_equal)(input_pd, external_input_pd)) {
    DLOG(INFO) << "Reorder external input to input";
    status = LATE(mkldnn_primitive_create)
        (&input_memory, input_pd, NULL, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
      LATE(mkldnn_primitive_desc_destroy(conv_pd));
      return mojom::OP_FAILED;
    }
    size_t input_size = LATE(mkldnn_memory_primitive_desc_get_size)(input_pd);
    void* input_buffer = base::AlignedAlloc(input_size, ALIGNMENT);
    status = LATE(mkldnn_memory_set_data_handle)(input_memory, input_buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to set data handle " << status;
      LATE(mkldnn_primitive_desc_destroy(conv_pd));
      LATE(mkldnn_primitive_destroy(input_memory));
      base::AlignedFree(input_buffer);
      return mojom::OP_FAILED;
    }
    DLOG(INFO) << "[MKLDNN] succeed to set memory data handle with size "
               << input_size;
    std::string input_id = external_input_id + "-reordered";
    compiled_model_->memories[input_id] = {input_memory, input_buffer};
    DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for " << input_id;
    result = MkldnnAddReorder(external_input_id, input_id);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy(conv_pd));
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse exteranl input as input";
    input_memory = external_input_memory;
  }

  DLOG(INFO) << "Add weights memory";
  // Use mkldnn_nhwc as weights format, since there is no mkldnn_ohwi.
  // Please refer to https://github.com/intel/mkl-dnn/issues/156
  mkldnn_memory_format_t weights_format = mkldnn_nhwc;
  result = MkldnnAddMemory(filter_idx, &weights_format);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy(conv_pd));
    return result;
  }
  std::string external_weights_id = base::NumberToString(filter_idx);
  mkldnn_primitive_t external_weights_memory =
      compiled_model_->memories[external_weights_id].primitive;
  const_mkldnn_primitive_desc_t external_weights_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)
      (external_weights_memory, &external_weights_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    LATE(mkldnn_primitive_desc_destroy(conv_pd));
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t weights_memory;
  const_mkldnn_primitive_desc_t weights_pd =
      LATE(mkldnn_primitive_desc_query_pd)(conv_pd, mkldnn_query_weights_pd, 0);
  if (!LATE(mkldnn_memory_primitive_desc_equal)(
      weights_pd, external_weights_pd)) {
    DLOG(INFO) << "Reorder external weigths to weights";
    status = LATE(mkldnn_primitive_create)
        (&weights_memory, weights_pd, NULL, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
      return mojom::OP_FAILED;
    }
    size_t weights_size =
        LATE(mkldnn_memory_primitive_desc_get_size)(weights_pd);
    void* weights_buffer = base::AlignedAlloc(weights_size, ALIGNMENT);
    status = LATE(mkldnn_memory_set_data_handle)
        (weights_memory, weights_buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to set data handle " << status;
      LATE(mkldnn_primitive_desc_destroy(conv_pd));
      LATE(mkldnn_primitive_destroy(weights_memory));
      base::AlignedFree(weights_buffer);
      return mojom::OP_FAILED;
    }
    DLOG(INFO) << "[MKLDNN] succeed to set data handle with size " << weights_size;
    std::string weights_id = external_weights_id + "-reordered";
    compiled_model_->memories[weights_id] = {weights_memory, weights_buffer};
    DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for "
               << weights_id;
    result = MkldnnAddReorder(external_weights_id, weights_id, false);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy(conv_pd));
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse external weights as weights";
    weights_memory = external_weights_memory;
  }

  DLOG(INFO) << "Add bias memory";
  result = MkldnnAddMemory(bias_idx);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy(conv_pd));
    return result;
  }
  mkldnn_primitive_t bias_memory =
      compiled_model_->memories[base::NumberToString(bias_idx)].primitive;

  DLOG(INFO) << "Add output memory";
  mkldnn_primitive_t output_memory;
  const_mkldnn_primitive_desc_t output_pd =
      LATE(mkldnn_primitive_desc_query_pd)(conv_pd, mkldnn_query_dst_pd, 0);
  status = LATE(mkldnn_primitive_create)
      (&output_memory, output_pd, NULL, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
    return mojom::OP_FAILED;
  }
  size_t output_size = LATE(mkldnn_memory_primitive_desc_get_size)(output_pd);
  void *output_buffer = base::AlignedAlloc(output_size, ALIGNMENT);
  status = LATE(mkldnn_memory_set_data_handle)(output_memory, output_buffer);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to set data handle " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to set data handle with size " << output_size;
  std::string output_id(base::NumberToString(output_index));
  compiled_model_->memories[output_id] =
      {output_memory, output_buffer};
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitive for " << output_id;

  mkldnn_primitive_at_t conv_srcs[] = {
    mkldnn_primitive_at(input_memory, 0),
    mkldnn_primitive_at(weights_memory, 0),
    mkldnn_primitive_at(bias_memory, 0)
  };
  const_mkldnn_primitive_t conv_dsts[] = { output_memory };

  mkldnn_primitive_t conv;
  status = LATE(mkldnn_primitive_create)(&conv, conv_pd, conv_srcs, conv_dsts);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create convolution primitive " << status;
    return mojom::OP_FAILED;
  }
  OperationMklDnn operation(conv);
  compiled_model_->operations.push_back(operation);

  DLOG(INFO) << "[MKLDNN] succeed to create convolution primitive";
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
