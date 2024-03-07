// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/graph_builder.h"

#include "base/containers/fixed_flat_set.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/coremltools/mlmodel/format/FeatureTypes.pb.h"
#include "third_party/coremltools/mlmodel/format/MIL.pb.h"

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

CoreML::Specification::MILSpec::DataType OperandTypeToMILDataType(
    webnn::mojom::Operand::DataType data_type) {
  switch (data_type) {
    case webnn::mojom::Operand::DataType::kFloat32:
      return CoreML::Specification::MILSpec::DataType::FLOAT32;
    case webnn::mojom::Operand::DataType::kFloat16:
      return CoreML::Specification::MILSpec::DataType::FLOAT16;
    case webnn::mojom::Operand::DataType::kInt32:
      return CoreML::Specification::MILSpec::DataType::INT32;
    case webnn::mojom::Operand::DataType::kUint32:
      return CoreML::Specification::MILSpec::DataType::UINT32;
    case webnn::mojom::Operand::DataType::kInt64:
      return CoreML::Specification::MILSpec::DataType::INT64;
    case webnn::mojom::Operand::DataType::kUint64:
      return CoreML::Specification::MILSpec::DataType::UINT64;
    case webnn::mojom::Operand::DataType::kInt8:
      return CoreML::Specification::MILSpec::DataType::INT8;
    case webnn::mojom::Operand::DataType::kUint8:
      return CoreML::Specification::MILSpec::DataType::UINT8;
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

  program_ = ml_model_.mutable_mlprogram();
  program_->set_version(1);

  // Creates a Program with a single main function, and a single block within
  // the function. The block contains all the ops right now.
  // TODO(https://crbug.com/327216253): figure out when to use CoreML7 for some
  // ops.
  auto& main_function = (*program_->mutable_functions())["main"];
  // CoreML6 means specification version 7.
  main_function.set_opset("CoreML6");
  auto& block = (*main_function.mutable_block_specializations())["CoreML6"];

  // Add inputs.
  const IdToOperandMap& id_to_operand_map = graph_info.id_to_operand_map;
  for (auto& input_id : graph_info.input_operands) {
    RETURN_IF_ERROR(AddInput(id_to_operand_map, input_id, main_function));
  }

  // TODO(https://crbug.com/327217753): support constants written to a separate
  // weight file.

  // Add operations.
  for (auto& operation : graph_info.operations) {
    switch (operation->which()) {
      case mojom::Operation::Tag::kElementWiseBinary: {
        RETURN_IF_ERROR(AddOperationForBinary(
            id_to_operand_map, *operation->get_element_wise_binary(), block));
        break;
      }
      default: {
        return base::unexpected("This operator is not implemented.");
      }
    }
  }

  // Add output.
  for (auto& output_id : graph_info.output_operands) {
    block.add_outputs(
        GetCoreMLNameFromOperand(output_id, *id_to_operand_map.at(output_id)));
    RETURN_IF_ERROR(AddOutput(id_to_operand_map, output_id));
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

[[nodiscard]] base::expected<void, std::string> GraphBuilder::AddInput(
    const IdToOperandMap& id_to_operand_map,
    uint64_t input_id,
    CoreML::Specification::MILSpec::Function& main_function) {
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_input();
  const OperandPtr& operand = id_to_operand_map.at(input_id);
  RETURN_IF_ERROR(
      PopulateFeatureDescription(input_id, *operand, feature_description));

  PopulateNamedValueType(input_id, *id_to_operand_map.at(input_id),
                         main_function.add_inputs());

  CHECK(input_name_to_id_map_.try_emplace(operand->name.value(), input_id)
            .second);
  return base::ok();
}

[[nodiscard]] base::expected<void, std::string> GraphBuilder::AddOutput(
    const IdToOperandMap& id_to_operand_map,
    uint64_t output_id) {
  const auto output_iterator = id_to_op_input_info_map_.find(output_id);
  CHECK(output_iterator != id_to_op_input_info_map_.end());
  const OperandPtr& operand = id_to_operand_map.at(output_id);
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_output();
  RETURN_IF_ERROR(
      PopulateFeatureDescription(output_id, *operand, feature_description));
  return base::ok();
}

base::expected<void, std::string> GraphBuilder::AddOperationForBinary(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ElementWiseBinary& operation,
    CoreML::Specification::MILSpec::Block& block) {
  auto* op = block.add_operations();

  auto input_lhs = id_to_op_input_info_map_.at(operation.lhs_operand);
  auto input_rhs = id_to_op_input_info_map_.at(operation.rhs_operand);
  // Input keys (x, y) and supported types are defined in coremltools.
  // https://github.com/apple/coremltools/blob/b416f36054af9ca9d10b2d74ba215d0454677ca0/coremltools/converters/mil/mil/ops/defs/iOS15/elementwise_binary.py#L33
  static constexpr auto kSupportedBinaryOpsTypes =
      base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
          {CoreML::Specification::MILSpec::DataType::FLOAT16,
           CoreML::Specification::MILSpec::DataType::FLOAT32,
           CoreML::Specification::MILSpec::DataType::INT32});

  if (!kSupportedBinaryOpsTypes.contains(input_lhs.mil_data_type) ||
      !kSupportedBinaryOpsTypes.contains(input_rhs.mil_data_type)) {
    return base::unexpected("Unsupported input datatype.");
  }

  (*op->mutable_inputs())["x"].add_arguments()->set_name(input_lhs.coreml_name);
  (*op->mutable_inputs())["y"].add_arguments()->set_name(input_rhs.coreml_name);

  switch (operation.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      op->set_type("add");
      break;
    }
    case mojom::ElementWiseBinary::Kind::kDiv: {
      op->set_type("real_div");
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMul: {
      op->set_type("mul");
      break;
    }
    case mojom::ElementWiseBinary::Kind::kSub: {
      op->set_type("sub");
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMax: {
      op->set_type("maximum");
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMin: {
      op->set_type("minimum");
      break;
    }
    case mojom::ElementWiseBinary::Kind::kPow: {
      op->set_type("pow");
      break;
    }
    default:
      return base::unexpected("Unimplemented Binary Operator.");
  }

  PopulateNamedValueType(operation.output_operand,
                         *id_to_operand_map.at(operation.output_operand),
                         op->add_outputs());

  return base::ok();
}

[[nodiscard]] const GraphBuilder::OperandInfo* GraphBuilder::GetOperandInfo(
    uint64_t operand_id) const {
  const auto input_iterator = id_to_op_input_info_map_.find(operand_id);
  CHECK(input_iterator != id_to_op_input_info_map_.end());
  return &input_iterator->second;
}

base::expected<void, std::string> GraphBuilder::PopulateFeatureDescription(
    uint64_t operand_id,
    const webnn::mojom::Operand& operand,
    ::CoreML::Specification::FeatureDescription* feature_description) {
  auto* feature_type = feature_description->mutable_type();
  auto* array_feature_type = feature_type->mutable_multiarraytype();
  switch (operand.data_type) {
    case webnn::mojom::Operand::DataType::kFloat32:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_FLOAT32);
      break;
    case webnn::mojom::Operand::DataType::kFloat16:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_FLOAT16);
      break;
    case webnn::mojom::Operand::DataType::kInt32:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_INT32);
      break;
    case webnn::mojom::Operand::DataType::kUint32:
    case webnn::mojom::Operand::DataType::kInt64:
    case webnn::mojom::Operand::DataType::kUint64:
    case webnn::mojom::Operand::DataType::kInt8:
    case webnn::mojom::Operand::DataType::kUint8:
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

void GraphBuilder::PopulateNamedValueType(
    uint64_t operand_id,
    const webnn::mojom::Operand& operand,
    CoreML::Specification::MILSpec::NamedValueType* value_type) {
  value_type->set_name(GetCoreMLNameFromOperand(operand_id, operand));
  auto* tensor_type = value_type->mutable_type()->mutable_tensortype();
  auto mil_data_type = OperandTypeToMILDataType(operand.data_type);
  tensor_type->set_datatype(mil_data_type);
  tensor_type->set_rank(operand.dimensions.empty() ? 1
                                                   : operand.dimensions.size());
  if (operand.dimensions.empty()) {
    tensor_type->set_rank(1);
    tensor_type->add_dimensions()->mutable_constant()->set_size(1);
  } else {
    tensor_type->set_rank(operand.dimensions.size());
    for (int dimension : operand.dimensions) {
      tensor_type->add_dimensions()->mutable_constant()->set_size(dimension);
    }
  }

  // WebNN allows 0d scalar operands to have empty dimensions.
  // At the input and output nodes, these can be treated as a 1D tensor to
  // satisfy CoreML's requirement of having at least 1 dimension.
  CHECK(id_to_op_input_info_map_
            .try_emplace(
                operand_id,
                OperandInfo(GetCoreMLNameFromOperand(operand_id, operand),
                            operand.dimensions.empty()
                                ? std::vector<uint32_t>({1})
                                : operand.dimensions,
                            operand.data_type, mil_data_type))
            .second);
}

GraphBuilder::OperandInfo::OperandInfo(
    std::string coreml_name,
    std::vector<uint32_t> dimensions,
    webnn::mojom::Operand::DataType data_type,
    CoreML::Specification::MILSpec::DataType mil_data_type)
    : coreml_name(std::move(coreml_name)),
      dimensions(std::move(dimensions)),
      data_type(data_type),
      mil_data_type(std::move(mil_data_type)) {}

GraphBuilder::OperandInfo::OperandInfo() = default;
GraphBuilder::OperandInfo::~OperandInfo() = default;
GraphBuilder::OperandInfo::OperandInfo(OperandInfo&) = default;
GraphBuilder::OperandInfo::OperandInfo(OperandInfo&&) = default;

}  // namespace webnn::coreml
