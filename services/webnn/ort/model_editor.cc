// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/model_editor.h"

#include <ranges>

#include "base/types/fixed_array.h"
#include "services/webnn/ort/external_weights_manager.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_status.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace webnn::ort {

namespace {

// Domains
constexpr char kOrtDefaultDomain[] = "";
// Domain "com.microsoft" is required by EPContext op for exporting the compiled
// model.
// https://onnxruntime.ai/docs/execution-providers/EP-Context-Design.html#epcontext-op-schema
constexpr char kMSDomain[] = "com.microsoft";
// Domain "com.ms.internal.nhwc" provides alternative implementations of
// operators that are compatible with NHWC layout. This domain is required for
// certain operators when using WebGPU EP. See:
// https://github.com/microsoft/onnxruntime/blob/main/js/web/docs/webgpu-operators.md
constexpr char kMSInternalNhwcDomain[] = "com.ms.internal.nhwc";
// Domain "com.microsoft.nchwc" provides a layout optimization for default CPU
// EP, it's required when the optimization level is set to "ENABLE_ALL".
// https://onnxruntime.ai/docs/performance/model-optimizations/graph-optimizations.html#layout-optimizations
// TODO(crbug.com/442483649): Remove this domain once the ORT issue is fixed.
// https://github.com/microsoft/onnxruntime/issues/25914
constexpr char kMSNchwcDomain[] = "com.microsoft.nchwc";
// Domain "com.microsoft.dml" is required by DirectML EP for certain fused
// operators when the optimization level is set to "ENABLE_ALL", such as
// DmlFusedConv. See more details at
// https://github.com/microsoft/onnxruntime/blob/main/docs/OperatorKernels.md#dmlexecutionprovider
// TODO(crbug.com/442483649): Remove this domain once the ORT issue is fixed.
// https://github.com/microsoft/onnxruntime/issues/25914
constexpr char kMSDmlDomain[] = "com.microsoft.dml";

// Opset versions
constexpr int32_t kOrtOpsetVersion = 21;
// The op set version for domain "com.microsoft".
// https://github.com/microsoft/onnxruntime/blob/main/docs/ContribOperators.md#version-26
constexpr int32_t kEPContextOpsetVersion = 1;
// Domain "com.ms.internal.nhwc" provides operator implementations that are
// functionally equivalent to the standard ai.onnx domain but optimized for NHWC
// layout. So we should use the same opset version as the default domain.
constexpr int32_t kMSInternalNhwcDomainOpsetVersion = kOrtOpsetVersion;
// The op set version for domain "com.microsoft.nchwc".
// https://github.com/microsoft/onnxruntime/blob/main/docs/OperatorKernels.md#operators-implemented-by-cpuexecutionprovider
constexpr int32_t kMSNchwcDomainOpsetVersion = 1;
// The op set version for domain "com.microsoft.dml".
// https://github.com/microsoft/onnxruntime/blob/main/docs/OperatorKernels.md#dmlexecutionprovider
constexpr int32_t kMSDmlDomainOpsetVersion = 1;

// The minimum size (in bytes) to add the initializer as external data. An
// initializer less than 128 bytes might be used for shape inferencing which
// doesn't support external data.
// https://github.com/microsoft/onnxruntime/blob/c1ef02f74b0d648cc7e8558805fa90846ea11a35/include/onnxruntime/core/session/onnxruntime_c_api.h#L5564
constexpr size_t kMinExternalDataSize = 128;

const OrtApi* GetOrtApi() {
  return PlatformFunctions::GetInstance()->ort_api();
}

const OrtModelEditorApi* GetOrtModelEditorApi() {
  return PlatformFunctions::GetInstance()->ort_model_editor_api();
}

ScopedOrtValueInfo CreateOrtValueInfo(base::cstring_view name,
                                      const OperandDescriptor& descriptor) {
  const OrtApi* ort_api = GetOrtApi();
  ScopedOrtTensorTypeAndShapeInfo tensor_type_and_shape_info;
  CHECK_STATUS(ort_api->CreateTensorTypeAndShapeInfo(
      ScopedOrtTensorTypeAndShapeInfo::Receiver(tensor_type_and_shape_info)
          .get()));
  CHECK_STATUS(ort_api->SetTensorElementType(
      tensor_type_and_shape_info.get(),
      WebnnToOnnxDataType(descriptor.data_type())));

  std::vector<int64_t> int64_shape = WebnnToOnnxShape(descriptor.shape());
  CHECK_STATUS(ort_api->SetDimensions(tensor_type_and_shape_info.get(),
                                      int64_shape.data(), int64_shape.size()));

  const OrtModelEditorApi* ort_model_editor_api = GetOrtModelEditorApi();
  ScopedOrtTypeInfo type_info;
  CHECK_STATUS(ort_model_editor_api->CreateTensorTypeInfo(
      tensor_type_and_shape_info.get(),
      ScopedOrtTypeInfo::Receiver(type_info).get()));

  ScopedOrtValueInfo value_info;
  CHECK_STATUS(ort_model_editor_api->CreateValueInfo(
      name.c_str(), type_info.get(),
      ScopedOrtValueInfo::Receiver(value_info).get()));
  return value_info;
}

}  // namespace

ModelEditor::ModelInfo::ModelInfo()
    : external_weights_manager(std::make_unique<ExternalWeightsManager>()) {}
ModelEditor::ModelInfo::~ModelInfo() = default;

ModelEditor::ModelEditor() : model_info_(std::make_unique<ModelInfo>()) {
  const OrtModelEditorApi* ort_model_editor_api = GetOrtModelEditorApi();
  CHECK_STATUS(ort_model_editor_api->CreateGraph(
      ScopedOrtGraph::Receiver(graph_).get()));
}

ModelEditor::~ModelEditor() = default;

void ModelEditor::AddInput(base::cstring_view name,
                           const mojom::Operand& input) {
  CHECK(!has_built_);
  inputs_.push_back(CreateOrtValueInfo(name, input.descriptor));
  CHECK(input.name.has_value());
  operand_input_name_to_onnx_input_name_map.emplace_back(input.name.value(),
                                                         name);
}

void ModelEditor::AddOutput(base::cstring_view name,
                            const mojom::Operand& output) {
  CHECK(!has_built_);
  outputs_.push_back(CreateOrtValueInfo(name, output.descriptor));
  CHECK(output.name.has_value());
  operand_output_name_to_onnx_output_name_map.emplace_back(output.name.value(),
                                                           name);
}

void ModelEditor::AddInitializer(
    base::cstring_view name,
    std::unique_ptr<WebNNConstantOperand> constant_operand) {
  CHECK(!has_built_);

  bool use_external_data =
      constant_operand->ByteSpan().size() >= kMinExternalDataSize;
  const OperandDescriptor& descriptor = constant_operand->descriptor();
  ONNXTensorElementDataType data_type =
      WebnnToOnnxDataType(descriptor.data_type());
  std::vector<int64_t> int64_shape = WebnnToOnnxShape(descriptor.shape());
  if (use_external_data) {
    AddInitializerAsExternalData(name, data_type, int64_shape,
                                 constant_operand->TakeData());
  } else {
    AddInitializerAsRawData(name, data_type, int64_shape,
                            constant_operand->ByteSpan());
  }
}

void ModelEditor::AddInitializer(base::cstring_view name,
                                 ONNXTensorElementDataType data_type,
                                 base::span<const int64_t> shape,
                                 base::span<const uint8_t> data) {
  CHECK(!has_built_);

  bool use_external_data = data.size() >= kMinExternalDataSize;
  if (use_external_data) {
    AddInitializerAsExternalData(name, data_type, shape,
                                 base::HeapArray<uint8_t>::CopiedFrom(data));
  } else {
    AddInitializerAsRawData(name, data_type, shape, data);
  }
}

void ModelEditor::AddInitializerAsRawData(base::cstring_view name,
                                          ONNXTensorElementDataType data_type,
                                          base::span<const int64_t> shape,
                                          base::span<const uint8_t> data) {
  const OrtApi* ort_api = GetOrtApi();
  // Get the default CPU allocator, as the initializers will be used during
  // graph optimization which will happen on CPU.
  OrtAllocator* allocator = nullptr;
  CHECK_STATUS(ort_api->GetAllocatorWithDefaultOptions(&allocator));
  CHECK(allocator);

  ScopedOrtValue initializer;
  CHECK_STATUS(ort_api->CreateTensorAsOrtValue(
      allocator, shape.data(), shape.size(), data_type,
      ScopedOrtValue::Receiver(initializer).get()));

  void* mutable_data = nullptr;
  CHECK_STATUS(ort_api->GetTensorMutableData(initializer.get(), &mutable_data));
  // `mutable_data` can be nullptr when there is zero dimension in `shape`
  // a.k.a. empty tensors. While WebNN doesn't support zero dimensions the ORT
  // backend may need this feature in some cases, e.g., the ONNX Reshape op's
  // "new shape" can be an empty tensor when reshaping the input tensor to a
  // scalar.

  // SAFETY: `mutable_data` was created to hold a tensor of `shape` and
  // `data_type`.
  size_t tensor_size = 0;
  CHECK_STATUS(ort_api->GetTensorSizeInBytes(initializer.get(), &tensor_size));
  UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(mutable_data), tensor_size))
      .copy_from(data);

  const OrtModelEditorApi* ort_model_editor_api = GetOrtModelEditorApi();
  // Graph will own the initializer.
  CHECK_STATUS(ort_model_editor_api->AddInitializerToGraph(
      graph_.get(), name.c_str(), initializer.release(),
      /*data_is_external=*/false));
}

void ModelEditor::AddInitializerAsExternalData(
    base::cstring_view name,
    ONNXTensorElementDataType data_type,
    base::span<const int64_t> shape,
    base::HeapArray<uint8_t> data) {
  ScopedOrtValue initializer =
      model_info_->external_weights_manager->CreateInitializer(
          std::move(data), shape, data_type);

  const OrtModelEditorApi* ort_model_editor_api = GetOrtModelEditorApi();
  // Graph will own the initializer.
  CHECK_STATUS(ort_model_editor_api->AddInitializerToGraph(
      graph_.get(), name.c_str(), initializer.release(),
      /*data_is_external=*/true));
}

ScopedOrtOpAttr ModelEditor::CreateAttribute(base::cstring_view name,
                                             OrtOpAttrData data) {
  CHECK(!has_built_);

  const OrtApi* ort_api = GetOrtApi();
  ScopedOrtOpAttr attribute;
  std::visit(absl::Overload{
                 [&](int64_t int_data) {
                   CHECK_STATUS(ort_api->CreateOpAttr(
                       name.c_str(), &int_data,
                       /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
                       ScopedOrtOpAttr::Receiver(attribute).get()));
                 },
                 [&](float float_data) {
                   CHECK_STATUS(ort_api->CreateOpAttr(
                       name.c_str(), &float_data,
                       /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_FLOAT,
                       ScopedOrtOpAttr::Receiver(attribute).get()));
                 },
                 [&](base::cstring_view string_data) {
                   CHECK_STATUS(ort_api->CreateOpAttr(
                       name.c_str(), string_data.data(), string_data.size(),
                       OrtOpAttrType::ORT_OP_ATTR_STRING,
                       ScopedOrtOpAttr::Receiver(attribute).get()));
                 },
                 [&](base::span<const int64_t> ints_data) {
                   CHECK_STATUS(ort_api->CreateOpAttr(
                       name.c_str(), ints_data.data(), ints_data.size(),
                       OrtOpAttrType::ORT_OP_ATTR_INTS,
                       ScopedOrtOpAttr::Receiver(attribute).get()));
                 },
                 [&](base::span<const float> floats_data) {
                   CHECK_STATUS(ort_api->CreateOpAttr(
                       name.c_str(), floats_data.data(), floats_data.size(),
                       OrtOpAttrType::ORT_OP_ATTR_FLOATS,
                       ScopedOrtOpAttr::Receiver(attribute).get()));
                 },
                 [&](base::span<const char*> strings_data) {
                   CHECK_STATUS(ort_api->CreateOpAttr(
                       name.c_str(), strings_data.data(), strings_data.size(),
                       OrtOpAttrType::ORT_OP_ATTR_STRINGS,
                       ScopedOrtOpAttr::Receiver(attribute).get()));
                 }},
             data);

  return attribute;
}

void ModelEditor::AddNode(base::cstring_view op_type,
                          base::cstring_view node_name,
                          base::span<const char*> inputs,
                          base::span<const char*> outputs,
                          base::span<ScopedOrtOpAttr> attributes) {
  CHECK(!has_built_);

  base::FixedArray<OrtOpAttr*> node_attrs(attributes.size());
  std::ranges::transform(attributes, node_attrs.begin(),
                         [](auto& attr) { return attr.release(); });

  const OrtModelEditorApi* ort_model_editor_api = GetOrtModelEditorApi();
  // Node will own the attributes.
  ScopedOrtNode node;
  CHECK_STATUS(ort_model_editor_api->CreateNode(
      op_type.c_str(), kOrtDefaultDomain, node_name.c_str(), inputs.data(),
      inputs.size(), outputs.data(), outputs.size(), node_attrs.data(),
      node_attrs.size(), ScopedOrtNode::Receiver(node).get()));
  // Graph will own the node.
  CHECK_STATUS(
      ort_model_editor_api->AddNodeToGraph(graph_.get(), node.release()));
}

std::unique_ptr<ModelEditor::ModelInfo> ModelEditor::BuildAndTakeModelInfo() {
  CHECK(!has_built_);

  const OrtModelEditorApi* ort_model_editor_api = GetOrtModelEditorApi();
  // Graph will own the inputs and outputs.
  base::FixedArray<OrtValueInfo*> graph_inputs(inputs_.size());
  std::ranges::transform(inputs_, graph_inputs.begin(),
                         [](auto& input) { return input.release(); });
  CHECK_STATUS(ort_model_editor_api->SetGraphInputs(
      graph_.get(), graph_inputs.data(), graph_inputs.size()));

  base::FixedArray<OrtValueInfo*> graph_outputs(outputs_.size());
  std::ranges::transform(outputs_, graph_outputs.begin(),
                         [](auto& output) { return output.release(); });
  CHECK_STATUS(ort_model_editor_api->SetGraphOutputs(
      graph_.get(), graph_outputs.data(), graph_outputs.size()));

  std::array<const char*, 5> domains = {kOrtDefaultDomain, kMSDomain,
                                        kMSInternalNhwcDomain, kMSNchwcDomain,
                                        kMSDmlDomain};
  std::array<int32_t, 5> opset_versions = {
      kOrtOpsetVersion, kEPContextOpsetVersion,
      kMSInternalNhwcDomainOpsetVersion, kMSNchwcDomainOpsetVersion,
      kMSDmlDomainOpsetVersion};
  CHECK_STATUS(ort_model_editor_api->CreateModel(
      domains.data(), opset_versions.data(), domains.size(),
      ScopedOrtModel::Receiver(model_info_->model).get()));

  // Model will own the graph.
  CHECK_STATUS(ort_model_editor_api->AddGraphToModel(model_info_->model.get(),
                                                     graph_.release()));

  has_built_ = true;

  model_info_->operand_input_name_to_onnx_input_name =
      base::flat_map<std::string, std::string>(
          std::move(operand_input_name_to_onnx_input_name_map));
  model_info_->operand_output_name_to_onnx_output_name =
      base::flat_map<std::string, std::string>(
          std::move(operand_output_name_to_onnx_output_name_map));

  return std::move(model_info_);
}

}  // namespace webnn::ort
