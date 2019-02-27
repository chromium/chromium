// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_mkl_dnn.h"

#include <string>
#include <utility>

#include "base/memory/aligned_memory.h"
#include "base/strings/string_number_conversions.h"
#include "services/ml/execution_impl_mkl_dnn.h"
#include "services/ml/mkl_dnn_symbol_table.h"
#include "services/ml/public/mojom/constants.mojom.h"

static const uint32_t ALIGNMENT = 64;

namespace ml {

OperationMklDnn::OperationMklDnn() : primitive(nullptr), type(-1) {}
OperationMklDnn::OperationMklDnn(mkldnn_primitive_t mkldnn_primitive)
    : primitive(mkldnn_primitive), type(-1) {}
OperationMklDnn::OperationMklDnn(const mojom::OperationPtr& op)
    : primitive(nullptr), type(op->type) {
  for (auto index : op->inputs) {
    inputs.push_back(base::NumberToString(index));
  }
  for (auto index : op->outputs) {
    outputs.push_back(base::NumberToString(index));
  }
}
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
        LOG(ERROR) << "[MKLDNN] failed to destroy operation primitive "
                   << status;
      }
      DLOG(INFO) << "[MKLDNN] succeed to destroy operation primitive";
    }
  }
  for (std::map<std::string, mkldnn_primitive_t>::iterator itr =
           memories.begin();
       itr != memories.end(); ++itr) {
    DLOG(INFO) << "To destropy memory primitive for " << itr->first;
    mkldnn_primitive_t primitive = itr->second;
    void* buffer = nullptr;
    status = LATE(mkldnn_memory_get_data_handle)(primitive, &buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to get memory data handle " << status;
    }
    status = LATE(mkldnn_primitive_destroy)(primitive);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to destroy memory primitive " << status;
    } else {
      DLOG(INFO) << "[MKLDNN] succeed to destroy memory primitive";
      if (buffer) {
        base::AlignedFree(buffer);
        DLOG(INFO) << "succeed to free buffer";
      }
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
    : CompilationDelegate(), compilation_(compilation) {}

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
  return std::make_unique<ExecutionImplMklDnn>(std::move(compiled_model_),
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
  status = LATE(mkldnn_engine_create)(&compiled_model_->engine, mkldnn_cpu, 0);
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

    if (operation->outputs.size() != 1) {
      LOG(ERROR) << "Only 1 output is supported";
      return mojom::BAD_DATA;
    }

    int32_t result = mojom::NOT_ERROR;
    if (type == mojom::ADD || type == mojom::MUL) {
      result = MkldnnAddElementwise(operation);
    } else if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D ||
               type == mojom::ATROUS_CONV_2D ||
               type == mojom::ATROUS_DEPTHWISE_CONV_2D) {
      result = MkldnnAddConvolution(operation);
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      result = MkldnnAddPooling(operation);
    } else if (type == mojom::SOFTMAX) {
      result = MkldnnAddSoftmax(operation);
    } else if (type == mojom::RESHAPE) {
      result = MkldnnAddReshape(operation);
    } else if (type == mojom::CONCATENATION) {
      result = MkldnnAddConcatenation(operation);
    } else if (type == mojom::FULLY_CONNECTED) {
      result = MkldnnAddFullyConnected(operation);
    } else if (type == mojom::RESIZE_BILINEAR) {
      result = MkldnnAddResizeBilinear(operation);
    } else {
      LOG(ERROR) << "Operation type " << type << " is not supported.";
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
    std::vector<int32_t>& mkldnn_dims,
    mkldnn_memory_format_t format) {
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
    if (format == mkldnn_hwigo) {
      // for depthwise weights, come in the order: (g, o, i, h, w)
      // WebNN order is ihwo where o is the number of filters
      mkldnn_dims.resize(5);
      mkldnn_dims[0] = dimensions[3];
      mkldnn_dims[1] = 1;
      mkldnn_dims[2] = 1;
      mkldnn_dims[3] = dimensions[1];
      mkldnn_dims[4] = dimensions[2];
    } else {
      // mkldnn logical dimensions come in the order: (n, c, h, w)
      // WebNN order is nhwc
      mkldnn_dims[0] = dimensions[0];
      mkldnn_dims[1] = dimensions[3];
      mkldnn_dims[2] = dimensions[1];
      mkldnn_dims[3] = dimensions[2];
    }
  } else {
    LOG(ERROR) << "Tensor rank " << dimensions.size() << " is not supproted";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnGetDataType(
    int32_t type,
    mkldnn_data_type_t* mkldnn_type) {
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

int32_t CompilationDelegateMklDnn::MkldnnCreateMemoryByQueryType(
    const mkldnn_primitive_desc_t& pd,
    mkldnn_query_t query_type,
    mkldnn_primitive_t& output_memory) {
  const_mkldnn_primitive_desc_t output_pd =
      LATE(mkldnn_primitive_desc_query_pd)(pd, query_type, 0);
  mkldnn_status_t status =
      LATE(mkldnn_primitive_create)(&output_memory, output_pd, NULL, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
    return mojom::OP_FAILED;
  }
  size_t buffer_size = LATE(mkldnn_memory_primitive_desc_get_size)(output_pd);
  void* buffer = base::AlignedAlloc(buffer_size, ALIGNMENT);
  status = LATE(mkldnn_memory_set_data_handle)(output_memory, buffer);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to set data handle to memory primitive "
               << status;
    LATE(mkldnn_primitive_destroy)(output_memory);
    base::AlignedFree(buffer);
    return mojom::OP_FAILED;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnCreateMemoryPrimitveDescriptor(
    int32_t index,
    mkldnn_primitive_desc_t& memory_pd,
    mkldnn_memory_format_t* user_format) {
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
  result = MkldnnGetDims(operand->dimensions, dims, format);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  mkldnn_memory_desc_t memory_desc;
  mkldnn_status_t status = LATE(mkldnn_memory_desc_init)(
      &memory_desc, dims.size(), dims.data(), data_type, format);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  status = LATE(mkldnn_memory_primitive_desc_create)(&memory_pd, &memory_desc,
                                                     compiled_model_->engine);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create memory primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnCreateMemory(
    uint32_t index,
    mkldnn_primitive_t& memory,
    mkldnn_memory_format_t* user_format) {
  mkldnn_primitive_desc_t memory_pd;
  int32_t result =
      MkldnnCreateMemoryPrimitveDescriptor(index, memory_pd, user_format);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  mkldnn_status_t status =
      LATE(mkldnn_primitive_create)(&memory, memory_pd, NULL, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
    status = LATE(mkldnn_primitive_desc_destroy)(memory_pd);
    return mojom::OP_FAILED;
  }
  size_t buffer_size = LATE(mkldnn_memory_primitive_desc_get_size)(memory_pd);
  void* buffer = base::AlignedAlloc(buffer_size, ALIGNMENT);
  status = LATE(mkldnn_memory_set_data_handle)(memory, buffer);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to set memory data " << status;
    base::AlignedFree(buffer);
    status = LATE(mkldnn_primitive_desc_destroy)(memory_pd);
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to set memory data handle with size "
             << buffer_size;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  std::string index_id(base::NumberToString(index));
  if (model->values.find(index_id) != model->values.end()) {
    const mojom::OperandValueInfoPtr& value_info = model->values[index_id];
    auto mapping = compilation_->MapMemory(index);
    memcpy(buffer, mapping.get(), value_info->length);
    DLOG(INFO) << "[MKLDNN] copy user data with size " << value_info->length
               << " to memory primitive buffer with size " << buffer_size;
  }
  status = LATE(mkldnn_primitive_desc_destroy)(memory_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to destroy memory primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for " << index_id;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddInput(uint32_t index) {
  std::string input_id(base::NumberToString(index));
  if (compiled_model_->memories.find(input_id) !=
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is existing";
    return mojom::BAD_DATA;
  }
  mkldnn_primitive_t memory;
  int32_t result = MkldnnCreateMemory(index, memory);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  compiled_model_->memories[input_id] = memory;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddOutput(uint32_t index) {
  std::string output_id(base::NumberToString(index));
  if (compiled_model_->memories.find(output_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Output memory is not ready";
    return mojom::BAD_DATA;
  }
  mkldnn_primitive_t internal_output_memory =
      compiled_model_->memories[output_id];
  const_mkldnn_primitive_desc_t internal_output_pd;
  mkldnn_status_t status = LATE(mkldnn_primitive_get_primitive_desc)(
      internal_output_memory, &internal_output_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t output_pd;
  int32_t result = MkldnnCreateMemoryPrimitveDescriptor(index, output_pd);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  if (!LATE(mkldnn_memory_primitive_desc_equal)(output_pd,
                                                internal_output_pd)) {
    DLOG(INFO) << "Reorder internal output to output";
    mkldnn_primitive_t reordered_memory;
    result = MkldnnCreateMemory(index, reordered_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(output_pd);
      return result;
    }
    // Put the reordered output as model output.
    std::string internal_output_id = output_id + "-internal";
    compiled_model_->memories[internal_output_id] = internal_output_memory;
    compiled_model_->memories[output_id] = reordered_memory;
    result = MkldnnAddReorder(internal_output_id, output_id);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(output_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "No need to reorder internal output to output";
  }
  LATE(mkldnn_primitive_desc_destroy)(output_pd);
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
  mkldnn_primitive_t input = compiled_model_->memories[input_name];
  const_mkldnn_primitive_desc_t input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(input, &input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t output = compiled_model_->memories[output_name];
  const_mkldnn_primitive_desc_t output_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(output, &output_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t reorder_pd;
  status = LATE(mkldnn_reorder_primitive_desc_create)(&reorder_pd, input_pd,
                                                      output_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] falied to create reorder primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_at_t inputs = {input, 0};
  const_mkldnn_primitive_t outputs[] = {output};
  mkldnn_primitive_t reorder;
  status =
      LATE(mkldnn_primitive_create)(&reorder, reorder_pd, &inputs, outputs);
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
    status = LATE(mkldnn_primitive_destroy)(reorder);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to destroy reorder primitive " << status;
      return mojom::OP_FAILED;
    }
    status = LATE(mkldnn_stream_destroy)(stream);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to destroy stream " << status;
      return mojom::OP_FAILED;
    }
    // Release memory primitive and buffer.
    void* buffer = nullptr;
    status = LATE(mkldnn_memory_get_data_handle)(input, &buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to get memory data handle " << status;
      return mojom::OP_FAILED;
    }
    status = LATE(mkldnn_primitive_destroy)(input);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to destroy memory primitive " << status;
      return mojom::OP_FAILED;
    }
    if (buffer)
      base::AlignedFree(buffer);
    compiled_model_->memories.erase(input_name);
    DLOG(INFO) << "[MKLDNN] succeed to destroy primitive for " << input_name;
  } else {
    OperationMklDnn operation(reorder);
    compiled_model_->operations.push_back(operation);
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddFusedActivation(
    const std::string& input_name,
    const std::string& output_name,
    int32_t fuse_code) {
  if (compiled_model_->memories.find(input_name) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  mkldnn_status_t status;
  mkldnn_primitive_t input_memory = compiled_model_->memories[input_name];
  const_mkldnn_primitive_desc_t input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(input_memory, &input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  const mkldnn_memory_desc_t* input_md =
      LATE(mkldnn_primitive_desc_query_memory_d)(input_pd);
  mkldnn_eltwise_desc_t activation_desc;
  mkldnn_alg_kind_t relu_kind;
  float alpha = 0.0;
  if (fuse_code == mojom::FUSED_RELU) {
    relu_kind = mkldnn_eltwise_relu;
  } else if (fuse_code == mojom::FUSED_RELU1) {
    relu_kind = mkldnn_eltwise_bounded_relu;
    alpha = 1.0;
  } else if (fuse_code == mojom::FUSED_RELU6) {
    relu_kind = mkldnn_eltwise_bounded_relu;
    alpha = 6.0;
  } else {
    LOG(ERROR) << "Fuse code " << fuse_code << " is not supported";
    return mojom::BAD_DATA;
  }
  status = LATE(mkldnn_eltwise_forward_desc_init)(
      &activation_desc, mkldnn_forward, relu_kind, input_md, alpha, 0.0);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init eltwise descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t activation_pd;
  status = LATE(mkldnn_primitive_desc_create)(&activation_pd, &activation_desc,
                                              compiled_model_->engine, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t output_memory;
  int32_t result = MkldnnCreateMemoryByQueryType(
      activation_pd, mkldnn_query_dst_pd, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(activation_pd);
    return result;
  }
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitive for "
             << output_name;
  compiled_model_->memories[output_name] = output_memory;

  mkldnn_primitive_t activation;
  mkldnn_primitive_at_t inputs[] = {LATE(mkldnn_primitive_at)(input_memory, 0)};
  const_mkldnn_primitive_t outputs[] = {output_memory};

  status = LATE(mkldnn_primitive_create)(&activation, activation_pd, inputs,
                                         outputs);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to set data handle to memory primitive "
               << status;
    LATE(mkldnn_primitive_desc_destroy)(activation_pd);
    return mojom::OP_FAILED;
  }
  LATE(mkldnn_primitive_desc_destroy)(activation_pd);

  OperationMklDnn operation(activation);
  compiled_model_->operations.push_back(operation);

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddElementwise(
    const mojom::OperationPtr& operation) {
  if (operation->type != mojom::ADD) {
    LOG(ERROR) << "Operation type " << operation->type << " is not supported.";
    return mojom::BAD_DATA;
  }
  ElementWiseParams params;
  int32_t result = compilation_->GetElementWiseParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  if (operation->inputs.size() != 3) {
    LOG(ERROR) << "The number of inputs is not 3";
    return mojom::BAD_DATA;
  }
  mkldnn_status_t status;
  std::vector<float> scales;
  std::vector<const_mkldnn_primitive_desc_t> input_pds;
  std::vector<mkldnn_primitive_t> input_memories;
  for (size_t index = 0; index < 2; ++index) {
    std::string input_id(base::NumberToString(operation->inputs[index]));
    if (compiled_model_->memories.find(input_id) ==
        compiled_model_->memories.end()) {
      // Setup constants
      const mojom::ModelInfoPtr& model = compilation_->GetModel();
      if (model->values.find(input_id) != model->values.end()) {
        mkldnn_primitive_t constant_memory;
        result = MkldnnCreateMemory(operation->inputs[index], constant_memory);
        if (result != mojom::NOT_ERROR) {
          return result;
        }
        compiled_model_->memories[input_id] = constant_memory;
      }
    }

    mkldnn_primitive_t input_memory = compiled_model_->memories[input_id];
    input_memories.push_back(input_memory);
    const_mkldnn_primitive_desc_t input_pd;
    status = LATE(mkldnn_primitive_get_primitive_desc)(input_memory, &input_pd);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
      return mojom::OP_FAILED;
    }
    scales.push_back(1.0);
    input_pds.push_back(input_pd);
  }

  mkldnn_primitive_desc_t sum_pd;
  status = LATE(mkldnn_sum_primitive_desc_create)(
      &sum_pd, NULL, input_pds.size(), scales.data(), input_pds.data());
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create sum primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }

  DLOG(INFO) << "Add output memory";
  mkldnn_primitive_t output_memory;
  result =
      MkldnnCreateMemoryByQueryType(sum_pd, mkldnn_query_dst_pd, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(sum_pd);
    return result;
  }
  std::string output_id(base::NumberToString(operation->outputs[0]));
  std::string sum_output_id(output_id);
  if (params.fuse_code != mojom::FUSED_NONE) {
    // Need to add activation primitive layer.
    // The output of sum becomes pre-activation
    sum_output_id += std::string("-pre-activation");
  }
  compiled_model_->memories[sum_output_id] = output_memory;
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitive for "
             << sum_output_id;

  mkldnn_primitive_at_t srcs[] = {
      LATE(mkldnn_primitive_at)(input_memories[0], 0),
      LATE(mkldnn_primitive_at)(input_memories[1], 0)};
  const_mkldnn_primitive_t dsts[] = {output_memory};

  mkldnn_primitive_t sum;
  status = LATE(mkldnn_primitive_create)(&sum, sum_pd, srcs, dsts);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create sum primitive " << status;
    LATE(mkldnn_primitive_desc_destroy)(sum_pd);
    return mojom::OP_FAILED;
  }
  LATE(mkldnn_primitive_desc_destroy)(sum_pd);

  OperationMklDnn mkldnn_operation(sum);
  compiled_model_->operations.push_back(mkldnn_operation);

  if (params.fuse_code != mojom::FUSED_NONE) {
    // Append an activation primitive that uses sum's output memory as input
    // and output is named as output_id.
    result =
        MkldnnAddFusedActivation(sum_output_id, output_id, params.fuse_code);
    if (result != mojom::NOT_ERROR)
      return result;
  }

  DLOG(INFO) << "[MKLDNN] succeed to create sum primitive";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddConvolution(
    const mojom::OperationPtr& operation) {
  ConvParams params;
  int32_t result = compilation_->GetConvParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  if (params.depthwise && params.depthwise_multiplier != 1) {
    LOG(ERROR) << "depthwise_multiplier " << params.depthwise_multiplier
               << " is not supported";
    return mojom::BAD_DATA;
  }
  mkldnn_status_t status;
  mkldnn_memory_desc_t input_desc;
  // Input logical order is nchw
  int input_dims[4] = {params.input_batch, params.input_channel,
                       params.input_height, params.input_width};
  status = LATE(mkldnn_memory_desc_init)(&input_desc, 4, input_dims, mkldnn_f32,
                                         mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t weights_desc;
  if (params.depthwise) {
    // Weights logical order is (g, o, i, h, w)
    int weights_dims[5] = {params.depth_out, 1, 1, params.filter_height,
                           params.filter_width};
    status = LATE(mkldnn_memory_desc_init)(&weights_desc, 5, weights_dims,
                                           mkldnn_f32, mkldnn_any);
  } else {
    // Weights logical order is oihw
    int weights_dims[4] = {params.depth_out, params.depth_in,
                           params.filter_height, params.filter_width};
    status = LATE(mkldnn_memory_desc_init)(&weights_desc, 4, weights_dims,
                                           mkldnn_f32, mkldnn_any);
  }
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t bias_desc;
  int bias_dims[1] = {params.bias_length};
  status = LATE(mkldnn_memory_desc_init)(&bias_desc, 1, bias_dims, mkldnn_f32,
                                         mkldnn_x);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t output_desc;
  // Output logical order is nchw
  int output_dims[4] = {params.output_batch, params.output_channel,
                        params.output_height, params.output_width};
  status = LATE(mkldnn_memory_desc_init)(&output_desc, 4, output_dims,
                                         mkldnn_f32, mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }

  mkldnn_convolution_desc_t conv_desc;
  int strides[2], dilates[2];
  if (params.atrous) {
    // MKLDNN dilation starts from 0.
    dilates[0] = params.dilation_width - 1;
    dilates[1] = params.dilation_height - 1;
    strides[0] = 1;
    strides[1] = 1;
  } else {
    dilates[0] = 0;
    dilates[1] = 0;
    strides[0] = params.stride_width;
    strides[1] = params.stride_height;
  }
  int pad_left[2] = {params.padding_top, params.padding_left};
  int pad_right[2] = {params.padding_bottom, params.padding_right};
  status = LATE(mkldnn_dilated_convolution_forward_desc_init)(
      &conv_desc, mkldnn_forward, mkldnn_convolution_direct, &input_desc,
      &weights_desc, &bias_desc, &output_desc, strides, dilates, pad_left,
      pad_right, mkldnn_padding_zero);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init convolution descriptor " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to init convolution descriptor";

  mkldnn_primitive_desc_t conv_pd;
  if (params.fuse_code == mojom::FUSED_NONE || params.depthwise) {
    status = LATE(mkldnn_primitive_desc_create)(&conv_pd, &conv_desc,
                                                compiled_model_->engine, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR)
          << "[MKLDNN] failed to create convolution primitive descriptor "
          << status;
      return mojom::OP_FAILED;
    }
  } else {
    // mkl-dnn only supports fused activation for normal convolution.
    mkldnn_primitive_attr_t attr;
    status = LATE(mkldnn_primitive_attr_create)(&attr);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create primitive attribute " << status;
      return mojom::OP_FAILED;
    }
    mkldnn_post_ops_t post_ops;
    status = LATE(mkldnn_post_ops_create)(&post_ops);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create post ops " << status;
      return mojom::OP_FAILED;
    }
    if (params.fuse_code == mojom::FUSED_RELU) {
      status = LATE(mkldnn_post_ops_append_eltwise)(post_ops, 1.0,
                                                    mkldnn_eltwise_relu, 0, 0);
    } else if (params.fuse_code == mojom::FUSED_RELU1 ||
               params.fuse_code == mojom::FUSED_RELU6) {
      float uppper_bound = params.fuse_code == mojom::FUSED_RELU1 ? 1.0 : 6.0;
      status = LATE(mkldnn_post_ops_append_eltwise)(
          post_ops, 1.0, mkldnn_eltwise_bounded_relu, uppper_bound, 0);
    } else {
      LOG(ERROR) << "[MKLDNN] fuse code " << params.fuse_code
                 << " is not supproted.";
      LATE(mkldnn_post_ops_destroy)(post_ops);
      LATE(mkldnn_primitive_attr_destroy)(attr);
      return mojom::BAD_DATA;
    }
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to append eltwise to post ops " << status;
      LATE(mkldnn_post_ops_destroy)(post_ops);
      LATE(mkldnn_primitive_attr_destroy)(attr);
      return mojom::OP_FAILED;
    }
    status = LATE(mkldnn_primitive_attr_set_post_ops)(attr, post_ops);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to set post ops to primitive attribute "
                 << status;
      return mojom::OP_FAILED;
    }
    status = LATE(mkldnn_primitive_desc_create_v2)(
        &conv_pd, &conv_desc, attr, compiled_model_->engine, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR)
          << "[MKLDNN] failed to create convolution primitive descriptor "
          << status;
      return mojom::OP_FAILED;
    }
    LATE(mkldnn_post_ops_destroy)(post_ops);
    LATE(mkldnn_primitive_attr_destroy)(attr);
  }

  DLOG(INFO) << "[MKLDNN] succeed to create convolution primitive descriptor";

  DLOG(INFO) << "Add input memory";
  std::string external_input_id(base::NumberToString(operation->inputs[0]));
  if (compiled_model_->memories.find(external_input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    LATE(mkldnn_primitive_desc_destroy)(conv_pd);
    return mojom::BAD_DATA;
  }
  mkldnn_primitive_t external_input_memory =
      compiled_model_->memories[external_input_id];
  const_mkldnn_primitive_desc_t external_input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(external_input_memory,
                                                     &external_input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    LATE(mkldnn_primitive_desc_destroy)(conv_pd);
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t input_memory;
  const_mkldnn_primitive_desc_t input_pd =
      LATE(mkldnn_primitive_desc_query_pd)(conv_pd, mkldnn_query_src_pd, 0);
  if (!LATE(mkldnn_memory_primitive_desc_equal)(input_pd, external_input_pd)) {
    DLOG(INFO) << "Reorder external input to input";
    status = LATE(mkldnn_primitive_create)(&input_memory, input_pd, NULL, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
      LATE(mkldnn_primitive_desc_destroy)(conv_pd);
      return mojom::OP_FAILED;
    }
    size_t input_size = LATE(mkldnn_memory_primitive_desc_get_size)(input_pd);
    void* input_buffer = base::AlignedAlloc(input_size, ALIGNMENT);
    status = LATE(mkldnn_memory_set_data_handle)(input_memory, input_buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to set data handle " << status;
      LATE(mkldnn_primitive_desc_destroy)(conv_pd);
      LATE(mkldnn_primitive_destroy)(input_memory);
      base::AlignedFree(input_buffer);
      return mojom::OP_FAILED;
    }
    DLOG(INFO) << "[MKLDNN] succeed to set memory data handle with size "
               << input_size;
    std::string input_id = external_input_id + "-reordered";
    compiled_model_->memories[input_id] = input_memory;
    DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for " << input_id;
    result = MkldnnAddReorder(external_input_id, input_id);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(conv_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse exteranl input as input";
    input_memory = external_input_memory;
  }

  DLOG(INFO) << "Add weights memory";
  // Use mkldnn_nhwc as weights format, since there is no mkldnn_ohwi.
  // Please refer to https://github.com/intel/mkl-dnn/issues/156
  mkldnn_memory_format_t weights_format;
  if (params.depthwise) {
    weights_format = mkldnn_hwigo;
  } else {
    weights_format = mkldnn_nhwc;
  }
  mkldnn_primitive_t external_weights_memory;
  std::string external_weights_id = base::NumberToString(operation->inputs[1]);
  result = MkldnnCreateMemory(operation->inputs[1], external_weights_memory,
                              &weights_format);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(conv_pd);
    return result;
  }
  compiled_model_->memories[external_weights_id] = external_weights_memory;
  const_mkldnn_primitive_desc_t external_weights_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(external_weights_memory,
                                                     &external_weights_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    LATE(mkldnn_primitive_desc_destroy)(conv_pd);
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t weights_memory;
  const_mkldnn_primitive_desc_t weights_pd =
      LATE(mkldnn_primitive_desc_query_pd)(conv_pd, mkldnn_query_weights_pd, 0);
  if (!LATE(mkldnn_memory_primitive_desc_equal)(weights_pd,
                                                external_weights_pd)) {
    DLOG(INFO) << "Reorder external weigths to weights";
    result = MkldnnCreateMemoryByQueryType(conv_pd, mkldnn_query_weights_pd,
                                           weights_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(conv_pd);
      return result;
    }
    std::string weights_id = external_weights_id + "-reordered";
    compiled_model_->memories[weights_id] = weights_memory;
    DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for "
               << weights_id;
    result = MkldnnAddReorder(external_weights_id, weights_id, true);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(conv_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse external weights as weights";
    weights_memory = external_weights_memory;
  }

  DLOG(INFO) << "Add bias memory";
  mkldnn_primitive_t bias_memory;
  std::string bias_id(base::NumberToString(operation->inputs[2]));
  result = MkldnnCreateMemory(operation->inputs[2], bias_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(conv_pd);
    return result;
  }
  compiled_model_->memories[bias_id] = bias_memory;

  DLOG(INFO) << "Add output memory";
  mkldnn_primitive_t output_memory;
  result = MkldnnCreateMemoryByQueryType(conv_pd, mkldnn_query_dst_pd,
                                         output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(conv_pd);
    return result;
  }
  std::string output_id(base::NumberToString(operation->outputs[0]));
  std::string conv_output_id(output_id);
  if (params.depthwise && params.fuse_code != mojom::FUSED_NONE) {
    // Need to add activation primitive layer.
    // The output of conv becomes pre-activation
    conv_output_id += std::string("-pre-activation");
  }
  compiled_model_->memories[conv_output_id] = output_memory;
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitive for "
             << conv_output_id;

  mkldnn_primitive_at_t conv_srcs[] = {
      LATE(mkldnn_primitive_at)(input_memory, 0),
      LATE(mkldnn_primitive_at)(weights_memory, 0),
      LATE(mkldnn_primitive_at)(bias_memory, 0)};
  const_mkldnn_primitive_t conv_dsts[] = {output_memory};

  mkldnn_primitive_t conv;
  status = LATE(mkldnn_primitive_create)(&conv, conv_pd, conv_srcs, conv_dsts);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create convolution primitive " << status;
    LATE(mkldnn_primitive_desc_destroy)(conv_pd);
    return mojom::OP_FAILED;
  }
  LATE(mkldnn_primitive_desc_destroy)(conv_pd);

  OperationMklDnn mkldnn_operation(conv);
  compiled_model_->operations.push_back(mkldnn_operation);

  DLOG(INFO) << "[MKLDNN] succeed to create convolution primitive";

  if (params.depthwise && params.fuse_code != mojom::FUSED_NONE) {
    // Append an activation primitive that uses conv's output memory as input
    // and output is named as output_id.
    result =
        MkldnnAddFusedActivation(conv_output_id, output_id, params.fuse_code);
    if (result != mojom::NOT_ERROR)
      return result;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddPooling(
    const mojom::OperationPtr& operation) {
  PoolingParams params;
  int32_t result = compilation_->GetPoolingParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  mkldnn_status_t status;
  std::string input_id(base::NumberToString(operation->inputs[0]));
  if (compiled_model_->memories.find(input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  mkldnn_primitive_t input_memory = compiled_model_->memories[input_id];
  const_mkldnn_primitive_desc_t input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(input_memory, &input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  const mkldnn_memory_desc_t* input_md =
      LATE(mkldnn_primitive_desc_query_memory_d)(input_pd);
  mkldnn_memory_desc_t output_md;
  int output_dims[4] = {params.output_batch, params.output_channel,
                        params.output_height, params.output_width};
  status = LATE(mkldnn_memory_desc_init)(&output_md, 4, output_dims, mkldnn_f32,
                                         mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init output memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_pooling_desc_t pool_desc;
  mkldnn_alg_kind_t pooling_kind;
  if (operation->type == mojom::MAX_POOL_2D) {
    pooling_kind = mkldnn_pooling_max;
  } else if (operation->type == mojom::AVERAGE_POOL_2D) {
    pooling_kind = mkldnn_pooling_avg;
  } else {
    LOG(ERROR) << "Pooling mode " << operation->type << " is not supported";
    return mojom::BAD_DATA;
  }
  int kernel[2] = {params.filter_width, params.filter_height};
  int strides[2] = {params.stride_width, params.stride_height};
  int pad_left[2] = {params.padding_top, params.padding_left};
  int pad_right[2] = {params.padding_bottom, params.padding_right};
  status = LATE(mkldnn_pooling_forward_desc_init)(
      &pool_desc, mkldnn_forward, pooling_kind, input_md, &output_md, strides,
      kernel, pad_left, pad_right, mkldnn_padding_zero);
  if (status != mojom::NOT_ERROR) {
    LOG(ERROR) << "[MKLDNN] failed to init pooling descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t pool_pd;
  status = LATE(mkldnn_primitive_desc_create)(&pool_pd, &pool_desc,
                                              compiled_model_->engine, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create pooling primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }

  mkldnn_primitive_t pool_indices_memory = nullptr;
  if (operation->type == mojom::MAX_POOL_2D) {
    DLOG(INFO) << "Add working space";
    result = MkldnnCreateMemoryByQueryType(pool_pd, mkldnn_query_workspace_pd,
                                           pool_indices_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(pool_pd);
      return result;
    }
    std::string working_space_id(base::NumberToString(operation->outputs[0]) +
                                 std::string("-working-space"));
    compiled_model_->memories[working_space_id] = pool_indices_memory;
    DLOG(INFO) << "[MKLDNN] succeed to create memory primitive for "
               << working_space_id;
  }

  DLOG(INFO) << "Add output memory";
  mkldnn_primitive_t output_memory;
  result = MkldnnCreateMemoryByQueryType(pool_pd, mkldnn_query_dst_pd,
                                         output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(pool_pd);
    return result;
  }
  std::string output_id(base::NumberToString(operation->outputs[0]));
  std::string pool_output_id(output_id);
  if (params.fuse_code != mojom::FUSED_NONE) {
    // Need to add activation primitive layer.
    // The output of pool becomes pre-activation
    pool_output_id += std::string("-pre-activation");
  }
  compiled_model_->memories[pool_output_id] = output_memory;
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitive for "
             << pool_output_id;

  mkldnn_primitive_at_t pool_srcs[] = {
      LATE(mkldnn_primitive_at)(input_memory, 0)};
  const_mkldnn_primitive_t pool_dsts[] = {output_memory, pool_indices_memory};

  mkldnn_primitive_t pool;
  status = LATE(mkldnn_primitive_create)(&pool, pool_pd, pool_srcs, pool_dsts);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create pooling primitive " << status;
    LATE(mkldnn_primitive_desc_destroy)(pool_pd);
    return mojom::OP_FAILED;
  }
  LATE(mkldnn_primitive_desc_destroy)(pool_pd);

  OperationMklDnn mkldnn_operation(pool);
  compiled_model_->operations.push_back(mkldnn_operation);

  DLOG(INFO) << "[MKLDNN] succeed to create pooling primitive";

  if (params.fuse_code != mojom::FUSED_NONE) {
    // Append an activation primitive that uses pool's output memory as input
    // and output is named as output_id.
    result =
        MkldnnAddFusedActivation(pool_output_id, output_id, params.fuse_code);
    if (result != mojom::NOT_ERROR)
      return result;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddSoftmax(
    const mojom::OperationPtr& operation) {
  SoftmaxParams params;
  int32_t result = compilation_->GetSoftmaxParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  // Check beta.
  if (params.beta != 1.0) {
    LOG(ERROR) << "beta " << params.beta << " is not supported.";
    return mojom::BAD_DATA;
  }

  std::string input_id(base::NumberToString(operation->inputs[0]));
  if (compiled_model_->memories.find(input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  mkldnn_status_t status;
  mkldnn_primitive_t input_memory = compiled_model_->memories[input_id];
  const_mkldnn_primitive_desc_t input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(input_memory, &input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  const mkldnn_memory_desc_t* input_md =
      LATE(mkldnn_primitive_desc_query_memory_d)(input_pd);
  mkldnn_softmax_desc_t softmax_desc;
  status = LATE(mkldnn_softmax_forward_desc_init)(
      &softmax_desc, mkldnn_forward, input_md, input_md->ndims - 1);
  if (status != mojom::NOT_ERROR) {
    LOG(ERROR) << "[MKLDNN] failed to init softmax descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t softmax_pd;
  status = LATE(mkldnn_primitive_desc_create)(&softmax_pd, &softmax_desc,
                                              compiled_model_->engine, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create softmax primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }

  DLOG(INFO) << "Add output memory";
  mkldnn_primitive_t output_memory;
  result = MkldnnCreateMemoryByQueryType(softmax_pd, mkldnn_query_dst_pd,
                                         output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(softmax_pd);
    return result;
  }
  std::string output_id(base::NumberToString(operation->outputs[0]));
  compiled_model_->memories[output_id] = output_memory;
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitive for " << output_id;

  mkldnn_primitive_at_t srcs[] = {LATE(mkldnn_primitive_at)(input_memory, 0)};
  const_mkldnn_primitive_t dsts[] = {output_memory};

  mkldnn_primitive_t softmax;
  status = LATE(mkldnn_primitive_create)(&softmax, softmax_pd, srcs, dsts);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create softmax primitive " << status;
    LATE(mkldnn_primitive_desc_destroy)(softmax_pd);
    return mojom::OP_FAILED;
  }
  LATE(mkldnn_primitive_desc_destroy)(softmax_pd);

  OperationMklDnn mkldnn_operation(softmax);
  compiled_model_->operations.push_back(mkldnn_operation);

  DLOG(INFO) << "[MKLDNN] succeed to create softmax primitive";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddReshape(
    const mojom::OperationPtr& operation) {
  const uint32_t input_index = operation->inputs[0];
  std::string input_id(base::NumberToString(input_index));
  if (compiled_model_->memories.find(input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  mkldnn_status_t status;
  mkldnn_primitive_t input_memory = compiled_model_->memories[input_id];
  const_mkldnn_primitive_desc_t input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(input_memory, &input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_desc_t reshape_input_pd;
  int32_t result =
      MkldnnCreateMemoryPrimitveDescriptor(input_index, reshape_input_pd);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  if (!LATE(mkldnn_memory_primitive_desc_equal)(input_pd, reshape_input_pd)) {
    DLOG(INFO) << "Reorder input to reshape_input";
    mkldnn_primitive_t reshape_input_memory;
    result = MkldnnCreateMemory(input_index, reshape_input_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(reshape_input_pd);
      return result;
    }
    std::string prereorder_input_id = input_id + "-pre-reorder";
    compiled_model_->memories[prereorder_input_id] = input_memory;
    compiled_model_->memories[input_id] = reshape_input_memory;
    result = MkldnnAddReorder(prereorder_input_id, input_id);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(reshape_input_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "No need to reorder input to reshape_input";
  }
  LATE(mkldnn_primitive_desc_destroy)(reshape_input_pd);

  mkldnn_primitive_t output_memory;
  const uint32_t output_index = operation->outputs[0];
  std::string output_id(base::NumberToString(output_index));
  result = MkldnnCreateMemory(output_index, output_memory);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  compiled_model_->memories[output_id] = output_memory;

  OperationMklDnn mkldnn_operation(operation);
  compiled_model_->operations.push_back(mkldnn_operation);
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddConcatenation(
    const mojom::OperationPtr& operation) {
  LOG(ERROR) << "Operation type " << operation->type << " is not supported.";
  return mojom::BAD_DATA;
}

int32_t CompilationDelegateMklDnn::MkldnnAddFullyConnected(
    const mojom::OperationPtr& operation) {
  FullyConnectedParams params;
  int32_t result = compilation_->GetFullyConnectedParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  mkldnn_status_t status;
  mkldnn_memory_desc_t ip_input_desc;
  // Input logical order is nc
  int ip_input_dims[2] = {params.input_batch_size, params.input_size};
  status = LATE(mkldnn_memory_desc_init)(&ip_input_desc, 2, ip_input_dims,
                                         mkldnn_f32, mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t weights_desc;
  // Weights logical order is oi
  int weights_dims[2] = {params.num_units, params.input_size};
  status = LATE(mkldnn_memory_desc_init)(&weights_desc, 2, weights_dims,
                                         mkldnn_f32, mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t bias_desc;
  int bias_dims[1] = {params.bias_num_units};
  status = LATE(mkldnn_memory_desc_init)(&bias_desc, 1, bias_dims, mkldnn_f32,
                                         mkldnn_x);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  mkldnn_memory_desc_t output_desc;
  // Output logical order is nc
  int output_dims[2] = {params.output_batch_size, params.output_num_units};
  status = LATE(mkldnn_memory_desc_init)(&output_desc, 2, output_dims,
                                         mkldnn_f32, mkldnn_any);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }

  mkldnn_inner_product_desc_t ip_desc;
  status = LATE(mkldnn_inner_product_forward_desc_init)(
      &ip_desc, mkldnn_forward, &ip_input_desc, &weights_desc, &bias_desc,
      &output_desc);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to init inner product descriptor " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[MKLDNN] succeed to init inner product descriptor";

  mkldnn_primitive_desc_t ip_pd;
  status = LATE(mkldnn_primitive_desc_create)(&ip_pd, &ip_desc,
                                              compiled_model_->engine, NULL);
  if (status != mkldnn_success) {
    LOG(ERROR)
        << "[MKLDNN] failed to create inner product primitive descriptor "
        << status;
    return mojom::OP_FAILED;
  }

  DLOG(INFO) << "[MKLDNN] succeed to create inner product primitive descriptor";

  DLOG(INFO) << "Add input memory";
  const uint32_t input_index = operation->inputs[0];
  std::string input_id(base::NumberToString(input_index));
  if (compiled_model_->memories.find(input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    LATE(mkldnn_primitive_desc_destroy)(ip_pd);
    return mojom::BAD_DATA;
  }
  mkldnn_primitive_t external_input_memory =
      compiled_model_->memories[input_id];
  const_mkldnn_primitive_desc_t external_input_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(external_input_memory,
                                                     &external_input_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    LATE(mkldnn_primitive_desc_destroy)(ip_pd);
    return mojom::OP_FAILED;
  }
  // Check whether we need to reshape the external input to 2D
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const mojom::OperandPtr& input = model->operands[operation->inputs[0]];
  if (input->dimensions.size() > 2) {
    // Reshape the input to 2D tensor
    DLOG(INFO) << "Reshape input rank " << input->dimensions.size() << " to 2D";
    mkldnn_primitive_desc_t input_pd;
    result = MkldnnCreateMemoryPrimitveDescriptor(input_index, input_pd);
    if (!LATE(mkldnn_memory_primitive_desc_equal)(external_input_pd,
                                                  input_pd)) {
      DLOG(INFO) << "Reorder exteranl_input to input";
      mkldnn_primitive_t reordered_input_memory;
      std::string reordered_input_id = input_id + "-reorder";
      result = MkldnnCreateMemory(input_index, reordered_input_memory);
      if (result != mojom::NOT_ERROR) {
        LATE(mkldnn_primitive_desc_destroy)(input_pd);
        LATE(mkldnn_primitive_desc_destroy)(ip_pd);
        return result;
      }
      compiled_model_->memories[reordered_input_id] = reordered_input_memory;
      result = MkldnnAddReorder(input_id, reordered_input_id);
      if (result != mojom::NOT_ERROR) {
        LATE(mkldnn_primitive_desc_destroy)(input_pd);
        LATE(mkldnn_primitive_desc_destroy)(ip_pd);
        return result;
      }
      input_id = reordered_input_id;
    } else {
      DLOG(INFO) << "No need to reorder external input to input";
    }
    LATE(mkldnn_primitive_desc_destroy)(input_pd);

    // Create the reshaped input memory
    mkldnn_memory_desc_t reshaped_input_desc;
    status = LATE(mkldnn_memory_desc_init)(
        &reshaped_input_desc, 2, ip_input_dims, mkldnn_f32, mkldnn_nc);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to init memory descriptor " << status;
      return mojom::OP_FAILED;
    }
    mkldnn_primitive_desc_t reshaped_input_pd;
    status = LATE(mkldnn_memory_primitive_desc_create)(
        &reshaped_input_pd, &reshaped_input_desc, compiled_model_->engine);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create memory primitive descriptor "
                 << status;
      return mojom::OP_FAILED;
    }
    mkldnn_primitive_t reshaped_input_memory;
    status = LATE(mkldnn_primitive_create)(&reshaped_input_memory,
                                           reshaped_input_pd, NULL, NULL);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to create memory primitive " << status;
      LATE(mkldnn_primitive_desc_destroy)(reshaped_input_pd);
      LATE(mkldnn_primitive_desc_destroy)(ip_pd);
      return mojom::OP_FAILED;
    }
    size_t buffer_size =
        LATE(mkldnn_memory_primitive_desc_get_size)(reshaped_input_pd);
    void* buffer = base::AlignedAlloc(buffer_size, ALIGNMENT);
    status = LATE(mkldnn_memory_set_data_handle)(reshaped_input_memory, buffer);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to set memory data " << status;
      base::AlignedFree(buffer);
      LATE(mkldnn_primitive_desc_destroy)(reshaped_input_pd);
      LATE(mkldnn_primitive_desc_destroy)(ip_pd);
      return mojom::OP_FAILED;
    }
    DLOG(INFO) << "[MKLDNN] succeed to set memory data handle with size "
               << buffer_size;
    std::string reshaped_input_id = input_id + "-reshape";
    compiled_model_->memories[reshaped_input_id] = reshaped_input_memory;
    status = LATE(mkldnn_primitive_desc_destroy)(reshaped_input_pd);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to destroy memory primitive descriptor "
                 << status;
      LATE(mkldnn_primitive_desc_destroy)(ip_pd);
      return mojom::OP_FAILED;
    }

    OperationMklDnn reshape;
    reshape.type = mojom::RESHAPE;
    reshape.inputs.push_back(input_id);
    reshape.outputs.push_back(reshaped_input_id);
    compiled_model_->operations.push_back(reshape);

    // Use reshaped input memory as external input.
    external_input_memory = reshaped_input_memory;
    status = LATE(mkldnn_primitive_get_primitive_desc)(reshaped_input_memory,
                                                       &external_input_pd);
    if (status != mkldnn_success) {
      LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
      LATE(mkldnn_primitive_desc_destroy)(ip_pd);
      return mojom::OP_FAILED;
    }
  } else {
    DLOG(INFO) << "No need to reshape to 2D tensor";
  }

  mkldnn_primitive_t ip_input_memory;
  const_mkldnn_primitive_desc_t ip_input_pd =
      LATE(mkldnn_primitive_desc_query_pd)(ip_pd, mkldnn_query_src_pd, 0);
  if (!LATE(mkldnn_memory_primitive_desc_equal)(ip_input_pd,
                                                external_input_pd)) {
    DLOG(INFO) << "Reorder external input to input";
    result = MkldnnCreateMemoryByQueryType(ip_pd, mkldnn_query_src_pd,
                                           ip_input_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(ip_pd);
      return result;
    }
    std::string ip_input_id = input_id + "-internal";
    compiled_model_->memories[ip_input_id] = ip_input_memory;
    DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for " << input_id;
    result = MkldnnAddReorder(input_id, ip_input_id);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(ip_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse exteranl input as input";
    ip_input_memory = external_input_memory;
  }

  DLOG(INFO) << "Add weights memory";
  const uint32_t weights_index = operation->inputs[1];
  std::string external_weights_id = base::NumberToString(weights_index);
  mkldnn_memory_format_t weights_format = mkldnn_oi;
  mkldnn_primitive_t external_weights_memory;
  result = MkldnnCreateMemory(weights_index, external_weights_memory,
                              &weights_format);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(ip_pd);
    return result;
  }
  compiled_model_->memories[external_weights_id] = external_weights_memory;
  const_mkldnn_primitive_desc_t external_weights_pd;
  status = LATE(mkldnn_primitive_get_primitive_desc)(external_weights_memory,
                                                     &external_weights_pd);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to get primitive descriptor " << status;
    LATE(mkldnn_primitive_desc_destroy)(ip_pd);
    return mojom::OP_FAILED;
  }
  mkldnn_primitive_t weights_memory;
  const_mkldnn_primitive_desc_t weights_pd =
      LATE(mkldnn_primitive_desc_query_pd)(ip_pd, mkldnn_query_weights_pd, 0);
  if (!LATE(mkldnn_memory_primitive_desc_equal)(weights_pd,
                                                external_weights_pd)) {
    DLOG(INFO) << "Reorder external weigths to weights";
    result = MkldnnCreateMemoryByQueryType(ip_pd, mkldnn_query_weights_pd,
                                           weights_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(ip_pd);
      return result;
    }
    std::string weights_id = external_weights_id + "-reordered";
    compiled_model_->memories[weights_id] = weights_memory;
    DLOG(INFO) << "[MKLDNN] succeed to create memory primitve for "
               << weights_id;
    result = MkldnnAddReorder(external_weights_id, weights_id, true);
    if (result != mojom::NOT_ERROR) {
      LATE(mkldnn_primitive_desc_destroy)(ip_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse external weights as weights";
    weights_memory = external_weights_memory;
  }

  DLOG(INFO) << "Add bias memory";
  uint32_t bias_index = operation->inputs[2];
  std::string bias_id(base::NumberToString(bias_index));
  mkldnn_primitive_t bias_memory;
  result = MkldnnCreateMemory(bias_index, bias_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(ip_pd);
    return result;
  }
  compiled_model_->memories[bias_id] = bias_memory;

  DLOG(INFO) << "Add output memory";
  mkldnn_primitive_t output_memory;
  result =
      MkldnnCreateMemoryByQueryType(ip_pd, mkldnn_query_dst_pd, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(mkldnn_primitive_desc_destroy)(ip_pd);
    return result;
  }
  std::string output_id(base::NumberToString(operation->outputs[0]));
  std::string ip_output_id(output_id);
  if (params.fuse_code != mojom::FUSED_NONE) {
    // Need to add activation primitive layer.
    // The output of ip becomes pre-activation
    ip_output_id += std::string("-pre-activation");
  }
  compiled_model_->memories[ip_output_id] = output_memory;
  DLOG(INFO) << "[MKLDNN] succeed to create memory primitive for "
             << ip_output_id;

  mkldnn_primitive_at_t ip_srcs[] = {
      LATE(mkldnn_primitive_at)(ip_input_memory, 0),
      LATE(mkldnn_primitive_at)(weights_memory, 0),
      LATE(mkldnn_primitive_at)(bias_memory, 0)};
  const_mkldnn_primitive_t ip_dsts[] = {output_memory};

  mkldnn_primitive_t ip;
  status = LATE(mkldnn_primitive_create)(&ip, ip_pd, ip_srcs, ip_dsts);
  if (status != mkldnn_success) {
    LOG(ERROR) << "[MKLDNN] failed to create inner product primitive "
               << status;
    LATE(mkldnn_primitive_desc_destroy)(ip_pd);
    return mojom::OP_FAILED;
  }
  LATE(mkldnn_primitive_desc_destroy)(ip_pd);

  OperationMklDnn mkldnn_operation(ip);
  compiled_model_->operations.push_back(mkldnn_operation);

  DLOG(INFO) << "[MKLDNN] succeed to create inner product primitive";

  if (params.fuse_code != mojom::FUSED_NONE) {
    // Append an activation primitive that uses ip's output memory as input
    // and output is named as output_id.
    result =
        MkldnnAddFusedActivation(ip_output_id, output_id, params.fuse_code);
    if (result != mojom::NOT_ERROR)
      return result;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMklDnn::MkldnnAddResizeBilinear(
    const mojom::OperationPtr& operation) {
  LOG(ERROR) << "Operation type " << operation->type << " is not supported.";
  return mojom::BAD_DATA;
}

}  // namespace ml
