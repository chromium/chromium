// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_builder_ort.h"

#include <array>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn::ort {

namespace {

// Element-wise binary ops
constexpr base::cstring_view kOpTypeAdd = "Add";
constexpr base::cstring_view kOpTypeSub = "Sub";
constexpr base::cstring_view kOpTypeMul = "Mul";
constexpr base::cstring_view kOpTypeDiv = "Div";
constexpr base::cstring_view kOpTypeMax = "Max";
constexpr base::cstring_view kOpTypeMin = "Min";
constexpr base::cstring_view kOpTypePow = "Pow";

// Element-wise unary ops
constexpr base::cstring_view kOpTypeAbs = "Abs";
constexpr base::cstring_view kOpTypeCeil = "Ceil";
constexpr base::cstring_view kOpTypeCos = "Cos";
constexpr base::cstring_view kOpTypeExp = "Exp";
constexpr base::cstring_view kOpTypeFloor = "Floor";
constexpr base::cstring_view kOpTypeLog = "Log";
constexpr base::cstring_view kOpTypeNeg = "Neg";
constexpr base::cstring_view kOpTypeSign = "Sign";
constexpr base::cstring_view kOpTypeSin = "Sin";
constexpr base::cstring_view kOpTypeTan = "Tan";
constexpr base::cstring_view kOpTypeIdentity = "Identity";
constexpr base::cstring_view kOpTypeSqrt = "Sqrt";
constexpr base::cstring_view kOpTypeErf = "Erf";
constexpr base::cstring_view kOpTypeReciprocal = "Reciprocal";
constexpr base::cstring_view kOpTypeCast = "Cast";

constexpr base::cstring_view kOpTypeClamp = "Clip";
constexpr base::cstring_view kOpTypeGelu = "Gelu";
constexpr base::cstring_view kOpTypeGemm = "Gemm";
constexpr base::cstring_view kOpTypeHardSwish = "HardSwish";
constexpr base::cstring_view kOpTypeRelu = "Relu";
constexpr base::cstring_view kOpTypeSigmoid = "Sigmoid";
constexpr base::cstring_view kOpTypeSoftsign = "Softsign";
constexpr base::cstring_view kOpTypeTanh = "Tanh";

// Pooling operations
constexpr base::cstring_view kOpTypeAveragePool2d = "AveragePool";
constexpr base::cstring_view kOpTypeMaxPool2d = "MaxPool";
constexpr base::cstring_view kOpTypeLpPool2d = "LpPool";

constexpr std::string_view kInserted = "Inserted";
constexpr std::string_view kUnderscore = "_";

std::string GetOperandName(std::string_view label, OperandId id) {
  return base::JoinString({label, base::NumberToString(id.value())},
                          kUnderscore);
}

// Maps a DataType to a `ONNXTensorElementDataType`. Other `TensorTypeMap`
// overloads may be declared below as needed.
//
// Example: TensorTypeMap<uint32_t>::value ->
// ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32
template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
struct TensorTypeMap;

template <>
struct TensorTypeMap<float> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
};

// Use uint16_t to carry bits of float16.
template <>
struct TensorTypeMap<uint16_t> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
};

template <>
struct TensorTypeMap<int32_t> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
};

template <>
struct TensorTypeMap<uint32_t> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32;
};

template <>
struct TensorTypeMap<int64_t> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
};

template <>
struct TensorTypeMap<uint64_t> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64;
};

template <>
struct TensorTypeMap<int8_t> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8;
};

template <>
struct TensorTypeMap<uint8_t> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
};

}  // namespace

// static
[[nodiscard]] base::expected<std::unique_ptr<ModelEditor::ModelInfo>,
                             mojom::ErrorPtr>
GraphBuilderOrt::CreateAndBuild(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands) {
  GraphBuilderOrt graph_builder(graph_info, std::move(context_properties),
                                std::move(constant_operands));
  return graph_builder.BuildModel();
}

GraphBuilderOrt::GraphBuilderOrt(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands)
    : graph_info_(graph_info),
      constant_operands_(std::move(constant_operands)),
      context_properties_(std::move(context_properties)),
      model_editor_(ModelEditor()) {}

GraphBuilderOrt::~GraphBuilderOrt() = default;

const mojom::Operand& GraphBuilderOrt::GetOperand(OperandId operand_id) const {
  return *graph_info_->operands.at(operand_id.value());
}

std::string GraphBuilderOrt::GetOperandNameById(OperandId operand_id) const {
  const mojom::Operand& operand = GetOperand(operand_id);
  return GetOperandName(operand.name.has_value() ? *operand.name : "",
                        operand_id);
}

std::string GraphBuilderOrt::GenerateOperationName(std::string_view label) {
  return base::JoinString({label, base::NumberToString(next_operation_id_++)},
                          kUnderscore);
}

std::string GraphBuilderOrt::GenerateOperandName() {
  next_operand_id_++;
  CHECK(next_operand_id_.IsValid());
  return base::JoinString(
      {kInserted, base::NumberToString(
                      static_cast<uint32_t>(next_operand_id_.ValueOrDie()))},
      kUnderscore);
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
std::string GraphBuilderOrt::CreateInitializer(
    base::span<const int64_t> shape,
    base::span<const DataType> data) {
  std::string name = GenerateOperandName();
  base::span<const uint8_t> byte_span;
  if constexpr (std::floating_point<DataType>) {
    // Floating point types do not have unique object representations, but
    // this code appears to be using a byte span to type-erase, which is fine.
    byte_span = base::as_byte_span(base::allow_nonunique_obj, data);
  } else {
    byte_span = base::as_byte_span(data);
  }

  model_editor_.AddInitializer(name, TensorTypeMap<DataType>::value, shape,
                               byte_span);
  return name;
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
std::string GraphBuilderOrt::CreateScalarInitializer(const DataType& value) {
  return CreateInitializer<DataType>(
      /*shape=*/{}, base::span_from_ref(value));
}

template <typename T>
void GraphBuilderOrt::AddBinaryOperation(const T& operation,
                                         base::cstring_view op_type) {
  const std::string node = GenerateOperationName(operation.label);
  const std::string lhs = GetOperandNameById(operation.lhs_operand_id);
  const std::string rhs = GetOperandNameById(operation.rhs_operand_id);
  const std::string output = GetOperandNameById(operation.output_operand_id);

  std::array<const char*, 2> inputs = {lhs.c_str(), rhs.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node, inputs, outputs);
}

template <typename T>
void GraphBuilderOrt::AddUnaryOperation(const T& operation,
                                        base::cstring_view op_type) {
  const std::string node = GenerateOperationName(operation.label);
  const std::string input = GetOperandNameById(operation.input_operand_id);
  const std::string output = GetOperandNameById(operation.output_operand_id);

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node, inputs, outputs);
}

void GraphBuilderOrt::AddCastOperation(const mojom::ElementWiseUnary& cast) {
  const std::string node = GenerateOperationName(cast.label);
  const std::string input = GetOperandNameById(cast.input_operand_id);
  const std::string output = GetOperandNameById(cast.output_operand_id);

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  constexpr base::cstring_view kAttrTo = "to";
  const OperandDataType output_data_type =
      GetOperand(cast.output_operand_id).descriptor.data_type();
  int64_t attr_to_data =
      static_cast<int64_t>(WebnnToOnnxDataType(output_data_type));
  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrTo, attr_to_data)};

  model_editor_.AddNode(kOpTypeCast, node, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddElementWiseBinaryOperation(
    const mojom::ElementWiseBinary& element_wise_binary) {
  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDescriptor& lhs_descriptor =
      GetOperand(element_wise_binary.lhs_operand_id).descriptor;
  const OperandDescriptor& rhs_descriptor =
      GetOperand(element_wise_binary.rhs_operand_id).descriptor;
  switch (element_wise_binary.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      CHECK(data_type_limits.add_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddBinaryOperation(element_wise_binary, kOpTypeAdd);
    }
    case mojom::ElementWiseBinary::Kind::kSub: {
      CHECK(data_type_limits.sub_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddBinaryOperation(element_wise_binary, kOpTypeSub);
    }
    case mojom::ElementWiseBinary::Kind::kMul: {
      CHECK(data_type_limits.mul_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddBinaryOperation(element_wise_binary, kOpTypeMul);
    }
    case mojom::ElementWiseBinary::Kind::kDiv: {
      CHECK(data_type_limits.div_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddBinaryOperation(element_wise_binary, kOpTypeDiv);
    }
    case mojom::ElementWiseBinary::Kind::kMax: {
      CHECK(data_type_limits.max_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddBinaryOperation(element_wise_binary, kOpTypeMax);
    }
    case mojom::ElementWiseBinary::Kind::kMin: {
      CHECK(data_type_limits.min_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddBinaryOperation(element_wise_binary, kOpTypeMin);
    }
    case mojom::ElementWiseBinary::Kind::kPow: {
      CHECK(data_type_limits.pow_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddBinaryOperation(element_wise_binary, kOpTypePow);
    }
    case mojom::ElementWiseBinary::Kind::kEqual:
    case mojom::ElementWiseBinary::Kind::kNotEqual:
    case mojom::ElementWiseBinary::Kind::kGreater:
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
    case mojom::ElementWiseBinary::Kind::kLesser:
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
    case mojom::ElementWiseBinary::Kind::kLogicalXor:
      NOTREACHED() << "[WebNN] Element-wise logical operations are not "
                      "supported.";
  }
}

void GraphBuilderOrt::AddElementWiseUnaryOperation(
    const mojom::ElementWiseUnary& element_wise_unary) {
  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDescriptor& input_descriptor =
      GetOperand(element_wise_unary.input_operand_id).descriptor;
  switch (element_wise_unary.kind) {
    case mojom::ElementWiseUnary::Kind::kAbs: {
      CHECK(data_type_limits.abs_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeAbs);
    }
    case mojom::ElementWiseUnary::Kind::kCeil: {
      CHECK(data_type_limits.ceil_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeCeil);
    }
    case mojom::ElementWiseUnary::Kind::kCos: {
      CHECK(data_type_limits.cos_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeCos);
    }
    case mojom::ElementWiseUnary::Kind::kExp: {
      CHECK(data_type_limits.exp_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeExp);
    }
    case mojom::ElementWiseUnary::Kind::kFloor: {
      CHECK(data_type_limits.floor_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeFloor);
    }
    case mojom::ElementWiseUnary::Kind::kLog: {
      CHECK(data_type_limits.log_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeLog);
    }
    case mojom::ElementWiseUnary::Kind::kNeg: {
      CHECK(data_type_limits.neg_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeNeg);
    }
    case mojom::ElementWiseUnary::Kind::kSign: {
      CHECK(data_type_limits.sign_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeSign);
    }
    case mojom::ElementWiseUnary::Kind::kSin: {
      CHECK(data_type_limits.sin_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeSin);
    }
    case mojom::ElementWiseUnary::Kind::kTan: {
      CHECK(data_type_limits.tan_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeTan);
    }
    case mojom::ElementWiseUnary::Kind::kIdentity: {
      CHECK(data_type_limits.identity_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeIdentity);
    }
    case mojom::ElementWiseUnary::Kind::kSqrt: {
      CHECK(data_type_limits.sqrt_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeSqrt);
    }
    case mojom::ElementWiseUnary::Kind::kErf: {
      CHECK(data_type_limits.erf_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeErf);
    }
    case mojom::ElementWiseUnary::Kind::kReciprocal: {
      CHECK(data_type_limits.reciprocal_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeReciprocal);
    }
    case mojom::ElementWiseUnary::Kind::kCast: {
      CHECK(data_type_limits.cast_input.Supports(input_descriptor));
      return AddCastOperation(element_wise_unary);
    }
    case mojom::ElementWiseUnary::Kind::kLogicalNot:
      NOTREACHED()
          << "[WebNN] Element-wise logical operations are not supported.";
  }
}

void GraphBuilderOrt::AddClampOperation(const mojom::Clamp& clamp) {
  const std::string node = GenerateOperationName(clamp.label);
  const std::string input = GetOperandNameById(clamp.input_operand_id);
  const std::string output = GetOperandNameById(clamp.output_operand_id);

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDescriptor& input_descriptor =
      GetOperand(clamp.input_operand_id).descriptor;
  CHECK(data_type_limits.clamp_input.Supports(input_descriptor));

  const OperandDataType input_data_type = input_descriptor.data_type();

  // Min and max are 0-D operands with the same data type of input.
  std::string min;
  std::string max;
  switch (input_data_type) {
    case OperandDataType::kFloat32: {
      min = CreateScalarInitializer(clamp.min_value);
      max = CreateScalarInitializer(clamp.max_value);
      break;
    }
    case OperandDataType::kFloat16: {
      min = CreateScalarInitializer(fp16_ieee_from_fp32_value(clamp.min_value));
      max = CreateScalarInitializer(fp16_ieee_from_fp32_value(clamp.max_value));
      break;
    }
    case OperandDataType::kInt32: {
      min = CreateScalarInitializer(
          base::saturated_cast<int32_t>(clamp.min_value));
      max = CreateScalarInitializer(
          base::saturated_cast<int32_t>(clamp.max_value));
      break;
    }
    case OperandDataType::kUint32: {
      min = CreateScalarInitializer(
          base::saturated_cast<uint32_t>(clamp.min_value));
      max = CreateScalarInitializer(
          base::saturated_cast<uint32_t>(clamp.max_value));
      break;
    }
    case OperandDataType::kInt64: {
      min = CreateScalarInitializer(
          base::saturated_cast<int64_t>(clamp.min_value));
      max = CreateScalarInitializer(
          base::saturated_cast<int64_t>(clamp.max_value));
      break;
    }
    case OperandDataType::kUint64: {
      min = CreateScalarInitializer(
          base::saturated_cast<uint64_t>(clamp.min_value));
      max = CreateScalarInitializer(
          base::saturated_cast<uint64_t>(clamp.max_value));
      break;
    }
    case OperandDataType::kInt8: {
      min = CreateScalarInitializer(
          base::saturated_cast<int8_t>(clamp.min_value));
      max = CreateScalarInitializer(
          base::saturated_cast<int8_t>(clamp.max_value));
      break;
    }
    case OperandDataType::kUint8: {
      min = CreateScalarInitializer(
          base::saturated_cast<uint8_t>(clamp.min_value));
      max = CreateScalarInitializer(
          base::saturated_cast<uint8_t>(clamp.max_value));
      break;
    }
    default: {
      NOTREACHED() << "[WebNN] Clamp only supports data type float32, float16, "
                      "int32, uint32, int64, uint64, int8 and uint8.";
    }
  }

  std::array<const char*, 3> inputs = {input.c_str(), min.c_str(), max.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeClamp, node, inputs, outputs);
}

void GraphBuilderOrt::AddGemmOperation(const mojom::Gemm& gemm) {
  const std::string node = GenerateOperationName(gemm.label);
  const std::string input_a = GetOperandNameById(gemm.a_operand_id);
  const std::string input_b = GetOperandNameById(gemm.b_operand_id);
  const std::string output = GetOperandNameById(gemm.output_operand_id);

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDescriptor& input_a_descriptor =
      GetOperand(gemm.a_operand_id).descriptor;
  const OperandDescriptor& input_b_descriptor =
      GetOperand(gemm.b_operand_id).descriptor;
  CHECK(data_type_limits.gemm_a.SupportsAll(
      {input_a_descriptor, input_b_descriptor}));
  CHECK_EQ(input_a_descriptor.data_type(), input_b_descriptor.data_type());

  std::vector<const char*> inputs = {input_a.c_str(), input_b.c_str()};
  std::string input_c;
  if (gemm.c_operand_id.has_value()) {
    const OperandDescriptor& input_c_descriptor =
        GetOperand(*gemm.c_operand_id).descriptor;
    CHECK(data_type_limits.gemm_c.Supports(input_c_descriptor));
    CHECK_EQ(input_c_descriptor.data_type(), input_a_descriptor.data_type());

    input_c = GetOperandNameById(*gemm.c_operand_id);
    inputs.push_back(input_c.c_str());
  }
  std::array<const char*, 1> outputs = {output.c_str()};

  constexpr base::cstring_view kAttrAlpha = "alpha";
  constexpr base::cstring_view kAttrBeta = "beta";
  constexpr base::cstring_view kAttrTransA = "transA";
  constexpr base::cstring_view kAttrTransB = "transB";
  std::array<ScopedOrtOpAttr, 4> attributes = {
      model_editor_.CreateAttribute(kAttrAlpha, gemm.alpha),
      model_editor_.CreateAttribute(kAttrBeta, gemm.beta),
      model_editor_.CreateAttribute(kAttrTransA,
                                    static_cast<int64_t>(gemm.a_transpose)),
      model_editor_.CreateAttribute(kAttrTransB,
                                    static_cast<int64_t>(gemm.b_transpose))};

  model_editor_.AddNode(kOpTypeGemm, node, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddPool2dOperation(const mojom::Pool2d& pool2d) {
  std::vector<ScopedOrtOpAttr> attributes;
  constexpr base::cstring_view kAttrDilations = "dilations";
  constexpr base::cstring_view kAttrStrides = "strides";
  constexpr base::cstring_view kAttrKernelShape = "kernel_shape";
  constexpr base::cstring_view kAttrPads = "pads";
  constexpr base::cstring_view kAttrCeilMode = "ceil_mode";

  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(pool2d.dilations->height),
      base::checked_cast<int64_t>(pool2d.dilations->width)};
  attributes.push_back(
      model_editor_.CreateAttribute(kAttrDilations, dilations));

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(pool2d.strides->height),
      base::checked_cast<int64_t>(pool2d.strides->width)};
  attributes.push_back(model_editor_.CreateAttribute(kAttrStrides, strides));

  std::array<int64_t, 2> window_dimensions = {
      base::checked_cast<int64_t>(pool2d.window_dimensions->height),
      base::checked_cast<int64_t>(pool2d.window_dimensions->width)};
  attributes.push_back(
      model_editor_.CreateAttribute(kAttrKernelShape, window_dimensions));

  // ONNX's pads are [beginning_height, beginning_width, ending_height,
  // ending_width].
  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(pool2d.padding->beginning->height),
      base::checked_cast<int64_t>(pool2d.padding->beginning->width),
      base::checked_cast<int64_t>(pool2d.padding->ending->height),
      base::checked_cast<int64_t>(pool2d.padding->ending->width)};
  attributes.push_back(model_editor_.CreateAttribute(kAttrPads, pads));

  // Calculate the ceil_mode.
  const OperandDescriptor& input_descriptor =
      GetOperand(pool2d.input_operand_id).descriptor;
  const std::vector<uint32_t>& input_shape = input_descriptor.shape();
  const std::vector<uint32_t>& output_shape =
      GetOperand(pool2d.output_operand_id).descriptor.shape();

  CHECK_EQ(context_properties_.input_operand_layout, InputOperandLayout::kNchw);
  uint32_t input_height = input_shape[2];
  uint32_t output_height = output_shape[2];
  const auto float_output_height = CalculateConv2dOutputSize(
      input_height, pool2d.window_dimensions->height,
      pool2d.padding->beginning->height, pool2d.padding->ending->height,
      pool2d.strides->height, pool2d.dilations->height, pool2d.label);
  CHECK(float_output_height.has_value());

  int64_t ceil_mode = float_output_height.value() < output_height ? 1 : 0;
  attributes.push_back(model_editor_.CreateAttribute(kAttrCeilMode, ceil_mode));

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  base::cstring_view op_type;
  switch (pool2d.kind) {
    case mojom::Pool2d::Kind::kAveragePool2d: {
      CHECK(data_type_limits.average_pool2d_input.Supports(input_descriptor));
      op_type = kOpTypeAveragePool2d;
      break;
    }
    case mojom::Pool2d::Kind::kMaxPool2d: {
      CHECK(data_type_limits.max_pool2d_input.Supports(input_descriptor));
      op_type = kOpTypeMaxPool2d;
      break;
    }
    case mojom::Pool2d::Kind::kL2Pool2d: {
      CHECK(data_type_limits.l2_pool2d_input.Supports(input_descriptor));
      constexpr base::cstring_view kAttrP = "p";
      op_type = kOpTypeLpPool2d;
      attributes.push_back(
          model_editor_.CreateAttribute(kAttrP, static_cast<int64_t>(2)));
      break;
    }
  }

  const std::string node = GenerateOperationName(pool2d.label);
  const std::string input = GetOperandNameById(pool2d.input_operand_id);
  const std::string output = GetOperandNameById(pool2d.output_operand_id);
  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node, inputs, outputs, attributes);
}

[[nodiscard]] base::expected<std::unique_ptr<ModelEditor::ModelInfo>,
                             mojom::ErrorPtr>
GraphBuilderOrt::BuildModel() {
  for (OperandId input_id : graph_info_->input_operands) {
    model_editor_.AddInput(GetOperandNameById(input_id), GetOperand(input_id));
  }

  for (auto& [constant_id, constant_operand] : constant_operands_) {
    model_editor_.AddInitializer(GetOperandNameById(constant_id),
                                 std::move(constant_operand));
  }
  constant_operands_.clear();

  for (const mojom::OperationPtr& operation : graph_info_->operations) {
    const DataTypeLimits& data_type_limits =
        context_properties_.data_type_limits;
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
      case mojom::Operation::Tag::kGelu: {
        CHECK(data_type_limits.gelu_input.Supports(
            GetOperand(operation->get_gelu()->input_operand_id).descriptor));
        AddUnaryOperation(*operation->get_gelu(), kOpTypeGelu);
        break;
      }
      case mojom::Operation::Tag::kGemm: {
        AddGemmOperation(*operation->get_gemm());
        break;
      }
      case mojom::Operation::Tag::kHardSwish: {
        CHECK(data_type_limits.hard_swish_input.Supports(
            GetOperand(operation->get_hard_swish()->input_operand_id)
                .descriptor));
        AddUnaryOperation(*operation->get_hard_swish(), kOpTypeHardSwish);
        break;
      }
      case mojom::Operation::Tag::kPool2d: {
        AddPool2dOperation(*operation->get_pool2d());
        break;
      }
      case mojom::Operation::Tag::kRelu: {
        CHECK(data_type_limits.relu_input.Supports(
            GetOperand(operation->get_relu()->input_operand_id).descriptor));
        AddUnaryOperation(*operation->get_relu(), kOpTypeRelu);
        break;
      }
      case mojom::Operation::Tag::kSigmoid: {
        CHECK(data_type_limits.sigmoid_input.Supports(
            GetOperand(operation->get_sigmoid()->input_operand_id).descriptor));
        AddUnaryOperation(*operation->get_sigmoid(), kOpTypeSigmoid);
        break;
      }
      case mojom::Operation::Tag::kSoftsign: {
        CHECK(data_type_limits.softsign_input.Supports(
            GetOperand(operation->get_softsign()->input_operand_id)
                .descriptor));
        AddUnaryOperation(*operation->get_softsign(), kOpTypeSoftsign);
        break;
      }
      case mojom::Operation::Tag::kTanh: {
        CHECK(data_type_limits.tanh_input.Supports(
            GetOperand(operation->get_tanh()->input_operand_id).descriptor));
        AddUnaryOperation(*operation->get_tanh(), kOpTypeTanh);
        break;
      }
      case mojom::Operation::Tag::kArgMinMax:
      case mojom::Operation::Tag::kBatchNormalization:
      case mojom::Operation::Tag::kConcat:
      case mojom::Operation::Tag::kConv2d:
      case mojom::Operation::Tag::kCumulativeSum:
      case mojom::Operation::Tag::kDequantizeLinear:
      case mojom::Operation::Tag::kElu:
      case mojom::Operation::Tag::kExpand:
      case mojom::Operation::Tag::kGather:
      case mojom::Operation::Tag::kGatherElements:
      case mojom::Operation::Tag::kGatherNd:
      case mojom::Operation::Tag::kGru:
      case mojom::Operation::Tag::kGruCell:
      case mojom::Operation::Tag::kHardSigmoid:
      case mojom::Operation::Tag::kInstanceNormalization:
      case mojom::Operation::Tag::kLayerNormalization:
      case mojom::Operation::Tag::kLinear:
      case mojom::Operation::Tag::kLstm:
      case mojom::Operation::Tag::kLstmCell:
      case mojom::Operation::Tag::kMatmul:
      case mojom::Operation::Tag::kLeakyRelu:
      case mojom::Operation::Tag::kPad:
      case mojom::Operation::Tag::kPrelu:
      case mojom::Operation::Tag::kQuantizeLinear:
      case mojom::Operation::Tag::kReduce:
      case mojom::Operation::Tag::kResample2d:
      case mojom::Operation::Tag::kReshape:
      case mojom::Operation::Tag::kReverse:
      case mojom::Operation::Tag::kScatterElements:
      case mojom::Operation::Tag::kScatterNd:
      case mojom::Operation::Tag::kSlice:
      case mojom::Operation::Tag::kSoftmax:
      case mojom::Operation::Tag::kSoftplus:
      case mojom::Operation::Tag::kSplit:
      case mojom::Operation::Tag::kTile:
      case mojom::Operation::Tag::kTranspose:
      case mojom::Operation::Tag::kTriangular:
      case mojom::Operation::Tag::kWhere:
        NOTREACHED() << "[WebNN] Unsupported operation.";
    }
  }

  for (OperandId output_id : graph_info_->output_operands) {
    model_editor_.AddOutput(GetOperandNameById(output_id),
                            GetOperand(output_id));
  }

  return model_editor_.BuildAndTakeModelInfo();
}

}  // namespace webnn::ort
