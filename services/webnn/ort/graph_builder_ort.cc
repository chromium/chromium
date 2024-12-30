// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_builder_ort.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/webnn_constant_operand.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn {

namespace {

constexpr char kOrtDomainName[] = "";
constexpr int32_t kOrtOpsetVersion = 21;

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
constexpr char kOpTypeCast[] = "Cast";

constexpr char kOpTypeClamp[] = "Clip";
constexpr char kOpTypeConv2d[] = "Conv";
constexpr char kOpTypeGemm[] = "Gemm";

constexpr char kOpTypeMatmul[] = "Matmul";
constexpr char kOpTypeAveragePool2d[] = "AveragePool";
constexpr char kOpTypeMaxPool2d[] = "MaxPool";
constexpr char kOpTypeLpPool2d[] = "LpPool";

constexpr char kOpTypeRelu[] = "Relu";
constexpr char kOpTypeReshape[] = "Reshape";
constexpr char kOpTypeSoftmax[] = "Softmax";
constexpr char kOpTypeTranspose[] = "Transpose";

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
        constant_operands,
    scoped_refptr<AllocatorOrt> allocator) {
  GraphBuilderOrt graph_builder(graph_info, std::move(context_properties),
                                std::move(constant_operands), allocator);

  RETURN_IF_ERROR(graph_builder.BuildModel());
  return std::move(graph_builder.result_);
}

GraphBuilderOrt::GraphBuilderOrt(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    scoped_refptr<AllocatorOrt> allocator)
    : allocator_(allocator),
      graph_info_(graph_info),
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

uint64_t GraphBuilderOrt::NewInitializerAsRawData(
    base::span<const uint32_t> shape,
    base::span<const uint8_t> data,
    OperandDataType data_type) {
  std::string name = GetInsertedOperandName(next_operand_id_);
  OperandInfo operand_info{name, data_type, shape};

  ScopedOrtValue initializer;
  CHECK_STATUS(GetOrtApi()->CreateTensorAsOrtValue(
      allocator_->allocator(), operand_info.int64_shape.data(),
      operand_info.int64_shape.size(), operand_info.onnx_data_type,
      initializer.get_pptr()));

  void* ort_tensor_raw_data = nullptr;
  CHECK_STATUS(GetOrtApi()->GetTensorMutableData(initializer.get_ptr(),
                                                 &ort_tensor_raw_data));
  CHECK(ort_tensor_raw_data);
  UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(ort_tensor_raw_data), data.size()))
      .copy_from(data);
  CHECK_STATUS(GetOrtGraphApi()->AddInitializer(graph_.get_ptr(), name.c_str(),
                                                initializer.get_pptr()));

  CHECK(result_->operand_infos
            .try_emplace(next_operand_id_, std::move(operand_info))
            .second);
  return next_operand_id_++;
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

  // auto weight = base::HeapArray<uint8_t>::CopiedFrom(operand.ByteSpan());
  // result_->weights.push_back(std::move(weight));

  // ScopedOrtValue initializer;
  // CHECK_STATUS(GetOrtApi()->CreateTensorWithDataAsOrtValue(
  //     allocator_->memory_info(), result_->weights.back().data(),
  //     result_->weights.back().size(), operand_info.int64_shape.data(),
  //     operand_info.int64_shape.size(), operand_info.onnx_data_type,
  //     initializer.get_pptr()));
  // CHECK_STATUS(GetOrtGraphApi()->AddInitializer(graph_.get_ptr(),
  // name.c_str(),
  //                                               initializer.get_pptr()));
  ScopedOrtValue initializer;
  CHECK_STATUS(GetOrtApi()->CreateTensorAsOrtValue(
      allocator_->allocator(), operand_info.int64_shape.data(),
      operand_info.int64_shape.size(), operand_info.onnx_data_type,
      initializer.get_pptr()));

  void* ort_tensor_raw_data = nullptr;
  CHECK_STATUS(GetOrtApi()->GetTensorMutableData(initializer.get_ptr(),
                                                 &ort_tensor_raw_data));
  CHECK(ort_tensor_raw_data);
  UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(ort_tensor_raw_data),
                            operand.ByteSpan().size()))
      .copy_from(operand.ByteSpan());
  CHECK_STATUS(GetOrtGraphApi()->AddInitializer(graph_.get_ptr(), name.c_str(),
                                                initializer.get_pptr()));
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

void GraphBuilderOrt::AddCastOperation(const mojom::ElementWiseUnary& cast) {
  const std::string node_name = GetNodeName(cast.label);
  const std::string input_name = GetOperandName(cast.input_operand_id);
  const std::string output_name = GetOperandName(cast.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  const OperandDataType output_data_type =
      GetOperand(cast.output_operand_id).descriptor.data_type();

  ScopedOrtOpAttr attr_to;
  int64_t to_data_type = static_cast<int64_t>(
      OperandTypeToONNXTensorElementDataType(output_data_type));
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"to", &to_data_type, /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
      attr_to.get_pptr()));

  ScopedOrtNode node;
  std::array<OrtOpAttr**, 1> attributes = {attr_to.get_pptr()};
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      kOpTypeCast, kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(), node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

void GraphBuilderOrt::AddClampOperation(const mojom::Clamp& clamp) {
  const std::string node_name = GetNodeName(clamp.label);
  const std::string input_name = GetOperandName(clamp.input_operand_id);
  const std::string output_name = GetOperandName(clamp.output_operand_id);

  const OperandDataType input_data_type =
      GetOperand(clamp.output_operand_id).descriptor.data_type();

  // Min and max are 0-D operands with the same data type of input.

  base::HeapArray<uint8_t> min_value;
  base::HeapArray<uint8_t> max_value;
  switch (input_data_type) {
    case OperandDataType::kFloat32: {
      min_value = base::HeapArray<uint8_t>::CopiedFrom(base::span(
          reinterpret_cast<const uint8_t*>(&clamp.min_value), sizeof(float)));
      max_value = base::HeapArray<uint8_t>::CopiedFrom(base::span(
          reinterpret_cast<const uint8_t*>(&clamp.max_value), sizeof(float)));
      break;
    }
    case OperandDataType::kFloat16: {
      uint16_t fp16_min = fp16_ieee_from_fp32_value(clamp.min_value);
      uint16_t fp16_max = fp16_ieee_from_fp32_value(clamp.max_value);
      min_value = base::HeapArray<uint8_t>::CopiedFrom(base::span(
          reinterpret_cast<const uint8_t*>(&fp16_min), sizeof(uint16_t)));
      max_value = base::HeapArray<uint8_t>::CopiedFrom(base::span(
          reinterpret_cast<const uint8_t*>(&fp16_max), sizeof(uint16_t)));
      break;
    }
    // TODO: Add other data type support.
    // https://onnx.ai/onnx/operators/onnx__Clip.html
    default:
      NOTREACHED()
          << "[WebNN] Clamp only supports float32 and float16 data type.";
  }

  uint64_t min_id = NewInitializerAsRawData(
      /*shape=*/{}, min_value, input_data_type);
  const std::string min_name = GetInsertedOperandName(min_id);
  uint64_t max_id = NewInitializerAsRawData(
      /*shape=*/{}, max_value, input_data_type);
  const std::string max_name = GetInsertedOperandName(max_id);

  std::array<const char*, 3> input_names = {input_name.c_str(),
                                            min_name.c_str(), max_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtNode node;
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      kOpTypeClamp, kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      /*attributes=*/nullptr, /*attribs_len=*/0, node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

void GraphBuilderOrt::AddConv2dOperation(const mojom::Conv2d& conv2d) {
  const std::string node_name = GetNodeName(conv2d.label);
  const std::string input_name = GetOperandName(conv2d.input_operand_id);
  const std::string filter_name = GetOperandName(conv2d.filter_operand_id);
  const std::string output_name = GetOperandName(conv2d.output_operand_id);
  std::vector<const char*> input_names;
  std::string bias_name;
  if (conv2d.bias_operand_id) {
    bias_name = GetOperandName(conv2d.bias_operand_id.value());
    input_names = {input_name.c_str(), filter_name.c_str(), bias_name.c_str()};
  } else {
    input_names = {input_name.c_str(), filter_name.c_str()};
  }
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtOpAttr attr_dilations;
  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(conv2d.dilations->height),
      base::checked_cast<int64_t>(conv2d.dilations->width)};
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"dilations", dilations.data(), dilations.size(),
      OrtOpAttrType::ORT_OP_ATTR_INTS, attr_dilations.get_pptr()));

  ScopedOrtOpAttr attr_group;
  int64_t group = base::checked_cast<int64_t>(conv2d.groups);
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"group", &group, /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
      attr_group.get_pptr()));

  ScopedOrtOpAttr attr_pads;
  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(conv2d.padding->beginning->height),
      base::checked_cast<int64_t>(conv2d.padding->beginning->width),
      base::checked_cast<int64_t>(conv2d.padding->ending->height),
      base::checked_cast<int64_t>(conv2d.padding->ending->width)};
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"pads", pads.data(), pads.size(),
      OrtOpAttrType::ORT_OP_ATTR_INTS, attr_pads.get_pptr()));

  ScopedOrtOpAttr attr_strides;
  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(conv2d.strides->height),
      base::checked_cast<int64_t>(conv2d.strides->width)};
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"strides", strides.data(), strides.size(),
      OrtOpAttrType::ORT_OP_ATTR_INTS, attr_strides.get_pptr()));

  ScopedOrtNode node;
  std::array<OrtOpAttr**, 4> attributes = {
      attr_dilations.get_pptr(),
      attr_group.get_pptr(),
      attr_pads.get_pptr(),
      attr_strides.get_pptr(),
  };
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      kOpTypeConv2d, kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(), node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

void GraphBuilderOrt::AddGemmOperation(const mojom::Gemm& gemm) {
  const std::string node_name = GetNodeName(gemm.label);
  const std::string input_a_name = GetOperandName(gemm.a_operand_id);
  const std::string input_b_name = GetOperandName(gemm.b_operand_id);
  const std::string output_name = GetOperandName(gemm.output_operand_id);

  std::vector<const char*> input_names;
  std::string input_c_name;
  if (gemm.c_operand_id.has_value()) {
    input_c_name = GetOperandName(gemm.c_operand_id.value());
    input_names = {input_a_name.c_str(), input_b_name.c_str(),
                   input_c_name.c_str()};
  } else {
    input_names = {input_a_name.c_str(), input_b_name.c_str()};
  }
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtOpAttr attr_alpha;
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"alpha", &gemm.alpha, /*len=*/1,
      OrtOpAttrType::ORT_OP_ATTR_FLOAT, attr_alpha.get_pptr()));

  ScopedOrtOpAttr attr_beta;
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"beta", &gemm.beta, /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_FLOAT,
      attr_beta.get_pptr()));

  ScopedOrtOpAttr attr_transA;
  int64_t trans_a = static_cast<int64_t>(gemm.a_transpose);
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"transA", &trans_a, /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
      attr_transA.get_pptr()));

  ScopedOrtOpAttr attr_transB;
  int64_t trans_b = static_cast<int64_t>(gemm.b_transpose);
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"transB", &trans_b, /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
      attr_transB.get_pptr()));

  std::array<OrtOpAttr**, 4> attributes = {
      attr_alpha.get_pptr(), attr_beta.get_pptr(), attr_transA.get_pptr(),
      attr_transB.get_pptr()};

  ScopedOrtNode node;
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      kOpTypeGemm, kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(), node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()))
}

void GraphBuilderOrt::AddLogicalNotOperation(
    const mojom::ElementWiseUnary& logical_not) {}

void GraphBuilderOrt::AddMatmulOperation(const mojom::Matmul& matmul) {
  const std::string node_name = GetNodeName(matmul.label);
  const std::string input_a_name = GetOperandName(matmul.a_operand_id);
  const std::string input_b_name = GetOperandName(matmul.b_operand_id);
  const std::string output_name = GetOperandName(matmul.output_operand_id);

  std::array<const char*, 2> input_names = {input_a_name.c_str(),
                                            input_b_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtNode node;
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      kOpTypeMatmul, kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      /*attributes=*/nullptr, /*attribs_len=*/0, node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()))
}

void GraphBuilderOrt::AddReshapeOperation(const mojom::Reshape& reshape) {
  const std::string node_name = GetNodeName(reshape.label);
  const std::string input_name = GetOperandName(reshape.input_operand_id);
  const std::string output_name = GetOperandName(reshape.output_operand_id);

  const OperandDescriptor& output_descriptor =
      GetOperand(reshape.output_operand_id).descriptor;
  const std::vector<uint32_t>& output_shape = output_descriptor.shape();
  // Shape is an operand with data type int64, not an attribute.
  std::vector<uint32_t> shape_dims = {
      base::checked_cast<uint32_t>(output_shape.size())};
  std::vector<int64_t> shape_values;
  base::ranges::transform(
      output_shape, std::back_inserter(shape_values),
      [](uint32_t dim) { return static_cast<int64_t>(dim); });
  uint64_t shape_id = NewInitializerAsRawData(
      shape_dims,
      base::span(reinterpret_cast<const uint8_t*>(shape_values.data()),
                 sizeof(int64_t) * shape_values.size()),
      OperandDataType::kInt64);
  const std::string shape_name = GetInsertedOperandName(shape_id);

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            shape_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtNode node;
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      kOpTypeReshape, kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      /*attributes=*/nullptr, /*attribs_len=*/0, node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

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

void GraphBuilderOrt::AddTransposeOperation(const mojom::Transpose& transpose) {
  const std::string node_name = GetNodeName(transpose.label);
  const std::string input_name = GetOperandName(transpose.input_operand_id);
  const std::string output_name = GetOperandName(transpose.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtOpAttr attr_perm;
  std::vector<int64_t> permutation(transpose.permutation.begin(),
                                   transpose.permutation.end());
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"perm", permutation.data(), permutation.size(),
      OrtOpAttrType::ORT_OP_ATTR_INTS, attr_perm.get_pptr()));

  ScopedOrtNode node;
  std::array<OrtOpAttr**, 1> attributes = {attr_perm.get_pptr()};
  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      kOpTypeTranspose, kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(), node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

void GraphBuilderOrt::AddPool2dOperation(const mojom::Pool2d& pool2d) {
  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(pool2d.dilations->height),
      base::checked_cast<int64_t>(pool2d.dilations->width)};
  ScopedOrtOpAttr attr_dilations;
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"dilations", dilations.data(), /*len=*/2,
      OrtOpAttrType::ORT_OP_ATTR_INTS, attr_dilations.get_pptr()));

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(pool2d.strides->height),
      base::checked_cast<int64_t>(pool2d.strides->width)};
  ScopedOrtOpAttr attr_strides;
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"strides", strides.data(), /*len=*/2,
      OrtOpAttrType::ORT_OP_ATTR_INTS, attr_strides.get_pptr()));

  std::array<int64_t, 2> window_dimensions = {
      base::checked_cast<int64_t>(pool2d.window_dimensions->height),
      base::checked_cast<int64_t>(pool2d.window_dimensions->width)};
  ScopedOrtOpAttr attr_kernel_shape;
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"kernel_shape", window_dimensions.data(), /*len=*/2,
      OrtOpAttrType::ORT_OP_ATTR_INTS, attr_kernel_shape.get_pptr()));

  // ONNX's pads are [beginning_height, beginning_width, ending_height,
  // ending_width]
  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(pool2d.padding->beginning->height),
      base::checked_cast<int64_t>(pool2d.padding->beginning->width),
      base::checked_cast<int64_t>(pool2d.padding->ending->height),
      base::checked_cast<int64_t>(pool2d.padding->ending->width)};
  ScopedOrtOpAttr attr_pads;
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"pads", pads.data(), /*len=*/4, OrtOpAttrType::ORT_OP_ATTR_INTS,
      attr_pads.get_pptr()));

  // Calculate the ceil_mode.
  const std::vector<uint32_t>& input_shape =
      GetOperand(pool2d.input_operand_id).descriptor.shape();
  const std::vector<uint32_t>& output_shape =
      GetOperand(pool2d.output_operand_id).descriptor.shape();
  uint32_t input_height, output_height;
  switch (context_properties_.input_operand_layout) {
    case InputOperandLayout::kNhwc:
      input_height = input_shape[1];
      output_height = output_shape[1];
      break;
    case InputOperandLayout::kNchw:
      input_height = input_shape[2];
      output_height = output_shape[2];
      break;
  }
  const auto float_output_height = CalculateConv2dOutputSize(
      input_height, pool2d.window_dimensions->height,
      pool2d.padding->beginning->height, pool2d.padding->ending->height,
      pool2d.strides->height, pool2d.dilations->height, pool2d.label);
  CHECK(float_output_height.has_value());

  int64_t ceil_mode = float_output_height.value() < output_height ? 1 : 0;
  ScopedOrtOpAttr attr_ceil_mode;
  CHECK_STATUS(GetOrtApi()->CreateOpAttr(
      /*name=*/"ceil_mode", &ceil_mode, /*len=*/1,
      OrtOpAttrType::ORT_OP_ATTR_INT, attr_ceil_mode.get_pptr()));

  // P value of the Lp norm used to pool over the input data.
  std::optional<ScopedOrtOpAttr> attr_p;
  std::optional<int64_t> p;
  std::string op_type;
  switch (pool2d.kind) {
    case mojom::Pool2d::Kind::kAveragePool2d: {
      op_type = kOpTypeAveragePool2d;
      break;
    }
    case mojom::Pool2d::Kind::kMaxPool2d: {
      op_type = kOpTypeMaxPool2d;
      break;
    }
    case mojom::Pool2d::Kind::kL2Pool2d: {
      op_type = kOpTypeLpPool2d;
      p = 2;
      attr_p.emplace();
      CHECK_STATUS(GetOrtApi()->CreateOpAttr(
          /*name=*/"p", &p.value(), /*len=*/1, OrtOpAttrType::ORT_OP_ATTR_INT,
          attr_p.value().get_pptr()));
      break;
    }
  }

  ScopedOrtNode node;
  std::vector<OrtOpAttr**> attributes = {
      attr_dilations.get_pptr(), attr_strides.get_pptr(),
      attr_kernel_shape.get_pptr(), attr_pads.get_pptr(),
      attr_ceil_mode.get_pptr()};
  if (op_type == kOpTypeLpPool2d) {
    CHECK(attr_p.has_value());
    CHECK(p.has_value());
    attributes.push_back(attr_p.value().get_pptr());
  }

  const std::string node_name = GetNodeName(pool2d.label);
  const std::string input_name = GetOperandName(pool2d.input_operand_id);
  const std::string output_name = GetOperandName(pool2d.output_operand_id);
  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  CHECK_STATUS(GetOrtGraphApi()->CreateNode(
      op_type.data(), kOrtDomainName, node_name.c_str(), input_names.data(),
      input_names.size(), output_names.data(), output_names.size(),
      attributes.data(), attributes.size(), node.get_pptr()));
  CHECK_STATUS(GetOrtGraphApi()->AddNode(graph_.get_ptr(), node.get_pptr()));
}

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
      case mojom::Operation::Tag::kClamp: {
        AddClampOperation(*operation->get_clamp());
        break;
      }
      case mojom::Operation::Tag::kElementWiseBinary: {
        AddElementWiseBinaryOperation(*operation->get_element_wise_binary());
        break;
      }
      case mojom::Operation::Tag::kElementWiseUnary: {
        AddElementWiseUnaryOperation(*operation->get_element_wise_unary());
        break;
      }
      case mojom::Operation::Tag::kConv2d: {
        AddConv2dOperation(*operation->get_conv2d());
        break;
      }
      case mojom::Operation::Tag::kGemm: {
        AddGemmOperation(*operation->get_gemm());
        break;
      }
      case mojom::Operation::Tag::kMatmul: {
        AddMatmulOperation(*operation->get_matmul());
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
      case mojom::Operation::Tag::kTranspose: {
        AddTransposeOperation(*operation->get_transpose());
        break;
      }
      case mojom::Operation::Tag::kPool2d: {
        AddPool2dOperation(*operation->get_pool2d());
        break;
      }
      case mojom::Operation::Tag::kArgMinMax:
      case mojom::Operation::Tag::kBatchNormalization:
      case mojom::Operation::Tag::kConcat:
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
      case mojom::Operation::Tag::kPad:
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
