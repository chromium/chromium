// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/model_editor.h"

#include <numeric>
#include <ranges>

#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/types/fixed_array.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_status.h"

namespace webnn::ort {

namespace {

// Domains
constexpr char kOrtDefaultDomain[] = "";
constexpr char kMSDomain[] = "com.microsoft";

// Opset versions
constexpr int32_t kOrtOpsetVersion = 21;

// EPContext op is used for exporting the compiled model.
// https://onnxruntime.ai/docs/execution-providers/EP-Context-Design.html#onnxruntime-ep-context-cache-feature-design
constexpr int32_t kEPContextOpsetVersion = 1;

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

std::vector<int64_t> VectorUint32ToInt64(base::span<const uint32_t> vec) {
  return std::vector<int64_t>(vec.begin(), vec.end());
}

size_t CalculateOrtTensorSizeInBytes(base::span<const int64_t> shape,
                                     ONNXTensorElementDataType data_type) {
  base::CheckedNumeric<uint64_t> element_size_in_bits;
  switch (data_type) {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: {
      element_size_in_bits = 64;
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: {
      element_size_in_bits = 32;
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: {
      element_size_in_bits = 16;
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: {
      element_size_in_bits = 8;
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4: {
      element_size_in_bits = 4;
      break;
    }
    default: {
      NOTREACHED()
          << "CalculateOrtTensorSizeInBytes() only supports WebNN data types.";
    }
  }
  auto tensor_size_in_bits = std::accumulate(
      shape.begin(), shape.end(), element_size_in_bits, std::multiplies());

  return ((tensor_size_in_bits + 7) / 8).ValueOrDie<size_t>();
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

  std::vector<int64_t> int64_shape = VectorUint32ToInt64(descriptor.shape());
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

ModelEditor::ModelInfo::ModelInfo() = default;
ModelEditor::ModelInfo::~ModelInfo() = default;

ModelEditor::ModelEditor() : model_info_(std::make_unique<ModelInfo>()) {
  const OrtApi* ort_api = GetOrtApi();
  // Create a CPU memory info, the constants will always be created in
  // CPU memory.
  CHECK_STATUS(ort_api->CreateCpuMemoryInfo(
      OrtDeviceAllocator, OrtMemTypeDefault,
      ScopedOrtMemoryInfo::Receiver(memory_info_).get()));

  const OrtModelEditorApi* ort_model_editor_api = GetOrtModelEditorApi();
  CHECK_STATUS(ort_model_editor_api->CreateGraph(
      ScopedOrtGraph::Receiver(graph_).get()));
}

ModelEditor::~ModelEditor() = default;

void ModelEditor::AddInput(base::cstring_view name,
                           const OperandDescriptor& descriptor) {
  CHECK(!has_built_);
  inputs_.push_back(CreateOrtValueInfo(name, descriptor));
}

void ModelEditor::AddOutput(base::cstring_view name,
                            const OperandDescriptor& descriptor) {
  CHECK(!has_built_);
  outputs_.push_back(CreateOrtValueInfo(name, descriptor));
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
  std::vector<int64_t> int64_shape = VectorUint32ToInt64(descriptor.shape());
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
  UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(mutable_data),
                            CalculateOrtTensorSizeInBytes(shape, data_type)))
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
  CHECK_EQ(data.size(), CalculateOrtTensorSizeInBytes(shape, data_type));

  // The data will not be copied into the graph, so it must be stored outside.
  model_info_->external_data.push_back(std::move(data));

  const OrtApi* ort_api = GetOrtApi();
  ScopedOrtValue initializer;
  // TODO(crbug.com/411465403): Use `CreateTensorWithDataAndDeleterAsOrtValue()`
  // to let ORT take the ownership of the tensor and free it when no longer in
  // use.
  CHECK_STATUS(ort_api->CreateTensorWithDataAsOrtValue(
      memory_info_.get(), model_info_->external_data.back().data(),
      model_info_->external_data.back().size(), shape.data(), shape.size(),
      data_type, ScopedOrtValue::Receiver(initializer).get()));

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
  std::visit(base::Overloaded{
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

  std::array<const char*, 2> domains = {kOrtDefaultDomain, kMSDomain};
  std::array<int32_t, 2> opset_versions = {kOrtOpsetVersion,
                                           kEPContextOpsetVersion};
  CHECK_STATUS(ort_model_editor_api->CreateModel(
      domains.data(), opset_versions.data(), domains.size(),
      ScopedOrtModel::Receiver(model_info_->model).get()));

  // Model will own the graph.
  CHECK_STATUS(ort_model_editor_api->AddGraphToModel(model_info_->model.get(),
                                                     graph_.release()));

  has_built_ = true;

  return std::move(model_info_);
}

}  // namespace webnn::ort
