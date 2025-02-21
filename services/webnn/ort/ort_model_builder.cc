// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_model_builder.h"

#include "base/notreached.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"

namespace webnn {

namespace {

constexpr char kOrtDomainName[] = "";
constexpr int32_t kOrtOpsetVersion = 21;

// Define the minimum size(in bytes) to use external data.
constexpr size_t kMinExternalDataSize = 128;

}  // namespace

namespace ort {

OrtModelBuilder::ModelInfo::ModelInfo() = default;
OrtModelBuilder::ModelInfo::~ModelInfo() = default;

ScopedOrtValueInfoPtr CreateOrtValueInfo(std::string_view name,
                                         base::span<const int64_t> shape,
                                         ONNXTensorElementDataType data_type) {
  ScopedOrtTensorTypeAndShapeInfoPtr tensor_type_and_shape_info;
  CHECK_STATUS(GetOrtApi()->CreateTensorTypeAndShapeInfo(
      tensor_type_and_shape_info.GetAddressOf()));
  CHECK_STATUS(
      GetOrtApi()->SetTensorElementType(tensor_type_and_shape_info, data_type));
  CHECK_STATUS(GetOrtApi()->SetDimensions(tensor_type_and_shape_info,
                                          shape.data(), shape.size()));

  ScopedOrtTypeInfoPtr type_info;
  CHECK_STATUS(GetOrtApi()->CreateTensorTypeInfo(tensor_type_and_shape_info,
                                                 type_info.GetAddressOf()));

  ScopedOrtValueInfoPtr value_info;
  CHECK_STATUS(GetOrtModelBuilderApi()->CreateValueInfo(
      name.data(), type_info, value_info.GetAddressOf()));
  return value_info;
}

OrtModelBuilder::OrtModelBuilder()
    : model_info_(std::make_unique<ModelInfo>()) {
  // WebNN constants are in CPU memory.
  CHECK_STATUS(GetOrtApi()->CreateCpuMemoryInfo(
      OrtDeviceAllocator, OrtMemTypeDefault, memory_info_.GetAddressOf()));
  CHECK_STATUS(GetOrtModelBuilderApi()->CreateGraph(graph_.GetAddressOf()));
}

OrtModelBuilder::~OrtModelBuilder() = default;

void OrtModelBuilder::AddInput(std::string_view name,
                               base::span<const int64_t> shape,
                               ONNXTensorElementDataType data_type) {
  inputs_.push_back(CreateOrtValueInfo(name, shape, data_type));
}

void OrtModelBuilder::AddOutput(std::string_view name,
                                base::span<const int64_t> shape,
                                ONNXTensorElementDataType data_type) {
  outputs_.push_back(CreateOrtValueInfo(name, shape, data_type));
}

[[nodiscard]] ScopedOrtStatusPtr OrtModelBuilder::AddInitializer(
    std::string_view name,
    base::span<const int64_t> shape,
    base::span<const uint8_t> data,
    ONNXTensorElementDataType data_type) {
  bool data_is_external = data.size() >= kMinExternalDataSize;
  if (data_is_external) {
    return AddInitializerAsExternalData(name, shape, data, data_type);
  } else {
    return AddInitializerAsRawData(name, shape, data, data_type);
  }
}

[[nodiscard]] ScopedOrtStatusPtr OrtModelBuilder::AddInitializerAsRawData(
    std::string_view name,
    base::span<const int64_t> shape,
    base::span<const uint8_t> data,
    ONNXTensorElementDataType data_type) {
  ScopedOrtValuePtr initializer;

  OrtAllocator* allocator = nullptr;
  // Always use CPU allocator for raw data.
  RETURN_STATUS_IF_FAILED(
      GetOrtApi()->GetAllocatorWithDefaultOptions(&allocator));
  CHECK(allocator);

  RETURN_STATUS_IF_FAILED(GetOrtApi()->CreateTensorAsOrtValue(
      allocator, shape.data(), shape.size(), data_type,
      initializer.GetAddressOf()));

  void* ort_tensor_raw_data = nullptr;
  RETURN_STATUS_IF_FAILED(
      GetOrtApi()->GetTensorMutableData(initializer, &ort_tensor_raw_data));
  // ort_tensor_raw_data can be nullprt when data is empty.
  UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(ort_tensor_raw_data), data.size()))
      .copy_from(data);

  // Graph will own the initializer.
  RETURN_STATUS_IF_FAILED(GetOrtModelBuilderApi()->AddInitializerToGraph(
      graph_, name.data(), initializer.Release(), /*data_is_external=*/false));

  return ScopedOrtStatusPtr();
}

[[nodiscard]] ScopedOrtStatusPtr OrtModelBuilder::AddInitializerAsExternalData(
    std::string_view name,
    base::span<const int64_t> shape,
    base::span<const uint8_t> data,
    ONNXTensorElementDataType data_type) {
  ScopedOrtValuePtr initializer;

  auto weight = base::HeapArray<uint8_t>::CopiedFrom(data);
  model_info_->external_data.push_back(std::move(weight));

  // TODO(https://github.com/shiyi9801/chromium/issues/45): Use
  // `CreateTensorWithDataAndDeleterAsOrtValue()`.
  RETURN_STATUS_IF_FAILED(GetOrtApi()->CreateTensorWithDataAsOrtValue(
      memory_info_, model_info_->external_data.back().data(),
      model_info_->external_data.back().size(), shape.data(), shape.size(),
      data_type, initializer.GetAddressOf()));

  // Graph will own the initializer.
  RETURN_STATUS_IF_FAILED(GetOrtModelBuilderApi()->AddInitializerToGraph(
      graph_, name.data(), initializer.Release(), /*data_is_external=*/true));

  return ScopedOrtStatusPtr();
}

ScopedOrtOpAttrPtr OrtModelBuilder::CreateAttribute(std::string_view name,
                                                    OrtOpAttrData data) {
  ScopedOrtOpAttrPtr attribute;
  if (absl::holds_alternative<int64_t>(data)) {
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), &absl::get<int64_t>(data), /*len=*/1,
        OrtOpAttrType::ORT_OP_ATTR_INT, attribute.GetAddressOf()));
  } else if (absl::holds_alternative<float>(data)) {
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), &absl::get<float>(data), /*len=*/1,
        OrtOpAttrType::ORT_OP_ATTR_FLOAT, attribute.GetAddressOf()));
  } else if (absl::holds_alternative<std::string_view>(data)) {
    std::string_view string_data = absl::get<std::string_view>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), string_data.data(), string_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_STRING, attribute.GetAddressOf()));
  } else if (absl::holds_alternative<base::span<const int64_t>>(data)) {
    base::span<const int64_t> ints_data =
        absl::get<base::span<const int64_t>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), ints_data.data(), ints_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_INTS, attribute.GetAddressOf()));
  } else if (absl::holds_alternative<base::span<const float>>(data)) {
    base::span<const float> floats_data =
        absl::get<base::span<const float>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), floats_data.data(), floats_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_FLOATS, attribute.GetAddressOf()));
  } else if (absl::holds_alternative<base::span<const char*>>(data)) {
    base::span<const char*> strings_data =
        absl::get<base::span<const char*>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), strings_data.data(), strings_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_STRINGS, attribute.GetAddressOf()));
  }
  return attribute;
}

void OrtModelBuilder::AddNode(std::string_view op_type,
                              std::string_view node_name,
                              base::span<const char*> input_names,
                              base::span<const char*> output_names,
                              base::span<OrtOpAttr*> attributes) {
  ScopedOrtNodePtr node;
  CHECK_STATUS(GetOrtModelBuilderApi()->CreateNode(
      op_type.data(), kOrtDomainName, node_name.data(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(), node.GetAddressOf()));
  // Graph will own the node.
  CHECK_STATUS(GetOrtModelBuilderApi()->AddNodeToGraph(graph_, node.Release()));
}

std::unique_ptr<OrtModelBuilder::ModelInfo>
OrtModelBuilder::BuildAndTakeModelInfo() {
  // Graph will own the input/output `OrtValueInfo`s.
  std::vector<OrtValueInfo*> graph_inputs;
  graph_inputs.reserve(inputs_.size());
  for (auto& input : inputs_) {
    graph_inputs.push_back(input.Release());
  }
  CHECK_STATUS(GetOrtModelBuilderApi()->SetGraphInputs(
      graph_, graph_inputs.data(), graph_inputs.size()));

  std::vector<OrtValueInfo*> graph_outputs;
  graph_outputs.reserve(outputs_.size());
  for (auto& output : outputs_) {
    graph_outputs.push_back(output.Release());
  }
  CHECK_STATUS(GetOrtModelBuilderApi()->SetGraphOutputs(
      graph_, graph_outputs.data(), graph_outputs.size()));

  std::vector<const char*> domain_names = {kOrtDomainName};
  std::vector<int32_t> opset_versions = {kOrtOpsetVersion};

  CHECK_STATUS(GetOrtModelBuilderApi()->CreateModel(
      domain_names.data(), opset_versions.data(), domain_names.size(),
      model_info_->model.GetAddressOf()));

  // Model will own the graph.
  CHECK_STATUS(GetOrtModelBuilderApi()->AddGraphToModel(model_info_->model,
                                                        graph_.Release()));

  return std::move(model_info_);
}

}  // namespace ort

}  // namespace webnn
