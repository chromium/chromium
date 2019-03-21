// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_cl_dnn.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/cl_dnn_custom_kernels.h"
#include "services/ml/cl_dnn_symbol_table.h"
#include "services/ml/execution_impl_cl_dnn.h"
#include "services/ml/public/mojom/constants.mojom.h"
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
constexpr char kClDnnVersion[] = "21.1";
#endif

namespace ml {

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
      LOG(ERROR) << "[clDNN] failed to release cldnn memory " << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
    }
  }
  DLOG(INFO) << "[clDNN] succeed to release memories";

  if (topology_) {
    LATE(cldnn_release_topology)(topology_, &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to release cldnn topology " << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
    }
    DLOG(INFO) << "[clDNN] succeed to release topology";
  }

  if (engine_) {
    LATE(cldnn_release_engine)(engine_, &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to release cldnn engine " << status << " "
                 << std::string(LATE(cldnn_get_last_error_message)());
    }
    DLOG(INFO) << "[clDNN] succeed to release engine";
  }

  if (program_) {
    LATE(cldnn_release_program)(program_, &status);
    if (status != CLDNN_SUCCESS) {
      LOG(ERROR) << "[clDNN] failed to release program " << status << " "
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

int32_t CompilationDelegateClDnn::CreateExecution(
    std::unique_ptr<mojom::Execution>& execution,
    mojom::ExecutionInitParamsPtr params) {
  execution = std::make_unique<ExecutionImplClDnn>(this, std::move(params));
  return mojom::NOT_ERROR;
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
    LOG(ERROR) << "[clDNN] failed to get cldnn version";
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
    LOG(ERROR) << "[clDNN]: failed to get cldnn ocl engine count " << status
               << " " << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] ocl engine count: " << engine_count;
  if (engine_count < 1) {
    LOG(ERROR) << "[clDNN] ocl engine is not available " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  engine_ = LATE(cldnn_create_engine)(cldnn_engine_ocl, 0, nullptr, &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to create cldnn ocl engine " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    engine_ = nullptr;
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeeded to create cldnn ocl engine " << engine_;

  cldnn_engine_info engine_info = LATE(cldnn_get_engine_info)(engine_, &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get cldnn engine info " << status << " "
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
    LOG(ERROR) << "[clDNN] failed to create cldnn topology " << status << " "
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
    if (operation->outputs.size() != 1) {
      LOG(ERROR) << "Only 1 output is supported";
      return mojom::BAD_DATA;
    }
    const int32_t type = operation->type;
    int32_t result = mojom::NOT_ERROR;
    if (type == mojom::ADD || type == mojom::MUL) {
      result = CldnnAddElementwise(operation);
    } else if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D ||
               type == mojom::ATROUS_CONV_2D ||
               type == mojom::ATROUS_DEPTHWISE_CONV_2D) {
      result = CldnnAddConvolution(operation);
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      result = CldnnAddPooling(operation);
    } else if (type == mojom::SOFTMAX) {
      result = CldnnAddSoftmax(operation);
    } else if (type == mojom::RESHAPE) {
      result = CldnnAddReshape(operation);
    } else if (type == mojom::CONCATENATION) {
      result = CldnnAddConcatenation(operation);
    } else if (type == mojom::FULLY_CONNECTED) {
      result = CldnnAddFullyConnected(operation);
    } else if (type == mojom::RESIZE_BILINEAR) {
      result = CldnnAddResizeBilinear(operation);
    } else {
      LOG(ERROR) << "Operation type " << type << " is not supported.";
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
    LOG(ERROR) << "[clDNN] failed to build program " << status << " "
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
    LOG(ERROR) << "Only TENSOR_FLOAT32 operand type is supported";
    return mojom::BAD_DATA;
  }
  layout = {.data_type = cldnn_f32, .format = format, .padding = {}};
  if (dimensions.size() == 1) {
    layout.size = {1, 1, 2, 0, {1, 1, dimensions[0], 1, 1, 1, 1, 1}};
  } else if (dimensions.size() == 2) {
    // HW -> {batch, feature, width, height}
    layout.size = {
        1, 1, 2, 0, {1, 1, dimensions[1], dimensions[0], 1, 1, 1, 1}};
  } else if (dimensions.size() == 3) {
    // HWC -> {batch, feature, width, height}
    layout.size = {
        1,
        1,
        2,
        0,
        {1, dimensions[2], dimensions[1], dimensions[0], 1, 1, 1, 1}};
  } else if (dimensions.size() == 4) {
    // NHWC -> {batch, feature, width, height}
    layout.size = {1,
                   1,
                   2,
                   0,
                   {dimensions[0], dimensions[3], dimensions[2], dimensions[1],
                    1, 1, 1, 1}};
  } else {
    LOG(ERROR) << "Operand dimensions size " << dimensions.size()
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
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
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
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
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
    LOG(ERROR) << "[clDNN] failed to allocate memory " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  memories_.push_back(memory);

  void* memory_ptr = LATE(cldnn_lock_memory)(memory, &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
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
    LOG(ERROR) << "Operand dimensions size " << operand->dimensions.size()
               << " is not supported.";
    return mojom::BAD_DATA;
  }

  LATE(cldnn_unlock_memory)(memory, &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  cldnn_primitive_type_id type_id = LATE(cldnn_data_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
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
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
    activation_desc.additional_params.a = -1.0;
    activation_desc.additional_params.b = 1.0;
  } else if (fuse_code == mojom::FUSED_RELU6) {
    activation_desc.activation_func = activation_clamp;
    activation_desc.additional_params.a = 0.0;
    activation_desc.additional_params.b = 6.0;
  } else {
    LOG(ERROR) << "Fuse code " << fuse_code << " is not supported";
    return mojom::BAD_DATA;
  }

  // Setup additional_params_input as empty.
  std::string empty;
  activation_desc.additional_params_input = empty.c_str();

  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&activation_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add activation primitive with id " << id;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddElementwise(
    const mojom::OperationPtr& operation) {
  // Setup element-wise parameters.
  ElementWiseParams params;
  int32_t result = compilation_->GetElementWiseParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  const std::vector<uint32_t>& inputs = operation->inputs;
  const uint32_t output_index = operation->outputs[0];

  // Create element-wise descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_eltwise_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
      result = CldnnAddData(inputs[i]);
      if (result != mojom::NOT_ERROR) {
        return result;
      }
    }
  }
  eltwise_desc.input = {.data = input_ids_array.data(),
                        .size = input_ids_array.size()};

  // Setup mode.
  if (operation->type == mojom::ADD) {
    eltwise_desc.mode = cldnn_eltwise_sum;
  } else if (operation->type == mojom::MUL) {
    eltwise_desc.mode = cldnn_eltwise_prod;
  }

  // Use output index as primitive id.
  std::string id_str(base::NumberToString(output_index));

  // Setup activiation.
  if (params.fuse_code == mojom::FUSED_NONE) {
    eltwise_desc.with_activation = 0;
  } else if (params.fuse_code == mojom::FUSED_RELU) {
    eltwise_desc.with_activation = 1;
    eltwise_desc.activation_negative_slope = 0.0;
  } else if (params.fuse_code == mojom::FUSED_RELU1 ||
             params.fuse_code == mojom::FUSED_RELU6) {
    eltwise_desc.with_activation = 0;
    id_str = id_str + "-func";
  } else {
    LOG(ERROR) << "Fuse code " << params.fuse_code << " is not supported";
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
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add eltwise primitive with id " << id_str;

  // Handle RELU1 and RELU6 fused code as dedicated activation primitive.
  if (params.fuse_code == mojom::FUSED_RELU1 ||
      params.fuse_code == mojom::FUSED_RELU6) {
    std::string fuse_id_str = base::NumberToString(output_index);
    int32_t result =
        CldnnAddActivationByFusedCode(id_str, fuse_id_str, params.fuse_code);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddConvolution(
    const mojom::OperationPtr& operation) {
  // Setup convolution params.
  ConvParams params;
  int32_t result = compilation_->GetConvParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  const uint32_t input_index = operation->inputs[0];
  const uint32_t weights_index = operation->inputs[1];
  const uint32_t bias_index = operation->inputs[2];
  const uint32_t output_index = operation->outputs[0];

  // Create convolution descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_convolution_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
  if (params.depthwise) {
    auto weights_mapping = compilation_->MapMemory(weights_index);
    const float* weights_value_ptr =
        reinterpret_cast<const float*>(weights_mapping.get());
    const cldnn_layout weights_layout = {
        .data_type = cldnn_f32,
        .format = cldnn_format_bfyx,
        .size = {1,
                 1,
                 2,
                 0,
                 {1, 1, params.filter_width, params.filter_height, 1, 1, 1, 1}},
        .padding = {}};
    weight_ids_array.resize(params.depth_out);
    weight_ids.resize(params.depth_out);

    auto bias_mapping = compilation_->MapMemory(bias_index);
    const float* bias_value_ptr =
        reinterpret_cast<const float*>(bias_mapping.get());
    const cldnn_layout bias_layout = {
        .data_type = cldnn_f32,
        .format = cldnn_format_bfyx,
        .size = {1, 1, 2, 0, {1, 1, 1, 1, 1, 1, 1, 1}},
        .padding = {}};
    bias_ids_array.resize(params.depth_out);
    bias_ids.resize(params.depth_out);
    for (size_t c = 0; c < params.depth_out; ++c) {
      cldnn_memory weights_memory =
          LATE(cldnn_allocate_memory)(engine_, weights_layout, &status);
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to allocate memory " << status << " "
                   << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      memories_.push_back(weights_memory);

      float* filter_ptr = reinterpret_cast<float*>(
          LATE(cldnn_lock_memory)(weights_memory, &status));
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
                   << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }

      for (size_t y = 0; y < params.filter_height; ++y) {
        for (size_t x = 0; x < params.filter_width; ++x) {
          filter_ptr[y * params.filter_width + x] =
              weights_value_ptr[y * params.filter_width * params.depth_out +
                                x * params.depth_out + c];
        }
      }
      LATE(cldnn_unlock_memory)(weights_memory, &status);
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
                   << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }

      cldnn_primitive_type_id type_id = LATE(cldnn_data_type_id)(&status);
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to get primitive type id " << status
                   << " " << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      std::string id_str = base::NumberToString(weights_index) +
                           std::string("-") + base::NumberToString(c);
      weight_ids[c] = id_str;
      weight_ids_array[c] = weight_ids[c].c_str();
      const cldnn_data_desc weights_data_desc = {
          .type = type_id, .id = id_str.c_str(), .mem = weights_memory};
      LATE(cldnn_add_primitive)
      (topology_,
       reinterpret_cast<const cldnn_primitive_desc*>(&weights_data_desc),
       &status);
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                   << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      DLOG(INFO) << "[clDNN] succeed to add data primitive with id " << id_str;

      cldnn_memory bias_memory =
          LATE(cldnn_allocate_memory)(engine_, bias_layout, &status);
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to allocate memory " << status << " "
                   << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      memories_.push_back(bias_memory);

      float* bias_ptr = reinterpret_cast<float*>(
          LATE(cldnn_lock_memory)(bias_memory, &status));
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
                   << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      *bias_ptr = *(bias_value_ptr + c);

      LATE(cldnn_unlock_memory)(bias_memory, &status);
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
                   << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }

      id_str = base::NumberToString(bias_index) + std::string("-") +
               base::NumberToString(c);
      bias_ids[c] = id_str;
      bias_ids_array[c] = bias_ids[c].c_str();
      const cldnn_data_desc bias_data_desc = {
          .type = type_id, .id = id_str.c_str(), .mem = bias_memory};
      LATE(cldnn_add_primitive)
      (topology_,
       reinterpret_cast<const cldnn_primitive_desc*>(&bias_data_desc), &status);
      if (status != CLDNN_SUCCESS) {
        LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
                   << std::string(LATE(cldnn_get_last_error_message)());
        return mojom::OP_FAILED;
      }
      DLOG(INFO) << "[clDNN] succeed to add data primitive with id " << id_str;
    }
    conv_desc.weights = {.data = weight_ids_array.data(),
                         .size = weight_ids_array.size()};
    conv_desc.bias = {.data = bias_ids_array.data(),
                      .size = bias_ids_array.size()};
    conv_desc.split = params.depth_out;
  } else {
    CldnnAddData(weights_index);
    weight_ids_array.resize(1);
    weight_ids.resize(1);
    weight_ids[0] = base::NumberToString(weights_index);
    weight_ids_array[0] = weight_ids[0].c_str();
    conv_desc.weights = {.data = weight_ids_array.data(),
                         .size = weight_ids_array.size()};

    CldnnAddData(bias_index);
    bias_ids_array.resize(1);
    bias_ids.resize(1);
    bias_ids[0] = base::NumberToString(bias_index);
    bias_ids_array[0] = bias_ids[0].c_str();
    conv_desc.bias = {.data = bias_ids_array.data(),
                      .size = bias_ids_array.size()};

    conv_desc.split = 1;
  }

  conv_desc.input_offset = {
      1,
      1,
      2,
      0,
      {0, 0, -params.padding_left, -params.padding_top, 0, 0, 0, 0}};

  // Setup stride.
  conv_desc.stride = {
      1,
      1,
      2,
      0,
      {1, 1, params.stride_width, params.stride_height, 1, 1, 1, 1}};

  std::string id_str = base::NumberToString(output_index);
  // Setup activation.
  conv_desc.activation_negative_slope = 0.0;
  if (params.fuse_code == mojom::FUSED_NONE) {
    conv_desc.with_activation = 0;
  } else if (params.fuse_code == mojom::FUSED_RELU) {
    conv_desc.with_activation = 1;
  } else if (params.fuse_code == mojom::FUSED_RELU1 ||
             params.fuse_code == mojom::FUSED_RELU6) {
    conv_desc.with_activation = 0;
    id_str = id_str + "-func";
  } else {
    LOG(ERROR) << "Fuse code " << params.fuse_code << " is not supported";
    return mojom::BAD_DATA;
  }

  // Setup dilation.
  conv_desc.dilation = {
      1,
      1,
      2,
      0,
      {1, 1, params.dilation_width, params.dilation_height, 1, 1, 1, 1}};

  // Setup output.
  conv_desc.with_output_size = 1;
  conv_desc.output_size = {
      1,
      1,
      2,
      0,
      {params.output_batch, params.output_channel, params.output_width,
       params.output_height, 1, 1, 1, 1}};

  // Add primitive into topology.
  conv_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&conv_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add conv primitive with id " << id_str;

  // Handle RELU1 and RELU6 fused code as dedicated activation primitive.
  if (params.fuse_code == mojom::FUSED_RELU1 ||
      params.fuse_code == mojom::FUSED_RELU6) {
    std::string fuse_id_str = base::NumberToString(output_index);
    result =
        CldnnAddActivationByFusedCode(id_str, fuse_id_str, params.fuse_code);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddPooling(
    const mojom::OperationPtr& operation) {
  // Setup pooling params.
  PoolingParams params;
  int32_t result = compilation_->GetPoolingParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  const uint32_t input_index = operation->inputs[0];
  const uint32_t output_index = operation->outputs[0];

  // Create pooling descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_pooling_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
  if (operation->type == mojom::MAX_POOL_2D) {
    pool_desc.mode = cldnn_pooling_max;
  } else if (operation->type == mojom::AVERAGE_POOL_2D) {
    pool_desc.mode = cldnn_pooling_average;
  } else {
    LOG(ERROR) << "Pooling mode " << operation->type << " is not supported";
    return mojom::BAD_DATA;
  }

  // Setup kernel size.
  pool_desc.size = {
      1,
      1,
      2,
      0,
      {1, 1, params.filter_width, params.filter_height, 1, 1, 1, 1}};

  pool_desc.input_offset = {
      1,
      1,
      2,
      0,
      {0, 0, -params.padding_left, -params.padding_top, 0, 0, 0, 0}};

  // Setup stride.
  pool_desc.stride = {
      1,
      1,
      2,
      0,
      {1, 1, params.stride_width, params.stride_height, 1, 1, 1, 1}};

  // Setup output.
  pool_desc.with_output_size = 1;
  pool_desc.output_size = {
      1,
      1,
      2,
      0,
      {params.output_batch, params.output_channel, params.output_width,
       params.output_height, 1, 1, 1, 1}};

  // Setup argmax.
  std::string empty;
  pool_desc.argmax = empty.c_str();

  // Setup fuse code (with a dedicated activation primitive).
  std::string id_str = base::NumberToString(output_index);
  if (params.fuse_code != mojom::FUSED_NONE) {
    id_str = id_str + "-func";
  }

  // Add primitive into topology.
  pool_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&pool_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add pooling primitive with id " << id_str;

  // Handle fused code as dedicated activation primitive.
  if (params.fuse_code != mojom::FUSED_NONE) {
    std::string fuse_id_str = base::NumberToString(output_index);
    result =
        CldnnAddActivationByFusedCode(id_str, fuse_id_str, params.fuse_code);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddSoftmax(
    const mojom::OperationPtr& operation) {
  // Setup softmax params.
  SoftmaxParams params;
  int32_t result = compilation_->GetSoftmaxParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  const uint32_t input_index = operation->inputs[0];
  const uint32_t output_index = operation->outputs[0];

  // Create softmax descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_softmax_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_softmax_desc softmax_desc = {.type = type_id};

  // Setup inputs.
  std::vector<std::string> input_ids(1);
  std::vector<cldnn_primitive_id> input_ids_array(1);
  input_ids[0] = base::NumberToString(input_index);
  input_ids_array[0] = input_ids[0].c_str();
  softmax_desc.input = {.data = input_ids_array.data(),
                        .size = input_ids_array.size()};

  // Check beta.
  if (params.beta != 1.0) {
    LOG(ERROR) << "beta " << params.beta << " is not supported.";
    return mojom::BAD_DATA;
  }

  // Setup dimension.
  softmax_desc.dimension = cldnn_softmax_normalize_fyx;

  // Setup id and add into topology.
  std::string id_str(base::NumberToString(output_index));
  softmax_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&softmax_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add softmax primitive with id " << id_str;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddReshape(
    const mojom::OperationPtr& operation) {
  const uint32_t input_index = operation->inputs[0];
  const uint32_t output_index = operation->outputs[0];

  // Create reshape descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_reshape_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_reshape_desc reshape_desc = {.type = type_id};

  // Setup inputs.
  std::vector<std::string> input_ids(1);
  std::vector<cldnn_primitive_id> input_ids_array(1);
  input_ids[0] = base::NumberToString(input_index);
  input_ids_array[0] = input_ids[0].c_str();
  reshape_desc.input = {.data = input_ids_array.data(),
                        .size = input_ids_array.size()};

  // Setup output shape.
  cldnn_layout layout;
  const mojom::OperandPtr& operand =
      compilation_->GetModel()->operands[output_index];
  int32_t result = CldnnGetLayout(operand->type, operand->dimensions, layout);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  reshape_desc.output_shape = layout.size;

  // Setup id and add into topology.
  std::string id_str(base::NumberToString(output_index));
  reshape_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&reshape_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add reshape primitive with id " << id_str;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddConcatenation(
    const mojom::OperationPtr& operation) {
  // Setup concatenation params.
  ConcatParams params;
  int32_t result = compilation_->GetConcatParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  const std::vector<uint32_t>& inputs = operation->inputs;
  const uint32_t output_index = operation->outputs[0];

  // Create concatenation descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_concatenation_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
  if (rank == 1) {
    if (params.axis == 0) {
      concat_desc.axis = cldnn_concatenation_along_x;
    } else {
      LOG(ERROR) << "axis " << params.axis << " is not supported.";
      return mojom::BAD_DATA;
    }
  } else if (rank == 2) {
    // HW -> yx
    if (params.axis == 0) {
      concat_desc.axis = cldnn_concatenation_along_y;
    } else if (params.axis == 1) {
      concat_desc.axis = cldnn_concatenation_along_x;
    } else {
      LOG(ERROR) << "axis " << params.axis << " is not supported.";
      return mojom::BAD_DATA;
    }
  } else if (rank == 3) {
    // HWC -> yxf
    if (params.axis == 0) {
      concat_desc.axis = cldnn_concatenation_along_y;
    } else if (params.axis == 1) {
      concat_desc.axis = cldnn_concatenation_along_x;
    } else if (params.axis == 2) {
      concat_desc.axis = cldnn_concatenation_along_f;
    } else {
      LOG(ERROR) << "axis " << params.axis << " is not supported.";
      return mojom::BAD_DATA;
    }
  } else if (rank == 4) {
    // NHWC -> byxf
    if (params.axis == 0) {
      concat_desc.axis = cldnn_concatenation_along_b;
    } else if (params.axis == 1) {
      concat_desc.axis = cldnn_concatenation_along_y;
    } else if (params.axis == 2) {
      concat_desc.axis = cldnn_concatenation_along_x;
    } else if (params.axis == 3) {
      concat_desc.axis = cldnn_concatenation_along_f;
    } else {
      LOG(ERROR) << "axis " << params.axis << " is not supported.";
      return mojom::BAD_DATA;
    }
  } else {
    LOG(ERROR) << "rank " << rank << " is not supported.";
    return mojom::BAD_DATA;
  }

  // Setup id and add into topology.
  std::string id_str(base::NumberToString(output_index));
  concat_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&concat_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add concatenation primitive with id "
             << id_str;
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddFullyConnected(
    const mojom::OperationPtr& operation) {
  // Setup fully connected params.
  FullyConnectedParams params;
  int32_t result = compilation_->GetFullyConnectedParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  const uint32_t input_index = operation->inputs[0];
  const uint32_t weights_index = operation->inputs[1];
  const uint32_t bias_index = operation->inputs[2];
  const uint32_t output_index = operation->outputs[0];

  // Create fully_connected descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id =
      LATE(cldnn_fully_connected_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
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
      1,
      1,
      2,
      0,
      {params.input_batch_size, 1, params.input_size, 1, 1, 1, 1, 1}};

  // Setup id and add into topology.
  std::string reshape_id_str(base::NumberToString(input_index) +
                             std::string("-reshaped"));
  reshape_desc.id = reshape_id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&reshape_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
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
      .size =
          {1, 1, 2, 0, {params.num_units, 1, params.input_size, 1, 1, 1, 1, 1}},
      .padding = {}};
  cldnn_memory weights_memory =
      LATE(cldnn_allocate_memory)(engine_, weights_layout, &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to allocate memory " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  float* weights_memory_ptr = reinterpret_cast<float*>(
      LATE(cldnn_lock_memory)(weights_memory, &status));
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to lock memory " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  const mojom::OperandValueInfoPtr& weights_info =
      model->values[base::NumberToString(weights_index)];
  auto mapping = compilation_->MapMemory(weights_index);
  memcpy(weights_memory_ptr, mapping.get(), weights_info->length);

  LATE(cldnn_unlock_memory)(weights_memory, &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to unlock memory " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }

  type_id = LATE(cldnn_data_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  const std::string weights_index_str(base::NumberToString(weights_index));
  const cldnn_data_desc weights_data_desc = {
      .type = type_id, .id = weights_index_str.c_str(), .mem = weights_memory};
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&weights_data_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add data primitive with id "
             << weights_index_str;

  fc_desc.weights = weights_index_str.c_str();

  // Setup bias.
  CldnnAddData(bias_index);
  const std::string bias_index_str(base::NumberToString(bias_index));
  fc_desc.bias = bias_index_str.c_str();

  std::string id_str(base::NumberToString(output_index));
  // Setup activation.
  fc_desc.activation_negative_slope = 0.0;
  if (params.fuse_code == mojom::FUSED_NONE) {
    fc_desc.with_activation = 0;
  } else if (params.fuse_code == mojom::FUSED_RELU) {
    fc_desc.with_activation = 1;
  } else if (params.fuse_code == mojom::FUSED_RELU1 ||
             params.fuse_code == mojom::FUSED_RELU6) {
    fc_desc.with_activation = 0;
    id_str = id_str + "-func";
  } else {
    LOG(ERROR) << "Fuse code " << params.fuse_code << " is not supported";
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
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add fc primitive with id " << id_str;

  // Handle RELU1 and RELU6 fused code as dedicated activation primitive.
  if (params.fuse_code == mojom::FUSED_RELU1 ||
      params.fuse_code == mojom::FUSED_RELU6) {
    std::string fuse_id_str = base::NumberToString(output_index);
    result =
        CldnnAddActivationByFusedCode(id_str, fuse_id_str, params.fuse_code);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }

  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateClDnn::CldnnAddResizeBilinear(
    const mojom::OperationPtr& operation) {
  // Setup resize bilinear params.
  ResizeBilinearParams params;
  int32_t result = compilation_->GetResizeBilinearParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return result;
  const uint32_t input_index = operation->inputs[0];
  const uint32_t output_index = operation->outputs[0];

  // Create custom gpu primitive descriptor.
  cldnn_status status;
  cldnn_primitive_type_id type_id =
      LATE(cldnn_custom_gpu_primitive_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_custom_gpu_primitive_desc custom_desc = {.type = type_id};

  // Setup inputs.
  std::string input(base::NumberToString(input_index));
  std::vector<cldnn_primitive_id> input_ids_array = {input.c_str()};
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
  build_options += std::string("-Dpad_beg_=0 -Dpad_end_=0 ");
  if (params.align_corners) {
    build_options += std::string("-DALIGN_CORNERS");
  }
  custom_desc.build_options = build_options.c_str();

  // Setup output layout
  cldnn_layout output_layout;
  const mojom::OperandPtr& operand =
      compilation_->GetModel()->operands[output_index];
  result = CldnnGetLayout(operand->type, operand->dimensions, output_layout,
                          cldnn_format_bfyx);
  if (result != mojom::NOT_ERROR) {
    return result;
  }
  custom_desc.output_layout = output_layout;

  // Setup work group size
  std::vector<size_t> gws = {params.new_height, params.new_width};
  custom_desc.gws = gws.data();
  custom_desc.gws_num = gws.size();
  std::vector<size_t> lws = {};
  custom_desc.lws = lws.data();
  custom_desc.lws_num = lws.size();

  // Setup id and add into topology.
  std::string id_str(base::NumberToString(output_index));
  custom_desc.id = id_str.c_str();
  LATE(cldnn_add_primitive)
  (topology_, reinterpret_cast<const cldnn_primitive_desc*>(&custom_desc),
   &status);
  if (status != CLDNN_SUCCESS) {
    LOG(ERROR) << "[clDNN] failed to add primitive " << status << " "
               << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  DLOG(INFO) << "[clDNN] succeed to add custom_gpu primitive with id "
             << id_str;

  return mojom::NOT_ERROR;
}

}  // namespace ml
