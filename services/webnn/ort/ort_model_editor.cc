// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_model_editor.h"

#include <ranges>

#include "base/logging.h"
#include "base/notreached.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/webnn_constant_operand.h"

namespace webnn {

namespace ort {

OrtModelEditor::WeightsDeleter::WeightsDeleter() {
  const OrtApi* ort_api = GetOrtApi();
  CHECK(IsSuccess(ort_api->CreateCpuMemoryInfo(
      OrtDeviceAllocator, OrtMemTypeDefault,
      ScopedOrtMemoryInfo::Receiver(cpu_memory_info).get())));

  // `OrtAllocator` is a C-style structure (with a pointer to its definition),
  // `Info` and `Free` are just function pointers that need to be set to lambda
  // functions.
  OrtAllocator::version = ORT_API_VERSION;
  OrtAllocator::Info = [](const OrtAllocator* this_) -> const OrtMemoryInfo* {
    return static_cast<const WeightsDeleter*>(this_)->cpu_memory_info.get();
  };
  OrtAllocator::Free = [](OrtAllocator* this_, void* p) -> void {
    static_cast<WeightsDeleter*>(this_)->FreeImpl(p);
  };
  OrtAllocator::Alloc = [](OrtAllocator* /*this_*/, size_t /*size*/) -> void* {
    NOTREACHED() << "[WebNN] OrtAllocator::Alloc() should never be called.";
  };
  OrtAllocator::Reserve = [](OrtAllocator* /*this_*/,
                             size_t /*size*/) -> void* {
    NOTREACHED() << "[WebNN] OrtAllocator::Reserve() should never be called.";
  };
}

OrtModelEditor::WeightsDeleter::~WeightsDeleter() {
  LOG_IF(FATAL, !weights.empty())
      << "[WebNN] All the weights should be freed by ORT before "
         "`WeightsDeleter` is destroyed.";
}

void OrtModelEditor::WeightsDeleter::Take(base::HeapArray<uint8_t> data) {
  weights.push_back(std::move(data));
}

void OrtModelEditor::WeightsDeleter::FreeImpl(void* p) {
  // Exactly one element should be erased.
  CHECK_EQ(std::erase_if(weights,
                         [p](const base::HeapArray<uint8_t>& data) {
                           return data.data() == p;
                         }),
           1u);
}

OrtModelEditor::ModelInfo::ModelInfo()
    : weights_deleter(std::make_unique<WeightsDeleter>()) {}
OrtModelEditor::ModelInfo::~ModelInfo() = default;

ScopedOrtValueInfo CreateOrtValueInfo(std::string_view name,
                                      base::span<const int64_t> shape,
                                      ONNXTensorElementDataType data_type) {
  ScopedOrtTensorTypeAndShapeInfo tensor_type_and_shape_info;
  CHECK(IsSuccess(GetOrtApi()->CreateTensorTypeAndShapeInfo(
      ScopedOrtTensorTypeAndShapeInfo::Receiver(tensor_type_and_shape_info)
          .get())));
  CHECK(IsSuccess(GetOrtApi()->SetTensorElementType(
      tensor_type_and_shape_info.get(), data_type)));
  CHECK(IsSuccess(GetOrtApi()->SetDimensions(tensor_type_and_shape_info.get(),
                                             shape.data(), shape.size())));

  ScopedOrtTypeInfo type_info;
  CHECK(IsSuccess(GetOrtModelEditorApi()->CreateTensorTypeInfo(
      tensor_type_and_shape_info.get(),
      ScopedOrtTypeInfo::Receiver(type_info).get())));

  ScopedOrtValueInfo value_info;
  CHECK(IsSuccess(GetOrtModelEditorApi()->CreateValueInfo(
      name.data(), type_info.get(),
      ScopedOrtValueInfo::Receiver(value_info).get())));
  return value_info;
}

OrtModelEditor::OrtModelEditor() : model_info_(std::make_unique<ModelInfo>()) {
  CHECK(IsSuccess(GetOrtModelEditorApi()->CreateGraph(
      ScopedOrtGraph::Receiver(graph_).get())));
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
    std::unique_ptr<WebNNConstantOperand> constant_operand) {
  bool use_external_data =
      constant_operand->ByteSpan().size() >= kMinExternalDataSize;
  const OperandDescriptor& descriptor = constant_operand->descriptor();
  std::vector<int64_t> int64_shape(descriptor.shape().begin(),
                                   descriptor.shape().end());
  ONNXTensorElementDataType data_type =
      OperandTypeToONNXTensorElementDataType(descriptor.data_type());
  if (use_external_data) {
    return AddInitializerAsExternalData(
        name, int64_shape, constant_operand->TakeData(), data_type);
  } else {
    return AddInitializerAsRawData(name, int64_shape,
                                   constant_operand->ByteSpan(), data_type);
  }
}

[[nodiscard]] ScopedOrtStatus OrtModelEditor::AddInitializer(
    std::string_view name,
    base::span<const int64_t> shape,
    base::span<const uint8_t> data,
    ONNXTensorElementDataType data_type) {
  bool data_is_external = data.size() >= kMinExternalDataSize;
  if (data_is_external) {
    return AddInitializerAsExternalData(
        name, shape, base::HeapArray<uint8_t>::CopiedFrom(data), data_type);
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
    base::HeapArray<uint8_t> weight,
    ONNXTensorElementDataType data_type) {
  ScopedOrtValue initializer;

  RETURN_STATUS_IF_FAILED(GetOrtApi()->CreateTensorWithDataAndDeleterAsOrtValue(
      model_info_->weights_deleter.get(), weight.data(), weight.size(),
      shape.data(), shape.size(), data_type,
      ScopedOrtValue::Receiver(initializer).get()));

  model_info_->weights_deleter->Take(std::move(weight));

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
    CHECK(IsSuccess(
        GetOrtApi()->CreateOpAttr(name.data(), &absl::get<int64_t>(data),
                                  /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
                                  ScopedOrtOpAttr::Receiver(attribute).get())));
  } else if (absl::holds_alternative<float>(data)) {
    CHECK(IsSuccess(
        GetOrtApi()->CreateOpAttr(name.data(), &absl::get<float>(data),
                                  /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_FLOAT,
                                  ScopedOrtOpAttr::Receiver(attribute).get())));
  } else if (absl::holds_alternative<std::string_view>(data)) {
    std::string_view string_data = absl::get<std::string_view>(data);
    CHECK(IsSuccess(GetOrtApi()->CreateOpAttr(
        name.data(), string_data.data(), string_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_STRING,
        ScopedOrtOpAttr::Receiver(attribute).get())));
  } else if (absl::holds_alternative<base::span<const int64_t>>(data)) {
    base::span<const int64_t> ints_data =
        absl::get<base::span<const int64_t>>(data);
    CHECK(IsSuccess(GetOrtApi()->CreateOpAttr(
        name.data(), ints_data.data(), ints_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_INTS,
        ScopedOrtOpAttr::Receiver(attribute).get())));
  } else if (absl::holds_alternative<base::span<const float>>(data)) {
    base::span<const float> floats_data =
        absl::get<base::span<const float>>(data);
    CHECK(IsSuccess(GetOrtApi()->CreateOpAttr(
        name.data(), floats_data.data(), floats_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_FLOATS,
        ScopedOrtOpAttr::Receiver(attribute).get())));
  } else if (absl::holds_alternative<base::span<const char*>>(data)) {
    base::span<const char*> strings_data =
        absl::get<base::span<const char*>>(data);
    CHECK(IsSuccess(GetOrtApi()->CreateOpAttr(
        name.data(), strings_data.data(), strings_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_STRINGS,
        ScopedOrtOpAttr::Receiver(attribute).get())));
  }
  return attribute;
}

void OrtModelEditor::AddNode(std::string_view op_type,
                             std::string_view node_name,
                             base::span<const char*> inputs,
                             base::span<const char*> outputs,
                             std::vector<ScopedOrtOpAttr> attributes,
                             std::string_view domain_name) {
  std::vector<OrtOpAttr*> attr_ptrs;
  attr_ptrs.reserve(attributes.size());
  std::ranges::transform(attributes, std::back_inserter(attr_ptrs),
                         [](auto& attr) { return attr.release(); });

  // Node will own the attributes.
  ScopedOrtNode node;
  CHECK(IsSuccess(GetOrtModelEditorApi()->CreateNode(
      op_type.data(), domain_name.data(), node_name.data(), inputs.data(),
      inputs.size(), outputs.data(), outputs.size(), attr_ptrs.data(),
      attr_ptrs.size(), ScopedOrtNode::Receiver(node).get())));
  // Graph will own the node.
  CHECK(IsSuccess(
      GetOrtModelEditorApi()->AddNodeToGraph(graph_.get(), node.release())));
}

std::unique_ptr<OrtModelEditor::ModelInfo>
OrtModelEditor::BuildAndTakeModelInfo() {
  // Graph will own the input/output `OrtValueInfo`s.
  std::vector<OrtValueInfo*> graph_inputs;
  graph_inputs.reserve(inputs_.size());
  for (auto& input : inputs_) {
    graph_inputs.push_back(input.release());
  }
  CHECK(IsSuccess(GetOrtModelEditorApi()->SetGraphInputs(
      graph_.get(), graph_inputs.data(), graph_inputs.size())));

  std::vector<OrtValueInfo*> graph_outputs;
  graph_outputs.reserve(outputs_.size());
  for (auto& output : outputs_) {
    graph_outputs.push_back(output.release());
  }
  CHECK(IsSuccess(GetOrtModelEditorApi()->SetGraphOutputs(
      graph_.get(), graph_outputs.data(), graph_outputs.size())));

  std::vector<const char*> domain_names = {kOrtDomain, kMSDomain,
                                           kMSNchwcDomain};
  std::vector<int32_t> opset_versions = {
      kOrtOpsetVersion, kEPContextOpsetVersion, kMSNchwcDomainOpsetVersion};
  if (base::FeatureList::IsEnabled(mojom::features::kWebNNOrtDml)) {
    domain_names.push_back(kMSDmlDomain);
    opset_versions.push_back(kMSDmlDomainOpsetVersion);
  }

  if (base::FeatureList::IsEnabled(mojom::features::kWebNNOrtWebGPU)) {
    domain_names.push_back(kMSInternalNhwcDomain);
    opset_versions.push_back(kMSInternalNhwcDomainOpsetVersion);
  }

  CHECK(IsSuccess(GetOrtModelEditorApi()->CreateModel(
      domain_names.data(), opset_versions.data(), domain_names.size(),
      ScopedOrtModel::Receiver(model_info_->model).get())));

  // Model will own the graph.
  CHECK(IsSuccess(GetOrtModelEditorApi()->AddGraphToModel(
      model_info_->model.get(), graph_.release())));

  return std::move(model_info_);
}

}  // namespace ort

}  // namespace webnn
