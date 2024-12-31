// Copyright 2024 The Chromium Authors
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

}  // namespace

namespace ort {

OrtModelBuilder::ModelInfo::ModelInfo() = default;
OrtModelBuilder::ModelInfo::~ModelInfo() = default;

OrtModelBuilder::OrtModelBuilder(scoped_refptr<AllocatorOrt> allocator)
    : allocator_(std::move(allocator)),
      model_info_(std::make_unique<ModelInfo>()) {
  CHECK_STATUS(GetOrtGraphApi()->CreateGraph(graph_.get_pptr()));
}
OrtModelBuilder::~OrtModelBuilder() = default;

void OrtModelBuilder::AddInput(std::string_view name,
                               base::span<const int64_t> shape,
                               ONNXTensorElementDataType data_type) {
  ScopedOrtShape input_shape;
  CHECK_STATUS(GetOrtGraphApi()->CreateFixedShape(shape.data(), shape.size(),
                                                  input_shape.get_pptr()));

  ScopedOrtValueInfo input_info;
  CHECK_STATUS(GetOrtGraphApi()->CreateTensorValueInfo(
      name.data(), data_type, input_shape.get_pptr(), input_info.get_pptr()));
  CHECK_STATUS(
      GetOrtGraphApi()->AddInput(graph_.get_ptr(), input_info.get_pptr()));
}

void OrtModelBuilder::AddOutput(std::string_view name,
                                base::span<const int64_t> shape,
                                ONNXTensorElementDataType data_type) {
  ScopedOrtShape output_shape;
  CHECK_STATUS(GetOrtGraphApi()->CreateFixedShape(shape.data(), shape.size(),
                                                  output_shape.get_pptr()));

  ScopedOrtValueInfo output_info;
  CHECK_STATUS(GetOrtGraphApi()->CreateTensorValueInfo(
      name.data(), data_type, output_shape.get_pptr(), output_info.get_pptr()));
  CHECK_STATUS(
      GetOrtGraphApi()->AddOutput(graph_.get_ptr(), output_info.get_pptr()));
}

void OrtModelBuilder::AddInitializerAsRawData(
    std::string_view name,
    base::span<const int64_t> shape,
    base::span<const uint8_t> data,
    ONNXTensorElementDataType data_type) {
  ScopedOrtValue initializer;
  CHECK_STATUS(GetOrtApi()->CreateTensorAsOrtValue(
      allocator_->allocator(), shape.data(), shape.size(), data_type,
      initializer.get_pptr()));

  void* ort_tensor_raw_data = nullptr;
  CHECK_STATUS(GetOrtApi()->GetTensorMutableData(initializer.get_ptr(),
                                                 &ort_tensor_raw_data));
  CHECK(ort_tensor_raw_data);
  UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(ort_tensor_raw_data), data.size()))
      .copy_from(data);
  CHECK_STATUS(GetOrtGraphApi()->AddInitializer(graph_.get_ptr(), name.data(),
                                                initializer.get_pptr()));
}

void OrtModelBuilder::AddInitializerAsExternalData(
    std::string_view name,
    base::span<const int64_t> shape,
    base::span<const uint8_t> data,
    ONNXTensorElementDataType data_type) {
  auto weight = base::HeapArray<uint8_t>::CopiedFrom(data);
  model_info_->external_data.push_back(std::move(weight));

  ScopedOrtValue initializer;
  CHECK_STATUS(GetOrtApi()->CreateTensorWithDataAsOrtValue(
      allocator_->memory_info(), model_info_->external_data.back().data(),
      model_info_->external_data.back().size(), shape.data(), shape.size(),
      data_type, initializer.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddInitializer(graph_.get_ptr(), name.data(),
                                                initializer.get_pptr()));
}

void OrtModelBuilder::CreateAttribute(ScopedOrtOpAttr& attribute,
                                      std::string_view name,
                                      OrtOpAttrData data) {
  if (absl::holds_alternative<int64_t>(data)) {
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), &absl::get<int64_t>(data), /*len=*/1,
        OrtOpAttrType::ORT_OP_ATTR_INT, attribute.get_pptr()));
  } else if (absl::holds_alternative<float>(data)) {
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), &absl::get<float>(data), /*len=*/1,
        OrtOpAttrType::ORT_OP_ATTR_FLOAT, attribute.get_pptr()));
  } else if (absl::holds_alternative<std::string_view>(data)) {
    std::string_view string_data = absl::get<std::string_view>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), string_data.data(), string_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_STRING, attribute.get_pptr()));
  } else if (absl::holds_alternative<base::span<const int64_t>>(data)) {
    base::span<const int64_t> ints_data =
        absl::get<base::span<const int64_t>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), ints_data.data(), ints_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_INTS, attribute.get_pptr()));
  } else if (absl::holds_alternative<base::span<const float>>(data)) {
    base::span<const float> floats_data =
        absl::get<base::span<const float>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), floats_data.data(), floats_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_FLOATS, attribute.get_pptr()));
  } else if (absl::holds_alternative<base::span<const char*>>(data)) {
    base::span<const char*> strings_data =
        absl::get<base::span<const char*>>(data);
    CHECK_STATUS(GetOrtApi()->CreateOpAttr(
        name.data(), strings_data.data(), strings_data.size(),
        OrtOpAttrType::ORT_OP_ATTR_STRINGS, attribute.get_pptr()));
  }
}

void OrtModelBuilder::AddNode(std::string_view op_type,
                              std::string_view node_name,
                              base::span<const char*> input_names,
                              base::span<const char*> output_names,
                              base::span<OrtOpAttr**> attributes) {
  ScopedOrtNode node;
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      op_type.data(), kOrtDomainName, node_name.data(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(), node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

std::unique_ptr<OrtModelBuilder::ModelInfo>
OrtModelBuilder::BuildAndTakeModelInfo() {
  std::vector<const char*> domain_names = {kOrtDomainName};
  std::vector<int32_t> opset_versions = {kOrtOpsetVersion};

  CHECK_STATUS(GetOrtGraphApi()->CreateModel(
      domain_names.data(), opset_versions.data(), domain_names.size(),
      model_info_->model.get_pptr()));

  CHECK_STATUS(GetOrtGraphApi()->AddGraph(model_info_->model.get_ptr(),
                                          graph_.get_pptr()));

  return std::move(model_info_);
}

}  // namespace ort

}  // namespace webnn
