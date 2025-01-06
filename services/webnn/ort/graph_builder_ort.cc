// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_builder_ort.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "base/types/fixed_array.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/webnn_constant_operand.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn {

namespace {

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
constexpr char kOpTypeExpand[] = "Expand";
constexpr char kOpTypeGemm[] = "Gemm";
constexpr char kOpTypeInstanceNormalization[] = "InstanceNormalization";
constexpr char kOpTypeMatMul[] = "MatMul";

// Pooling operations
constexpr char kOpTypeAveragePool2d[] = "AveragePool";
constexpr char kOpTypeMaxPool2d[] = "MaxPool";
constexpr char kOpTypeLpPool2d[] = "LpPool";

// Reduction operations
constexpr char kOpTypeReduceL1[] = "ReduceL1";
constexpr char kOpTypeReduceL2[] = "ReduceL2";
constexpr char kOpTypeReduceLogSum[] = "ReduceLogSum";
constexpr char kOpTypeReduceLogSumExp[] = "ReduceLogSumExp";
constexpr char kOpTypeReduceMax[] = "ReduceMax";
constexpr char kOpTypeReduceMean[] = "ReduceMean";
constexpr char kOpTypeReduceMin[] = "ReduceMin";
constexpr char kOpTypeReduceProd[] = "ReduceProd";
constexpr char kOpTypeReduceSum[] = "ReduceSum";
constexpr char kOpTypeReduceSumSquare[] = "ReduceSumSquare";

constexpr char kOpTypeRelu[] = "Relu";
constexpr char kOpTypeReshape[] = "Reshape";
constexpr char kOpTypeSlice[] = "Slice";
constexpr char kOpTypeSoftmax[] = "Softmax";
constexpr char kOpTypeTranspose[] = "Transpose";
constexpr char kOpTypeWhere[] = "Where";

// constexpr char kBuildGraphError[] = "Failed to build graph.";

base::unexpected<mojom::ErrorPtr> NewNotSupportedError(std::string message) {
  return base::unexpected(mojom::Error::New(
      mojom::Error::Code::kNotSupportedError, std::move(message)));
}

base::unexpected<mojom::ErrorPtr> NewUnknownError(std::string message) {
  return base::unexpected(
      mojom::Error::New(mojom::Error::Code::kUnknownError, std::move(message)));
}

// TODO(https://github.com/shiyi9801/chromium/issues/63): Make name generation
// more robust. Inserted operands should also have a unique id, so here they're
// named by their ids for now.
std::string GetInsertedOperandName(uint64_t operand_id) {
  return base::NumberToString(operand_id);
}

// TODO(https://github.com/shiyi9801/chromium/issues/63): Make name generation
// more robust. Add extra index to label to make it unique since ONNX doesn't
// allow duplicate node names.
std::string GetNodeName(std::string_view label) {
  static int64_t index = 0;
  return base::JoinString({label, base::NumberToString(index++)}, "_");
}

std::string MapReduceKindToOrtOpType(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return kOpTypeReduceL1;
    case mojom::Reduce::Kind::kL2:
      return kOpTypeReduceL2;
    case mojom::Reduce::Kind::kLogSum:
      return kOpTypeReduceLogSum;
    case mojom::Reduce::Kind::kLogSumExp:
      return kOpTypeReduceLogSumExp;
    case mojom::Reduce::Kind::kMax:
      return kOpTypeReduceMax;
    case mojom::Reduce::Kind::kMean:
      return kOpTypeReduceMean;
    case mojom::Reduce::Kind::kMin:
      return kOpTypeReduceMin;
    case mojom::Reduce::Kind::kProduct:
      return kOpTypeReduceProd;
    case mojom::Reduce::Kind::kSum:
      return kOpTypeReduceSum;
    case mojom::Reduce::Kind::kSumSquare:
      return kOpTypeReduceSumSquare;
  }
}

std::vector<uint16_t> ConvertFloat32ToFloat16(base::span<const float> data) {
  std::vector<uint16_t> data_fp16(data.size());
  for (size_t i = 0; i < data.size(); ++i) {
    data_fp16[i] = fp16_ieee_from_fp32_value(data[i]);
  }
  return data_fp16;
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

const GraphBuilderOrt::OperandInfo& GraphBuilderOrt::Result::GetOperandInfo(
    uint64_t operand_id) const {
  auto it = id_to_operand_info.find(operand_id);
  CHECK(it != id_to_operand_info.end());
  return it->second;
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
    : graph_info_(graph_info),
      constant_operands_(std::move(constant_operands)),
      context_properties_(std::move(context_properties)),
      model_builder_(OrtModelBuilder(std::move(allocator))),
      result_(std::make_unique<Result>()) {
  for (const auto& [id, _] : graph_info.id_to_operand_map) {
    next_operand_id_ = std::max(next_operand_id_, id + 1);
  }
}

GraphBuilderOrt::~GraphBuilderOrt() = default;

const mojom::Operand& GraphBuilderOrt::GetOperand(uint64_t operand_id) {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

// TODO(https://github.com/shiyi9801/chromium/issues/63): Make name generation
// more robust.
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

std::string GraphBuilderOrt::CreateInitializerAsRawData(
    base::span<const uint32_t> shape,
    base::span<const uint8_t> data,
    OperandDataType data_type) {
  std::string name = GetInsertedOperandName(next_operand_id_);
  OperandInfo operand_info{name, data_type, shape};

  model_builder_.AddInitializerAsRawData(name, operand_info.int64_shape, data,
                                         operand_info.onnx_data_type);

  CHECK(result_->id_to_operand_info
            .try_emplace(next_operand_id_, std::move(operand_info))
            .second);
  next_operand_id_++;
  return name;
}

void GraphBuilderOrt::AddInput(uint64_t input_id) {
  const mojom::Operand& operand = GetOperand(input_id);
  std::string name = GetOperandName(input_id);

  OperandInfo operand_info{name, operand.descriptor.data_type(),
                           operand.descriptor.shape()};

  model_builder_.AddInput(name, operand_info.int64_shape,
                          operand_info.onnx_data_type);

  CHECK(
      result_->id_to_operand_info.try_emplace(input_id, std::move(operand_info))
          .second);
}

void GraphBuilderOrt::AddOutput(uint64_t output_id) {
  const mojom::Operand& operand = GetOperand(output_id);
  std::string name = GetOperandName(output_id);

  OperandInfo operand_info{name, operand.descriptor.data_type(),
                           operand.descriptor.shape()};

  model_builder_.AddOutput(name, operand_info.int64_shape,
                           operand_info.onnx_data_type);

  CHECK(result_->id_to_operand_info
            .try_emplace(output_id, std::move(operand_info))
            .second);
}

void GraphBuilderOrt::AddInitializerAsExternalData(uint64_t constant_id) {
  const WebNNConstantOperand& operand = *constant_operands_.at(constant_id);
  std::string name = GetOperandName(constant_id);

  OperandInfo operand_info{name, operand.descriptor().data_type(),
                           operand.descriptor().shape()};

  model_builder_.AddInitializerAsExternalData(name, operand_info.int64_shape,
                                              operand.ByteSpan(),
                                              operand_info.onnx_data_type);

  CHECK(result_->id_to_operand_info
            .try_emplace(constant_id, std::move(operand_info))
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

  model_builder_.AddNode(op_type, node_name, input_names, output_names);
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

  model_builder_.AddNode(op_type, node_name, input_names, output_names);
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

  int64_t to_data_type = static_cast<int64_t>(
      OperandTypeToONNXTensorElementDataType(output_data_type));
  ScopedOrtOpAttrPtr attr_to;
  model_builder_.CreateAttribute(attr_to, /*name=*/"to", to_data_type);

  std::array<OrtOpAttr*, 1> attributes = {attr_to};

  model_builder_.AddNode(kOpTypeCast, node_name, input_names, output_names,
                         attributes);
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
    // TODO(https://github.com/shiyi9801/chromium/issues/60): Add other data
    // types support. https://onnx.ai/onnx/operators/onnx__Clip.html
    default:
      NOTREACHED()
          << "[WebNN] Clamp only supports float32 and float16 data type.";
  }

  // Verified that we can also use external data here.
  // TODO(https://github.com/shiyi9801/chromium/issues/52): Determine whether to
  // use raw data or external data, which one is better?
  const std::string min_name = CreateInitializerAsRawData(
      /*shape=*/{}, min_value, input_data_type);
  const std::string max_name = CreateInitializerAsRawData(
      /*shape=*/{}, max_value, input_data_type);

  std::array<const char*, 3> input_names = {input_name.c_str(),
                                            min_name.c_str(), max_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_builder_.AddNode(kOpTypeClamp, node_name, input_names, output_names);
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

  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(conv2d.dilations->height),
      base::checked_cast<int64_t>(conv2d.dilations->width)};
  ScopedOrtOpAttrPtr attr_dilations;
  model_builder_.CreateAttribute(attr_dilations, /*name=*/"dilations",
                                 dilations);

  int64_t group = base::checked_cast<int64_t>(conv2d.groups);
  ScopedOrtOpAttrPtr attr_group;
  model_builder_.CreateAttribute(attr_group, /*name=*/"group", group);

  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(conv2d.padding->beginning->height),
      base::checked_cast<int64_t>(conv2d.padding->beginning->width),
      base::checked_cast<int64_t>(conv2d.padding->ending->height),
      base::checked_cast<int64_t>(conv2d.padding->ending->width)};
  ScopedOrtOpAttrPtr attr_pads;
  model_builder_.CreateAttribute(attr_pads, /*name=*/"pads", pads);

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(conv2d.strides->height),
      base::checked_cast<int64_t>(conv2d.strides->width)};
  ScopedOrtOpAttrPtr attr_strides;
  model_builder_.CreateAttribute(attr_strides, /*name=*/"strides", strides);

  std::array<OrtOpAttr*, 4> attributes = {
      attr_dilations,
      attr_group,
      attr_pads,
      attr_strides,
  };
  model_builder_.AddNode(kOpTypeConv2d, node_name, input_names, output_names,
                         attributes);
}

void GraphBuilderOrt::AddExpandOperation(const mojom::Expand& expand) {
  const std::string node_name = GetNodeName(expand.label);
  const std::string input_name = GetOperandName(expand.input_operand_id);
  const std::string output_name = GetOperandName(expand.output_operand_id);

  const OperandDescriptor& output_descriptor =
      GetOperand(expand.output_operand_id).descriptor;
  const std::vector<uint32_t>& output_shape = output_descriptor.shape();
  // Shape is an operand with data type int64, not an attribute.
  std::vector<uint32_t> shape_dims = {
      base::checked_cast<uint32_t>(output_shape.size())};
  std::vector<int64_t> shape_values;
  base::ranges::transform(
      output_shape, std::back_inserter(shape_values),
      [](uint32_t dim) { return static_cast<int64_t>(dim); });
  // Expand op needs parameter *shape* as raw data to do shape inference.
  const std::string shape_name = CreateInitializerAsRawData(
      shape_dims,
      base::span(reinterpret_cast<const uint8_t*>(shape_values.data()),
                 sizeof(int64_t) * shape_values.size()),
      OperandDataType::kInt64);

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            shape_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_builder_.AddNode(kOpTypeExpand, node_name, input_names, output_names);
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

  ScopedOrtOpAttrPtr attr_alpha;
  model_builder_.CreateAttribute(attr_alpha, /*name=*/"alpha", gemm.alpha);
  ScopedOrtOpAttrPtr attr_beta;
  model_builder_.CreateAttribute(attr_beta, /*name=*/"beta", gemm.beta);

  int64_t trans_a = static_cast<int64_t>(gemm.a_transpose);
  ScopedOrtOpAttrPtr attr_transA;
  model_builder_.CreateAttribute(attr_transA, /*name=*/"transA", trans_a);

  int64_t trans_b = static_cast<int64_t>(gemm.b_transpose);
  ScopedOrtOpAttrPtr attr_transB;
  model_builder_.CreateAttribute(attr_transB, /*name=*/"transB", trans_b);

  std::array<OrtOpAttr*, 4> attributes = {attr_alpha, attr_beta, attr_transA,
                                          attr_transB};

  model_builder_.AddNode(kOpTypeGemm, node_name, input_names, output_names,
                         attributes);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddInstanceNormalizationOperation(
    const mojom::InstanceNormalization& instance_normalization) {
  const OperandDataType input_data_type =
      GetOperand(instance_normalization.output_operand_id)
          .descriptor.data_type();

  const std::string input_name =
      GetOperandName(instance_normalization.input_operand_id);
  std::vector<const char*> input_names = {input_name.c_str()};

  const std::vector<uint32_t>& input_shape =
      GetOperand(instance_normalization.input_operand_id).descriptor.shape();
  // TODO(crbug.com/387312212): Support NHWC layout
  if (instance_normalization.layout ==
      mojom::InputOperandLayout::kChannelsLast) {
    return NewNotSupportedError(
        "[WebNN] Currently InstanceNormalization only supports NCHW layout.");
  }
  CHECK_EQ(context_properties_.input_operand_layout, InputOperandLayout::kNchw);
  uint32_t input_channel = input_shape[1];
  std::vector<uint32_t> constant_dims = {input_channel};

  std::string scale_name, bias_name;
  // ONNX requires scale and bias inputs. And they must be uploaded as raw data,
  // otherwise there will be runtime error.
  if (instance_normalization.scale_operand_id) {
    scale_name =
        GetOperandName(instance_normalization.scale_operand_id.value());
    input_names.push_back(scale_name.c_str());
  } else {
    std::vector<float> scale_data(input_channel, 1.0f);
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> scale_data_fp16 =
            ConvertFloat32ToFloat16(scale_data);
        scale_name = CreateInitializerAsRawData(
            constant_dims,
            base::span(reinterpret_cast<const uint8_t*>(scale_data_fp16.data()),
                       sizeof(uint16_t) * scale_data_fp16.size()),
            input_data_type);
        break;
      }
      case OperandDataType::kFloat32: {
        scale_name = CreateInitializerAsRawData(
            constant_dims,
            base::span(reinterpret_cast<const uint8_t*>(scale_data.data()),
                       sizeof(float) * scale_data.size()),
            input_data_type);
        break;
      }
      default:
        NOTREACHED() << "[WebNN] InstanceNormalization only supports float32 "
                        "and float16 data type.";
    }

    input_names.push_back(scale_name.c_str());
  }

  if (instance_normalization.bias_operand_id) {
    bias_name = GetOperandName(instance_normalization.bias_operand_id.value());
    input_names.push_back(bias_name.c_str());
  } else {
    std::vector<float> bias_data(input_channel, 0.0f);
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> bias_data_fp16 =
            ConvertFloat32ToFloat16(bias_data);
        bias_name = CreateInitializerAsRawData(
            constant_dims,
            base::span(reinterpret_cast<const uint8_t*>(bias_data_fp16.data()),
                       sizeof(uint16_t) * bias_data_fp16.size()),
            input_data_type);
        break;
      }
      case OperandDataType::kFloat32: {
        bias_name = CreateInitializerAsRawData(
            constant_dims,
            base::span(reinterpret_cast<const uint8_t*>(bias_data.data()),
                       sizeof(float) * bias_data.size()),
            input_data_type);
        break;
      }
      default:
        NOTREACHED() << "[WebNN] InstanceNormalization only supports float32 "
                        "and float16 data type.";
    }

    input_names.push_back(bias_name.c_str());
  }

  ScopedOrtOpAttrPtr attr_epsilon;
  model_builder_.CreateAttribute(attr_epsilon, /*name=*/"epsilon",
                                 instance_normalization.epsilon);
  std::array<OrtOpAttr*, 1> attributes = {attr_epsilon};

  const std::string node_name = GetNodeName(instance_normalization.label);
  const std::string output_name =
      GetOperandName(instance_normalization.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};
  model_builder_.AddNode(kOpTypeInstanceNormalization, node_name, input_names,
                         output_names, attributes);

  return base::ok();
}

void GraphBuilderOrt::AddLogicalNotOperation(
    const mojom::ElementWiseUnary& logical_not) {}

void GraphBuilderOrt::AddMatMulOperation(const mojom::Matmul& matmul) {
  const std::string node_name = GetNodeName(matmul.label);
  const std::string input_a_name = GetOperandName(matmul.a_operand_id);
  const std::string input_b_name = GetOperandName(matmul.b_operand_id);
  const std::string output_name = GetOperandName(matmul.output_operand_id);

  std::array<const char*, 2> input_names = {input_a_name.c_str(),
                                            input_b_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_builder_.AddNode(kOpTypeMatMul, node_name, input_names, output_names);
}

void GraphBuilderOrt::AddPool2dOperation(const mojom::Pool2d& pool2d) {
  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(pool2d.dilations->height),
      base::checked_cast<int64_t>(pool2d.dilations->width)};
  ScopedOrtOpAttrPtr attr_dilations;
  model_builder_.CreateAttribute(attr_dilations, /*name=*/"dilations",
                                 dilations);

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(pool2d.strides->height),
      base::checked_cast<int64_t>(pool2d.strides->width)};
  ScopedOrtOpAttrPtr attr_strides;
  model_builder_.CreateAttribute(attr_strides, /*name=*/"strides", strides);

  std::array<int64_t, 2> window_dimensions = {
      base::checked_cast<int64_t>(pool2d.window_dimensions->height),
      base::checked_cast<int64_t>(pool2d.window_dimensions->width)};
  ScopedOrtOpAttrPtr attr_kernel_shape;
  model_builder_.CreateAttribute(attr_kernel_shape,
                                 /*name=*/"kernel_shape", window_dimensions);

  // ONNX's pads are [beginning_height, beginning_width, ending_height,
  // ending_width]
  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(pool2d.padding->beginning->height),
      base::checked_cast<int64_t>(pool2d.padding->beginning->width),
      base::checked_cast<int64_t>(pool2d.padding->ending->height),
      base::checked_cast<int64_t>(pool2d.padding->ending->width)};
  ScopedOrtOpAttrPtr attr_pads;
  model_builder_.CreateAttribute(attr_pads, /*name=*/"pads", pads);

  // Calculate the ceil_mode.
  const std::vector<uint32_t>& input_shape =
      GetOperand(pool2d.input_operand_id).descriptor.shape();
  const std::vector<uint32_t>& output_shape =
      GetOperand(pool2d.output_operand_id).descriptor.shape();

  CHECK_EQ(context_properties_.input_operand_layout, InputOperandLayout::kNchw);
  uint32_t input_height = input_shape[2], output_height = output_shape[2];
  const auto float_output_height = CalculateConv2dOutputSize(
      input_height, pool2d.window_dimensions->height,
      pool2d.padding->beginning->height, pool2d.padding->ending->height,
      pool2d.strides->height, pool2d.dilations->height, pool2d.label);
  CHECK(float_output_height.has_value());

  int64_t ceil_mode = float_output_height.value() < output_height ? 1 : 0;
  ScopedOrtOpAttrPtr attr_ceil_mode;
  model_builder_.CreateAttribute(attr_ceil_mode, /*name=*/"ceil_mode",
                                 ceil_mode);

  // P value of the Lp norm used to pool over the input data.
  std::optional<ScopedOrtOpAttrPtr> attr_p;
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
      model_builder_.CreateAttribute(attr_p.value(), /*name=*/"p", p.value());
      break;
    }
  }

  std::vector<OrtOpAttr*> attributes = {attr_dilations, attr_strides,
                                        attr_kernel_shape, attr_pads,
                                        attr_ceil_mode};
  if (op_type == kOpTypeLpPool2d) {
    CHECK(attr_p.has_value());
    CHECK(p.has_value());
    attributes.push_back(attr_p.value());
  }

  const std::string node_name = GetNodeName(pool2d.label);
  const std::string input_name = GetOperandName(pool2d.input_operand_id);
  const std::string output_name = GetOperandName(pool2d.output_operand_id);
  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_builder_.AddNode(op_type, node_name, input_names, output_names,
                         attributes);
}

// TODO(https://github.com/shiyi9801/chromium/issues/53): 'reduceSumSquare
// float32 1D tensor with empty axes' test case fails
void GraphBuilderOrt::AddReduceOperation(const mojom::Reduce& reduce) {
  const std::string input_name = GetOperandName(reduce.input_operand_id);
  std::vector<const char*> input_names = {input_name.c_str()};

  std::vector<int64_t> axes(reduce.axes.begin(), reduce.axes.end());
  std::string axes_name;
  if (!axes.empty()) {
    // axes is an operand with data type int64, not an attribute.
    std::vector<uint32_t> axes_dims = {
        base::checked_cast<uint32_t>(axes.size())};
    axes_name = CreateInitializerAsRawData(
        axes_dims,
        base::span(reinterpret_cast<const uint8_t*>(axes.data()),
                   sizeof(int64_t) * axes.size()),
        OperandDataType::kInt64);
    input_names.push_back(axes_name.c_str());
  }

  ScopedOrtOpAttrPtr attr_keepdims;
  int64_t keepdims = reduce.keep_dimensions ? 1 : 0;
  model_builder_.CreateAttribute(attr_keepdims, /*name=*/"keepdims", keepdims);

  ScopedOrtOpAttrPtr attr_noop_with_empty_axes;
  // According to
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-reduce, if
  // axes is empty, the operation is a noop, no dimensions are reduced.
  int64_t noop_with_empty_axes = 1;
  model_builder_.CreateAttribute(attr_noop_with_empty_axes,
                                 /*name=*/"noop_with_empty_axes",
                                 noop_with_empty_axes);

  const std::string node_name = GetNodeName(reduce.label);
  const std::string output_name = GetOperandName(reduce.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};
  std::string reduce_op_type = MapReduceKindToOrtOpType(reduce.kind);
  std::array<OrtOpAttr*, 2> attributes = {attr_keepdims,
                                          attr_noop_with_empty_axes};
  model_builder_.AddNode(reduce_op_type, node_name, input_names, output_names,
                         attributes);
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
  const std::string shape_name = CreateInitializerAsRawData(
      shape_dims,
      base::span(reinterpret_cast<const uint8_t*>(shape_values.data()),
                 sizeof(int64_t) * shape_values.size()),
      OperandDataType::kInt64);

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            shape_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_builder_.AddNode(kOpTypeReshape, node_name, input_names, output_names);
}

void GraphBuilderOrt::AddSliceOperation(const mojom::Slice& slice) {
  const std::string node_name = GetNodeName(slice.label);
  const std::string input_name = GetOperandName(slice.input_operand_id);
  const std::string output_name = GetOperandName(slice.output_operand_id);

  auto range = slice.ranges;
  base::FixedArray<int64_t> beginnings(slice.ranges.size());
  base::FixedArray<int64_t> endings(slice.ranges.size());
  base::FixedArray<int64_t> strides(slice.ranges.size());
  for (size_t i = 0; i < slice.ranges.size(); ++i) {
    beginnings[i] = base::checked_cast<int64_t>(slice.ranges[i].start);
    endings[i] = base::checked_cast<int64_t>(slice.ranges[i].start +
                                             slice.ranges[i].size);
    strides[i] = base::checked_cast<int64_t>(slice.ranges[i].stride);
  }

  // Starts is an operand with data type int64, not an attribute.
  std::vector<uint32_t> starts_shape = {
      base::checked_cast<uint32_t>(beginnings.size())};
  // Slice op needs parameter *starts* as raw data to do shape inference.
  const std::string starts_name = CreateInitializerAsRawData(
      starts_shape,
      base::span(reinterpret_cast<const uint8_t*>(beginnings.data()),
                 sizeof(int64_t) * beginnings.size()),
      OperandDataType::kInt64);

  // Ends is an operand with data type int64, not an attribute.
  std::vector<uint32_t> ends_shape = {
      base::checked_cast<uint32_t>(endings.size())};
  // Slice op needs parameter *ends* as raw data to do shape inference.
  const std::string ends_name = CreateInitializerAsRawData(
      ends_shape,
      base::span(reinterpret_cast<const uint8_t*>(endings.data()),
                 sizeof(int64_t) * endings.size()),
      OperandDataType::kInt64);

  // Steps is an operand with data type int64, not an attribute.
  std::vector<uint32_t> steps_shape = {
      base::checked_cast<uint32_t>(strides.size())};
  // Slice op needs parameter *steps* as raw data to do shape inference.
  const std::string steps_name = CreateInitializerAsRawData(
      steps_shape,
      base::span(reinterpret_cast<const uint8_t*>(strides.data()),
                 sizeof(int64_t) * strides.size()),
      OperandDataType::kInt64);

  // Axes is an optional input, if not provided, it is an empty string and will
  // be treated as [0, 1, …, len(starts) - 1]:
  // https://onnx.ai/onnx/operators/onnx__Slice.html#inputs
  const std::string axes_name = "";
  std::array<const char*, 5> input_names = {
      input_name.c_str(), starts_name.c_str(), ends_name.c_str(),
      axes_name.c_str(), steps_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_builder_.AddNode(kOpTypeSlice, node_name, input_names, output_names);
}

void GraphBuilderOrt::AddSoftmaxOperation(const mojom::Softmax& softmax) {
  const std::string node_name = GetNodeName(softmax.label);
  const std::string input_name = GetOperandName(softmax.input_operand_id);
  const std::string output_name = GetOperandName(softmax.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  int64_t axis = static_cast<int64_t>(softmax.axis);
  ScopedOrtOpAttrPtr attr_axis;
  model_builder_.CreateAttribute(attr_axis, /*name=*/"axis", axis);

  std::array<OrtOpAttr*, 1> attributes = {attr_axis};
  model_builder_.AddNode(kOpTypeSoftmax, node_name, input_names, output_names,
                         attributes);
}

void GraphBuilderOrt::AddTransposeOperation(const mojom::Transpose& transpose) {
  const std::string node_name = GetNodeName(transpose.label);
  const std::string input_name = GetOperandName(transpose.input_operand_id);
  const std::string output_name = GetOperandName(transpose.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  std::vector<int64_t> permutation(transpose.permutation.begin(),
                                   transpose.permutation.end());
  ScopedOrtOpAttrPtr attr_perm;
  model_builder_.CreateAttribute(attr_perm, /*name=*/"perm", permutation);

  std::array<OrtOpAttr*, 1> attributes = {attr_perm};
  model_builder_.AddNode(kOpTypeTranspose, node_name, input_names, output_names,
                         attributes);
}

void GraphBuilderOrt::AddWhereOperation(const mojom::Where& where) {
  const std::string node_name = GetNodeName(where.label);
  // ONNX only supports bool data type for the condition input of Where, insert
  // a Cast node to convert the condition input to bool.
  std::string cast_node_output_name =
      "inserted_cast_node_output_" + GetInsertedOperandName(next_operand_id_);
  {
    std::string cast_node_name = "inserted_cast_node_before_" + node_name;
    const std::string condition_name =
        GetOperandName(where.condition_operand_id);
    std::array<const char*, 1> cast_input_names = {condition_name.c_str()};
    std::array<const char*, 1> cast_output_names = {
        cast_node_output_name.c_str()};

    ScopedOrtOpAttrPtr attr_to;
    int64_t to_data_type =
        static_cast<int64_t>(ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
    model_builder_.CreateAttribute(attr_to, /*name=*/"to", to_data_type);

    std::array<OrtOpAttr*, 1> cast_attributes = {attr_to};
    model_builder_.AddNode(kOpTypeCast, cast_node_name, cast_input_names,
                           cast_output_names, cast_attributes);
    next_operand_id_++;
  }

  const std::string true_value_name =
      GetOperandName(where.true_value_operand_id);
  const std::string false_value_name =
      GetOperandName(where.false_value_operand_id);
  const std::string output_name = GetOperandName(where.output_operand_id);
  std::array<const char*, 3> input_names = {cast_node_output_name.c_str(),
                                            true_value_name.c_str(),
                                            false_value_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_builder_.AddNode(kOpTypeWhere, node_name, input_names, output_names);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::BuildModel() {
  // Add inputs.
  for (uint64_t input_id : graph_info_->input_operands) {
    AddInput(input_id);
  }

  // Add initializers.
  for (const auto& [constant_id, _] : constant_operands_) {
    AddInitializerAsExternalData(constant_id);
  }

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
      case mojom::Operation::Tag::kExpand: {
        AddExpandOperation(*operation->get_expand());
        break;
      }
      case mojom::Operation::Tag::kGemm: {
        AddGemmOperation(*operation->get_gemm());
        break;
      }
      case mojom::Operation::Tag::kInstanceNormalization: {
        RETURN_IF_ERROR(AddInstanceNormalizationOperation(
            *operation->get_instance_normalization()));
        break;
      }
      case mojom::Operation::Tag::kMatmul: {
        AddMatMulOperation(*operation->get_matmul());
        break;
      }
      case mojom::Operation::Tag::kPool2d: {
        AddPool2dOperation(*operation->get_pool2d());
        break;
      }
      case mojom::Operation::Tag::kReduce: {
        AddReduceOperation(*operation->get_reduce());
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
      case mojom::Operation::Tag::kSlice: {
        AddSliceOperation(*operation->get_slice());
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
      case mojom::Operation::Tag::kWhere: {
        AddWhereOperation(*operation->get_where());
        break;
      }
      case mojom::Operation::Tag::kArgMinMax:
      case mojom::Operation::Tag::kBatchNormalization:
      case mojom::Operation::Tag::kConcat:
      case mojom::Operation::Tag::kCumulativeSum:
      case mojom::Operation::Tag::kDequantizeLinear:
      case mojom::Operation::Tag::kElu:
      case mojom::Operation::Tag::kGather:
      case mojom::Operation::Tag::kGatherElements:
      case mojom::Operation::Tag::kGatherNd:
      case mojom::Operation::Tag::kGelu:
      case mojom::Operation::Tag::kGru:
      case mojom::Operation::Tag::kGruCell:
      case mojom::Operation::Tag::kHardSigmoid:
      case mojom::Operation::Tag::kHardSwish:
      case mojom::Operation::Tag::kLayerNormalization:
      case mojom::Operation::Tag::kLeakyRelu:
      case mojom::Operation::Tag::kLinear:
      case mojom::Operation::Tag::kLstm:
      case mojom::Operation::Tag::kLstmCell:
      case mojom::Operation::Tag::kPad:
      case mojom::Operation::Tag::kPrelu:
      case mojom::Operation::Tag::kQuantizeLinear:
      case mojom::Operation::Tag::kResample2d:
      case mojom::Operation::Tag::kReverse:
      case mojom::Operation::Tag::kScatterElements:
      case mojom::Operation::Tag::kScatterNd:
      case mojom::Operation::Tag::kSigmoid:
      case mojom::Operation::Tag::kSoftplus:
      case mojom::Operation::Tag::kSoftsign:
      case mojom::Operation::Tag::kSplit:
      case mojom::Operation::Tag::kTanh:
      case mojom::Operation::Tag::kTile:
      case mojom::Operation::Tag::kTriangular:
        return NewNotSupportedError("op is not supported.");
    }
  }
  // Add outputs.
  for (uint64_t output_id : graph_info_->output_operands) {
    AddOutput(output_id);
  }

  result_->model_info = model_builder_.BuildAndTakeModelInfo();

  return base::ok();
}

}  // namespace ort

}  // namespace webnn
