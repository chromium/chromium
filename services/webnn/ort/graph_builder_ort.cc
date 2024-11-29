// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_builder_ort.h"

#include <fcntl.h>
#include <io.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/webnn_constant_operand.h"

namespace webnn {

namespace {

// Element-wise unary
constexpr char kOpTypeAbs[] = "Abs";
constexpr char kOpTypeCeil[] = "Ceil";
constexpr char kOpTypeCos[] = "Cos";
constexpr char kOpTypeExp[] = "Exp";
constexpr char kOpTypeFloor[] = "Floor";
constexpr char kOpTypeLog[] = "Log";
constexpr char kOpTypeNeg[] = "Neg";
constexpr char kOpTypeSign[] = "Sign";
constexpr char kOpTypeSin[] = "Sin";
constexpr char kOpTypeTan[] = "Tan";
constexpr char kOpTypeLogicalNot[] = "Not";
constexpr char kOpTypeIdentity[] = "Identity";
constexpr char kOpTypeSqrt[] = "Sqrt";
constexpr char kOpTypeErf[] = "Erf";
constexpr char kOpTypeReciprocal[] = "Reciprocal";
constexpr char kOpTypeCast[] = "Cast";

constexpr char kOpTypeRelu[] = "Relu";

constexpr char kBuildGraphError[] = "Failed to build graph.";

const base::FilePath::CharType kOnnxModelFileName[] =
    FILE_PATH_LITERAL("model.onnx");

onnx::TensorProto::DataType OperandTypeToOnnxDataType(
    OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return onnx::TensorProto::DataType::TensorProto_DataType_FLOAT;
    case OperandDataType::kFloat16:
      return onnx::TensorProto::DataType::TensorProto_DataType_FLOAT16;
    case OperandDataType::kInt32:
      return onnx::TensorProto::DataType::TensorProto_DataType_INT32;
    case OperandDataType::kUint32:
      return onnx::TensorProto::DataType::TensorProto_DataType_UINT32;
    case OperandDataType::kInt64:
      return onnx::TensorProto::DataType::TensorProto_DataType_INT64;
    case OperandDataType::kUint64:
      return onnx::TensorProto::DataType::TensorProto_DataType_UINT64;
    case OperandDataType::kInt8:
      return onnx::TensorProto::DataType::TensorProto_DataType_INT8;
    case OperandDataType::kUint8:
      return onnx::TensorProto::DataType::TensorProto_DataType_UINT8;
    case OperandDataType::kInt4:
      return onnx::TensorProto::DataType::TensorProto_DataType_INT4;
    case OperandDataType::kUint4:
      return onnx::TensorProto::DataType::TensorProto_DataType_UINT4;
  }
}

base::unexpected<mojom::ErrorPtr> NewNotSupportedError(std::string message) {
  return base::unexpected(mojom::Error::New(
      mojom::Error::Code::kNotSupportedError, std::move(message)));
}

base::unexpected<mojom::ErrorPtr> NewUnknownError(std::string message) {
  return base::unexpected(
      mojom::Error::New(mojom::Error::Code::kUnknownError, std::move(message)));
}

}  // namespace

namespace ort {

GraphBuilderOrt::OperandInfo::OperandInfo(
    std::string name,
    base::span<const uint32_t> shape,
    onnx::TensorProto::DataType onnx_data_type)
    : name(std::move(name)),
      shape(shape.begin(), shape.end()),
      onnx_data_type(onnx_data_type) {}

GraphBuilderOrt::OperandInfo::OperandInfo() = default;
GraphBuilderOrt::OperandInfo::~OperandInfo() = default;
GraphBuilderOrt::OperandInfo::OperandInfo(OperandInfo&) = default;
GraphBuilderOrt::OperandInfo::OperandInfo(OperandInfo&&) = default;

GraphBuilderOrt::Result::Result(base::FilePath working_directory)
    : working_directory(std::move(working_directory)) {}
GraphBuilderOrt::Result::~Result() = default;

const base::FilePath& GraphBuilderOrt::Result::GetModelFilePath() {
  return working_directory;
}

const GraphBuilderOrt::OperandInfo& GraphBuilderOrt::Result::GetOperandInfo(
    uint64_t operand_id) const {
  auto it = operand_infos.find(operand_id);
  CHECK(it != operand_infos.end());
  return it->second;
}

// static
base::expected<std::unique_ptr<GraphBuilderOrt::Result>, mojom::ErrorPtr>
GraphBuilderOrt::CreateAndBuild(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    const base::FilePath& working_directory) {
  GraphBuilderOrt graph_builder(graph_info, std::move(context_properties),
                                std::move(constant_operands),
                                std::move(working_directory));

  RETURN_IF_ERROR(graph_builder.BuildModel());
  RETURN_IF_ERROR(graph_builder.SerializeModel());
  return std::move(graph_builder.result_);
}

GraphBuilderOrt::GraphBuilderOrt(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::FilePath working_directory)
    : graph_info_(graph_info),
      constant_operands_(std::move(constant_operands)),
      context_properties_(std::move(context_properties)),
      result_(std::make_unique<Result>(std::move(working_directory))) {}

GraphBuilderOrt::~GraphBuilderOrt() = default;

const mojom::Operand& GraphBuilderOrt::GetOperand(uint64_t operand_id) {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

std::string GraphBuilderOrt::GetOperandName(uint64_t operand_id) {
  const mojom::Operand& operand = GetOperand(operand_id);
  switch (operand.kind) {
    case mojom::Operand::Kind::kInput: {
      CHECK(operand.name.has_value());
      // Add a prefix to avoid possible name collision.
      return base::JoinString({"input", operand.name.value()}, "_");
    }
    case mojom::Operand::Kind::kConstant: {
      // It's okay to use operand id as name directly since operand id is guaranteed to be unique.
      return base::NumberToString(operand_id);
    }
    case mojom::Operand::Kind::kOutput: {
      if (operand.name.has_value()) {
        return base::JoinString({"output", operand.name.value()}, "_");
      } else {
        return base::NumberToString(operand_id);
      }
    }
  }
}

void GraphBuilderOrt::AddInput(uint64_t input_id) {
  const mojom::Operand& operand = GetOperand(input_id);
  std::string name = GetOperandName(input_id);

  OperandInfo operand_info{
      name, operand.descriptor.shape(),
      OperandTypeToOnnxDataType(operand.descriptor.data_type())};

  onnx::ValueInfoProto& input = *(model_.mutable_graph()->add_input());
  input.set_name(name);

  onnx::TypeProto_Tensor& tensor_type =
      *(input.mutable_type()->mutable_tensor_type());
  tensor_type.set_elem_type(static_cast<int32_t>(operand_info.onnx_data_type));
  onnx::TensorShapeProto& shape = *tensor_type.mutable_shape();
  for (uint32_t dim : operand_info.shape) {
    shape.add_dim()->set_dim_value(static_cast<int64_t>(dim));
  }

  CHECK(result_->operand_infos.try_emplace(input_id, std::move(operand_info)).second);
}

void GraphBuilderOrt::AddOutput(uint64_t output_id) {
  const mojom::Operand& operand = GetOperand(output_id);
  std::string name = GetOperandName(output_id);

  OperandInfo operand_info{
      name, operand.descriptor.shape(),
      OperandTypeToOnnxDataType(operand.descriptor.data_type())};

  onnx::ValueInfoProto& output = *(model_.mutable_graph()->add_output());
  output.set_name(name);

  onnx::TypeProto_Tensor& tensor_type =
      *(output.mutable_type()->mutable_tensor_type());
  tensor_type.set_elem_type(static_cast<int32_t>(operand_info.onnx_data_type));
  onnx::TensorShapeProto& shape = *tensor_type.mutable_shape();
  for (uint32_t dim : operand_info.shape) {
    shape.add_dim()->set_dim_value(static_cast<int64_t>(dim));
  }

  CHECK(result_->operand_infos.try_emplace(output_id, std::move(operand_info)).second);
}

void GraphBuilderOrt::AddInitializer(uint64_t constant_id) {
  const WebNNConstantOperand& operand = *constant_operands_.at(constant_id);
  std::string name = GetOperandName(constant_id);

  OperandInfo operand_info{
      name, operand.descriptor().shape(),
      OperandTypeToOnnxDataType(operand.descriptor().data_type())};

  onnx::TensorProto& initializer = *(model_.mutable_graph()->add_initializer());
  initializer.set_name(name);
  initializer.set_data_type(operand_info.onnx_data_type);
  for (uint32_t dim : operand_info.shape) {
    initializer.add_dims(static_cast<int64_t>(dim));
  }

  initializer.mutable_raw_data()->assign(
      reinterpret_cast<const char*>(operand.ByteSpan().data()),
      operand.ByteSpan().size());

  CHECK(
      result_->operand_infos.try_emplace(constant_id, std::move(operand_info)).second);
}

template<typename T>
void GraphBuilderOrt::AddUnaryOperation(const T& operation, std::string op_type) {
  onnx::NodeProto& node = *(model_.mutable_graph()->add_node());
  node.set_name(operation.label);
  node.set_op_type(op_type);
  node.add_input(GetOperandName(operation.input_operand_id));
  node.add_output(GetOperandName(operation.output_operand_id));


  // CHECK(result_->operand_infos.try_emplace(operation.output_operand_id, std::move(operand_info)).second);
}

void GraphBuilderOrt::AddElementWiseUnaryOperation(const mojom::ElementWiseUnary& element_wise_unary) {
  switch(element_wise_unary.kind) {
    case mojom::ElementWiseUnary::Kind::kAbs:
      AddUnaryOperation(element_wise_unary, kOpTypeAbs);
      break;
    case mojom::ElementWiseUnary::Kind::kCeil:
      AddUnaryOperation(element_wise_unary, kOpTypeCeil);
      break;
    case mojom::ElementWiseUnary::Kind::kCos:
      AddUnaryOperation(element_wise_unary, kOpTypeCos);
      break;
    case mojom::ElementWiseUnary::Kind::kExp:
      AddUnaryOperation(element_wise_unary, kOpTypeExp);
      break;
    case mojom::ElementWiseUnary::Kind::kFloor:
      AddUnaryOperation(element_wise_unary, kOpTypeFloor);
      break;
    case mojom::ElementWiseUnary::Kind::kLog:
      AddUnaryOperation(element_wise_unary, kOpTypeLog);
      break;
    case mojom::ElementWiseUnary::Kind::kNeg:
      AddUnaryOperation(element_wise_unary, kOpTypeNeg);
      break;
    case mojom::ElementWiseUnary::Kind::kSign:
      AddUnaryOperation(element_wise_unary, kOpTypeSign);
      break;
    case mojom::ElementWiseUnary::Kind::kSin:
      AddUnaryOperation(element_wise_unary, kOpTypeSin);
      break;
    case mojom::ElementWiseUnary::Kind::kTan:
      AddUnaryOperation(element_wise_unary, kOpTypeTan);
      break;
    case mojom::ElementWiseUnary::Kind::kLogicalNot:
      AddUnaryOperation(element_wise_unary, kOpTypeLogicalNot);
      break;
    case mojom::ElementWiseUnary::Kind::kIdentity:
      AddUnaryOperation(element_wise_unary, kOpTypeIdentity);
      break;
    case mojom::ElementWiseUnary::Kind::kSqrt:
      AddUnaryOperation(element_wise_unary, kOpTypeSqrt);
      break;
    case mojom::ElementWiseUnary::Kind::kErf:
      AddUnaryOperation(element_wise_unary, kOpTypeErf);
      break;
    case mojom::ElementWiseUnary::Kind::kReciprocal:
      AddUnaryOperation(element_wise_unary, kOpTypeReciprocal);
      break;
    case mojom::ElementWiseUnary::Kind::kCast:
      AddCastOperation(element_wise_unary);
      break;
  }
}

void GraphBuilderOrt::AddCastOperation(const mojom::ElementWiseUnary& cast) {
  onnx::NodeProto& node = *(model_.mutable_graph()->add_node());
  node.set_name(cast.label);
  node.set_op_type(kOpTypeCast);
  node.add_input(GetOperandName(cast.input_operand_id));
  node.add_output(GetOperandName(cast.output_operand_id));

  onnx::AttributeProto& attribute = *node.add_attribute();
  attribute.set_name("to");
  attribute.set_type(onnx::AttributeProto::AttributeType::AttributeProto_AttributeType_INT);

  const mojom::Operand& output_operand = GetOperand(cast.output_operand_id);
  const OperandDataType output_data_type = output_operand.descriptor.data_type();
  attribute.set_i(static_cast<int64_t>(OperandTypeToOnnxDataType(output_data_type)));
}

void GraphBuilderOrt::AddLogicalNotOperation(const mojom::ElementWiseUnary& logical_not) {
  onnx::NodeProto& node = *(model_.mutable_graph()->add_node());
  // Add cast


  node.set_name(logical_not.label);
  node.set_op_type(kOpTypeLogicalNot);
  node.add_input(GetOperandName(logical_not.input_operand_id));
  node.add_output(GetOperandName(logical_not.output_operand_id));

  // Add cast
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::BuildModel() {
  // Based on comment in onnx.proto
  // IR VERSION 10 published on March 25, 2024
  // Added UINT4, INT4.
  // use the newest ir version to support uint4/int4.
  model_.set_ir_version(onnx::Version::IR_VERSION_2024_3_25);

  //
  onnx::OperatorSetIdProto& operator_set_id = *model_.add_opset_import();
  operator_set_id.set_domain("");
  operator_set_id.set_version(14);

  // Add inputs.
  for (uint64_t input_id : graph_info_->input_operands) {
    AddInput(input_id);
  }

  // Add initializers.
  for (const auto& [constant_id, _] : constant_operands_) {
    AddInitializer(constant_id);
  }

  // Add operations.
  for (const mojom::OperationPtr& operation : graph_info_->operations) {
    switch (operation->which()) {
      case mojom::Operation::Tag::kElementWiseUnary: {
        AddElementWiseUnaryOperation(*operation->get_element_wise_unary());
        break;
      }
      case mojom::Operation::Tag::kRelu: {
        AddUnaryOperation(*operation->get_relu(), kOpTypeRelu);
        break;
      }
      case mojom::Operation::Tag::kArgMinMax:
      case mojom::Operation::Tag::kBatchNormalization:
      case mojom::Operation::Tag::kClamp:
      case mojom::Operation::Tag::kConcat:
      case mojom::Operation::Tag::kConv2d:
      case mojom::Operation::Tag::kCumulativeSum:
      case mojom::Operation::Tag::kDequantizeLinear:
      case mojom::Operation::Tag::kElementWiseBinary:
      case mojom::Operation::Tag::kElu:
      case mojom::Operation::Tag::kExpand:
      case mojom::Operation::Tag::kGather:
      case mojom::Operation::Tag::kGatherElements:
      case mojom::Operation::Tag::kGatherNd:
      case mojom::Operation::Tag::kGelu:
      case mojom::Operation::Tag::kGemm:
      case mojom::Operation::Tag::kGru:
      case mojom::Operation::Tag::kGruCell:
      case mojom::Operation::Tag::kHardSigmoid:
      case mojom::Operation::Tag::kHardSwish:
      case mojom::Operation::Tag::kLayerNormalization:
      case mojom::Operation::Tag::kInstanceNormalization:
      case mojom::Operation::Tag::kLeakyRelu:
      case mojom::Operation::Tag::kLinear:
      case mojom::Operation::Tag::kLstm:
      case mojom::Operation::Tag::kLstmCell:
      case mojom::Operation::Tag::kMatmul:
      case mojom::Operation::Tag::kPad:
      case mojom::Operation::Tag::kPool2d:
      case mojom::Operation::Tag::kPrelu:
      case mojom::Operation::Tag::kQuantizeLinear:
      case mojom::Operation::Tag::kReduce:
      case mojom::Operation::Tag::kResample2d:
      case mojom::Operation::Tag::kReshape:
      case mojom::Operation::Tag::kReverse:
      case mojom::Operation::Tag::kScatterElements:
      case mojom::Operation::Tag::kScatterNd:
      case mojom::Operation::Tag::kSigmoid:
      case mojom::Operation::Tag::kSlice:
      case mojom::Operation::Tag::kSoftmax:
      case mojom::Operation::Tag::kSoftplus:
      case mojom::Operation::Tag::kSoftsign:
      case mojom::Operation::Tag::kSplit:
      case mojom::Operation::Tag::kTanh:
      case mojom::Operation::Tag::kTile:
      case mojom::Operation::Tag::kTranspose:
      case mojom::Operation::Tag::kTriangular:
      case mojom::Operation::Tag::kWhere:
        return NewNotSupportedError("op is not supported.");
    }
  }

  // Add outputs.
  for (uint64_t output_id : graph_info_->output_operands) {
    AddOutput(output_id);
  }

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderOrt::SerializeModel() {
  base::FilePath model_file_path =
      result_->working_directory.Append(kOnnxModelFileName);
  base::File model_file(model_file_path,
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  LOG(ERROR) << "model file = " << model_file_path;

  if (!model_file.IsValid()) {
    LOG(ERROR) << "[WebNN] Unable to open " << model_file_path << ": "
               << base::File::ErrorToString(model_file.error_details());
    return NewUnknownError("Model file is invalid.");
  }

  int fd = _open_osfhandle(
      reinterpret_cast<intptr_t>(model_file.GetPlatformFile()), _O_WRONLY);
  if (fd < 0) {
    LOG(ERROR) << "[WebNN] Failed to open handle from " << model_file_path;
    return NewUnknownError(kBuildGraphError);
  }

  bool result = model_.SerializeToFileDescriptor(fd);
  if (!result) {
    LOG(ERROR) << "[WebNN] Failed to serialize model to file descriptor.";
    return NewUnknownError(kBuildGraphError);
  }


  // result = model_.SerializeToFileDescriptor(
  //     reinterpret_cast<intptr_t>(model_file.GetPlatformFile()));

  return NewUnknownError(kBuildGraphError);
}

}  // namespace ort

}  // namespace webnn
