// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/graph_builder.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/coremltools/mlmodel/format/FeatureTypes.pb.h"

namespace webnn::coreml {

using mojom::Operand;
using mojom::OperandPtr;
using mojom::Operation;

namespace {

std::string GetCoreMLNameFromOperand(uint64_t operand_id,
                                     const Operand& operand) {
  switch (operand.kind) {
    case Operand::Kind::kInput:
      CHECK(operand.name.has_value());
      return GetCoreMLNameFromInput(operand.name.value());
    case Operand::Kind::kConstant:
      return base::NumberToString(operand_id);
    case Operand::Kind::kOutput:
      if (operand.name.has_value()) {
        return GetCoreMLNameFromOutput(operand.name.value());
      } else {
        // Intermediate outputs don't have names so use operand_id instead.
        return base::NumberToString(operand_id);
      }
  }
}

}  // namespace

std::string GetCoreMLNameFromInput(const std::string& input_name) {
  // Prefix is added to user provided names to avoid collision with intermediate
  // operands' names
  return base::StrCat({"input_", input_name});
}

std::string GetCoreMLNameFromOutput(const std::string& output_name) {
  // Prefix is added to user provided names to avoid collision with intermediate
  // operands' names
  return base::StrCat({"output_", output_name});
}

// static
[[nodiscard]] base::expected<std::unique_ptr<GraphBuilder>, std::string>
GraphBuilder::CreateAndBuild(const mojom::GraphInfo& graph_info) {
  auto graph_builder = base::WrapUnique(new GraphBuilder());
  auto build_result = graph_builder->BuildCoreMLModel(graph_info);
  if (!build_result.has_value()) {
    return base::unexpected(build_result.error());
  }
  return graph_builder;
}

[[nodiscard]] base::expected<void, std::string> GraphBuilder::BuildCoreMLModel(
    const mojom::GraphInfo& graph_info) {
  CHECK_EQ(ml_model_.specificationversion(), 0);
  // Based on comment in Model.proto
  //  * 7 : iOS 16, macOS 13, tvOS 16, watchOS 9 (Core ML 6)
  //  * - FLOAT16 array data type
  //  * - GRAYSCALE_FLOAT16 image color space.
  // use the model specification version supported on macOS 13 which is
  // version 7.
  ml_model_.set_specificationversion(7);
  ml_model_.set_isupdatable(false);

  // Add inputs.
  const IdToOperandMap& id_to_operand_map = graph_info.id_to_operand_map;
  for (auto& input_id : graph_info.input_operands) {
    RETURN_IF_ERROR(CreateInputNode(id_to_operand_map, input_id));
  }

  neural_network_ = ml_model_.mutable_neuralnetwork();
  neural_network_->set_arrayinputshapemapping(
      ::CoreML::Specification::NeuralNetworkMultiArrayShapeMapping::
          EXACT_ARRAY_MAPPING);

  // Add constants.
  for (auto& [key, buffer] : graph_info.constant_id_to_buffer_map) {
    RETURN_IF_ERROR(CreateConstantLayer(id_to_operand_map, buffer, key));
  }

  // Add operations.
  for (auto& operation : graph_info.operations) {
    switch (operation->which()) {
      case mojom::Operation::Tag::kElementWiseBinary: {
        RETURN_IF_ERROR(CreateOperatorNodeForBinary(
            id_to_operand_map, *operation->get_element_wise_binary()));
        break;
      }
      default: {
        return base::unexpected("This operator is not implemented.");
      }
    }
  }

  // Add output.
  for (auto& output_id : graph_info.output_operands) {
    RETURN_IF_ERROR(CreateOutputNode(id_to_operand_map, output_id));
  }
  return base::ok();
}

GraphBuilder::GraphBuilder() = default;
GraphBuilder::~GraphBuilder() = default;

std::string GraphBuilder::GetSerializedCoreMLModel() {
  std::string serialized_model;
  ml_model_.SerializeToString(&serialized_model);
  return serialized_model;
}

const GraphBuilder::OperandInfo* GraphBuilder::FindInputOperandInfo(
    const std::string& input_name) const {
  auto id = input_name_to_id_map_.find(input_name);
  if (id == input_name_to_id_map_.end()) {
    return nullptr;
  }
  return GetOperandInfo(id->second);
}

[[nodiscard]] base::expected<void, std::string> GraphBuilder::CreateInputNode(
    const IdToOperandMap& id_to_operand_map,
    uint64_t input_id) {
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_input();
  const OperandPtr& operand = id_to_operand_map.at(input_id);
  RETURN_IF_ERROR(
      PopulateFeatureDescription(input_id, *operand, feature_description));
  // WebNN allows 0d scalar operands to have empty dimensions.
  // At the input and output nodes, these can be treated as a 1D tensor to
  // satisfy CoreML's requirement of having at least 1 dimension.
  CHECK(id_to_layer_input_info_map_
            .try_emplace(input_id, OperandInfo(feature_description->name(),
                                               operand->dimensions.empty()
                                                   ? std::vector<uint32_t>({1})
                                                   : operand->dimensions,
                                               operand->data_type))
            .second);
  CHECK(input_name_to_id_map_.try_emplace(operand->name.value(), input_id)
            .second);
  return base::ok();
}

[[nodiscard]] base::expected<void, std::string> GraphBuilder::CreateOutputNode(
    const IdToOperandMap& id_to_operand_map,
    uint64_t output_id) {
  const auto output_iterator = id_to_layer_input_info_map_.find(output_id);
  CHECK(output_iterator != id_to_layer_input_info_map_.end());
  const OperandPtr& operand = id_to_operand_map.at(output_id);
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_output();
  RETURN_IF_ERROR(
      PopulateFeatureDescription(output_id, *operand, feature_description));
  return base::ok();
}

base::expected<void, std::string> GraphBuilder::CreateOperatorNodeForBinary(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ElementWiseBinary& operation) {
  auto* neural_network_layer = neural_network_->add_layers();
  neural_network_layer->mutable_name()->assign(
      base::NumberToString(layer_count_++));
  AddInputToNeuralNetworkLayer(operation.lhs_operand, neural_network_layer);
  AddInputToNeuralNetworkLayer(operation.rhs_operand, neural_network_layer);
  switch (operation.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      neural_network_layer->mutable_add();
      break;
    }
    default:
      return base::unexpected("Unimplemented Binary Operator");
  }
  AddOutputToNeuralNetworkLayer(id_to_operand_map, operation.output_operand,
                                neural_network_layer);
  return base::ok();
}

base::expected<void, std::string> GraphBuilder::CreateConstantLayer(
    const IdToOperandMap& id_to_operand_map,
    const mojo_base::BigBuffer& buffer,
    uint64_t constant_id) {
  auto* neural_network_layer = neural_network_->add_layers();
  neural_network_layer->mutable_name()->assign(
      base::NumberToString(layer_count_++));
  auto* constant_tensor = neural_network_layer->mutable_loadconstantnd();

  auto& operand = id_to_operand_map.at(constant_id);
  switch (operand->data_type) {
    case Operand::DataType::kFloat32: {
      const auto* data = reinterpret_cast<const float*>(buffer.data());
      std::copy(data, data + buffer.size() / sizeof(*data),
                google::protobuf::RepeatedFieldBackInserter(
                    constant_tensor->mutable_data()->mutable_floatvalue()));
      break;
    }
    case Operand::DataType::kFloat16: {
      // float16value has type of bytes (std::string)
      constant_tensor->mutable_data()->mutable_float16value()->assign(
          buffer.data(), buffer.data() + buffer.size());
      break;
    }
    // TODO: support quantized values.
    case Operand::DataType::kInt64:
    case Operand::DataType::kInt32:
    case Operand::DataType::kInt8:
    case Operand::DataType::kUint64:
    case Operand::DataType::kUint32:
    case Operand::DataType::kUint8:
      return base::unexpected("Unsupported constant datatype");
  }

  for (int dimension : operand->dimensions) {
    constant_tensor->mutable_shape()->Add(dimension);
  }
  AddOutputToNeuralNetworkLayer(id_to_operand_map, constant_id,
                                neural_network_layer);
  return base::ok();
}

[[nodiscard]] const GraphBuilder::OperandInfo* GraphBuilder::GetOperandInfo(
    uint64_t operand_id) const {
  const auto input_iterator = id_to_layer_input_info_map_.find(operand_id);
  CHECK(input_iterator != id_to_layer_input_info_map_.end());
  return &input_iterator->second;
}

base::expected<void, std::string> GraphBuilder::PopulateFeatureDescription(
    uint64_t operand_id,
    const webnn::mojom::Operand& operand,
    ::CoreML::Specification::FeatureDescription* feature_description) {
  auto* feature_type = feature_description->mutable_type();
  auto* array_feature_type = feature_type->mutable_multiarraytype();
  switch (operand.data_type) {
    case webnn::mojom::Operand_DataType::kFloat32:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_FLOAT32);
      break;
    case webnn::mojom::Operand_DataType::kFloat16:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_FLOAT16);
      break;
    case webnn::mojom::Operand_DataType::kInt32:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_INT32);
      break;
    case webnn::mojom::Operand_DataType::kUint32:
    case webnn::mojom::Operand_DataType::kInt64:
    case webnn::mojom::Operand_DataType::kUint64:
    case webnn::mojom::Operand_DataType::kInt8:
    case webnn::mojom::Operand_DataType::kUint8:
      return base::unexpected("Unsupported input datatype.");
  }
  // FeatureDescriptions are about input and output features, WebNN allows
  // scalar operands to have empty dimensions. At the input and output layers
  // these can be treated as a 1D tensor to satisfy CoreML's requirement of
  // having atleast 1 dimension.
  if (operand.dimensions.empty()) {
    array_feature_type->add_shape(1);
  } else {
    for (int dimension : operand.dimensions) {
      array_feature_type->add_shape(dimension);
    }
  }
  feature_description->mutable_name()->assign(
      GetCoreMLNameFromOperand(operand_id, operand));
  return base::ok();
}

void GraphBuilder::AddInputToNeuralNetworkLayer(
    uint64_t input_id,
    CoreML::Specification::NeuralNetworkLayer* neural_network_layer) {
  const OperandInfo* operand = GetOperandInfo(input_id);
  neural_network_layer->add_input(operand->coreml_name);
  auto* tensor = neural_network_layer->add_inputtensor();
  if (operand->dimensions.empty()) {
    tensor->set_rank(1);
    tensor->add_dimvalue(1);
  } else {
    tensor->set_rank(operand->dimensions.size());
    for (int dimension : operand->dimensions) {
      tensor->add_dimvalue(dimension);
    }
  }
}

void GraphBuilder::AddOutputToNeuralNetworkLayer(
    const IdToOperandMap& id_to_operand_map,
    uint64_t output_id,
    ::CoreML::Specification::NeuralNetworkLayer* neural_network_layer) {
  const OperandPtr& operand = id_to_operand_map.at(output_id);
  auto coreml_name = GetCoreMLNameFromOperand(output_id, *operand);
  CHECK(
      id_to_layer_input_info_map_
          .try_emplace(output_id, OperandInfo(coreml_name, operand->dimensions,
                                              operand->data_type))
          .second);
  neural_network_layer->add_output(coreml_name);
  auto* tensor = neural_network_layer->add_outputtensor();
  if (operand->dimensions.empty()) {
    tensor->set_rank(1);
    tensor->add_dimvalue(1);
  } else {
    tensor->set_rank(operand->dimensions.size());
    for (int dimension : operand->dimensions) {
      tensor->add_dimvalue(dimension);
    }
  }
}

GraphBuilder::OperandInfo::OperandInfo(std::string coreml_name,
                                       std::vector<uint32_t> dimensions,
                                       webnn::mojom::Operand_DataType data_type)
    : coreml_name(std::move(coreml_name)),
      dimensions(std::move(dimensions)),
      data_type(data_type) {}

GraphBuilder::OperandInfo::OperandInfo() = default;
GraphBuilder::OperandInfo::~OperandInfo() = default;
GraphBuilder::OperandInfo::OperandInfo(OperandInfo&) = default;
GraphBuilder::OperandInfo::OperandInfo(OperandInfo&&) = default;

}  // namespace webnn::coreml
