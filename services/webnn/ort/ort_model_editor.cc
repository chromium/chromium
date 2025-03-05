// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_model_editor.h"

#include "base/notreached.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"

namespace webnn {

namespace {

// Domains
constexpr char kOrtDomainName[] = "";
constexpr char kMSDomainName[] = "com.microsoft";

// Opsets
constexpr int32_t kOrtOpsetVersion = 21;
// EPContext op is used for exporting the EP context cache model.
// https://onnxruntime.ai/docs/execution-providers/EP-Context-Design.html#onnxruntime-ep-context-cache-feature-design
constexpr int32_t kEPContextOpsetVersion = 1;

// Define the minimum size(in bytes) to use external data.
constexpr size_t kMinExternalDataSize = 128;

}  // namespace

namespace ort {

OrtModelEditor::ModelInfo::ModelInfo() = default;
OrtModelEditor::ModelInfo::~ModelInfo() = default;

ScopedOrtValueInfo CreateOrtValueInfo(std::string_view name,
                                      base::span<const int64_t> shape,
                                      ONNXTensorElementDataType data_type) {
  ScopedOrtTensorTypeAndShapeInfo tensor_type_and_shape_info;
  CHECK_STATUS(GetOrtApi()->CreateTensorTypeAndShapeInfo(
      ScopedOrtTensorTypeAndShapeInfo::Receiver(tensor_type_and_shape_info)
          .get()));
  CHECK_STATUS(GetOrtApi()->SetTensorElementType(
      tensor_type_and_shape_info.get(), data_type));
  CHECK_STATUS(GetOrtApi()->SetDimensions(tensor_type_and_shape_info.get(),
                                          shape.data(), shape.size()));

  ScopedOrtTypeInfo type_info;
  CHECK_STATUS(GetOrtModelEditorApi()->CreateTensorTypeInfo(
      tensor_type_and_shape_info.get(),
      ScopedOrtTypeInfo::Receiver(type_info).get()));

  ScopedOrtValueInfo value_info;
  CHECK_STATUS(GetOrtModelEditorApi()->CreateValueInfo(
      name.data(), type_info.get(),
      ScopedOrtValueInfo::Receiver(value_info).get()));
  return value_info;
}

OrtModelEditor::OrtModelEditor() : model_info_(std::make_unique<ModelInfo>()) {
  // WebNN constants are in CPU memory.
  CHECK_STATUS(GetOrtApi()->CreateCpuMemoryInfo(
      OrtDeviceAllocator, OrtMemTypeDefault,
      ScopedOrtMemoryInfo::Receiver(memory_info_).get()));
  CHECK_STATUS(GetOrtModelEditorApi()->CreateGraph(
      ScopedOrtGraph::Receiver(graph_).get()));
}

OrtModelEditor::~OrtModelEditor() = default;

void OrtModelEditor::AddInput(std::string_view name,
                              base::span<const int64_t> shape,
                              ONNXTensorElementDataType data_type) {
  inputs_.push_back(CreateOrtValueInfo(name, shape, data_type));
}

void OrtModelEditor::AddOutput(std::string_view name,
                               base::span<const int64_t> shape,
                               ONNXTensorElementDataType data_type) {
  outputs_.push_back(CreateOrtValueInfo(name, shape, data_type));
}

[[nodiscard]] ScopedOrtStatus OrtModelEditor::AddInitializer(
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

[[nodiscard]] ScopedOrtStatus OrtModelEditor::AddInitializerAsRawData(
    std::string_view name,
    base::span<const int64_t> shape,
    base::span<const uint8_t> data,
    ONNXTensorElementDataType data_type) {
  ScopedOrtValue initializer;

  OrtAllocator* allocator = nullptr;
  // Always use CPU allocator for raw data.
  RETURN_STATUS_IF_FAILED(
      GetOrtApi()->GetAllocatorWithDefaultOptions(&allocator));
  CHECK(allocator);

  RETURN_STATUS_IF_FAILED(GetOrtApi()->CreateTensorAsOrtValue(
      allocator, shape.data(), shape.size(), data_type,
      ScopedOrtValue::Receiver(initializer).get()));

  void* ort_tensor_raw_data = nullptr;
  RETURN_STATUS_IF_FAILED(GetOrtApi()->GetTensorMutableData(
      initializer.get(), &ort_tensor_raw_data));
  // ort_tensor_raw_data can be nullprt when data is empty.
  UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(ort_tensor_raw_data), data.size()))
      .copy_from(data);

  // Graph will own the initializer.
  RETURN_STATUS_IF_FAILED(GetOrtModelEditorApi()->AddInitializerToGraph(
      graph_.get(), name.data(), initializer.release(),
      /*data_is_external=*/false));

  return ScopedOrtStatus();
}

[[nodiscard]] ScopedOrtStatus OrtModelEditor::AddInitializerAsExternalData(
    std::string_view name,
    base::span<const int64_t> shape,
    base::span<const uint8_t> data,
    ONNXTensorElementDataType data_type) {
  ScopedOrtValue initializer;

  auto weight = base::HeapArray<uint8_t>::CopiedFrom(data);
  model_info_->external_data.push_back(std::move(weight));

  // TODO(https://github.com/shiyi9801/chromium/issues/45): Use
  // `CreateTensorWithDataAndDeleterAsOrtValue()`.
  RETURN_STATUS_IF_FAILED(GetOrtApi()->CreateTensorWithDataAsOrtValue(
      memory_info_.get(), model_info_->external_data.back().data(),
      model_info_->external_data.back().size(), shape.data(), shape.size(),
      data_type, ScopedOrtValue::Receiver(initializer).get()));

  // Graph will own the initializer.
  RETURN_STATUS_IF_FAILED(GetOrtModelEditorApi()->AddInitializerToGraph(
      graph_.get(), name.data(), initializer.release(),
      /*data_is_external=*/true));

  return ScopedOrtStatus();
}

ScopedOrtOpAttr OrtModelEditor::CreateAttribute(std::string_view name,
                                                OrtOpAttrData data) {
  ScopedOrtOpAttr attribute;
  if (absl::holds_alternative<int64_t>(data)) {
    CHECK_STATUS(
        GetOrtApi()->CreateOpAttr(name.data(), &absl::get<int64_t>(data),
                                  /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
                                  ScopedOrtOpAttr::Receiver(attribute).get()));
  } else if (absl::holds_alternative<float>(data)) {
    CHECK_STATUS(
        GetOrtApi()->CreateOpAttr(name.data(), &absl::get<float>(data),
                                  /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_FLOAT,
                                  ScopedOrtOpAttr::Receiver(attribute).get()));
  } else if (absl::holds_alternative<std::string_view>(data)) {
    std::string_view string_data = absl::get<std::string_view>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), string_data.data(), string_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_STRING,
        ScopedOrtOpAttr::Receiver(attribute).get()));
  } else if (absl::holds_alternative<base::span<const int64_t>>(data)) {
    base::span<const int64_t> ints_data =
        absl::get<base::span<const int64_t>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), ints_data.data(), ints_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_INTS,
        ScopedOrtOpAttr::Receiver(attribute).get()));
  } else if (absl::holds_alternative<base::span<const float>>(data)) {
    base::span<const float> floats_data =
        absl::get<base::span<const float>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), floats_data.data(), floats_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_FLOATS,
        ScopedOrtOpAttr::Receiver(attribute).get()));
  } else if (absl::holds_alternative<base::span<const char*>>(data)) {
    base::span<const char*> strings_data =
        absl::get<base::span<const char*>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), strings_data.data(), strings_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_STRINGS,
        ScopedOrtOpAttr::Receiver(attribute).get()));
  }
  return attribute;
}

void OrtModelEditor::AddNode(std::string_view op_type,
                             std::string_view node_name,
                             base::span<const char*> input_names,
                             base::span<const char*> output_names,
                             base::span<OrtOpAttr*> attributes) {
  ScopedOrtNode node;
  CHECK_STATUS(GetOrtModelEditorApi()->CreateNode(
      op_type.data(), kOrtDomainName, node_name.data(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(),
      ScopedOrtNode::Receiver(node).get()));
  // Graph will own the node.
  CHECK_STATUS(
      GetOrtModelEditorApi()->AddNodeToGraph(graph_.get(), node.release()));
}

std::unique_ptr<OrtModelEditor::ModelInfo>
OrtModelEditor::BuildAndTakeModelInfo() {
  // Graph will own the input/output `OrtValueInfo`s.
  std::vector<OrtValueInfo*> graph_inputs;
  graph_inputs.reserve(inputs_.size());
  for (auto& input : inputs_) {
    graph_inputs.push_back(input.release());
  }
  CHECK_STATUS(GetOrtModelEditorApi()->SetGraphInputs(
      graph_.get(), graph_inputs.data(), graph_inputs.size()));

  std::vector<OrtValueInfo*> graph_outputs;
  graph_outputs.reserve(outputs_.size());
  for (auto& output : outputs_) {
    graph_outputs.push_back(output.release());
  }
  CHECK_STATUS(GetOrtModelEditorApi()->SetGraphOutputs(
      graph_.get(), graph_outputs.data(), graph_outputs.size()));

  std::array<const char*, 2> domain_names = {kOrtDomainName, kMSDomainName};
  std::array<int32_t, 2> opset_versions = {kOrtOpsetVersion,
                                           kEPContextOpsetVersion};

  CHECK_STATUS(GetOrtModelEditorApi()->CreateModel(
      domain_names.data(), opset_versions.data(), domain_names.size(),
      ScopedOrtModel::Receiver(model_info_->model).get()));

  // Model will own the graph.
  CHECK_STATUS(GetOrtModelEditorApi()->AddGraphToModel(model_info_->model.get(),
                                                       graph_.release()));

  return std::move(model_info_);
}

}  // namespace ort

}  // namespace webnn
