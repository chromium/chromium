// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/model_impl_cl_dnn.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_impl_cl_dnn.h"
#include "services/ml/public/interfaces/constants.mojom.h"
#include "third_party/clDNN/api/C/activation.h"
#include "third_party/clDNN/api/C/concatenation.h"
#include "third_party/clDNN/api/C/convolution.h"
#include "third_party/clDNN/api/C/data.h"
#include "third_party/clDNN/api/C/eltwise.h"
#include "third_party/clDNN/api/C/fully_connected.h"
#include "third_party/clDNN/api/C/input_layout.h"
#include "third_party/clDNN/api/C/pooling.h"
#include "third_party/clDNN/api/C/reorder.h"
#include "third_party/clDNN/api/C/reshape.h"
#include "third_party/clDNN/api/C/softmax.h"

#if defined(OS_LINUX)
constexpr char kClDnnVersion[] = "9.1";

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
                                     int32_t& padding_tail) {
  padding_head = 0;
  padding_tail = 0;

  if (padding_same) {
    int32_t out_size = (in_size + stride - 1) / stride;
    int32_t tmp = (out_size - 1) * stride + filter_size;
    if (tmp > in_size) {
      padding_head = (tmp - in_size) / 2;
      padding_tail = (tmp - in_size) - padding_head;
    }
  }
}

}  // namespace

ModelImplClDnn::ModelImplClDnn() : engine_(nullptr), topology_(nullptr) {
#if defined(OS_LINUX)
  if (!GetClDnnSymbolTable()->Load()) {
    LOG(ERROR) << "[clDNN] failed to load clDNN library";
    return;
  }
#endif

  cldnn_status status;
  cldnn_version version = LATE(cldnn_get_version)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get cldnn version";
    return;
  }

  const std::string major_version =
      std::to_string(version.build) + "." + std::to_string(version.major);
  const std::string cl_dnn_version = major_version + "." +
                                     std::to_string(version.minor) + "." +
                                     std::to_string(version.revision);
  DLOG(INFO) << "[clDNN] version: " << cl_dnn_version;
#if defined(OS_LINUX)
  if (major_version != kClDnnVersion) {
    LOG(ERROR) << "[clDNN] current clDNN version" << cl_dnn_version
               << " isn't supported, please install OpenVINO 2018 R3 that "
                  "inlucdes verified version "
               << kClDnnVersion;
    return;
  }
#endif

  uint32_t engine_count =
      LATE(cldnn_get_engine_count)(cldnn_engine_ocl, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN]: failed to get cldnn ocl engine count " << status
                << " " << std::string(LATE(cldnn_get_last_error_message)());
    return;
  }
  DLOG(INFO) << "[clDNN] ocl engine count: " << engine_count;
  if (engine_count < 1) {
    DLOG(ERROR) << "[clDNN] ocl engine is not available " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return;
  }
  engine_ = LATE(cldnn_create_engine)(cldnn_engine_ocl, 0, nullptr, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to create cldnn ocl engine " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    engine_ = nullptr;
    return;
  }
  DLOG(INFO) << "[clDNN] succeeded to create cldnn ocl engine " << engine_;

  cldnn_engine_info engine_info = LATE(cldnn_get_engine_info)(engine_, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get cldnn engine info " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return;
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

  topology_ = LATE(cldnn_create_topology)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to create cldnn topology " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    topology_ = nullptr;
  }
  DLOG(INFO) << "[clDNN] succeed to create topology";
}

bool ModelImplClDnn::IsValid() {
  return engine_ && topology_ ? true : false;
}

ModelImplClDnn::~ModelImplClDnn() {
  if (!IsValid())
    return;

  cldnn_status status;
  for (size_t i = 0; i < memories_.size(); ++i) {
    LATE(cldnn_release_memory)(memories_[i], &status);
    if (status != CLDNN_SUCCESS) {
      DLOG(ERROR) << "[clDNN] failed to release cldnn memory " << status << " "
                  << std::string(LATE(cldnn_get_last_error_message)());
    }
  }
  DLOG(INFO) << "[clDNN] succeed to release memories";

  LATE(cldnn_release_topology)(topology_, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to release cldnn topology " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
  }
  DLOG(INFO) << "[clDNN] succeed to release topology";

  LATE(cldnn_release_engine)(engine_, &status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to release cldnn engine " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
  }
  DLOG(INFO) << "[clDNN] succeed to release engine";
}

void ModelImplClDnn::Finish(mojom::ModelInfoPtr model_info,
                            FinishCallback callback) {
  DLOG(INFO) << "ModelImplClDnn::Finish";
  int32_t result = mojom::NOT_ERROR;
  DLOG(INFO) << "operands(" << model_info->operands.size() << ")";
  for (size_t i = 0; i < model_info->operands.size(); ++i) {
    DLOG(INFO) << "  operand[" << i << "]";
    const mojom::OperandPtr& operand = model_info->operands[i];
    result = AddOperand(operand->type, operand->dimensions, operand->scale,
                        operand->zeroPoint);
    if (result != mojom::NOT_ERROR) {
      std::move(callback).Run(result);
      return;
    }
  }
  DLOG(INFO) << "values(" << model_info->values.size() << ")";
  memory_size_ = model_info->memory_size;
  auto mapping = model_info->memory->Map(memory_size_);
  const int8_t* base = static_cast<const int8_t*>(mapping.get());
  memory_.reset(new int8_t[memory_size_]);
  memcpy(memory_.get(), base, memory_size_);
  for (size_t i = 0; i < model_info->values.size(); ++i) {
    DLOG(INFO) << "  values[" << i << "]";
    const mojom::OperandValueInfoPtr& value_info = model_info->values[i];
    result = SetOperandValue(
        value_info->index,
        static_cast<const void*>(memory_.get() + value_info->offset),
        value_info->length);
    if (result != mojom::NOT_ERROR) {
      std::move(callback).Run(result);
      return;
    }
    ValueInfo value;
    value.index = value_info->index;
    value.offset = value_info->offset;
    value.length = value_info->length;
    values_[value_info->index] = value;
  }
  DLOG(INFO) << "inputs(" << model_info->inputs.size() << ")";
  DLOG(INFO) << "outputs(" << model_info->outputs.size() << ")";
  result = IdentifyInputsAndOutputs(model_info->inputs, model_info->outputs);
  if (result != mojom::NOT_ERROR) {
    std::move(callback).Run(result);
    return;
  }
  DLOG(INFO) << "operations(" << model_info->operations.size() << ")";
  for (size_t i = 0; i < model_info->operations.size(); ++i) {
    DLOG(INFO) << "  operation[" << i << "]";
    const mojom::OperationPtr& operation = model_info->operations[i];
    result =
        AddOperation(operation->type, operation->inputs, operation->outputs);
    if (result != mojom::NOT_ERROR) {
      std::move(callback).Run(result);
      return;
    }
  }

  std::move(callback).Run(mojom::NOT_ERROR);
}

void ModelImplClDnn::CreateCompilation(CreateCompilationCallback callback) {
  DLOG(INFO) << "ModelImplClDnn::CreateCompilation";
  auto init_params = mojom::CompilationInitParams::New();
  mojom::CompilationPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::make_unique<CompilationImplClDnn>(this),
                          mojo::MakeRequest(&ptr_info));
  init_params->compilation = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

int32_t ModelImplClDnn::AddOperand(int32_t type,
                                   const std::vector<uint32_t>& dimensions,
                                   float scale,
                                   int32_t zeroPoint) {
  DLOG(INFO) << "  ModelImplClDnn::AddOperand";
  DLOG(INFO) << "    "
             << "type: " << type;
  DLOG(INFO) << "    "
             << "dimensions(" << dimensions.size()
             << "): " << VectorToString(dimensions.data(), dimensions.size());
  DLOG(INFO) << "    "
             << "scale: " << scale;
  DLOG(INFO) << "    "
             << "zeroPoint: " << zeroPoint;
  Operand operand;
  operand.type = type;
  operand.dimensions = dimensions;
  operand.scale = scale;
  operand.zeroPoint = zeroPoint;
  operands_.push_back(operand);

  return mojom::NOT_ERROR;
}

int32_t ModelImplClDnn::SetOperandValue(uint32_t index,
                                        const void* buffer,
                                        uint32_t length) {
  DLOG(INFO) << "  ModelImplClDnn::SetOperandValue";
  DLOG(INFO) << "    "
             << "index: " << index;
  DLOG(INFO) << "    "
             << "length: " << length;
  if (index > operands_.size()) {
    return mojom::BAD_DATA;
  }
  auto operand = operands_[index];
  if (operand.type == mojom::TENSOR_FLOAT32 || operand.type == mojom::FLOAT32) {
    const float* value = static_cast<const float*>(buffer);
    uint32_t size = length / 4;
    DLOG(INFO) << "    "
               << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_INT32 ||
             operand.type == mojom::INT32) {
    const int32_t* value = static_cast<const int32_t*>(buffer);
    uint32_t size = length / 4;
    DLOG(INFO) << "    "
               << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_QUANT8_ASYMM) {
    const int8_t* value = static_cast<const int8_t*>(buffer);
    uint32_t size = length;
    DLOG(INFO) << "    "
               << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::UINT32) {
    const uint32_t* value = static_cast<const uint32_t*>(buffer);
    uint32_t size = length;
    DLOG(INFO) << "    "
               << "buffer(" << size << "): " << VectorToString(value, size);
  }

  return mojom::NOT_ERROR;
}

int32_t ModelImplClDnn::AddOperation(int32_t type,
                                     const std::vector<uint32_t>& inputs,
                                     const std::vector<uint32_t>& outputs) {
  DLOG(INFO) << "  ModelImplClDnn::AddOperation";
  DLOG(INFO) << "    "
             << "type: " << type;
  DLOG(INFO) << "    "
             << "inputs(" << inputs.size()
             << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "    "
             << "outputs(" << outputs.size()
             << "): " << VectorToString(outputs.data(), outputs.size());
  Operation operation;
  operation.type = type;
  operation.inputs = inputs;
  operation.outputs = outputs;
  operations_.push_back(operation);

  if (operation.outputs.size() != 1) {
    DLOG(ERROR) << "Only 1 output is supported";
    return mojom::BAD_DATA;
  }

  int32_t result = mojom::NOT_ERROR;
  if (type == mojom::ADD || type == mojom::MUL) {
    result = CldnnAddElementwise(type, inputs, outputs);
  } else if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D) {
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
  } else {
    DLOG(ERROR) << "Operation type " << type << " is not supported.";
    return mojom::BAD_DATA;
  }
  return result;
}

int32_t ModelImplClDnn::IdentifyInputsAndOutputs(
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  DLOG(INFO) << "  ModelImplClDnn::IdentifyInputsAndOutputs";
  DLOG(INFO) << "    "
             << "inputs(" << inputs.size()
             << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "    "
             << "outputs(" << outputs.size()
             << "): " << VectorToString(outputs.data(), outputs.size());
  inputs_ = inputs;
  outputs_ = outputs;

  int32_t result;
  for (size_t i = 0; i < inputs_.size(); ++i) {
    result = CldnnAddInputLayout(inputs_[i]);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }
  for (size_t i = 0; i < outputs_.size(); ++i) {
    result = CldnnAddReorderForOperand(outputs_[i], cldnn_format_byxf);
    if (result != mojom::NOT_ERROR) {
      return result;
    }
  }
  return mojom::NOT_ERROR;
}

int32_t ModelImplClDnn::CldnnGetLayout(const Operand& operand,
                                       cldnn_layout& layout,
                                       int32_t format) {
  if (operand.type != mojom::TENSOR_FLOAT32) {
    DLOG(ERROR) << "Only TENSOR_FLOAT32 operand type is supported";
    return mojom::BAD_DATA;
  }
  layout = {.data_type = cldnn_f32, .format = format, .padding = {}};
  if (operand.dimensions.size() == 1) {
    layout.size = {1, 1, 2, {1, 1, operand.dimensions[0], 1, 1, 1, 1, 1}};
  } else if (operand.dimensions.size() == 2) {
    // HW -> {batch, feature, width, height}
    layout.size = {
        1,
        1,
        2,
        {1, 1, operand.dimensions[1], operand.dimensions[0], 1, 1, 1, 1}};
  } else if (operand.dimensions.size() == 3) {
    // HWC -> {batch, feature, width, height}
    layout.size = {1,
                   1,
                   2,
                   {1, operand.dimensions[2], operand.dimensions[1],
                    operand.dimensions[0], 1, 1, 1, 1}};
  } else if (operand.dimensions.size() == 4) {
    // NHWC -> {batch, feature, width, height}
    layout.size = {1,
                   1,
                   2,
                   {operand.dimensions[0], operand.dimensions[3],
                    operand.dimensions[2], operand.dimensions[1], 1, 1, 1, 1}};
  } else {
    DLOG(ERROR) << "Operand dimensions size " << operand.dimensions.size()
                << " is not supported.";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

int32_t ModelImplClDnn::CldnnAddInputLayout(uint32_t index) {
  cldnn_status status;
  const Operand operand = operands_[index];
  cldnn_layout layout;
  int32_t result = CldnnGetLayout(operand, layout, cldnn_format_byxf);
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

int32_t ModelImplClDnn::CldnnAddReorderForOperand(int32_t index,
                                                  int32_t target_format) {
  cldnn_status status;
  cldnn_primitive_type_id type_id = LATE(cldnn_reorder_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  const std::string output_id_str = base::NumberToString(index);
  const std::string id_str = output_id_str + std::string("-reordered");
  cldnn_reorder_desc reorder_desc = {
      .type = type_id,
      .id = id_str.c_str(),
      .output_format = cldnn_format_type(target_format),
      .output_data_type = cldnn_f32,
  };
  // Setup inputs.
  std::vector<cldnn_primitive_id> input_ids_array(1);
  input_ids_array[0] = output_id_str.c_str();
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
  DLOG(INFO) << "[clDNN] succeed to add reorder primitve with id " << id_str;
  return mojom::NOT_ERROR;
}

int32_t ModelImplClDnn::CldnnAddData(uint32_t index) {
  cldnn_status status;
  const Operand operand = operands_[index];
  cldnn_layout layout;
  int32_t result = CldnnGetLayout(operand, layout);
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
  ValueInfo value_info = values_.at(index);
  const void* value_ptr =
      reinterpret_cast<const void*>(memory_.get() + value_info.offset);
  if (operand.dimensions.size() == 1 || operand.dimensions.size() == 2) {
    memcpy(memory_ptr, value_ptr, value_info.length);
  } else if (operand.dimensions.size() == 3 || operand.dimensions.size() == 4) {
    // NHWC -> bfyx
    const bool rank3 = operand.dimensions.size() == 3;
    const uint32_t batches = rank3 ? 1 : operand.dimensions[0];
    const uint32_t channels =
        rank3 ? operand.dimensions[2] : operand.dimensions[3];
    const uint32_t height =
        rank3 ? operand.dimensions[0] : operand.dimensions[1];
    const uint32_t width =
        rank3 ? operand.dimensions[1] : operand.dimensions[2];
    float* dst = reinterpret_cast<float*>(memory_ptr);
    const float* src = reinterpret_cast<const float*>(value_ptr);
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
    DLOG(ERROR) << "Operand dimensions size " << operand.dimensions.size()
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

int32_t ModelImplClDnn::CldnnAddActivationByFusedCode(const std::string& input,
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

int32_t ModelImplClDnn::CldnnAddElementwise(
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
    if (values_.find(inputs[i]) != values_.end()) {
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
  int32_t fuse_code = getScalarInt32(values_[inputs[2]], memory_.get());
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

int32_t ModelImplClDnn::CldnnAddConvolution(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  const bool depthwise = type == mojom::DEPTHWISE_CONV_2D ? true : false;
  const uint32_t output_index = outputs[0];
  const Operand& output = operands_[output_index];
  const int32_t output_batch = output.dimensions[0];
  const int32_t output_height = output.dimensions[1];
  const int32_t output_width = output.dimensions[2];
  const int32_t output_channel = output.dimensions[3];
  uint32_t index = 0;
  const uint32_t input_index = inputs[index++];
  const Operand& input = operands_[input_index];
  const int32_t input_height = input.dimensions[1];
  const int32_t input_width = input.dimensions[2];

  const uint32_t filter_idx = inputs[index++];
  Operand& filter = operands_[filter_idx];
  int32_t depth_out, depth_in;
  if (depthwise) {
    depth_out = filter.dimensions[3];
  } else {
    depth_out = filter.dimensions[0];
    depth_in = filter.dimensions[3];
  }
  const int32_t filter_height = filter.dimensions[1];
  const int32_t filter_width = filter.dimensions[2];

  const uint32_t bias_idx = inputs[index++];

  bool implicit_padding;
  int32_t padding_left, padding_right, padding_top, padding_bottom,
      padding_code;
  if ((!depthwise && inputs.size() == 10) ||
      (depthwise && inputs.size() == 11)) {
    implicit_padding = false;
    padding_left = getScalarInt32(values_[inputs[index++]], memory_.get());
    padding_right = getScalarInt32(values_[inputs[index++]], memory_.get());
    padding_top = getScalarInt32(values_[inputs[index++]], memory_.get());
    padding_bottom = getScalarInt32(values_[inputs[index++]], memory_.get());
  } else if ((!depthwise && inputs.size() == 7) ||
             (depthwise && inputs.size() == 8)) {
    implicit_padding = true;
    padding_code = getScalarInt32(values_[inputs[index++]], memory_.get());
  } else {
    DLOG(ERROR) << "Inputs size is incorrect";
    return mojom::BAD_DATA;
  }
  const int32_t stride_width =
      getScalarInt32(values_[inputs[index++]], memory_.get());
  const int32_t stride_height =
      getScalarInt32(values_[inputs[index++]], memory_.get());
  int32_t depthwise_multiplier;
  if (depthwise) {
    depthwise_multiplier =
        getScalarInt32(values_[inputs[index++]], memory_.get());
    if (depthwise_multiplier != 1) {
      DLOG(ERROR) << "  depthwise_multiplier " << depthwise_multiplier
                  << " is not supported.";
      return mojom::BAD_DATA;
    }
    depth_in = depth_out / depthwise_multiplier;
  }
  const int32_t fuse_code =
      getScalarInt32(values_[inputs[index++]], memory_.get());

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
    const ValueInfo weights_info = values_.at(filter_idx);
    const float* weights_value_ptr =
        reinterpret_cast<const float*>(memory_.get() + weights_info.offset);
    const cldnn_layout weights_layout = {
        .data_type = cldnn_f32,
        .format = cldnn_format_bfyx,
        .size = {1, 1, 2, {1, 1, filter_width, filter_height, 1, 1, 1, 1}},
        .padding = {}};
    weight_ids_array.resize(depth_out);
    weight_ids.resize(depth_out);

    const ValueInfo bias_info = values_.at(bias_idx);
    const float* bias_value_ptr =
        reinterpret_cast<const float*>(memory_.get() + bias_info.offset);
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
                             padding_right);
    CalculateExplicitPadding(padding_code == mojom::PADDING_SAME, input_height,
                             stride_height, filter_height, padding_top,
                             padding_bottom);
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
  conv_desc.dilation = {1, 1, 2, {1, 1, 1, 1, 1, 1, 1, 1}};

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

int32_t ModelImplClDnn::CldnnAddPooling(int32_t type,
                                        const std::vector<uint32_t>& inputs,
                                        const std::vector<uint32_t>& outputs) {
  const uint32_t output_index = outputs[0];
  const Operand& output = operands_[output_index];
  const int32_t output_batch = output.dimensions[0];
  const int32_t output_height = output.dimensions[1];
  const int32_t output_width = output.dimensions[2];
  const int32_t output_channel = output.dimensions[3];
  int32_t i = 0;
  const int32_t input_index = inputs[i++];
  const Operand& input = operands_[input_index];
  const int32_t input_height = input.dimensions[1];
  const int32_t input_width = input.dimensions[2];

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
    padding_left = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_right = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_top = getScalarInt32(values_[inputs[i++]], memory_.get());
    padding_bottom = getScalarInt32(values_[inputs[i++]], memory_.get());
  } else if (inputs.size() == 7) {
    implicit_padding = true;
    padding_code = getScalarInt32(values_[inputs[i++]], memory_.get());
  } else {
    DLOG(ERROR) << "  inputs size is incorrect";
    return mojom::BAD_DATA;
  }
  const int32_t stride_width =
      getScalarInt32(values_[inputs[i++]], memory_.get());
  const int32_t stride_height =
      getScalarInt32(values_[inputs[i++]], memory_.get());
  const int32_t filter_width =
      getScalarInt32(values_[inputs[i++]], memory_.get());
  const int32_t filter_height =
      getScalarInt32(values_[inputs[i++]], memory_.get());
  const int32_t fuse_code = getScalarInt32(values_[inputs[i++]], memory_.get());

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

int32_t ModelImplClDnn::CldnnAddSoftmax(int32_t type,
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
  const float beta = getScalarFloat(values_[inputs[1]], memory_.get());
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

int32_t ModelImplClDnn::CldnnAddReshape(int32_t type,
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
  int32_t result = CldnnGetLayout(operands_[outputs[0]], layout);
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

int32_t ModelImplClDnn::CldnnAddConcatenation(
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
  const int32_t inputs_count = inputs.size() - 1;
  std::vector<std::string> input_ids(inputs_count);
  std::vector<cldnn_primitive_id> input_ids_array(inputs_count);
  for (int32_t i = 0; i < inputs_count; ++i) {
    input_ids[i] = base::NumberToString(inputs[i]);
    input_ids_array[i] = input_ids[i].c_str();
    // Add constants.
    if (values_.find(inputs[i]) != values_.end()) {
      int32_t result = CldnnAddData(inputs[i]);
      if (result != mojom::NOT_ERROR) {
        return result;
      }
    }
  }
  concat_desc.input = {.data = input_ids_array.data(),
                       .size = input_ids_array.size()};

  // Setup axis
  const uint32_t rank = operands_[inputs[0]].dimensions.size();
  const int32_t axis =
      getScalarInt32(values_[inputs[inputs.size() - 1]], memory_.get());
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

int32_t ModelImplClDnn::CldnnAddFullyConnected(
    int32_t type,
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  // The output tensor, of shape [batch_size, num_units]
  const uint32_t output_index = outputs[0];
  const Operand& output = operands_[output_index];
  const int32_t output_batch_size = output.dimensions[0];
  const int32_t output_num_units = output.dimensions[1];

  uint32_t index = 0;
  const uint32_t input_index = inputs[index++];
  const Operand& input = operands_[input_index];
  // A tensor of at least rank 2, specifying the input.
  if (input.dimensions.size() < 2) {
    DLOG(ERROR) << "A tenosr of least rank 2.";
    return mojom::BAD_DATA;
  }

  const uint32_t weights_idx = inputs[index++];
  const Operand& weights = operands_[weights_idx];
  const uint32_t num_units = weights.dimensions[0];
  const uint32_t input_size = weights.dimensions[1];

  // According to Android NN API doc:
  // If rank is greater than 2, then it gets flattened to a 2-D Tensor.
  // The (flattened) 2-D Tensor is reshaped (if necessary) to
  // [batch_size, input_size], where "input_size" corresponds to the number of
  // inputs to the layer, matching the second dimension of weights, and
  // "batch_size" is calculated by dividing the number of elements by
  // "input_size".
  uint32_t input_batch_size;
  if (input.dimensions.size() > 2) {
    input_batch_size = product(input.dimensions) / input_size;
  } else {
    if (input.dimensions[1] != input_size) {
      DLOG(ERROR) << "input.dimensions[1] (" << input.dimensions[1] << ") "
                  << "!= input_size (" << input_size << ")";
      return mojom::BAD_DATA;
    }
    input_batch_size = input.dimensions[0];
  }

  // A 1-D tensor, of shape [num_units]
  const uint32_t bias_idx = inputs[index++];
  const Operand& bias = operands_[bias_idx];
  const uint32_t bias_num_units = bias.dimensions[0];
  if (bias_num_units != num_units) {
    DLOG(ERROR) << "bias_num_units (" << bias_num_units << ") != "
                << "num_units (" << num_units << ")";
    return mojom::BAD_DATA;
  }

  const int32_t fuse_code =
      getScalarInt32(values_[inputs[index++]], memory_.get());

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
  CldnnAddReorderForOperand(input_index, cldnn_format_bfyx);
  // Reshape to [input_batch_size, input_size]
  type_id = LATE(cldnn_reshape_type_id)(&status);
  if (status != CLDNN_SUCCESS) {
    DLOG(ERROR) << "[clDNN] failed to get primitive type id " << status << " "
                << std::string(LATE(cldnn_get_last_error_message)());
    return mojom::OP_FAILED;
  }
  cldnn_reshape_desc reshape_desc = {.type = type_id};
  std::vector<cldnn_primitive_id> input_ids_array(1);
  std::string reshape_input_str(base::NumberToString(input_index) +
                                std::string("-reordered"));
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

  const ValueInfo& weights_info = values_.at(weights_idx);
  const float* weights_value_ptr =
      reinterpret_cast<const float*>(memory_.get() + weights_info.offset);
  memcpy(weights_memory_ptr, weights_value_ptr, weights_info.length);

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

}  // namespace ml
