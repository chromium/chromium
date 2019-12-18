// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_dnnl.h"

#include <string>
#include <utility>

#include "base/memory/aligned_memory.h"
#include "base/strings/string_number_conversions.h"
#include "services/ml/dnnl_symbol_table.h"
#include "services/ml/execution_impl_dnnl.h"
#include "services/ml/public/mojom/constants.mojom.h"

static const uint32_t ALIGNMENT = 64;

namespace ml {
OperationDnnl::OperationDnnl() : primitive(nullptr), type(-1) {}
OperationDnnl::OperationDnnl(dnnl_primitive_t dnnl_primitive)
    : primitive(dnnl_primitive), type(-1) {}
OperationDnnl::OperationDnnl(const mojom::OperationPtr& op)
    : primitive(nullptr), type(op->type) {
  for (auto index : op->inputs) {
    inputs.push_back(base::NumberToString(index));
  }
  for (auto index : op->outputs) {
    outputs.push_back(base::NumberToString(index));
  }
}
OperationDnnl::~OperationDnnl() = default;
OperationDnnl::OperationDnnl(const OperationDnnl& rhs) {
  primitive = rhs.primitive;
  primitive_args = rhs.primitive_args;
  type = rhs.type;
  inputs = rhs.inputs;
  outputs = rhs.outputs;
}

CompiledModelDnnl::CompiledModelDnnl() {}
CompiledModelDnnl::~CompiledModelDnnl() {
  dnnl_status_t status;
  for (std::vector<OperationDnnl>::iterator itr = operations.begin();
       itr != operations.end(); ++itr) {
    if (itr->primitive) {
      status = LATE(dnnl_primitive_destroy)(itr->primitive);
      if (status != dnnl_success) {
        LOG(ERROR) << "[DNNL] failed to destroy operation primitive " << status;
      }
      DLOG(INFO) << "[DNNL] succeed to destroy operation primitive";
    }
  }
  for (std::map<std::string, dnnl_memory_t>::iterator itr = memories.begin();
       itr != memories.end(); ++itr) {
    DLOG(INFO) << "To destropy memory primitive for " << itr->first;
    dnnl_memory_t primitive = itr->second;
    void* buffer = nullptr;
    status = LATE(dnnl_memory_get_data_handle)(primitive, &buffer);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to get memory data handle " << status;
    }
    status = LATE(dnnl_memory_destroy)(primitive);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to destroy memory primitive " << status;
    } else {
      DLOG(INFO) << "[DNNL] succeed to destroy memory primitive";
      if (buffer) {
        base::AlignedFree(buffer);
        DLOG(INFO) << "succeed to free buffer";
      }
    }
  }
  if (engine) {
    status = LATE(dnnl_engine_destroy)(engine);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to destory engine " << status;
    }
    DLOG(INFO) << "[DNNL] succeed to destory engine";
  }
}

CompilationDelegateDnnl::CompilationDelegateDnnl(
    const CompilationImpl* compilation)
    : CompilationDelegate(), compilation_(compilation) {}

CompilationDelegateDnnl::~CompilationDelegateDnnl() {}

int32_t CompilationDelegateDnnl::Compile() {
  DLOG(INFO) << "CompilationDelegateDnnl::Compile";

  int32_t result = DnnlInit();
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  result = DnnlCompile();
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  DLOG(INFO) << "CompilationDelegateDnnl::Compile succeeds";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::CreateExecution(
    std::unique_ptr<mojom::Execution>& execution,
    mojom::ExecutionInitParamsPtr params) {
  execution = std::make_unique<ExecutionImplDnnl>(std::move(compiled_model_),
                                                  std::move(params));
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::DnnlInit() {
#if defined(OS_LINUX) || defined(OS_MACOSX)
  if (!GetDnnlSymbolTable()->Load()) {
    LOG(ERROR) << "[DNNL] failed to load DNNL library";
    return mojom::OP_FAILED;
  }
#endif

  compiled_model_.reset(new CompiledModelDnnl());

  dnnl_status_t status;
  status = LATE(dnnl_engine_create)(&compiled_model_->engine, dnnl_cpu, 0);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create engine " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[DNNL] succeed to create engine " << compiled_model_->engine;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::DnnlCompile() {
  int32_t result;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  for (size_t i = 0; i < model->inputs.size(); ++i) {
    result = AddInput(model->inputs[i]);
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
      result = AddElementwise(operation);
    } else if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D ||
               type == mojom::ATROUS_CONV_2D ||
               type == mojom::ATROUS_DEPTHWISE_CONV_2D) {
      result = AddConvolution(operation);
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      result = AddPooling(operation);
    } else if (type == mojom::SOFTMAX) {
      result = AddSoftmax(operation);
    } else if (type == mojom::LOGISTIC) {
      result = AddLogistic(operation);
    } else if (type == mojom::RESHAPE) {
      result = AddReshape(operation);
    } else if (type == mojom::CONCATENATION) {
      result = AddConcatenation(operation);
    } else if (type == mojom::FULLY_CONNECTED) {
      result = AddFullyConnected(operation);
    } else if (type == mojom::RESIZE_BILINEAR) {
      result = AddResizeBilinear(operation);
    } else {
      LOG(ERROR) << "Operation type " << type << " is not supported.";
      return mojom::BAD_DATA;
    }

    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  for (size_t i = 0; i < model->outputs.size(); ++i) {
    result = AddOutput(model->outputs[i]);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  DLOG(INFO) << "Succeed to compile";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::GetMemoryFormat(
    const std::vector<uint32_t>& dimensions,
    dnnl_format_tag_t* format) {
  if (dimensions.size() == 1) {
    *format = dnnl_x;
  } else if (dimensions.size() == 2) {
    *format = dnnl_nc;
  } else if (dimensions.size() == 3) {
    *format = dnnl_nwc;
  } else if (dimensions.size() == 4) {
    *format = dnnl_nhwc;
  } else {
    LOG(ERROR) << "Tensor rank " << dimensions.size() << " is not supproted";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::GetDims(
    const std::vector<uint32_t>& dimensions,
    std::vector<dnnl_dim_t>& dnnl_dims,
    dnnl_format_tag_t format) {
  dnnl_dims.resize(dimensions.size());
  if (dimensions.size() == 1) {
    dnnl_dims[0] = dimensions[0];
  } else if (dimensions.size() == 2) {
    dnnl_dims[0] = dimensions[0];
    dnnl_dims[1] = dimensions[1];
  } else if (dimensions.size() == 3) {
    // dnnl logical dimensions come in the order: (n, c, w)
    // WebNN order is nwc
    dnnl_dims[0] = dimensions[0];
    dnnl_dims[1] = dimensions[2];
    dnnl_dims[2] = dimensions[1];
  } else if (dimensions.size() == 4) {
    if (format == dnnl_hwigo) {
      // for depthwise weights, come in the order: (g, o, i, h, w)
      // WebNN order is ihwo where o is the number of filters
      dnnl_dims.resize(5);
      dnnl_dims[0] = dimensions[3];
      dnnl_dims[1] = 1;
      dnnl_dims[2] = 1;
      dnnl_dims[3] = dimensions[1];
      dnnl_dims[4] = dimensions[2];
    } else {
      // dnnl logical dimensions come in the order: (n, c, h, w)
      // WebNN order is nhwc
      dnnl_dims[0] = dimensions[0];
      dnnl_dims[1] = dimensions[3];
      dnnl_dims[2] = dimensions[1];
      dnnl_dims[3] = dimensions[2];
    }
  } else {
    LOG(ERROR) << "Tensor rank " << dimensions.size() << " is not supproted";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::GetDataType(int32_t type,
                                             dnnl_data_type_t* dnnl_type) {
  if (type == mojom::TENSOR_FLOAT32) {
    *dnnl_type = dnnl_f32;
  } else if (type == mojom::TENSOR_INT32) {
    *dnnl_type = dnnl_s32;
  } else {
    LOG(ERROR) << "Type " << type << " is not supported";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::CreateMemoryByQueryType(
    const dnnl_primitive_desc_t& pd,
    dnnl_query_t query_type,
    dnnl_memory_t& output_memory) {
  const dnnl_memory_desc_t* output_desc =
      LATE(dnnl_primitive_desc_query_md)(pd, query_type, 0);
  dnnl_status_t status = LATE(dnnl_memory_create)(
      &output_memory, output_desc, compiled_model_->engine, NULL);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create memory " << status;
    return mojom::OP_FAILED;
  }
  size_t buffer_size = LATE(dnnl_memory_desc_get_size)(output_desc);
  void* buffer = base::AlignedAlloc(buffer_size, ALIGNMENT);
  status = LATE(dnnl_memory_set_data_handle)(output_memory, buffer);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to set data handle to memory primitive "
               << status;
    LATE(dnnl_memory_destroy)(output_memory);
    base::AlignedFree(buffer);
    return mojom::OP_FAILED;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::CreateMemoryDescriptor(
    int32_t index,
    dnnl_memory_desc_t& memory_desc,
    dnnl_format_tag_t* user_format) {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const mojom::OperandPtr& operand = model->operands[index];
  dnnl_data_type_t data_type;
  int32_t result = GetDataType(operand->type, &data_type);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  dnnl_format_tag_t format;
  if (!user_format) {
    result = GetMemoryFormat(operand->dimensions, &format);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  } else {
    format = *user_format;
  }

  std::vector<dnnl_dim_t> dims;
  result = GetDims(operand->dimensions, dims, format);
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  dnnl_status_t status = LATE(dnnl_memory_desc_init_by_tag)(
      &memory_desc, dims.size(), dims.data(), data_type, format);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::CreateMemory(uint32_t index,
                                              dnnl_memory_t& memory,
                                              dnnl_format_tag_t* user_format) {
  dnnl_memory_desc_t memory_desc;
  int32_t result = CreateMemoryDescriptor(index, memory_desc, user_format);
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  dnnl_status_t status = LATE(dnnl_memory_create)(
      &memory, &memory_desc, compiled_model_->engine, DNNL_MEMORY_ALLOCATE);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create memory primitive " << status;
    return mojom::OP_FAILED;
  }
  size_t buffer_size = LATE(dnnl_memory_desc_get_size)(&memory_desc);
  void* buffer = base::AlignedAlloc(buffer_size, ALIGNMENT);
  status = LATE(dnnl_memory_set_data_handle)(memory, buffer);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to set memory data " << status;
    base::AlignedFree(buffer);
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[DNNL] succeed to set memory data handle with size "
             << buffer_size;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  std::string index_id(base::NumberToString(index));
  if (model->values.find(index_id) != model->values.end()) {
    const mojom::OperandValueInfoPtr& value_info = model->values[index_id];
    auto mapping = compilation_->MapMemory(index);
    memcpy(buffer, mapping.get(), value_info->length);
    DLOG(INFO) << "[DNNL] copy user data with size " << value_info->length
               << " to memory primitive buffer with size " << buffer_size;
  }
  DLOG(INFO) << "[DNNL] succeed to create memory for " << index_id;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddInput(uint32_t index) {
  std::string input_id(base::NumberToString(index));
  if (compiled_model_->memories.find(input_id) !=
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is existing";
    return mojom::BAD_DATA;
  }
  dnnl_memory_t memory;
  int32_t result = CreateMemory(index, memory);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  compiled_model_->memories[input_id] = memory;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddOutput(uint32_t index) {
  std::string output_id(base::NumberToString(index));
  if (compiled_model_->memories.find(output_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Output memory is not ready";
    return mojom::BAD_DATA;
  }
  dnnl_memory_t internal_output_memory = compiled_model_->memories[output_id];
  const dnnl_memory_desc_t* internal_output_desc;
  dnnl_status_t status = LATE(dnnl_memory_get_memory_desc)(
      internal_output_memory, &internal_output_desc);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_desc_t output_desc;
  int32_t result = CreateMemoryDescriptor(index, output_desc);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  if (!LATE(dnnl_memory_desc_equal)(&output_desc, internal_output_desc)) {
    DLOG(INFO) << "Reorder internal output to output";
    dnnl_memory_t reordered_memory;
    result = CreateMemory(index, reordered_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_memory_destroy)(reordered_memory);
      return result;
    }
    // Put the reordered output as model output.
    std::string internal_output_id = output_id + "-internal";
    compiled_model_->memories[internal_output_id] = internal_output_memory;
    compiled_model_->memories[output_id] = reordered_memory;
    result = AddReorder(internal_output_id, output_id);
    if (result != mojom::NOT_ERROR) {
      // LATE(dnnl_primitive_desc_destroy)(output_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "No need to reorder internal output to output";
  }
  // LATE(dnnl_primitive_desc_destroy)(output_pd);
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddReorder(const std::string& input_name,
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
  dnnl_status_t status;
  dnnl_memory_t input = compiled_model_->memories[input_name];
  const dnnl_memory_desc_t* input_desc;
  status = LATE(dnnl_memory_get_memory_desc)(input, &input_desc);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_t output = compiled_model_->memories[output_name];
  const dnnl_memory_desc_t* output_desc;
  status = LATE(dnnl_memory_get_memory_desc)(output, &output_desc);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_primitive_desc_t reorder_pd;
  status = LATE(dnnl_reorder_primitive_desc_create)(
      &reorder_pd, input_desc, compiled_model_->engine, output_desc,
      compiled_model_->engine, NULL);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] falied to create reorder primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }

  dnnl_primitive_t reorder;
  status = LATE(dnnl_primitive_create)(&reorder, reorder_pd);
  if (status != dnnl_success) {
    LATE(dnnl_primitive_desc_destroy)(reorder_pd);
    LOG(ERROR) << "[DNNL] failed to create reorder primitive " << status;
  }

  LATE(dnnl_primitive_desc_destroy)(reorder_pd);
  DLOG(INFO) << "[DNNL] succeed to create reorder primitive from " << input_name
             << " to " << output_name;

  if (run) {
    // Execute reorder primitive right now.
    uint32_t n = 1;
    dnnl_stream_t stream;
    dnnl_primitive_t net[1];
    net[0] = reorder;
    args_t net_args[1];

    net_args[0].push_back({DNNL_ARG_SRC, input});
    net_args[0].push_back({DNNL_ARG_DST, output});

    status = LATE(dnnl_stream_create)(&stream, compiled_model_->engine,
                                      dnnl_stream_default_flags);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to create stream " << status;
      LATE(dnnl_primitive_destroy)(reorder);
      return mojom::OP_FAILED;
    }

    for (uint32_t i = 0; i < n; ++i) {
      status = LATE(dnnl_primitive_execute)(net[i], stream, net_args[i].size(),
                                            net_args[i].data());
      if (status != dnnl_success) {
        LOG(ERROR) << "[DNNL] failed to submit stream " << status;
        LATE(dnnl_primitive_destroy)(reorder);
        LATE(dnnl_stream_destroy)(stream);
        return mojom::OP_FAILED;
      }
    }

    status = LATE(dnnl_stream_wait)(stream);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to wait stream " << status;
      LATE(dnnl_primitive_destroy)(reorder);
      LATE(dnnl_stream_destroy)(stream);
      return mojom::OP_FAILED;
    }

    status = LATE(dnnl_primitive_destroy)(reorder);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to destroy reorder primitive " << status;
      return mojom::OP_FAILED;
    }
    status = LATE(dnnl_stream_destroy)(stream);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to destroy stream " << status;
      return mojom::OP_FAILED;
    }
    // Release memory primitive and buffer.

    void* buffer = nullptr;
    status = LATE(dnnl_memory_get_data_handle)(input, &buffer);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to get memory data handle " << status;
      return mojom::OP_FAILED;
    }
    status = LATE(dnnl_memory_destroy)(input);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to destroy memory " << status;
      return mojom::OP_FAILED;
    }
    if (buffer)
      base::AlignedFree(buffer);
    compiled_model_->memories.erase(input_name);
    DLOG(INFO) << "[DNNL] succeed to destroy primitive for " << input_name;
  } else {
    OperationDnnl operation(reorder);
    operation.primitive_args.push_back({DNNL_ARG_SRC, input});
    operation.primitive_args.push_back({DNNL_ARG_DST, output});
    compiled_model_->operations.push_back(operation);
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddActivation(const std::string& input_name,
                                               const std::string& output_name,
                                               uint32_t type) {
  if (compiled_model_->memories.find(input_name) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  dnnl_status_t status;
  dnnl_memory_t input_memory = compiled_model_->memories[input_name];
  const dnnl_memory_desc_t* input_md;
  status = LATE(dnnl_memory_get_memory_desc)(input_memory, &input_md);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get memory descriptor " << status;
    return mojom::OP_FAILED;
  }

  dnnl_eltwise_desc_t activation_desc;
  dnnl_alg_kind_t alg_kind = dnnl_eltwise_relu;
  float alpha = 0.0;
  if (type == mojom::FUSED_RELU) {
    alg_kind = dnnl_eltwise_relu;
  } else if (type == mojom::FUSED_RELU1) {
    alg_kind = dnnl_eltwise_bounded_relu;
    alpha = 1.0;
  } else if (type == mojom::FUSED_RELU6) {
    alg_kind = dnnl_eltwise_bounded_relu;
    alpha = 6.0;
  } else if (type == mojom::LOGISTIC) {
    alg_kind = dnnl_eltwise_logistic;
  } else {
    LOG(ERROR) << type << " is not supported";
    return mojom::BAD_DATA;
  }

  status = LATE(dnnl_eltwise_forward_desc_init)(&activation_desc, dnnl_forward,
                                                alg_kind, input_md, alpha, 0.0);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init eltwise descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_primitive_desc_t activation_pd;
  status = LATE(dnnl_primitive_desc_create)(
      &activation_pd, &activation_desc, NULL, compiled_model_->engine, NULL);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_t output_memory;
  int32_t result =
      CreateMemoryByQueryType(activation_pd, dnnl_query_dst_md, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(activation_pd);
    return result;
  }
  DLOG(INFO) << "[DNNL] succeed to create memory primitive for " << output_name;
  compiled_model_->memories[output_name] = output_memory;

  dnnl_primitive_t activation;
  status = LATE(dnnl_primitive_create)(&activation, activation_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to set data handle to memory primitive "
               << status;
    LATE(dnnl_primitive_desc_destroy)(activation_pd);
    return mojom::OP_FAILED;
  }
  LATE(dnnl_primitive_desc_destroy)(activation_pd);

  OperationDnnl operation(activation);
  operation.primitive_args.push_back({DNNL_ARG_SRC, input_memory});
  operation.primitive_args.push_back({DNNL_ARG_DST, output_memory});
  compiled_model_->operations.push_back(operation);

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddElementwise(
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
  dnnl_status_t status;
  std::vector<float> scales;
  std::vector<dnnl_memory_desc_t> input_pds;
  std::vector<dnnl_memory_t> input_memories;
  for (size_t index = 0; index < 2; ++index) {
    std::string input_id(base::NumberToString(operation->inputs[index]));
    if (compiled_model_->memories.find(input_id) ==
        compiled_model_->memories.end()) {
      // Setup constants
      const mojom::ModelInfoPtr& model = compilation_->GetModel();
      if (model->values.find(input_id) != model->values.end()) {
        dnnl_memory_t constant_memory;
        result = CreateMemory(operation->inputs[index], constant_memory);
        if (result != mojom::NOT_ERROR) {
          return result;
        }
        compiled_model_->memories[input_id] = constant_memory;
      }
    }

    dnnl_memory_t input_memory = compiled_model_->memories[input_id];
    input_memories.push_back(input_memory);
    const dnnl_memory_desc_t* input_pd;
    status = LATE(dnnl_memory_get_memory_desc)(input_memory, &input_pd);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
      return mojom::OP_FAILED;
    }
    scales.push_back(1.0);
    input_pds.push_back(*input_pd);
  }

  dnnl_primitive_desc_t sum_pd;
  status = LATE(dnnl_sum_primitive_desc_create)(&sum_pd, NULL, input_pds.size(),
                                                scales.data(), input_pds.data(),
                                                NULL, compiled_model_->engine);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create sum primitive descriptor " << status;
    return mojom::OP_FAILED;
  }

  DLOG(INFO) << "Add output memory";
  dnnl_memory_t output_memory;
  result = CreateMemoryByQueryType(sum_pd, dnnl_query_dst_md, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(sum_pd);
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
  DLOG(INFO) << "[DNNL] succeed to create memory primitive for "
             << sum_output_id;

  dnnl_primitive_t sum;
  status = LATE(dnnl_primitive_create)(&sum, sum_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create sum primitive " << status;
    LATE(dnnl_primitive_desc_destroy)(sum_pd);
    return mojom::OP_FAILED;
  }
  LATE(dnnl_primitive_desc_destroy)(sum_pd);

  OperationDnnl dnnl_operation(sum);
  uint32_t input_size = input_memories.size();
  for (size_t i = 0; i < input_size; i++) {
    dnnl_operation.primitive_args.push_back(
        {DNNL_ARG_MULTIPLE_SRC + i, input_memories[i]});
  }
  dnnl_operation.primitive_args.push_back({DNNL_ARG_DST, output_memory});

  compiled_model_->operations.push_back(dnnl_operation);

  if (params.fuse_code != mojom::FUSED_NONE) {
    // Append an activation primitive that uses sum's output memory as input
    // and output is named as output_id.
    result = AddActivation(sum_output_id, output_id, params.fuse_code);
    if (result != mojom::NOT_ERROR)
      return result;
  }

  DLOG(INFO) << "[DNNL] succeed to create sum primitive";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddConvolution(
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
  dnnl_status_t status;
  dnnl_memory_desc_t input_desc;
  // Input logical order is nchw
  const dnnl_dim_t input_dims[4] = {params.input_batch, params.input_channel,
                                    params.input_height, params.input_width};
  status = LATE(dnnl_memory_desc_init_by_tag)(&input_desc, 4, input_dims,
                                              dnnl_f32, dnnl_format_tag_any);

  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_desc_t weights_desc;
  if (params.depthwise) {
    // Weights logical order is (g, o, i, h, w)
    const dnnl_dim_t weights_dims[5] = {
        params.depth_out, 1, 1, params.filter_height, params.filter_width};
    status = LATE(dnnl_memory_desc_init_by_tag)(&weights_desc, 5, weights_dims,
                                                dnnl_f32, dnnl_format_tag_any);
  } else {
    // Weights logical order is oihw
    const dnnl_dim_t weights_dims[4] = {params.depth_out, params.depth_in,
                                        params.filter_height,
                                        params.filter_width};
    status = LATE(dnnl_memory_desc_init_by_tag)(&weights_desc, 4, weights_dims,
                                                dnnl_f32, dnnl_format_tag_any);
  }
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }

  dnnl_memory_desc_t bias_desc;
  const dnnl_dim_t bias_dims[1] = {params.bias_length};
  status = LATE(dnnl_memory_desc_init_by_tag)(&bias_desc, 1, bias_dims,
                                              dnnl_f32, dnnl_x);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_desc_t output_desc;
  // Output logical order is nchw
  const dnnl_dim_t output_dims[4] = {params.output_batch, params.output_channel,
                                     params.output_height, params.output_width};
  status = LATE(dnnl_memory_desc_init_by_tag)(&output_desc, 4, output_dims,
                                              dnnl_f32, dnnl_format_tag_any);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }

  dnnl_convolution_desc_t conv_desc;
  dnnl_dim_t strides[2], dilates[2];
  if (params.atrous) {
    // DNNL dilation starts from 0.
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
  const dnnl_dim_t pad_left[2] = {params.padding_top, params.padding_left};
  const dnnl_dim_t pad_right[2] = {params.padding_bottom, params.padding_right};
  status = LATE(dnnl_dilated_convolution_forward_desc_init)(
      &conv_desc, dnnl_forward_inference, dnnl_convolution_direct, &input_desc,
      &weights_desc, &bias_desc, &output_desc, strides, dilates, pad_left,
      pad_right);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init convolution descriptor " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[DNNL] succeed to init convolution descriptor";

  dnnl_primitive_desc_t conv_pd;
  if (params.fuse_code == mojom::FUSED_NONE || params.depthwise) {
    status = LATE(dnnl_primitive_desc_create)(&conv_pd, &conv_desc, NULL,
                                              compiled_model_->engine, NULL);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to create convolution primitive descriptor "
                 << status;
      return mojom::OP_FAILED;
    }
  } else {
    // dnnl only supports fused activation for normal convolution.
    dnnl_primitive_attr_t attr;
    status = LATE(dnnl_primitive_attr_create)(&attr);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to create primitive attribute " << status;
      return mojom::OP_FAILED;
    }
    dnnl_post_ops_t post_ops;
    status = LATE(dnnl_post_ops_create)(&post_ops);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to create post ops " << status;
      return mojom::OP_FAILED;
    }
    if (params.fuse_code == mojom::FUSED_RELU) {
      status = LATE(dnnl_post_ops_append_eltwise)(post_ops, 1.0,
                                                  dnnl_eltwise_relu, 0, 0);
    } else if (params.fuse_code == mojom::FUSED_RELU1 ||
               params.fuse_code == mojom::FUSED_RELU6) {
      float uppper_bound = params.fuse_code == mojom::FUSED_RELU1 ? 1.0 : 6.0;
      status = LATE(dnnl_post_ops_append_eltwise)(
          post_ops, 1.0, dnnl_eltwise_bounded_relu, uppper_bound, 0);
    } else {
      LOG(ERROR) << "[DNNL] fuse code " << params.fuse_code
                 << " is not supproted.";
      LATE(dnnl_post_ops_destroy)(post_ops);
      LATE(dnnl_primitive_attr_destroy)(attr);
      return mojom::BAD_DATA;
    }
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to append eltwise to post ops " << status;
      LATE(dnnl_post_ops_destroy)(post_ops);
      LATE(dnnl_primitive_attr_destroy)(attr);
      return mojom::OP_FAILED;
    }
    status = LATE(dnnl_primitive_attr_set_post_ops)(attr, post_ops);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to set post ops to primitive attribute "
                 << status;
      return mojom::OP_FAILED;
    }
    status = LATE(dnnl_primitive_desc_create)(&conv_pd, &conv_desc, attr,
                                              compiled_model_->engine, NULL);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to create convolution primitive descriptor "
                 << status;
      return mojom::OP_FAILED;
    }
    LATE(dnnl_post_ops_destroy)(post_ops);
    LATE(dnnl_primitive_attr_destroy)(attr);
  }

  DLOG(INFO) << "[DNNL] succeed to create convolution primitive descriptor";

  std::string external_input_id(base::NumberToString(operation->inputs[0]));
  if (compiled_model_->memories.find(external_input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    LATE(dnnl_primitive_desc_destroy)(conv_pd);
    return mojom::BAD_DATA;
  }
  dnnl_memory_t external_input_memory =
      compiled_model_->memories[external_input_id];
  const dnnl_memory_desc_t* external_input_pd;
  status = LATE(dnnl_memory_get_memory_desc)(external_input_memory,
                                             &external_input_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
    LATE(dnnl_primitive_desc_destroy)(conv_pd);
    return mojom::OP_FAILED;
  }

  dnnl_memory_t input_memory;
  const dnnl_memory_desc_t* input_pd =
      LATE(dnnl_primitive_desc_query_md)(conv_pd, dnnl_query_src_md, 0);
  if (!LATE(dnnl_memory_desc_equal)(input_pd, external_input_pd)) {
    DLOG(INFO) << "Reorder external input to input";
    status = LATE(dnnl_memory_create)(&input_memory, input_pd,
                                      compiled_model_->engine, NULL);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to create memory primitive " << status;
      LATE(dnnl_primitive_desc_destroy)(conv_pd);
      return mojom::OP_FAILED;
    }
    size_t input_size = LATE(dnnl_memory_desc_get_size)(input_pd);
    void* input_buffer = base::AlignedAlloc(input_size, ALIGNMENT);
    status = LATE(dnnl_memory_set_data_handle)(input_memory, input_buffer);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to set data handle " << status;
      LATE(dnnl_primitive_desc_destroy)(conv_pd);
      // LATE(dnnl_primitive_destroy)(input_memory);
      base::AlignedFree(input_buffer);
      return mojom::OP_FAILED;
    }
    DLOG(INFO) << "[DNNL] succeed to set memory data handle with size "
               << input_size;
    std::string input_id = external_input_id + "-reordered";
    compiled_model_->memories[input_id] = input_memory;
    DLOG(INFO) << "[DNNL] succeed to create memory primitve for " << input_id;
    result = AddReorder(external_input_id, input_id);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_primitive_desc_destroy)(conv_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse exteranl input as input";
    input_memory = external_input_memory;
  }

  dnnl_format_tag_t weights_format;
  if (params.depthwise) {
    weights_format = dnnl_hwigo;
  } else {
    weights_format = dnnl_ohwi;
  }
  dnnl_memory_t external_weights_memory;
  std::string external_weights_id = base::NumberToString(operation->inputs[1]);
  result = CreateMemory(operation->inputs[1], external_weights_memory,
                        &weights_format);

  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(conv_pd);
    return result;
  }

  compiled_model_->memories[external_weights_id] = external_weights_memory;
  const dnnl_memory_desc_t* external_weights_pd;
  status = LATE(dnnl_memory_get_memory_desc)(external_weights_memory,
                                             &external_weights_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
    LATE(dnnl_primitive_desc_destroy)(conv_pd);
    return mojom::OP_FAILED;
  }
  dnnl_memory_t weights_memory;
  const dnnl_memory_desc_t* weights_pd =
      LATE(dnnl_primitive_desc_query_md)(conv_pd, dnnl_query_weights_md, 0);
  if (!LATE(dnnl_memory_desc_equal)(weights_pd, external_weights_pd)) {
    DLOG(INFO) << "Reorder external weigths to weights";
    result =
        CreateMemoryByQueryType(conv_pd, dnnl_query_weights_md, weights_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_primitive_desc_destroy)(conv_pd);
      return result;
    }
    std::string weights_id = external_weights_id + "-reordered";
    compiled_model_->memories[weights_id] = weights_memory;
    DLOG(INFO) << "[DNNL] succeed to create memory primitve for " << weights_id;
    result = AddReorder(external_weights_id, weights_id, true);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_primitive_desc_destroy)(conv_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse external weights as weights";
    weights_memory = external_weights_memory;
  }

  dnnl_memory_t bias_memory;
  std::string bias_id(base::NumberToString(operation->inputs[2]));
  result = CreateMemory(operation->inputs[2], bias_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(conv_pd);
    return result;
  }
  compiled_model_->memories[bias_id] = bias_memory;

  dnnl_memory_t output_memory;
  result = CreateMemoryByQueryType(conv_pd, dnnl_query_dst_md, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(conv_pd);
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
  DLOG(INFO) << "[DNNL] succeed to create memory primitive for "
             << conv_output_id;

  dnnl_primitive_t conv;
  status = LATE(dnnl_primitive_create)(&conv, conv_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create convolution primitive " << status;
    LATE(dnnl_primitive_desc_destroy)(conv_pd);
    return mojom::OP_FAILED;
  }
  LATE(dnnl_primitive_desc_destroy)(conv_pd);

  OperationDnnl dnnl_operation(conv);
  dnnl_operation.primitive_args.push_back({DNNL_ARG_SRC, input_memory});
  dnnl_operation.primitive_args.push_back({DNNL_ARG_WEIGHTS, weights_memory});
  dnnl_operation.primitive_args.push_back({DNNL_ARG_BIAS, bias_memory});
  dnnl_operation.primitive_args.push_back({DNNL_ARG_DST, output_memory});

  compiled_model_->operations.push_back(dnnl_operation);
  DLOG(INFO) << "[DNNL] succeed to create convolution primitive";

  if (params.depthwise && params.fuse_code != mojom::FUSED_NONE) {
    // Append an activation primitive that uses conv's output memory as input
    // and output is named as output_id.
    result = AddActivation(conv_output_id, output_id, params.fuse_code);
    if (result != mojom::NOT_ERROR)
      return result;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddPooling(
    const mojom::OperationPtr& operation) {
  PoolingParams params;
  int32_t result = compilation_->GetPoolingParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  dnnl_status_t status;
  std::string input_id(base::NumberToString(operation->inputs[0]));
  if (compiled_model_->memories.find(input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  dnnl_memory_t input_memory = compiled_model_->memories[input_id];
  const dnnl_memory_desc_t* input_pd;
  status = LATE(dnnl_memory_get_memory_desc)(input_memory, &input_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }

  dnnl_memory_desc_t output_md;
  dnnl_dim_t output_dims[4] = {params.output_batch, params.output_channel,
                               params.output_height, params.output_width};
  status = LATE(dnnl_memory_desc_init_by_tag)(&output_md, 4, output_dims,
                                              dnnl_f32, dnnl_format_tag_any);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init output memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_pooling_desc_t pool_desc;
  dnnl_alg_kind_t pooling_kind;
  if (operation->type == mojom::MAX_POOL_2D) {
    pooling_kind = dnnl_pooling_max;
  } else if (operation->type == mojom::AVERAGE_POOL_2D) {
    pooling_kind = dnnl_pooling_avg;
  } else {
    LOG(ERROR) << "Pooling mode " << operation->type << " is not supported";
    return mojom::BAD_DATA;
  }
  dnnl_dim_t kernel[2] = {params.filter_width, params.filter_height};
  dnnl_dim_t strides[2] = {params.stride_width, params.stride_height};
  dnnl_dim_t pad_left[2] = {params.padding_top, params.padding_left};
  dnnl_dim_t pad_right[2] = {params.padding_bottom, params.padding_right};
  status = LATE(dnnl_pooling_forward_desc_init)(
      &pool_desc, dnnl_forward, pooling_kind, input_pd, &output_md, strides,
      kernel, pad_left, pad_right);
  if (status != mojom::NOT_ERROR) {
    LOG(ERROR) << "[DNNL] failed to init pooling descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_primitive_desc_t pool_pd;
  status = LATE(dnnl_primitive_desc_create)(&pool_pd, &pool_desc, NULL,
                                            compiled_model_->engine, NULL);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create pooling primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }

  dnnl_memory_t pool_indices_memory = nullptr;
  if (operation->type == mojom::MAX_POOL_2D) {
    DLOG(INFO) << "Add working space";
    result = CreateMemoryByQueryType(pool_pd, dnnl_query_workspace_md,
                                     pool_indices_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_primitive_desc_destroy)(pool_pd);
      return result;
    }
    std::string working_space_id(base::NumberToString(operation->outputs[0]) +
                                 std::string("-working-space"));
    compiled_model_->memories[working_space_id] = pool_indices_memory;
    DLOG(INFO) << "[DNNL] succeed to create memory primitive for "
               << working_space_id;
  }

  DLOG(INFO) << "Add output memory";
  dnnl_memory_t output_memory;
  result = CreateMemoryByQueryType(pool_pd, dnnl_query_dst_md, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(pool_pd);
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
  DLOG(INFO) << "[DNNL] succeed to create memory primitive for "
             << pool_output_id;

  dnnl_primitive_t pool;
  status = LATE(dnnl_primitive_create)(&pool, pool_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create pooling primitive " << status;
    LATE(dnnl_primitive_desc_destroy)(pool_pd);
    return mojom::OP_FAILED;
  }
  LATE(dnnl_primitive_desc_destroy)(pool_pd);

  OperationDnnl dnnl_operation(pool);
  dnnl_operation.primitive_args.push_back({DNNL_ARG_SRC, input_memory});
  dnnl_operation.primitive_args.push_back({DNNL_ARG_DST, output_memory});
  dnnl_operation.primitive_args.push_back(
      {DNNL_ARG_WORKSPACE, pool_indices_memory});

  compiled_model_->operations.push_back(dnnl_operation);

  DLOG(INFO) << "[DNNL] succeed to create pooling primitive";

  if (params.fuse_code != mojom::FUSED_NONE) {
    // Append an activation primitive that uses pool's output memory as input
    // and output is named as output_id.
    result = AddActivation(pool_output_id, output_id, params.fuse_code);
    if (result != mojom::NOT_ERROR)
      return result;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddSoftmax(
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
  dnnl_status_t status;
  dnnl_memory_t input_memory = compiled_model_->memories[input_id];
  const dnnl_memory_desc_t* input_md;
  status = LATE(dnnl_memory_get_memory_desc)(input_memory, &input_md);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }

  dnnl_softmax_desc_t softmax_desc;
  status = LATE(dnnl_softmax_forward_desc_init)(&softmax_desc, dnnl_forward,
                                                input_md, input_md->ndims - 1);
  if (status != mojom::NOT_ERROR) {
    LOG(ERROR) << "[DNNL] failed to init softmax descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_primitive_desc_t softmax_pd;
  status = LATE(dnnl_primitive_desc_create)(&softmax_pd, &softmax_desc, NULL,
                                            compiled_model_->engine, NULL);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create softmax primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }

  DLOG(INFO) << "Add output memory";
  dnnl_memory_t output_memory;
  result =
      CreateMemoryByQueryType(softmax_pd, dnnl_query_dst_md, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(softmax_pd);
    return result;
  }
  std::string output_id(base::NumberToString(operation->outputs[0]));
  compiled_model_->memories[output_id] = output_memory;
  DLOG(INFO) << "[DNNL] succeed to create memory primitive for " << output_id;

  dnnl_primitive_t softmax;
  status = LATE(dnnl_primitive_create)(&softmax, softmax_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create softmax primitive " << status;
    LATE(dnnl_primitive_desc_destroy)(softmax_pd);
    return mojom::OP_FAILED;
  }
  LATE(dnnl_primitive_desc_destroy)(softmax_pd);

  OperationDnnl dnnl_operation(softmax);
  dnnl_operation.primitive_args.push_back({DNNL_ARG_SRC, input_memory});
  dnnl_operation.primitive_args.push_back({DNNL_ARG_DST, output_memory});

  compiled_model_->operations.push_back(dnnl_operation);

  DLOG(INFO) << "[DNNL] succeed to create softmax primitive";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddLogistic(
    const mojom::OperationPtr& operation) {
  int32_t result = AddActivation(base::NumberToString(operation->inputs[0]),
                                 base::NumberToString(operation->outputs[0]),
                                 mojom::LOGISTIC);

  if (result != mojom::NOT_ERROR) {
    return result;
  }

  DLOG(INFO) << "[DNNL] succeed to create Logistic primitive";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddReshape(
    const mojom::OperationPtr& operation) {
  const uint32_t input_index = operation->inputs[0];
  std::string input_id(base::NumberToString(input_index));
  if (compiled_model_->memories.find(input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    return mojom::BAD_DATA;
  }
  dnnl_status_t status;
  dnnl_memory_t input_memory = compiled_model_->memories[input_id];
  const dnnl_memory_desc_t* input_pd;
  status = LATE(dnnl_memory_get_memory_desc)(input_memory, &input_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_desc_t reshape_input_pd;
  int32_t result = CreateMemoryDescriptor(input_index, reshape_input_pd);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  if (!LATE(dnnl_memory_desc_equal)(input_pd, &reshape_input_pd)) {
    DLOG(INFO) << "Reorder input to reshape_input";
    dnnl_memory_t reshape_input_memory;
    result = CreateMemory(input_index, reshape_input_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_memory_destroy)(reshape_input_memory);
      return result;
    }
    std::string prereorder_input_id = input_id + "-pre-reorder";
    compiled_model_->memories[prereorder_input_id] = input_memory;
    compiled_model_->memories[input_id] = reshape_input_memory;
    result = AddReorder(prereorder_input_id, input_id);
    if (result != mojom::NOT_ERROR) {
      // LATE(dnnl_primitive_desc_destroy)(reshape_input_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "No need to reorder input to reshape_input";
  }
  // LATE(dnnl_primitive_desc_destroy)(reshape_input_pd);

  dnnl_memory_t output_memory;
  const uint32_t output_index = operation->outputs[0];
  std::string output_id(base::NumberToString(output_index));
  result = CreateMemory(output_index, output_memory);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  compiled_model_->memories[output_id] = output_memory;
  OperationDnnl dnnl_operation(operation);
  compiled_model_->operations.push_back(dnnl_operation);

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddConcatenation(
    const mojom::OperationPtr& operation) {
  ConcatParams params;
  int32_t result = compilation_->GetConcatParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  dnnl_status_t status;
  std::vector<dnnl_memory_desc_t> input_pds;
  args_t concat_args;
  std::vector<dnnl_memory_t> input_memories;
  for (size_t index = 0; index < operation->inputs.size() - 1; ++index) {
    std::string input_id(base::NumberToString(operation->inputs[index]));
    if (compiled_model_->memories.find(input_id) ==
        compiled_model_->memories.end()) {
      // Setup constants
      if (model->values.find(input_id) != model->values.end()) {
        dnnl_memory_t constant_memory;
        result = CreateMemory(operation->inputs[index], constant_memory);
        if (result != mojom::NOT_ERROR) {
          return result;
        }
        compiled_model_->memories[input_id] = constant_memory;
      }
    }

    dnnl_memory_t input_memory = compiled_model_->memories[input_id];
    concat_args.push_back({DNNL_ARG_MULTIPLE_SRC + index, input_memory});
    const dnnl_memory_desc_t* input_pd;
    status = LATE(dnnl_memory_get_memory_desc)(input_memory, &input_pd);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
      return mojom::OP_FAILED;
    }
    input_pds.push_back(*input_pd);
  }

  int concat_dimension = 0;
  const uint32_t rank =
      model->operands[operation->inputs[0]]->dimensions.size();
  if (rank == 1 || rank == 2) {
    concat_dimension = params.axis;
  } else if (rank == 3) {
    // HWC -> NCW
    if (params.axis == 0) {
      concat_dimension = 0;
    } else if (params.axis == 1) {
      concat_dimension = 2;
    } else if (params.axis == 2) {
      concat_dimension = 1;
    }
  } else if (rank == 4) {
    // NHWC -> NCHW
    if (params.axis == 0) {
      concat_dimension = 0;
    } else if (params.axis == 1) {
      concat_dimension = 2;
    } else if (params.axis == 2) {
      concat_dimension = 3;
    } else if (params.axis == 3) {
      concat_dimension = 1;
    }
  } else {
    LOG(ERROR) << "rank " << rank << " is not supported.";
    return mojom::BAD_DATA;
  }

  dnnl_primitive_desc_t concat_pd;
  status = LATE(dnnl_concat_primitive_desc_create)(
      &concat_pd, NULL, input_pds.size(), concat_dimension, input_pds.data(),
      NULL, compiled_model_->engine);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create concat primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }

  DLOG(INFO) << "Add output memory";
  dnnl_memory_t output_memory;
  result = CreateMemoryByQueryType(concat_pd, dnnl_query_dst_md, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(concat_pd);
    return result;
  }
  std::string output_id(base::NumberToString(operation->outputs[0]));
  compiled_model_->memories[output_id] = output_memory;
  DLOG(INFO) << "[DNNL] succeed to create memory primitive for " << output_id;
  concat_args.push_back({DNNL_ARG_DST, output_memory});

  dnnl_primitive_t concat;
  status = LATE(dnnl_primitive_create)(&concat, concat_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create concat primitive " << status;
    LATE(dnnl_primitive_desc_destroy)(concat_pd);
    return mojom::OP_FAILED;
  }
  LATE(dnnl_primitive_desc_destroy)(concat_pd);

  OperationDnnl dnnl_operation(concat);
  dnnl_operation.primitive_args = concat_args;
  compiled_model_->operations.push_back(dnnl_operation);
  DLOG(INFO) << "[DNNL] succeed to create concat primitive";

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddFullyConnected(
    const mojom::OperationPtr& operation) {
  FullyConnectedParams params;
  int32_t result = compilation_->GetFullyConnectedParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  dnnl_status_t status;
  dnnl_memory_desc_t ip_input_desc;
  // Input logical order is nc
  const dnnl_dim_t ip_input_dims[2] = {params.input_batch_size,
                                       params.input_size};
  status = LATE(dnnl_memory_desc_init_by_tag)(&ip_input_desc, 2, ip_input_dims,
                                              dnnl_f32, dnnl_format_tag_any);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_desc_t weights_desc;
  // Weights logical order is oi
  const dnnl_dim_t weights_dims[2] = {params.num_units, params.input_size};
  status = LATE(dnnl_memory_desc_init_by_tag)(&weights_desc, 2, weights_dims,
                                              dnnl_f32, dnnl_format_tag_any);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_desc_t bias_desc;
  const dnnl_dim_t bias_dims[1] = {params.bias_num_units};
  status = LATE(dnnl_memory_desc_init_by_tag)(&bias_desc, 1, bias_dims,
                                              dnnl_f32, dnnl_x);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }
  dnnl_memory_desc_t output_desc;
  // Output logical order is nc
  const dnnl_dim_t output_dims[2] = {params.output_batch_size,
                                     params.output_num_units};
  status = LATE(dnnl_memory_desc_init_by_tag)(&output_desc, 2, output_dims,
                                              dnnl_f32, dnnl_format_tag_any);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
    return mojom::OP_FAILED;
  }

  dnnl_inner_product_desc_t ip_desc;
  status = LATE(dnnl_inner_product_forward_desc_init)(
      &ip_desc, dnnl_forward, &ip_input_desc, &weights_desc, &bias_desc,
      &output_desc);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to init inner product descriptor " << status;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[DNNL] succeed to init inner product descriptor";

  dnnl_primitive_desc_t ip_pd;
  status = LATE(dnnl_primitive_desc_create)(&ip_pd, &ip_desc, NULL,
                                            compiled_model_->engine, NULL);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create inner product primitive descriptor "
               << status;
    return mojom::OP_FAILED;
  }

  DLOG(INFO) << "[DNNL] succeed to create inner product primitive descriptor";

  DLOG(INFO) << "Add input memory";
  const uint32_t input_index = operation->inputs[0];
  std::string input_id(base::NumberToString(input_index));
  if (compiled_model_->memories.find(input_id) ==
      compiled_model_->memories.end()) {
    LOG(ERROR) << "Input memory is not ready";
    LATE(dnnl_primitive_desc_destroy)(ip_pd);
    return mojom::BAD_DATA;
  }
  dnnl_memory_t external_input_memory = compiled_model_->memories[input_id];
  const dnnl_memory_desc_t* external_input_pd;
  status = LATE(dnnl_memory_get_memory_desc)(external_input_memory,
                                             &external_input_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
    LATE(dnnl_primitive_desc_destroy)(ip_pd);
    return mojom::OP_FAILED;
  }
  // Check whether we need to reshape the external input to 2D
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const mojom::OperandPtr& input = model->operands[operation->inputs[0]];
  if (input->dimensions.size() > 2) {
    // Reshape the input to 2D tensor
    DLOG(INFO) << "Reshape input rank " << input->dimensions.size() << " to 2D";
    dnnl_memory_desc_t input_pd;
    result = CreateMemoryDescriptor(input_index, input_pd);
    if (!LATE(dnnl_memory_desc_equal)(external_input_pd, &input_pd)) {
      DLOG(INFO) << "Reorder exteranl_input to input";
      dnnl_memory_t reordered_input_memory;
      std::string reordered_input_id = input_id + "-reorder";
      result = CreateMemory(input_index, reordered_input_memory);
      if (result != mojom::NOT_ERROR) {
        LATE(dnnl_primitive_desc_destroy)(ip_pd);
        return result;
      }
      compiled_model_->memories[reordered_input_id] = reordered_input_memory;
      result = AddReorder(input_id, reordered_input_id);
      if (result != mojom::NOT_ERROR) {
        LATE(dnnl_primitive_desc_destroy)(ip_pd);
        return result;
      }
      input_id = reordered_input_id;
    } else {
      DLOG(INFO) << "No need to reorder external input to input";
    }

    // Create the reshaped input memory
    dnnl_memory_desc_t reshaped_input_desc;
    status = LATE(dnnl_memory_desc_init_by_tag)(
        &reshaped_input_desc, 2, ip_input_dims, dnnl_f32, dnnl_nc);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to init memory descriptor " << status;
      return mojom::OP_FAILED;
    }

    dnnl_memory_t reshaped_input_memory;
    status =
        LATE(dnnl_memory_create)(&reshaped_input_memory, &reshaped_input_desc,
                                 compiled_model_->engine, NULL);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to create memory primitive " << status;
      LATE(dnnl_memory_destroy)(reshaped_input_memory);
      LATE(dnnl_primitive_desc_destroy)(ip_pd);
      return mojom::OP_FAILED;
    }
    size_t buffer_size = LATE(dnnl_memory_desc_get_size)(&reshaped_input_desc);
    void* buffer = base::AlignedAlloc(buffer_size, ALIGNMENT);
    status = LATE(dnnl_memory_set_data_handle)(reshaped_input_memory, buffer);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to set memory data " << status;
      base::AlignedFree(buffer);
      LATE(dnnl_memory_destroy)(reshaped_input_memory);
      LATE(dnnl_primitive_desc_destroy)(ip_pd);
      return mojom::OP_FAILED;
    }
    DLOG(INFO) << "[DNNL] succeed to set memory data handle with size "
               << buffer_size;
    std::string reshaped_input_id = input_id + "-reshape";
    compiled_model_->memories[reshaped_input_id] = reshaped_input_memory;
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to destroy memory primitive descriptor "
                 << status;
      LATE(dnnl_primitive_desc_destroy)(ip_pd);
      return mojom::OP_FAILED;
    }

    OperationDnnl reshape;
    reshape.type = mojom::RESHAPE;
    reshape.inputs.push_back(input_id);
    reshape.outputs.push_back(reshaped_input_id);
    compiled_model_->operations.push_back(reshape);

    // Use reshaped input memory as external input.
    external_input_memory = reshaped_input_memory;
    status = LATE(dnnl_memory_get_memory_desc)(reshaped_input_memory,
                                               &external_input_pd);
    if (status != dnnl_success) {
      LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
      LATE(dnnl_primitive_desc_destroy)(ip_pd);
      return mojom::OP_FAILED;
    }
  } else {
    DLOG(INFO) << "No need to reshape to 2D tensor";
  }

  dnnl_memory_t ip_input_memory;
  const dnnl_memory_desc_t* ip_input_pd =
      LATE(dnnl_primitive_desc_query_md)(ip_pd, dnnl_query_src_md, 0);
  if (!LATE(dnnl_memory_desc_equal)(ip_input_pd, external_input_pd)) {
    DLOG(INFO) << "Reorder external input to input";
    result = CreateMemoryByQueryType(ip_pd, dnnl_query_src_md, ip_input_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_primitive_desc_destroy)(ip_pd);
      return result;
    }
    std::string ip_input_id = input_id + "-internal";
    compiled_model_->memories[ip_input_id] = ip_input_memory;
    DLOG(INFO) << "[DNNL] succeed to create memory primitve for " << input_id;
    result = AddReorder(input_id, ip_input_id);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_primitive_desc_destroy)(ip_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse exteranl input as input";
    ip_input_memory = external_input_memory;
  }

  DLOG(INFO) << "Add weights memory";
  const uint32_t weights_index = operation->inputs[1];
  std::string external_weights_id = base::NumberToString(weights_index);
  dnnl_format_tag_t weights_format = dnnl_oi;
  dnnl_memory_t external_weights_memory;
  result =
      CreateMemory(weights_index, external_weights_memory, &weights_format);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(ip_pd);
    return result;
  }
  compiled_model_->memories[external_weights_id] = external_weights_memory;
  const dnnl_memory_desc_t* external_weights_pd;
  status = LATE(dnnl_memory_get_memory_desc)(external_weights_memory,
                                             &external_weights_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to get primitive descriptor " << status;
    LATE(dnnl_primitive_desc_destroy)(ip_pd);
    return mojom::OP_FAILED;
  }
  dnnl_memory_t weights_memory;
  const dnnl_memory_desc_t* weights_pd =
      LATE(dnnl_primitive_desc_query_md)(ip_pd, dnnl_query_weights_md, 0);
  if (!LATE(dnnl_memory_desc_equal)(weights_pd, external_weights_pd)) {
    DLOG(INFO) << "Reorder external weigths to weights";
    result =
        CreateMemoryByQueryType(ip_pd, dnnl_query_weights_md, weights_memory);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_primitive_desc_destroy)(ip_pd);
      return result;
    }
    std::string weights_id = external_weights_id + "-reordered";
    compiled_model_->memories[weights_id] = weights_memory;
    DLOG(INFO) << "[DNNL] succeed to create memory primitive for "
               << weights_id;
    result = AddReorder(external_weights_id, weights_id, true);
    if (result != mojom::NOT_ERROR) {
      LATE(dnnl_primitive_desc_destroy)(ip_pd);
      return result;
    }
  } else {
    DLOG(INFO) << "Reuse external weights as weights";
    weights_memory = external_weights_memory;
  }

  DLOG(INFO) << "Add bias memory";
  uint32_t bias_index = operation->inputs[2];
  std::string bias_id(base::NumberToString(bias_index));
  dnnl_memory_t bias_memory;
  result = CreateMemory(bias_index, bias_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(ip_pd);
    return result;
  }
  compiled_model_->memories[bias_id] = bias_memory;

  DLOG(INFO) << "Add output memory";
  dnnl_memory_t output_memory;
  result = CreateMemoryByQueryType(ip_pd, dnnl_query_dst_md, output_memory);
  if (result != mojom::NOT_ERROR) {
    LATE(dnnl_primitive_desc_destroy)(ip_pd);
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
  DLOG(INFO) << "[DNNL] succeed to create memory primitive for "
             << ip_output_id;

  dnnl_primitive_t ip;
  status = LATE(dnnl_primitive_create)(&ip, ip_pd);
  if (status != dnnl_success) {
    LOG(ERROR) << "[DNNL] failed to create inner product primitive " << status;
    LATE(dnnl_primitive_desc_destroy)(ip_pd);
    return mojom::OP_FAILED;
  }
  LATE(dnnl_primitive_desc_destroy)(ip_pd);

  OperationDnnl dnnl_operation(ip);

  dnnl_operation.primitive_args.push_back({DNNL_ARG_SRC, ip_input_memory});
  dnnl_operation.primitive_args.push_back({DNNL_ARG_WEIGHTS, weights_memory});
  dnnl_operation.primitive_args.push_back({DNNL_ARG_BIAS, bias_memory});
  dnnl_operation.primitive_args.push_back({DNNL_ARG_DST, output_memory});
  compiled_model_->operations.push_back(dnnl_operation);

  DLOG(INFO) << "[DNNL] succeed to create inner product primitive";

  if (params.fuse_code != mojom::FUSED_NONE) {
    // Append an activation primitive that uses ip's output memory as input
    // and output is named as output_id.
    result = AddActivation(ip_output_id, output_id, params.fuse_code);
    if (result != mojom::NOT_ERROR)
      return result;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateDnnl::AddResizeBilinear(
    const mojom::OperationPtr& operation) {
  LOG(ERROR) << "Operation type " << operation->type << " is not supported.";
  return mojom::BAD_DATA;
}

}  // namespace ml
