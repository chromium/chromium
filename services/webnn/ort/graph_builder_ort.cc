// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_builder_ort.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/webnn_constant_operand.h"

namespace webnn {

namespace {

constexpr char kOrtDomainName[] = "";
constexpr int32_t kOrtOpsetVersion = 18;

// Element-wise binary
constexpr char kOpTypeAdd[] = "Add";
constexpr char kOpTypeSub[] = "Sub";
constexpr char kOpTypeMul[] = "Mul";
constexpr char kOpTypeDiv[] = "Div";
constexpr char kOpTypeMax[] = "Max";
constexpr char kOpTypeMin[] = "Min";
constexpr char kOpTypePow[] = "Pow";
constexpr char kOpTypeEqual[] = "Equal";
constexpr char kOpTypeGreater[] = "Greater";
constexpr char kOpTypeGreaterOrEqual[] = "GreaterOrEqual";
constexpr char kOpTypeLesser[] = "Less";
constexpr char kOpTypeLesserOrEqual[] = "LessOrEqual";
constexpr char kOpTypeLogicalAnd[] = "And";
constexpr char kOpTypeLogicalOr[] = "Or";
constexpr char kOpTypeLogicalXor[] = "Xor";

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
// constexpr char kOpTypeCast[] = "Cast";

// constexpr char kOpTypeGemm[] = "Gemm";

// Pool2d
// constexpr char kOpTypeAveragePool2d[] = "AveragePool";

constexpr char kOpTypeRelu[] = "Relu";
// constexpr char kOpTypeReshape[] = "Reshape";
constexpr char kOpTypeSoftmax[] = "Softmax";

// constexpr char kBuildGraphError[] = "Failed to build graph.";

base::unexpected<mojom::ErrorPtr> NewNotSupportedError(std::string message) {
  return base::unexpected(mojom::Error::New(
      mojom::Error::Code::kNotSupportedError, std::move(message)));
}

base::unexpected<mojom::ErrorPtr> NewUnknownError(std::string message) {
  return base::unexpected(
      mojom::Error::New(mojom::Error::Code::kUnknownError, std::move(message)));
}

// TODO: Make name generation more robust.
// Inserted operands should also have a unique id, so here they're named by
// their ids for now.
std::string GetInsertedOperandName(uint64_t operand_id) {
  return base::NumberToString(operand_id);
}

// TODO: Make name generation more robust.
// Add extra index to label to make it unique since ONNX doesn't allow duplicate
// node names.
std::string GetNodeName(std::string_view label) {
  static int64_t index = 0;
  return base::JoinString({label, base::NumberToString(index++)}, "_");
}

}  // namespace

namespace ort {

GraphBuilderOrt::OperandInfo::OperandInfo(
    std::string name,
    OperandDataType data_type,
    base::span<const uint32_t> uint32_shape)
    : name(std::move(name)),
      onnx_data_type(OperandTypeToONNXTensorElementDataType(data_type)) {
  base::ranges::transform(
      uint32_shape.begin(), uint32_shape.end(), std::back_inserter(int64_shape),
      [](uint32_t dim) { return static_cast<int64_t>(dim); });
}

GraphBuilderOrt::OperandInfo::OperandInfo() = default;
GraphBuilderOrt::OperandInfo::~OperandInfo() = default;
GraphBuilderOrt::OperandInfo::OperandInfo(OperandInfo&) = default;
GraphBuilderOrt::OperandInfo::OperandInfo(OperandInfo&&) = default;

GraphBuilderOrt::Result::Result() = default;
GraphBuilderOrt::Result::~Result() = default;

const ScopedOrtModel& GraphBuilderOrt::Result::GetModel() {
  return model;
}

const GraphBuilderOrt::OperandInfo& GraphBuilderOrt::Result::GetOperandInfo(
    uint64_t operand_id) const {
  auto it = operand_infos.find(operand_id);
  CHECK(it != operand_infos.end());
  return it->second;
}

const std::map<uint64_t, GraphBuilderOrt::OperandInfo>&
GraphBuilderOrt::Result::id_to_operand_info_map() const {
  return operand_infos;
}

// static
base::expected<std::unique_ptr<GraphBuilderOrt::Result>, mojom::ErrorPtr>
GraphBuilderOrt::CreateAndBuild(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands) {
  GraphBuilderOrt graph_builder(graph_info, std::move(context_properties),
                                std::move(constant_operands));

  RETURN_IF_ERROR(graph_builder.BuildModel());
  return std::move(graph_builder.result_);
}

GraphBuilderOrt::GraphBuilderOrt(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands)
    : graph_info_(graph_info),
      constant_operands_(std::move(constant_operands)),
      context_properties_(std::move(context_properties)),
      result_(std::make_unique<Result>()) {
  for (const auto& [id, _] : graph_info.id_to_operand_map) {
    next_operand_id_ = std::max(next_operand_id_, id + 1);
  }
}

GraphBuilderOrt::~GraphBuilderOrt() = default;

const mojom::Operand& GraphBuilderOrt::GetOperand(uint64_t operand_id) {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

// TODO: Make name generation more robust.
std::string GraphBuilderOrt::GetOperandName(uint64_t operand_id) {
  const mojom::Operand& operand = GetOperand(operand_id);
  switch (operand.kind) {
    case mojom::Operand::Kind::kInput: {
      CHECK(operand.name.has_value());
      // Add a prefix to avoid possible name collision.
      return operand.name.value();
      // return base::JoinString({"input", operand.name.value()}, "_");
    }
    case mojom::Operand::Kind::kConstant: {
      // It's okay to use operand id as name directly since operand id is
      // guaranteed to be unique.
      return base::NumberToString(operand_id);
    }
    case mojom::Operand::Kind::kOutput: {
      if (operand.name.has_value()) {
        return operand.name.value();
        // return base::JoinString({"output", operand.name.value()}, "_");
      } else {
        return base::NumberToString(operand_id);
      }
    }
  }
}

void GraphBuilderOrt::AddInput(uint64_t input_id) {
  const mojom::Operand& operand = GetOperand(input_id);
  std::string name = GetOperandName(input_id);

  OperandInfo operand_info{name, operand.descriptor.data_type(),
                           operand.descriptor.shape()};
  ScopedOrtShape input_shape;
  CHECK_STATUS(GetOrtGraphApi()->CreateFixedShape(
      operand_info.int64_shape.data(), operand_info.int64_shape.size(),
      input_shape.get_pptr()));
  ScopedOrtValueInfo input_info;
  CHECK_STATUS(GetOrtGraphApi()->CreateTensorValueInfo(
      name.c_str(), operand_info.onnx_data_type, input_shape.get_pptr(),
      input_info.get_pptr()));
  CHECK_STATUS(
      GetOrtGraphApi()->AddInput(graph_.get_ptr(), input_info.get_pptr()));
  CHECK(result_->operand_infos.try_emplace(input_id, std::move(operand_info))
            .second);
}

void GraphBuilderOrt::AddOutput(uint64_t output_id) {
  const mojom::Operand& operand = GetOperand(output_id);
  std::string name = GetOperandName(output_id);

  OperandInfo operand_info{name, operand.descriptor.data_type(),
                           operand.descriptor.shape()};

  ScopedOrtShape output_shape;
  CHECK_STATUS(GetOrtGraphApi()->CreateFixedShape(
      operand_info.int64_shape.data(), operand_info.int64_shape.size(),
      output_shape.get_pptr()));

  ScopedOrtValueInfo output_info;
  CHECK_STATUS(GetOrtGraphApi()->CreateTensorValueInfo(
      name.c_str(), operand_info.onnx_data_type, output_shape.get_pptr(),
      output_info.get_pptr()));

  CHECK_STATUS(
      GetOrtGraphApi()->AddOutput(graph_.get_ptr(), output_info.get_pptr()));

  CHECK(result_->operand_infos.try_emplace(output_id, std::move(operand_info))
            .second);
}

void GraphBuilderOrt::AddInitializer(uint64_t constant_id) {
  const WebNNConstantOperand& operand = *constant_operands_.at(constant_id);
  std::string name = GetOperandName(constant_id);

  OperandInfo operand_info{name, operand.descriptor().data_type(),
                           operand.descriptor().shape()};

  ScopedOrtMemoryInfo memory_info;
  CHECK_STATUS(GetOrtApi()->CreateCpuMemoryInfo(
      OrtAllocatorType::OrtDeviceAllocator, OrtMemType::OrtMemTypeDefault,
      memory_info.get_pptr()));

  auto weight = base::HeapArray<uint8_t>::CopiedFrom(operand.ByteSpan());
  result_->weights.push_back(std::move(weight));

  ScopedOrtValue initializer;
  CHECK_STATUS(GetOrtApi()->CreateTensorWithDataAsOrtValue(
      memory_info.get_ptr(), result_->weights.back().data(),
      result_->weights.back().size(), operand_info.int64_shape.data(),
      operand_info.int64_shape.size(), operand_info.onnx_data_type,
      initializer.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddInitializer(graph_.get_ptr(), name.c_str(),
                                                initializer.get_pptr()));

  CHECK(result_->operand_infos.try_emplace(constant_id, std::move(operand_info))
            .second);
}

template <typename T>
void GraphBuilderOrt::AddBinaryOperation(const T& operation,
                                         std::string op_type) {
  const std::string node_name = GetNodeName(operation.label);
  const std::string lhs_name = GetOperandName(operation.lhs_operand_id);
  const std::string rhs_name = GetOperandName(operation.rhs_operand_id);
  const std::string output_name = GetOperandName(operation.output_operand_id);

  std::array<const char*, 2> input_names = {lhs_name.c_str(), rhs_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtNode node;
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      op_type.data(), kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      /*attributes=*/nullptr, /*attribs_len=*/0, node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

void GraphBuilderOrt::AddElementWiseBinaryOperation(
    const mojom::ElementWiseBinary& element_wise_binary) {
  switch (element_wise_binary.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      AddBinaryOperation(element_wise_binary, kOpTypeAdd);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kSub: {
      AddBinaryOperation(element_wise_binary, kOpTypeSub);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMul: {
      AddBinaryOperation(element_wise_binary, kOpTypeMul);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kDiv: {
      AddBinaryOperation(element_wise_binary, kOpTypeDiv);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMax: {
      AddBinaryOperation(element_wise_binary, kOpTypeMax);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMin: {
      AddBinaryOperation(element_wise_binary, kOpTypeMin);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kPow: {
      AddBinaryOperation(element_wise_binary, kOpTypePow);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kEqual: {
      AddBinaryOperation(element_wise_binary, kOpTypeEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      AddBinaryOperation(element_wise_binary, kOpTypeGreater);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual: {
      AddBinaryOperation(element_wise_binary, kOpTypeGreaterOrEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      AddBinaryOperation(element_wise_binary, kOpTypeLesser);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual: {
      AddBinaryOperation(element_wise_binary, kOpTypeLesserOrEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalAnd: {
      AddBinaryOperation(element_wise_binary, kOpTypeLogicalAnd);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalOr: {
      AddBinaryOperation(element_wise_binary, kOpTypeLogicalOr);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalXor: {
      AddBinaryOperation(element_wise_binary, kOpTypeLogicalXor);
      break;
    }
  }
}

template <typename T>
void GraphBuilderOrt::AddUnaryOperation(const T& operation,
                                        std::string_view op_type) {
  const std::string node_name = GetNodeName(operation.label);
  const std::string input_name = GetOperandName(operation.input_operand_id);
  const std::string output_name = GetOperandName(operation.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtNode node;
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      op_type.data(), kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      /*attributes=*/nullptr, /*attribs_len=*/0, node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

void GraphBuilderOrt::AddElementWiseUnaryOperation(
    const mojom::ElementWiseUnary& element_wise_unary) {
  switch (element_wise_unary.kind) {
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

void GraphBuilderOrt::AddCastOperation(const mojom::ElementWiseUnary& cast) {}

void GraphBuilderOrt::AddGemmOperation(const mojom::Gemm& gemm) {}

void GraphBuilderOrt::AddLogicalNotOperation(
    const mojom::ElementWiseUnary& logical_not) {}

void GraphBuilderOrt::AddReshapeOperation(const mojom::Reshape& reshape) {}

void GraphBuilderOrt::AddSoftmaxOperation(const mojom::Softmax& softmax) {
  const std::string node_name = GetNodeName(softmax.label);
  const std::string input_name = GetOperandName(softmax.input_operand_id);
  const std::string output_name = GetOperandName(softmax.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtOpAttr attr_axis;
  int64_t axis = static_cast<int64_t>(softmax.axis);
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"axis", &axis, /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
      attr_axis.get_pptr()));

  ScopedOrtNode node;
  std::array<OrtOpAttr**, 1> attributes = {attr_axis.get_pptr()};
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      kOpTypeSoftmax, kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(), node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

// TODO: Post to thread pool?
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::BuildModel() {
  ScopedOrtModel& model = result_->model;

  std::vector<const char*> domain_names = {kOrtDomainName};
  std::vector<int32_t> opset_versions = {kOrtOpsetVersion};
  CHECK_STATUS(
      GetOrtGraphApi()->CreateModel(domain_names.data(), opset_versions.data(),
                                    domain_names.size(), model.get_pptr()));

  CHECK_STATUS(GetOrtGraphApi()->CreateGraph(graph_.get_pptr()));

  // Add inputs.
  for (uint64_t input_id : graph_info_->input_operands) {
    AddInput(input_id);
  }

  // Add initializers.
  for (const auto& [constant_id, _] : constant_operands_) {
    AddInitializer(constant_id);
  }

  // TODO: Implement all operations.
  // Add operations.
  for (const mojom::OperationPtr& operation : graph_info_->operations) {
    switch (operation->which()) {
      case mojom::Operation::Tag::kElementWiseBinary: {
        AddElementWiseBinaryOperation(*operation->get_element_wise_binary());
        break;
      }
      case mojom::Operation::Tag::kElementWiseUnary: {
        AddElementWiseUnaryOperation(*operation->get_element_wise_unary());
        break;
      }
      case mojom::Operation::Tag::kGemm: {
        AddGemmOperation(*operation->get_gemm());
        break;
      }
      case mojom::Operation::Tag::kRelu: {
        AddUnaryOperation(*operation->get_relu(), kOpTypeRelu);
        break;
      }
      case mojom::Operation::Tag::kReshape: {
        AddReshapeOperation(*operation->get_reshape());
        break;
      }
      case mojom::Operation::Tag::kSoftmax: {
        AddSoftmaxOperation(*operation->get_softmax());
        break;
      }
      case mojom::Operation::Tag::kArgMinMax:
      case mojom::Operation::Tag::kBatchNormalization:
      case mojom::Operation::Tag::kClamp:
      case mojom::Operation::Tag::kConcat:
      case mojom::Operation::Tag::kConv2d:
      case mojom::Operation::Tag::kCumulativeSum:
      case mojom::Operation::Tag::kDequantizeLinear:
      case mojom::Operation::Tag::kElu:
      case mojom::Operation::Tag::kExpand:
      case mojom::Operation::Tag::kGather:
      case mojom::Operation::Tag::kGatherElements:
      case mojom::Operation::Tag::kGatherNd:
      case mojom::Operation::Tag::kGelu:
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
      case mojom::Operation::Tag::kReverse:
      case mojom::Operation::Tag::kScatterElements:
      case mojom::Operation::Tag::kScatterNd:
      case mojom::Operation::Tag::kSigmoid:
      case mojom::Operation::Tag::kSlice:
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

  CHECK_STATUS(GetOrtGraphApi()->AddGraph(model.get_ptr(), graph_.get_pptr()));

  return base::ok();
}

}  // namespace ort

}  // namespace webnn
