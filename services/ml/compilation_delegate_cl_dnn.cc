// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_cl_dnn.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/cl_dnn_custom_kernels.h"
#include "services/ml/execution_impl_cl_dnn.h"
#include "services/ml/public/interfaces/constants.mojom.h"
#include "third_party/clDNN/api/C/activation.h"
#include "third_party/clDNN/api/C/concatenation.h"
#include "third_party/clDNN/api/C/convolution.h"
#include "third_party/clDNN/api/C/custom_gpu_primitive.h"
#include "third_party/clDNN/api/C/data.h"
#include "third_party/clDNN/api/C/eltwise.h"
#include "third_party/clDNN/api/C/fully_connected.h"
#include "third_party/clDNN/api/C/input_layout.h"
#include "third_party/clDNN/api/C/pooling.h"
#include "third_party/clDNN/api/C/reorder.h"
#include "third_party/clDNN/api/C/reshape.h"
#include "third_party/clDNN/api/C/softmax.h"

#if defined(OS_LINUX)
constexpr char kClDnnVersion[] = "12.1";

ml::ClDnnSymbolTable* GetClDnnSymbolTable() {
  static ml::ClDnnSymbolTable* cl_dnn_symbol_table = new ml::ClDnnSymbolTable();
  return cl_dnn_symbol_table;
}
#endif

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

CompilationDelegateClDnn::CompilationDelegateClDnn(
    const CompilationImpl* compilation)
    : CompilationDelegate(),
      compilation_(compilation),
      engine_(nullptr),
      topology_(nullptr),
      program_(nullptr) {}

CompilationDelegateClDnn::~CompilationDelegateClDnn() {
  cldnn_status status;
  for (size_t i = 0; i < memories_.size(); ++i) {
    LATE(cldnn_release_memory)(memories_[i], &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to release cldnn memory " << status << " "
                  << std::string(LATE(cldnn_get_last_error_message)());
    }
  }
  DLOG(INFO) << "[clDNN] succeed to release memories";

  if (topology_) {
    LATE(cldnn_release_topology)(topology_, &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to release cldnn topology " << status << " "
                  << std::string(LATE(cldnn_get_last_error_message)());
    }
    DLOG(INFO) << "[clDNN] succeed to release topology";
  }

  if (engine_) {
    LATE(cldnn_release_engine)(engine_, &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to release cldnn engine " << status << " "
                  << std::string(LATE(cldnn_get_last_error_message)());
    }
    DLOG(INFO) << "[clDNN] succeed to release engine";
  }

  if (program_) {
    LATE(cldnn_release_program)(program_, &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to release program " << status << " "
                  << std::string(LATE(cldnn_get_last_error_message)());
    }
    DLOG(INFO) << "[clDNN] succeed to release program";
  }
}

int32_t CompilationDelegateClDnn::Compile() {
  DLOG(INFO) << "CompilationDelegateClDnn::Compile";

  int32_t result = CldnnInit();
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  result = CldnnCreateTopology();
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  result = CldnnCreateProgram();
  if (result != mojom::NOT_ERROR) {
    return result;
  }

  return mojom::NOT_ERROR;
}

std::unique_ptr<mojom::Execution> CompilationDelegateClDnn::CreateExecution(
    mojom::ExecutionInitParamsPtr params) {
  return std::make_unique<ExecutionImplClDnn>(this, std::move(params));
}

int32_t CompilationDelegateClDnn::CldnnInit() {
#if defined(OS_LINUX)
  if (!GetClDnnSymbolTable()->Load()) {
    LOG(ERROR) << "[clDNN] failed to load clDNN library";
    return mojom::OP_FAILED;
  }
#endif

  cldnn_status status;
  cldnn_version version = LATE(cldnn_get_version)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get cldnn version";
    return mojom::OP_FAILED;
  }

  const std::string major_version =
      std::to_string(version.build) + "." + std::to_string(version.major);
  const std::string cl_dnn_version = major_version + "." +
                                     std::to_string(version.minor) + "." +
                                     std::to_string(version.revision);
  DLOG(INFO) << "[clDNN] version: " << cl_dnn_version;

#if defined(OS_LINUX)
  if (major_version != kClDnnVersion) {
    LOG(ERROR) << "[clDNN] current clDNN version " << cl_dnn_version
               << " isn't supported, please use verified version "
               << kClDnnVersion;
    return mojom::OP_FAILED;
  }
#endif

  uint32_t engine_count =
      LATE(cldnn_get_engine_count)(cldnn_engine_ocl, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN]: failed to get cldnn ocl engine count " << status
                << " " << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] ocl engine count: " << engine_count;
  if (engine_count < 1) {
    DLOG(ERROR) << "[clDNN] ocl engine is not available " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  engine_ = LATE(cldnn_create_engine)(cldnn_engine_ocl, 0, nullptr, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to create cldnn ocl engine " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    engine_ = nullptr;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeeded to create cldnn ocl engine " << engine_;

  cldnn_engine_info engine_info = LATE(cldnn_get_engine_info)(engine_, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get cldnn engine info " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] engine info:\n"
             << "\tcores_count: " << engine_info.cores_count << "\n"
             << "\tcore_frequency: " << engine_info.core_frequency << "\n"
             << "\tmax_work_group_size: " << engine_info.max_work_group_size
             << "\n"
             << "\tmax_local_mem_size: " << engine_info.max_local_mem_size
             << "\n"
             << "\tmax_global_mem_size: " << engine_info.max_global_mem_size
             << "\n"
             << "\tmax_alloc_mem_size: " << engine_info.max_alloc_mem_size
             << "\n"
             << "\tmax_image2d_width: " << engine_info.max_image2d_width << "\n"
             << "\tmax_image2d_height: " << engine_info.max_image2d_height
             << "\n"
             << "\tsupports_fp16: " << engine_info.supports_fp16 << "\n"
             << "\tsupports_fp16_denorms: " << engine_info.supports_fp16_denorms
             << "\n"
             << "\tsupports_subgroups_short: "
             << engine_info.supports_subgroups_short << "\n"
             << "\tsupports_image: " << engine_info.supports_image << "\n";

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnCreateTopology() {
  cldnn_status status;
  topology_ = LATE(cldnn_create_topology)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to create cldnn topology " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    topology_ = nullptr;
    return mojom::OP_FAILED;
  }

  int32_t result;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  for (size_t i = 0; i < model->inputs.size(); ++i) {
    result = CldnnAddInputLayout(model->inputs[i]);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }
  for (size_t i = 0; i < model->outputs.size(); ++i) {
    const std::string reorder_input(base::NumberToString(model->outputs[i]));
    const std::string reorder_output(reorder_input + std::string("-reordered"));
    result = CldnnAddReorder(reorder_input, reorder_output, cldnn_format_byxf);
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
      result = CldnnAddElementwise(type, inputs, outputs);
    } else if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D ||
               type == mojom::ATROUS_CONV_2D ||
               type == mojom::ATROUS_DEPTHWISE_CONV_2D) {
      result = CldnnAddConvolution(type, inputs, outputs);
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      result = CldnnAddPooling(type, inputs, outputs);
    } else if (type == mojom::SOFTMAX) {
      result = CldnnAddSoftmax(type, inputs, outputs);
    } else if (type == mojom::RESHAPE) {
      result = CldnnAddReshape(type, inputs, outputs);
    } else if (type == mojom::CONCATENATION) {
      result = CldnnAddConcatenation(type, inputs, outputs);
    } else if (type == mojom::FULLY_CONNECTED) {
      result = CldnnAddFullyConnected(type, inputs, outputs);
    } else if (type == mojom::RESIZE_BILINEAR) {
      result = CldnnAddResizeBilinear(type, inputs, outputs);
    } else {
      DLOG(ERROR) << "Operation type " << type << " is not supported.";
      return mojom::BAD_DATA;
    }
  }

  if (result != mojom::NOT_ERROR) {
    return result;
  }

  DLOG(INFO) << "[clDNN] succeed to create topology";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnCreateProgram() {
  cldnn_status status;
  std::vector<cldnn_build_option> build_options;
  bool optimize_data = true;
  build_options.push_back(
      {.type = cldnn_build_option_optimize_data, .data = &optimize_data});
  program_ = LATE(cldnn_build_program)(engine_, topology_, build_options.data(),
                                       build_options.size(), &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to build program " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to build program";
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnGetLayout(
    int32_t type,
    const std::vector<uint32_t>& dimensions,
    cldnn_layout& layout,
    int32_t format) {
  if (type != mojom::TENSOR_FLOAT32) {
    DLOG(ERROR) << "Only TENSOR_FLOAT32 operand type is supported";
    return mojom::BAD_DATA;
  }
  layout = {.data_type = cldnn_f32, .format = format, .padding = {}};
  if (dimensions.size() == 1) {
    layout.size = {1, 1, 2, {1, 1, dimensions[0], 1, 1, 1, 1, 1}};
  } else if (dimensions.size() == 2) {
    // HW -> {batch, feature, width, height}
    layout.size = {1, 1, 2, {1, 1, dimensions[1], dimensions[0], 1, 1, 1, 1}};
  } else if (dimensions.size() == 3) {
    // HWC -> {batch, feature, width, height}
    layout.size = {
        1, 1, 2, {1, dimensions[2], dimensions[1], dimensions[0], 1, 1, 1, 1}};
  } else if (dimensions.size() == 4) {
    // NHWC -> {batch, feature, width, height}
    layout.size = {1,
                   1,
                   2,
                   {dimensions[0], dimensions[3], dimensions[2], dimensions[1],
                    1, 1, 1, 1}};
  } else {
    DLOG(ERROR) << "Operand dimensions size " << dimensions.size()
                << " is not supported.";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddInputLayout(uint32_t index) {
  cldnn_status status;
  cldnn_layout layout;
  const mojom::OperandPtr& operand = compilation_->GetModel()->operands[index];
  int32_t result = CldnnGetLayout(operand->type, operand->dimensions, layout,
                                  cldnn_format_byxf);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  cldnn_primitive_type_id type_id = LATE(cldnn_input_layout_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  std::string id_str = base::NumberToString(index);
  const cldnn_input_layout_desc input_layout_desc = {
      .type = type_id, .id = id_str.c_str(), .layout = layout};
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&input_layout_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add input layout primitve with id "
             << id_str;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddReorder(
    const std::string& input_name,
    const std::string& output_name,
    int32_t target_format) {
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_reorder_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_reorder_desc reorder_desc = {
      .type = type_id,
      .id = output_name.c_str(),
      .output_format = cldnn_format_type(target_format),
      .output_data_type = cldnn_f32,
  };
  // Setup inputs.
  std::vector<cldnn_primitive_id> input_ids_array(1);
  input_ids_array[0] = input_name.c_str();
  reorder_desc.input = {.data = input_ids_array.data(),
                        .size = input_ids_array.size()};
  // Setup mean mode.
  const std::string empty;
  reorder_desc.mean_subtract = empty.c_str();
  reorder_desc.subtract_per_feature = {.data = nullptr, .size = 0};
  reorder_desc.mean_mode = mean_none;

  // Add into topology.
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&reorder_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add reorder primitve with id "
             << output_name;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddData(uint32_t index) {
  cldnn_status status;
  cldnn_layout layout;
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const mojom::OperandPtr& operand = model->operands[index];
  int32_t result = CldnnGetLayout(operand->type, operand->dimensions, layout);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  cldnn_memory memory = LATE(cldnn_allocate_memory)(engine_, layout, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to allocate memory " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  memories_.push_back(memory);

  void* memory_ptr = LATE(cldnn_lock_memory)(memory, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  const mojom::OperandValueInfoPtr& value_info =
      model->values[base::NumberToString(index)];
  auto mapping = compilation_->MapMemory(index);
  if (operand->dimensions.size() == 1 || operand->dimensions.size() == 2) {
    memcpy(memory_ptr, mapping.get(), value_info->length);
  } else if (operand->dimensions.size() == 3 ||
             operand->dimensions.size() == 4) {
    // NHWC -> bfyx
    const bool rank3 = operand->dimensions.size() == 3;
    const uint32_t batches = rank3 ? 1 : operand->dimensions[0];
    const uint32_t channels =
        rank3 ? operand->dimensions[2] : operand->dimensions[3];
    const uint32_t height =
        rank3 ? operand->dimensions[0] : operand->dimensions[1];
    const uint32_t width =
        rank3 ? operand->dimensions[1] : operand->dimensions[2];
    float* dst = reinterpret_cast<float*>(memory_ptr);
    const float* src = reinterpret_cast<const float*>(mapping.get());
    for (uint32_t b = 0; b < batches; ++b) {
      for (uint32_t c = 0; c < channels; ++c) {
        for (uint32_t y = 0; y < height; ++y) {
          for (uint32_t x = 0; x < width; ++x) {
            dst[b * channels * height * width + c * height * width + y * width +
                x] = src[b * height * width * channels + y * width * channels +
                         x * channels + c];
          }
        }
      }
    }
  } else {
    DLOG(ERROR) << "Operand dimensions size " << operand->dimensions.size()
                << " is not supported.";
    return mojom::BAD_DATA;
  }

  LATE(cldnn_unlock_memory)(memory, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  cldnn_primitive_type_id type_id = LATE(cldnn_data_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  std::string id_str = base::NumberToString(index);
  const cldnn_data_desc data_desc = {
      .type = type_id, .id = id_str.c_str(), .mem = memory};
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&data_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  DLOG(INFO) << "[clDNN] succeed to add data primitive with id " << id_str;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddActivationByFusedCode(
    const std::string& input,
    const std::string& id,
    int32_t fuse_code) {
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_activation_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  cldnn_activation_desc activation_desc = {.type = type_id, .id = id.c_str()};

  // Setup inputs.
  std::vector<cldnn_primitive_id> input_ids_array(1);
  input_ids_array[0] = input.c_str();
  activation_desc.input = {.data = input_ids_array.data(),
                           .size = input_ids_array.size()};

  // Setup func and additional parameters.
  if (fuse_code == mojom::FUSED_RELU) {
    activation_desc.activation_func = activation_relu;
  } else if (fuse_code == mojom::FUSED_RELU1) {
    activation_desc.activation_func = activation_clamp;
    activation_desc.additional_params.a = 0.0;
    activation_desc.additional_params.b = 1.0;
  } else if (fuse_code == mojom::FUSED_RELU6) {
    activation_desc.activation_func = activation_clamp;
    activation_desc.additional_params.a = 0.0;
    activation_desc.additional_params.b = 6.0;
  } else {
    DLOG(ERROR) << "Fuse code " << fuse_code << " is not supported";
    return mojom::BAD_DATA;
  }

  // Setup additional_params_input as empty.
  std::string empty;
  activation_desc.additional_params_input = empty.c_str();

  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&activation_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add activation primitive with id " << id;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddElementwise(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_eltwise_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_eltwise_desc eltwise_desc = {.type = type_id};

  // Setup inputs.
  const int32_t inputs_count = 2;
  std::vector<std::string> input_ids(inputs_count);
  std::vector<cldnn_primitive_id> input_ids_array(inputs_count);
  for (size_t i = 0; i < inputs_count; ++i) {
    input_ids[i] = base::NumberToString(inputs[i]);
    input_ids_array[i] = input_ids[i].c_str();
    // Setup constants
    const mojom::ModelInfoPtr& model = compilation_->GetModel();
    if (model->values.find(base::NumberToString(inputs[i])) !=
        model->values.end()) {
      int32_t result = CldnnAddData(inputs[i]);
      if (result != mojom::NOT_ERROR) {
        return result;
      }
    }
  }
  eltwise_desc.input = {.data = input_ids_array.data(),
                        .size = input_ids_array.size()};

  // Setup mode.
  if (type == mojom::ADD) {
    eltwise_desc.mode = cldnn_eltwise_sum;
  } else if (type == mojom::MUL) {
    eltwise_desc.mode = cldnn_eltwise_prod;
  }

  // Use output index as primitive id.
  uint32_t output_index = outputs[0];
  std::string id_str(base::NumberToString(output_index));

  // Setup activiation.
  int32_t fuse_code = compilation_->GetScalarInt32(inputs[2]);
  if (fuse_code == mojom::FUSED_NONE) {
    eltwise_desc.with_activation = 0;
  } else if (fuse_code == mojom::FUSED_RELU) {
    eltwise_desc.with_activation = 1;
    eltwise_desc.activation_negative_slope = 0.0;
  } else if (fuse_code == mojom::FUSED_RELU1 ||
             fuse_code == mojom::FUSED_RELU6) {
    eltwise_desc.with_activation = 0;
    id_str = id_str + "-func";
  } else {
    DLOG(ERROR) << "Fuse code " << fuse_code << " is not supported";
    return mojom::BAD_DATA;
  }

  // Setup output quanitization factors
  std::string empty;
  eltwise_desc.output_calibration_factors = empty.c_str();
  eltwise_desc.output_quantization_factor = 1.0f;

  // Setup coefficient for SUM operation
  eltwise_desc.coefficients.data = nullptr;
  eltwise_desc.coefficients.size = 0;

  // Add primitive into topology.
  eltwise_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&eltwise_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add eltwise primitive with id " << id_str;

  // Handle RELU1 and RELU6 fused code as dedicated activation primitive.
  if (fuse_code == mojom::FUSED_RELU1 || fuse_code == mojom::FUSED_RELU6) {
    std::string fuse_id_str = base::NumberToString(output_index);
    int32_t result =
        CldnnAddActivationByFusedCode(id_str, fuse_id_str, fuse_code);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddConvolution(
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

  // Create convolution descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_convolution_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_convolution_desc conv_desc = {.type = type_id};

  // Setup inputs.
  std::vector<cldnn_primitive_id> input_ids_array(1);
  const std::string input_id = base::NumberToString(input_index);
  input_ids_array[0] = input_id.c_str();
  conv_desc.input = {.data = input_ids_array.data(),
                     .size = input_ids_array.size()};

  // Setup weights and bias.
  std::vector<cldnn_primitive_id> weight_ids_array;
  std::vector<std::string> weight_ids;
  std::vector<cldnn_primitive_id> bias_ids_array;
  std::vector<std::string> bias_ids;
  if (depthwise) {
    auto weights_mapping = compilation_->MapMemory(filter_idx);
    const float* weights_value_ptr =
        reinterpret_cast<const float*>(weights_mapping.get());
    const cldnn_layout weights_layout = {
        .data_type = cldnn_f32,
        .format = cldnn_format_bfyx,
        .size = {1, 1, 2, {1, 1, filter_width, filter_height, 1, 1, 1, 1}},
        .padding = {}};
    weight_ids_array.resize(depth_out);
    weight_ids.resize(depth_out);

    auto bias_mapping = compilation_->MapMemory(bias_idx);
    const float* bias_value_ptr =
        reinterpret_cast<const float*>(bias_mapping.get());
    const cldnn_layout bias_layout = {
        .data_type = cldnn_f32,
        .format = cldnn_format_bfyx,
        .size = {1, 1, 2, {1, 1, 1, 1, 1, 1, 1, 1}},
        .padding = {}};
    bias_ids_array.resize(depth_out);
    bias_ids.resize(depth_out);
    for (int32_t c = 0; c < depth_out; ++c) {
      cldnn_memory weights_memory =
          LATE(cldnn_allocate_memory)(engine_, weights_layout, &status);
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to allocate memory " << status << " "
                    << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      memories_.push_back(weights_memory);

      float* filter_ptr = reinterpret_cast<float*>(
          LATE(cldnn_lock_memory)(weights_memory, &status));
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
                    << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }

      for (int32_t y = 0; y < filter_height; ++y) {
        for (int32_t x = 0; x < filter_width; ++x) {
          filter_ptr[y * filter_width + x] =
              weights_value_ptr[y * filter_width * depth_out + x * depth_out +
                                c];
        }
      }
      LATE(cldnn_unlock_memory)(weights_memory, &status);
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
                    << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }

      cldnn_primitive_type_id type_id = LATE(cldnn_data_type_id)(&status);
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status
                    << " " << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      std::string id_str = base::NumberToString(filter_idx) + std::string("-") +
                           base::NumberToString(c);
      weight_ids[c] = id_str;
      weight_ids_array[c] = weight_ids[c].c_str();
      const cldnn_data_desc weights_data_desc = {
          .type = type_id, .id = id_str.c_str(), .mem = weights_memory};
      LATE(cldnn_add_primitive)
      (topology_,
       reinterpret_cast<const cldnn_primitive_desc*>(&weights_data_desc),
       &status);
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                    << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      DLOG(INFO) << "[clDNN] succeed to add data primitive with id " << id_str;

      cldnn_memory bias_memory =
          LATE(cldnn_allocate_memory)(engine_, bias_layout, &status);
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to allocate memory " << status << " "
                    << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      memories_.push_back(bias_memory);

      float* bias_ptr = reinterpret_cast<float*>(
          LATE(cldnn_lock_memory)(bias_memory, &status));
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
                    << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }

      *bias_ptr = *(bias_value_ptr + c);

      LATE(cldnn_unlock_memory)(bias_memory, &status);
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
                    << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }

      id_str = base::NumberToString(bias_idx) + std::string("-") +
               base::NumberToString(c);
      bias_ids[c] = id_str;
      bias_ids_array[c] = bias_ids[c].c_str();
      const cldnn_data_desc bias_data_desc = {
          .type = type_id, .id = id_str.c_str(), .mem = bias_memory};
      LATE(cldnn_add_primitive)
      (topology_,
       reinterpret_cast<const cldnn_primitive_desc*>(&bias_data_desc), &status);
      if (status != CLDNN_SUCCESS) {
        DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                    << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      DLOG(INFO) << "[clDNN] succeed to add data primitive with id " << id_str;
    }
    conv_desc.weights = {.data = weight_ids_array.data(),
                         .size = weight_ids_array.size()};
    conv_desc.bias = {.data = bias_ids_array.data(),
                      .size = bias_ids_array.size()};
    conv_desc.split = depth_out;
  } else {
    CldnnAddData(filter_idx);
    weight_ids_array.resize(1);
    weight_ids.resize(1);
    weight_ids[0] = base::NumberToString(filter_idx);
    weight_ids_array[0] = weight_ids[0].c_str();
    conv_desc.weights = {.data = weight_ids_array.data(),
                         .size = weight_ids_array.size()};

    CldnnAddData(bias_idx);
    bias_ids_array.resize(1);
    bias_ids.resize(1);
    bias_ids[0] = base::NumberToString(bias_idx);
    bias_ids_array[0] = bias_ids[0].c_str();
    conv_desc.bias = {.data = bias_ids_array.data(),
                      .size = bias_ids_array.size()};

    conv_desc.split = 1;
  }

  // Setup paddings.
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

  conv_desc.input_offset = {
      1, 1, 2, {0, 0, -padding_left, -padding_top, 0, 0, 0, 0}};

  // Setup stride.
  conv_desc.stride = {1, 1, 2, {1, 1, stride_width, stride_height, 1, 1, 1, 1}};

  std::string id_str = base::NumberToString(output_index);
  // Setup activation.
  conv_desc.activation_negative_slope = 0.0;
  if (fuse_code == mojom::FUSED_NONE) {
    conv_desc.with_activation = 0;
  } else if (fuse_code == mojom::FUSED_RELU) {
    conv_desc.with_activation = 1;
  } else if (fuse_code == mojom::FUSED_RELU1 ||
             fuse_code == mojom::FUSED_RELU6) {
    conv_desc.with_activation = 0;
    id_str = id_str + "-func";
  } else {
    DLOG(ERROR) << "Fuse code " << fuse_code << " is not supported";
    return mojom::BAD_DATA;
  }

  // Setup dilation.
  conv_desc.dilation = {
      1, 1, 2, {1, 1, dilation_width, dilation_height, 1, 1, 1, 1}};

  // Setup output.
  conv_desc.with_output_size = 1;
  conv_desc.output_size = {
      1,
      1,
      2,
      {output_batch, output_channel, output_width, output_height, 1, 1, 1, 1}};

  // Add primitive into topology.
  conv_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&conv_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add conv primitive with id " << id_str;

  // Handle RELU1 and RELU6 fused code as dedicated activation primitive.
  if (fuse_code == mojom::FUSED_RELU1 || fuse_code == mojom::FUSED_RELU6) {
    std::string fuse_id_str = base::NumberToString(output_index);
    int32_t result =
        CldnnAddActivationByFusedCode(id_str, fuse_id_str, fuse_code);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddPooling(
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

  // Create pooling descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_pooling_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_pooling_desc pool_desc = {.type = type_id};

  // Setup inputs.
  std::vector<cldnn_primitive_id> input_ids_array(1);
  const std::string input_id = base::NumberToString(input_index);
  input_ids_array[0] = input_id.c_str();
  pool_desc.input = {.data = input_ids_array.data(),
                     .size = input_ids_array.size()};

  // Setup mode.
  if (type == mojom::MAX_POOL_2D) {
    pool_desc.mode = cldnn_pooling_max;
  } else if (type == mojom::AVERAGE_POOL_2D) {
    pool_desc.mode = cldnn_pooling_average;
  } else {
    DLOG(ERROR) << "Pooling mode " << type << " is not supported";
    return mojom::BAD_DATA;
  }

  // Setup kernel size.
  pool_desc.size = {1, 1, 2, {1, 1, filter_width, filter_height, 1, 1, 1, 1}};

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
  pool_desc.input_offset = {
      1, 1, 2, {0, 0, -padding_left, -padding_top, 0, 0, 0, 0}};

  // Setup stride.
  pool_desc.stride = {1, 1, 2, {1, 1, stride_width, stride_height, 1, 1, 1, 1}};

  // Setup output.
  pool_desc.with_output_size = 1;
  pool_desc.output_size = {
      1,
      1,
      2,
      {output_batch, output_channel, output_width, output_height, 1, 1, 1, 1}};

  // Setup argmax.
  std::string empty;
  pool_desc.argmax = empty.c_str();

  // Setup fuse code.
  std::string id_str = base::NumberToString(output_index);
  if (fuse_code != mojom::FUSED_NONE) {
    id_str = id_str + "-func";
  }

  // Add primitive into topology.
  pool_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&pool_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add pooling primitive with id " << id_str;

  // Handle fused code as dedicated activation primitive.
  if (fuse_code != mojom::FUSED_NONE) {
    std::string fuse_id_str = base::NumberToString(output_index);
    int32_t result =
        CldnnAddActivationByFusedCode(id_str, fuse_id_str, fuse_code);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddSoftmax(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_softmax_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_softmax_desc softmax_desc = {.type = type_id};

  // Setup inputs.
  std::vector<std::string> input_ids(1);
  std::vector<cldnn_primitive_id> input_ids_array(1);
  input_ids[0] = base::NumberToString(inputs[0]);
  input_ids_array[0] = input_ids[0].c_str();
  softmax_desc.input = {.data = input_ids_array.data(),
                        .size = input_ids_array.size()};

  // Check beta.
  const float beta = compilation_->GetScalarFloat(inputs[1]);
  DLOG(INFO) << "  beta: " << beta;
  if (beta != 1.0) {
    DLOG(ERROR) << "beta " << beta << " is not supported.";
    return mojom::BAD_DATA;
  }

  // Setup dimension.
  softmax_desc.dimension = cldnn_softmax_normalize_fyx;

  // Setup id and add into topology.
  std::string id_str(base::NumberToString(outputs[0]));
  softmax_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&softmax_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add softmax primitive with id " << id_str;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddReshape(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_reshape_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_reshape_desc reshape_desc = {.type = type_id};

  // Setup inputs.
  std::vector<std::string> input_ids(1);
  std::vector<cldnn_primitive_id> input_ids_array(1);
  input_ids[0] = base::NumberToString(inputs[0]);
  input_ids_array[0] = input_ids[0].c_str();
  reshape_desc.input = {.data = input_ids_array.data(),
                        .size = input_ids_array.size()};

  // Setup output shape.
  cldnn_layout layout;
  const mojom::OperandPtr& operand =
      compilation_->GetModel()->operands[outputs[0]];
  int32_t result = CldnnGetLayout(operand->type, operand->dimensions, layout);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  reshape_desc.output_shape = layout.size;

  // Setup id and add into topology.
  std::string id_str(base::NumberToString(outputs[0]));
  reshape_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&reshape_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add reshape primitive with id " << id_str;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddConcatenation(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_concatenation_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_concatenation_desc concat_desc = {.type = type_id};

  // Setup inputs.
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const int32_t inputs_count = inputs.size() - 1;
  std::vector<std::string> input_ids(inputs_count);
  std::vector<cldnn_primitive_id> input_ids_array(inputs_count);
  for (int32_t i = 0; i < inputs_count; ++i) {
    input_ids[i] = base::NumberToString(inputs[i]);
    input_ids_array[i] = input_ids[i].c_str();
    // Add constants.
    if (model->values.find(base::NumberToString(inputs[i])) !=
        model->values.end()) {
      int32_t result = CldnnAddData(inputs[i]);
      if (result != mojom::NOT_ERROR) {
        return result;
      }
    }
  }
  concat_desc.input = {.data = input_ids_array.data(),
                       .size = input_ids_array.size()};

  // Setup axis
  const uint32_t rank = model->operands[inputs[0]]->dimensions.size();
  const int32_t axis = compilation_->GetScalarInt32(inputs[inputs.size() - 1]);
  if (rank == 1) {
    if (axis == 0) {
      concat_desc.axis = cldnn_concatenation_along_x;
    } else {
      DLOG(ERROR) << "axis " << axis << " is not supported.";
      return mojom::BAD_DATA;
    }
  } else if (rank == 2) {
    // HW -> yx
    if (axis == 0) {
      concat_desc.axis = cldnn_concatenation_along_y;
    } else if (axis == 1) {
      concat_desc.axis = cldnn_concatenation_along_x;
    } else {
      DLOG(ERROR) << "axis " << axis << " is not supported.";
      return mojom::BAD_DATA;
    }
  } else if (rank == 3) {
    // HWC -> yxf
    if (axis == 0) {
      concat_desc.axis = cldnn_concatenation_along_y;
    } else if (axis == 1) {
      concat_desc.axis = cldnn_concatenation_along_x;
    } else if (axis == 2) {
      concat_desc.axis = cldnn_concatenation_along_f;
    } else {
      DLOG(ERROR) << "axis " << axis << " is not supported.";
      return mojom::BAD_DATA;
    }
  } else if (rank == 4) {
    // NHWC -> byxf
    if (axis == 0) {
      concat_desc.axis = cldnn_concatenation_along_b;
    } else if (axis == 1) {
      concat_desc.axis = cldnn_concatenation_along_y;
    } else if (axis == 2) {
      concat_desc.axis = cldnn_concatenation_along_x;
    } else if (axis == 3) {
      concat_desc.axis = cldnn_concatenation_along_f;
    } else {
      DLOG(ERROR) << "axis " << axis << " is not supported.";
      return mojom::BAD_DATA;
    }
  } else {
    DLOG(ERROR) << "rank " << rank << " is not supported.";
    return mojom::BAD_DATA;
  }

  // Setup id and add into topology.
  std::string id_str(base::NumberToString(outputs[0]));
  concat_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&concat_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add concatenation primitive with id "
             << id_str;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddFullyConnected(
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

  // Create fully_connected descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id =
      LATE(cldnn_fully_connected_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_fully_connected_desc fc_desc = {.type = type_id};

  // Setup inputs.
  // FC only accepts yxfb, bfyx, byxf_af32, so reorder to bfyx in case input
  // is byxf.
  const std::string reorder_input_name(base::NumberToString(input_index));
  const std::string reorder_output_name(reorder_input_name +
                                        std::string("-reordered"));
  CldnnAddReorder(reorder_input_name, reorder_output_name, cldnn_format_bfyx);
  // Reshape to [input_batch_size, input_size]
  type_id = LATE(cldnn_reshape_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_reshape_desc reshape_desc = {.type = type_id};
  std::vector<cldnn_primitive_id> input_ids_array(1);
  std::string reshape_input_str(reorder_output_name);
  input_ids_array[0] = reshape_input_str.c_str();
  reshape_desc.input = {.data = input_ids_array.data(),
                        .size = input_ids_array.size()};

  // Setup output shape.
  reshape_desc.output_shape = {
      1, 1, 2, {input_batch_size, 1, input_size, 1, 1, 1, 1, 1}};

  // Setup id and add into topology.
  std::string reshape_id_str(base::NumberToString(input_index) +
                             std::string("-reshaped"));
  reshape_desc.id = reshape_id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&reshape_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add reshape primitive with id "
             << reshape_id_str;

  // Setup fc inputs.
  input_ids_array.clear();
  input_ids_array.resize(1);
  input_ids_array[0] = reshape_id_str.c_str();
  fc_desc.input = {.data = input_ids_array.data(),
                   .size = input_ids_array.size()};

  // Setup weights.
  // b - stands for size of the output (num_units)
  // x - stands for size of input (input_size)
  // refer to cldnn test fully_connected_gpu_test.cpp
  const cldnn_layout weights_layout = {
      .data_type = cldnn_f32,
      .format = cldnn_format_bfyx,
      .size = {1, 1, 2, {num_units, 1, input_size, 1, 1, 1, 1, 1}},
      .padding = {}};
  cldnn_memory weights_memory =
      LATE(cldnn_allocate_memory)(engine_, weights_layout, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to allocate memory " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  float* weights_memory_ptr = reinterpret_cast<float*>(
      LATE(cldnn_lock_memory)(weights_memory, &status));
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  const mojom::OperandValueInfoPtr& weights_info =
      model->values[base::NumberToString(weights_idx)];
  auto mapping = compilation_->MapMemory(weights_idx);
  memcpy(weights_memory_ptr, mapping.get(), weights_info->length);

  LATE(cldnn_unlock_memory)(weights_memory, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  type_id = LATE(cldnn_data_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  const std::string weights_idx_str(base::NumberToString(weights_idx));
  const cldnn_data_desc weights_data_desc = {
      .type = type_id, .id = weights_idx_str.c_str(), .mem = weights_memory};
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&weights_data_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add data primitive with id "
             << weights_idx_str;

  fc_desc.weights = weights_idx_str.c_str();

  // Setup bias.
  CldnnAddData(bias_idx);
  const std::string bias_idx_str(base::NumberToString(bias_idx));
  fc_desc.bias = bias_idx_str.c_str();

  std::string id_str(base::NumberToString(output_index));
  // Setup activation.
  fc_desc.activation_negative_slope = 0.0;
  if (fuse_code == mojom::FUSED_NONE) {
    fc_desc.with_activation = 0;
  } else if (fuse_code == mojom::FUSED_RELU) {
    fc_desc.with_activation = 1;
  } else if (fuse_code == mojom::FUSED_RELU1 ||
             fuse_code == mojom::FUSED_RELU6) {
    fc_desc.with_activation = 0;
    id_str = id_str + "-func";
  } else {
    DLOG(ERROR) << "Fuse code " << fuse_code << " is not supported";
    return mojom::BAD_DATA;
  }

  // Setup weights_quantization_factors and output_calibration_factors as empty
  std::string empty("");
  fc_desc.weights_quantization_factors = empty.c_str();
  fc_desc.output_calibration_factors = empty.c_str();

  // Setup input_quantization_factor and output_quantization_factor as zero
  fc_desc.input_quantization_factor = 0.0;
  fc_desc.output_quantization_factor = 0.0;

  // Add primitive into topology.
  fc_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&fc_desc), &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add fc primitive with id " << id_str;

  // Handle RELU1 and RELU6 fused code as dedicated activation primitive.
  if (fuse_code == mojom::FUSED_RELU1 || fuse_code == mojom::FUSED_RELU6) {
    std::string fuse_id_str = base::NumberToString(output_index);
    int32_t result =
        CldnnAddActivationByFusedCode(id_str, fuse_id_str, fuse_code);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddResizeBilinear(
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

  cldnn_status status;
  cldnn_primitive_type_id type_id =
      LATE(cldnn_custom_gpu_primitive_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_custom_gpu_primitive_desc custom_desc = {.type = type_id};

  // Setup inputs.
  // interp kernel optimizes for yxfb foramt, reorder to yxfb.
  std::string reorder_input(base::NumberToString(inputs[0]));
  std::string reorder_output(reorder_input + std::string("-reordered"));
  CldnnAddReorder(reorder_input, reorder_output, cldnn_format_yxfb);

  std::vector<cldnn_primitive_id> input_ids_array = {reorder_output.c_str()};
  custom_desc.input = {.data = input_ids_array.data(),
                       .size = input_ids_array.size()};

  // Setup kernel source and entry point
  std::string kernel(kInterpKernelSource);
  std::vector<cldnn_primitive_id> kernel_array = {kernel.c_str()};
  custom_desc.kernels_code = {.data = kernel_array.data(),
                              .size = kernel_array.size()};
  std::string entry_point(kInterpKernelEntryPoint);
  custom_desc.kernel_entry_point = entry_point.c_str();

  // Setup kernel arguments
  std::vector<cldnn_arg> parameters = {{arg_input, 0}, {arg_output, 0}};
  custom_desc.kernel_arguments = parameters.data();
  custom_desc.kernel_arguments_num = parameters.size();

  // Setup build options
  std::string build_options("-cl-mad-enable ");
  build_options += std::string("-Dpad_beg_=0 -Dpad_end_=0");
  custom_desc.build_options = build_options.c_str();

  // Setup output layout
  cldnn_layout output_layout;
  const mojom::OperandPtr& operand =
      compilation_->GetModel()->operands[outputs[0]];
  int32_t result = CldnnGetLayout(operand->type, operand->dimensions,
                                  output_layout, cldnn_format_yxfb);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  custom_desc.output_layout = output_layout;

  // Setup work group size
  std::vector<size_t> gws = {new_height, new_width};
  custom_desc.gws = gws.data();
  custom_desc.gws_num = gws.size();
  std::vector<size_t> lws = {};
  custom_desc.lws = lws.data();
  custom_desc.lws_num = lws.size();

  // Setup id and add into topology.
  std::string id_str(base::NumberToString(outputs[0]) + std::string("-yxfb"));
  custom_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&custom_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add custom_gpu primitive with id "
             << id_str;

  // insert a reorder back to bfyx
  CldnnAddReorder(id_str, base::NumberToString(outputs[0]), cldnn_format_bfyx);
  return mojom::NOT_ERROR;
}

}  // namespace ml
