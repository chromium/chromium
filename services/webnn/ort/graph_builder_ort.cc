// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_builder_ort.h"

#include <array>
#include <numeric>
#include <ranges>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/fixed_array.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn::ort {

namespace {

// ArgMin/Max ops
constexpr base::cstring_view kOpTypeArgMin = "ArgMin";
constexpr base::cstring_view kOpTypeArgMax = "ArgMax";

// Element-wise binary ops
constexpr base::cstring_view kOpTypeAdd = "Add";
constexpr base::cstring_view kOpTypeSub = "Sub";
constexpr base::cstring_view kOpTypeMul = "Mul";
constexpr base::cstring_view kOpTypeDiv = "Div";
constexpr base::cstring_view kOpTypeMax = "Max";
constexpr base::cstring_view kOpTypeMin = "Min";
constexpr base::cstring_view kOpTypePow = "Pow";
constexpr base::cstring_view kOpTypeEqual = "Equal";
constexpr base::cstring_view kOpTypeGreater = "Greater";
constexpr base::cstring_view kOpTypeGreaterOrEqual = "GreaterOrEqual";
constexpr base::cstring_view kOpTypeLesser = "Less";
constexpr base::cstring_view kOpTypeLesserOrEqual = "LessOrEqual";
constexpr base::cstring_view kOpTypeLogicalAnd = "And";
constexpr base::cstring_view kOpTypeLogicalOr = "Or";
constexpr base::cstring_view kOpTypeLogicalXor = "Xor";

// Element-wise unary ops
constexpr base::cstring_view kOpTypeAbs = "Abs";
constexpr base::cstring_view kOpTypeCeil = "Ceil";
constexpr base::cstring_view kOpTypeCos = "Cos";
constexpr base::cstring_view kOpTypeExp = "Exp";
constexpr base::cstring_view kOpTypeFloor = "Floor";
constexpr base::cstring_view kOpTypeLog = "Log";
constexpr base::cstring_view kOpTypeLogicalNot = "Not";
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
constexpr base::cstring_view kOpTypeConcat = "Concat";
constexpr base::cstring_view kOpTypeConv2d = "Conv";
constexpr base::cstring_view kOpTypeConvTranspose2d = "ConvTranspose";
constexpr base::cstring_view kOpTypeExpand = "Expand";
constexpr base::cstring_view kOpTypeGather = "Gather";
constexpr base::cstring_view kOpTypeGatherElements = "GatherElements";
constexpr base::cstring_view kOpTypeGatherND = "GatherND";
constexpr base::cstring_view kOpTypeGelu = "Gelu";
constexpr base::cstring_view kOpTypeGemm = "Gemm";
constexpr base::cstring_view kOpTypeLeakyRelu = "LeakyRelu";
constexpr base::cstring_view kOpTypeHardSwish = "HardSwish";
constexpr base::cstring_view kOpTypePad = "Pad";
constexpr base::cstring_view kOpTypePRelu = "PRelu";
constexpr base::cstring_view kOpTypeRelu = "Relu";
constexpr base::cstring_view kOpTypeReshape = "Reshape";
constexpr base::cstring_view kOpTypeScatterElements = "ScatterElements";
constexpr base::cstring_view kOpTypeScatterND = "ScatterND";
constexpr base::cstring_view kOpTypeSigmoid = "Sigmoid";
constexpr base::cstring_view kOpTypeSlice = "Slice";
constexpr base::cstring_view kOpTypeSoftmax = "Softmax";
constexpr base::cstring_view kOpTypeSoftsign = "Softsign";
constexpr base::cstring_view kOpTypeSplit = "Split";
constexpr base::cstring_view kOpTypeTanh = "Tanh";
constexpr base::cstring_view kOpTypeTile = "Tile";
constexpr base::cstring_view kOpTypeTranspose = "Transpose";

// Pooling operations
constexpr base::cstring_view kOpTypeAveragePool2d = "AveragePool";
constexpr base::cstring_view kOpTypeMaxPool2d = "MaxPool";
constexpr base::cstring_view kOpTypeLpPool2d = "LpPool";

constexpr std::string_view kInserted = "Inserted";
constexpr std::string_view kToEmulate = "ToEmulate";
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

// Calculate the output_padding according to the ONNX ConvTranspose2d
// documentation:
// https://onnx.ai/onnx/operators/onnx__ConvTranspose.html#summary
int64_t CalculateOutputPaddingSize(int64_t input_size,
                                   int64_t filter_size,
                                   int64_t stride,
                                   int64_t dilation,
                                   int64_t pad_begin,
                                   int64_t pad_end,
                                   int64_t output_size) {
  const auto output_padding =
      base::MakeCheckedNum(output_size) - stride * (input_size - 1) -
      ((filter_size - 1) * dilation + 1) + pad_begin + pad_end;
  // `output_padding` is validated by
  // `ValidateAndCalculateConvTranspose2dOutputSizes()`. Because Conv2d mojo
  // struct doesn't include `output_padding`, for ORT backend, we need to
  // re-compute it by using other attributes.
  CHECK(output_padding.IsValid());
  return output_padding.ValueOrDie();
}

}  // namespace

// static
[[nodiscard]] base::expected<std::unique_ptr<ModelEditor::ModelInfo>,
                             mojom::ErrorPtr>
GraphBuilderOrt::CreateAndBuild(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    bool is_external_data_supported) {
  GraphBuilderOrt graph_builder(graph_info, std::move(context_properties),
                                std::move(constant_operands),
                                is_external_data_supported);
  return graph_builder.BuildModel();
}

GraphBuilderOrt::GraphBuilderOrt(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    bool is_external_data_supported)
    : graph_info_(graph_info),
      constant_operands_(std::move(constant_operands)),
      context_properties_(std::move(context_properties)),
      model_editor_(ModelEditor(is_external_data_supported)) {}

GraphBuilderOrt::~GraphBuilderOrt() = default;

const mojom::Operand& GraphBuilderOrt::GetOperand(OperandId operand_id) const {
  return *graph_info_->operands.at(operand_id.value());
}

std::string GraphBuilderOrt::GetOperandNameById(OperandId operand_id) const {
  const mojom::Operand& operand = GetOperand(operand_id);
  return GetOperandName(operand.name.has_value() ? *operand.name : "",
                        operand_id);
}

std::string GraphBuilderOrt::GenerateNodeName(std::string_view label) {
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

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
std::string GraphBuilderOrt::Create1DInitializer(
    base::span<const DataType> data) {
  std::array<int64_t, 1> shape = {base::checked_cast<int64_t>(data.size())};
  return CreateInitializer<DataType>(shape, data);
}

std::string GraphBuilderOrt::CreateInt64InitializerForUint32Array(
    base::span<const uint32_t> array) {
  std::array<int64_t, 1> array_dims = {
      base::checked_cast<int64_t>(array.size())};
  base::FixedArray<int64_t> array_value(array.begin(), array.end());
  return CreateInitializer<int64_t>(array_dims, array_value);
}

std::string GraphBuilderOrt::CreateScalarInitializerForFloat(
    OperandDataType data_type,
    float value) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return CreateScalarInitializer(value);
    case OperandDataType::kFloat16:
      return CreateScalarInitializer(fp16_ieee_from_fp32_value(value));
    case OperandDataType::kInt32:
      return CreateScalarInitializer(base::saturated_cast<int32_t>(value));
    case OperandDataType::kUint32:
      return CreateScalarInitializer(base::saturated_cast<uint32_t>(value));
    case OperandDataType::kInt64:
      return CreateScalarInitializer(base::saturated_cast<int64_t>(value));
    case OperandDataType::kUint64:
      return CreateScalarInitializer(base::saturated_cast<uint64_t>(value));
    case OperandDataType::kInt8:
      return CreateScalarInitializer(base::saturated_cast<int8_t>(value));
    case OperandDataType::kUint8:
      return CreateScalarInitializer(base::saturated_cast<uint8_t>(value));
    case OperandDataType::kInt4:
    case OperandDataType::kUint4: {
      NOTREACHED();
    }
  }
}

void GraphBuilderOrt::AddCastNode(base::cstring_view node_name,
                                  base::cstring_view input,
                                  base::cstring_view output,
                                  ONNXTensorElementDataType to_data_type) {
  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};
  constexpr base::cstring_view kAttrTo = "to";
  int64_t attr_to_data = static_cast<int64_t>(to_data_type);
  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrTo, attr_to_data)};

  model_editor_.AddNode(kOpTypeCast, node_name, inputs, outputs, attributes);
}

std::string GraphBuilderOrt::CreateCastNode(
    base::cstring_view input,
    ONNXTensorElementDataType to_data_type) {
  const std::string output = GenerateOperandName();
  InsertCastNode(input, output, to_data_type);
  return output;
}

void GraphBuilderOrt::InsertCastNode(base::cstring_view input,
                                     base::cstring_view output,
                                     ONNXTensorElementDataType to_data_type) {
  const std::string node_name =
      GenerateNodeName(base::JoinString({kInserted, kOpTypeCast}, kUnderscore));
  AddCastNode(node_name, input, output, to_data_type);
}

void GraphBuilderOrt::AddExpandNode(base::cstring_view node_name,
                                    base::cstring_view input,
                                    base::cstring_view output,
                                    base::span<const uint32_t> shape) {
  // `new_shape` should be the name of an int64 tensor that specifies the
  // output's shape.
  const std::string new_shape = CreateInt64InitializerForUint32Array(shape);

  std::array<const char*, 2> inputs = {input.c_str(), new_shape.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeExpand, node_name, inputs, outputs);
}

std::string GraphBuilderOrt::CreateExpandNode(
    base::cstring_view input,
    base::span<const uint32_t> shape) {
  const std::string node_name = GenerateNodeName(
      base::JoinString({kInserted, kOpTypeExpand}, kUnderscore));
  const std::string output = GenerateOperandName();

  AddExpandNode(node_name, input, output, shape);
  return output;
}

void GraphBuilderOrt::AddSliceNode(base::cstring_view node_name,
                                   base::cstring_view input,
                                   base::cstring_view output,
                                   base::span<const int64_t> axes_value,
                                   base::span<const int64_t> starts_value,
                                   base::span<const int64_t> ends_value,
                                   base::span<const int64_t> steps_value) {
  // ONNX `Slice` op's `axes`, `starts`， `ends` and `steps` are operands of
  // data type int64 rather than attributes.
  const std::string axes = Create1DInitializer<int64_t>(axes_value);
  const std::string starts = Create1DInitializer<int64_t>(starts_value);
  const std::string ends = Create1DInitializer<int64_t>(ends_value);
  const std::string steps = Create1DInitializer<int64_t>(steps_value);

  std::array<const char*, 5> inputs = {
      input.c_str(), starts.c_str(), ends.c_str(), axes.c_str(), steps.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeSlice, node_name, inputs, outputs);
}

std::string GraphBuilderOrt::ClampIndices(base::cstring_view indices,
                                          OperandDataType data_type,
                                          uint32_t dim_size) {
  const std::string node_name = GenerateNodeName(
      base::JoinString({kInserted, kOpTypeClamp}, kUnderscore));
  const std::string output = GenerateOperandName();

  // The dimension size must be greater than 0.
  CHECK_GT(dim_size, 0u);

  std::string min;
  std::string max;
  switch (data_type) {
    case OperandDataType::kInt32: {
      // A valid dimension must be in the range of int32.
      // https://www.w3.org/TR/webnn/#valid-dimension
      min = CreateScalarInitializer(-base::checked_cast<int32_t>(dim_size));
      max = CreateScalarInitializer(base::checked_cast<int32_t>(dim_size - 1));
      break;
    }
    case OperandDataType::kInt64: {
      min = CreateScalarInitializer(-static_cast<int64_t>(dim_size));
      max = CreateScalarInitializer(static_cast<int64_t>(dim_size - 1));
      break;
    }
    default:
      NOTREACHED() << "[WebNN] Indices can only be one of the int32 and int64 "
                      "data types.";
  }

  std::array<const char*, 3> inputs = {indices.data(), min.c_str(),
                                       max.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeClamp, node_name, inputs, outputs);
  return output;
}

std::string GraphBuilderOrt::ClampGatherNDIndices(
    base::cstring_view indices,
    base::span<const uint32_t> input_shape,
    base::span<const uint32_t> indices_shape) {
  CHECK_GT(input_shape.size(), 0u);
  CHECK_GT(indices_shape.size(), 0u);

  uint32_t indices_last_dim_size = indices_shape[indices_shape.size() - 1];
  std::array<int64_t, 1> min_max_shape = {
      static_cast<int64_t>(indices_last_dim_size)};

  base::FixedArray<int64_t> min_value(indices_last_dim_size);
  base::FixedArray<int64_t> max_value(indices_last_dim_size);
  for (uint32_t axis = 0; axis < indices_last_dim_size; ++axis) {
    min_value[axis] = -static_cast<int64_t>(input_shape[axis]);
    max_value[axis] = static_cast<int64_t>(input_shape[axis]) - 1;
  }

  // ONNX Clip can only have `min` and `max` as scalars, so here use Min and Max
  // to emulate a clamp operation.
  std::string min = CreateInitializer<int64_t>(min_max_shape, min_value);
  const std::string max_node_name =
      GenerateNodeName(base::JoinString({kInserted, kOpTypeMax}, kUnderscore));
  const std::string max_output = GenerateOperandName();
  std::array<const char*, 2> max_inputs = {indices.c_str(), min.c_str()};
  std::array<const char*, 1> max_outputs = {max_output.c_str()};
  model_editor_.AddNode(kOpTypeMax, max_node_name, max_inputs, max_outputs);

  std::string max = CreateInitializer<int64_t>(min_max_shape, max_value);
  const std::string min_node_name =
      GenerateNodeName(base::JoinString({kInserted, kOpTypeMin}, kUnderscore));
  const std::string output = GenerateOperandName();
  std::array<const char*, 2> min_inputs = {max_output.c_str(), max.c_str()};
  std::array<const char*, 1> min_outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeMin, min_node_name, min_inputs, min_outputs);

  return output;
}

template <typename T>
void GraphBuilderOrt::AddBinaryOperation(const T& operation,
                                         base::cstring_view op_type) {
  const std::string node_name = GenerateNodeName(operation.label);
  const std::string lhs = GetOperandNameById(operation.lhs_operand_id);
  const std::string rhs = GetOperandNameById(operation.rhs_operand_id);
  const std::string output = GetOperandNameById(operation.output_operand_id);

  std::array<const char*, 2> inputs = {lhs.c_str(), rhs.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node_name, inputs, outputs);
}

template <typename T>
void GraphBuilderOrt::AddUnaryOperation(const T& operation,
                                        base::cstring_view op_type) {
  const std::string node_name = GenerateNodeName(operation.label);
  const std::string input = GetOperandNameById(operation.input_operand_id);
  const std::string output = GetOperandNameById(operation.output_operand_id);

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddArgMinMaxOperation(
    const mojom::ArgMinMax& arg_min_max) {
  const std::string node_name = GenerateNodeName(arg_min_max.label);
  const std::string input = GetOperandNameById(arg_min_max.input_operand_id);
  const std::string output = GetOperandNameById(arg_min_max.output_operand_id);

  CHECK(context_properties_.data_type_limits.arg_min_max_input.Supports(
      GetOperand(arg_min_max.input_operand_id).descriptor));
  OperandDataType output_data_type =
      GetOperand(arg_min_max.output_operand_id).descriptor.data_type();
  CHECK(context_properties_.data_type_limits.arg_min_max_output.Has(
      output_data_type));

  constexpr base::cstring_view kAttrAxis = "axis";
  constexpr base::cstring_view kAttrKeepDims = "keepdims";
  std::array<ScopedOrtOpAttr, 2> attributes = {
      model_editor_.CreateAttribute(kAttrAxis,
                                    static_cast<int64_t>(arg_min_max.axis)),
      model_editor_.CreateAttribute(
          kAttrKeepDims, static_cast<int64_t>(arg_min_max.keep_dimensions))};

  // ONNX ArgMin/Max only supports int64 output.
  bool need_cast = output_data_type != OperandDataType::kInt64;
  const std::string int64_output = need_cast ? GenerateOperandName() : output;

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {int64_output.c_str()};

  model_editor_.AddNode(arg_min_max.kind == mojom::ArgMinMax::Kind::kMax
                            ? kOpTypeArgMax
                            : kOpTypeArgMin,
                        node_name, inputs, outputs, attributes);

  if (need_cast) {
    // Here cast ArgMin/Max output from int64 to int32 is safe since WebNN
    // operand dimension must be in the range of int32.
    // https://www.w3.org/TR/webnn/#valid-dimension
    CHECK_EQ(output_data_type, OperandDataType::kInt32);
    InsertCastNode(int64_output, output, WebnnToOnnxDataType(output_data_type));
  }
}

void GraphBuilderOrt::AddCastOperation(const mojom::ElementWiseUnary& cast) {
  const std::string node_name = GenerateNodeName(cast.label);
  const std::string input = GetOperandNameById(cast.input_operand_id);
  const std::string output = GetOperandNameById(cast.output_operand_id);
  const OperandDataType output_data_type =
      GetOperand(cast.output_operand_id).descriptor.data_type();
  AddCastNode(node_name, input, output, WebnnToOnnxDataType(output_data_type));
}

void GraphBuilderOrt::AddConv2dOperation(const mojom::Conv2d& conv2d) {
  const std::string node_name = GenerateNodeName(conv2d.label);
  const std::string input = GetOperandNameById(conv2d.input_operand_id);
  const std::string filter = GetOperandNameById(conv2d.filter_operand_id);
  const std::string output = GetOperandNameById(conv2d.output_operand_id);

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  CHECK(data_type_limits.conv2d_input.Supports(
      GetOperand(conv2d.input_operand_id).descriptor));
  CHECK(data_type_limits.conv2d_input.Supports(
      GetOperand(conv2d.filter_operand_id).descriptor));
  std::vector<const char*> inputs = {input.c_str(), filter.c_str()};
  std::string bias;
  if (conv2d.bias_operand_id.has_value()) {
    CHECK(data_type_limits.conv2d_bias.Supports(
        GetOperand(conv2d.bias_operand_id.value()).descriptor));
    bias = GetOperandNameById(conv2d.bias_operand_id.value());
    inputs.push_back(bias.c_str());
  }
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(5);
  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(conv2d.dilations->height),
      base::checked_cast<int64_t>(conv2d.dilations->width)};
  constexpr base::cstring_view kAttrDilations = "dilations";
  attributes.push_back(
      model_editor_.CreateAttribute(kAttrDilations, dilations));

  int64_t group = base::checked_cast<int64_t>(conv2d.groups);
  constexpr base::cstring_view kAttrGroup = "group";
  attributes.push_back(model_editor_.CreateAttribute(kAttrGroup, group));

  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(conv2d.padding->beginning->height),
      base::checked_cast<int64_t>(conv2d.padding->beginning->width),
      base::checked_cast<int64_t>(conv2d.padding->ending->height),
      base::checked_cast<int64_t>(conv2d.padding->ending->width)};
  constexpr base::cstring_view kAttrPads = "pads";
  attributes.push_back(model_editor_.CreateAttribute(kAttrPads, pads));

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(conv2d.strides->height),
      base::checked_cast<int64_t>(conv2d.strides->width)};
  constexpr base::cstring_view kAttrStrides = "strides";
  attributes.push_back(model_editor_.CreateAttribute(kAttrStrides, strides));

  switch (conv2d.kind) {
    case mojom::Conv2d::Kind::kDirect:
      model_editor_.AddNode(kOpTypeConv2d, node_name, inputs, outputs,
                            attributes);
      break;
    case mojom::Conv2d::Kind::kTransposed:
      // According to the ONNX ConvTranspose2d documentation, `output_padding`
      // is a zero vector if not specified and `pads` will be auto generated if
      // `output_shape` is specified. So we need to calculate the
      // `output_padding` and explicitly set it to ensure that the attributes
      // information is not missing. Since the `pads` attribute has already been
      // set, there is no need to set `output_size` attribute.
      // https://onnx.ai/onnx/operators/onnx__ConvTranspose.html#attributes
      const std::vector<uint32_t>& input_shape =
          GetOperand(conv2d.input_operand_id).descriptor.shape();
      const std::vector<uint32_t>& filter_shape =
          GetOperand(conv2d.filter_operand_id).descriptor.shape();
      const std::vector<uint32_t>& output_shape =
          GetOperand(conv2d.output_operand_id).descriptor.shape();
      // Since ONNX Runtime uses nchw input layout and oihw filter layout，
      // input/filter/output_shape[2] and input/filter/output_shape[3] are used
      // here to access the height and width dimensions of the
      // input/filter/output_shape tensor shape.
      std::array<int64_t, 2> input_size = {
          base::checked_cast<int64_t>(input_shape[2]),
          base::checked_cast<int64_t>(input_shape[3])};
      std::array<int64_t, 2> filter_size = {
          base::checked_cast<int64_t>(filter_shape[2]),
          base::checked_cast<int64_t>(filter_shape[3])};
      std::array<int64_t, 2> output_size = {
          base::checked_cast<int64_t>(output_shape[2]),
          base::checked_cast<int64_t>(output_shape[3])};

      int64_t output_padding_height = CalculateOutputPaddingSize(
          input_size[0], filter_size[0], strides[0], dilations[0], pads[0],
          pads[2], output_size[0]);
      int64_t output_padding_width = CalculateOutputPaddingSize(
          input_size[1], filter_size[1], strides[1], dilations[1], pads[1],
          pads[3], output_size[1]);
      std::array<int64_t, 2> output_padding = {output_padding_height,
                                               output_padding_width};

      constexpr base::cstring_view kAttrOutputPadding = "output_padding";
      attributes.push_back(
          model_editor_.CreateAttribute(kAttrOutputPadding, output_padding));

      model_editor_.AddNode(kOpTypeConvTranspose2d, node_name, inputs, outputs,
                            attributes);
      break;
  }
}

// TODO(crbug.com/426228071): Eliminate redundant cast ops for bool and uint8
// data types conversion.
void GraphBuilderOrt::AddLogicalBinaryOperation(
    const mojom::ElementWiseBinary& logical_binary,
    base::cstring_view op_type) {
  const std::string node_name = GenerateNodeName(logical_binary.label);
  std::string lhs = GetOperandNameById(logical_binary.lhs_operand_id);
  std::string rhs = GetOperandNameById(logical_binary.rhs_operand_id);

  // Some ONNX logical binary operations only support bool input.
  if (logical_binary.kind == mojom::ElementWiseBinary::Kind::kLogicalAnd ||
      logical_binary.kind == mojom::ElementWiseBinary::Kind::kLogicalOr ||
      logical_binary.kind == mojom::ElementWiseBinary::Kind::kLogicalXor) {
    CHECK_EQ(GetOperand(logical_binary.lhs_operand_id).descriptor.data_type(),
             OperandDataType::kUint8);
    lhs = CreateCastNode(lhs, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);

    CHECK_EQ(GetOperand(logical_binary.rhs_operand_id).descriptor.data_type(),
             OperandDataType::kUint8);
    rhs = CreateCastNode(rhs, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
  }
  std::array<const char*, 2> inputs = {lhs.c_str(), rhs.c_str()};

  const std::string bool_output = GenerateOperandName();
  std::array<const char*, 1> outputs = {bool_output.c_str()};
  model_editor_.AddNode(op_type, node_name, inputs, outputs);

  // ONNX logical operators only support bool output. WebNN logical operators
  // support uint8 output. It is necessary to insert a cast operator after a
  // logical operator.
  const OperandDataType output_data_type =
      GetOperand(logical_binary.output_operand_id).descriptor.data_type();
  const std::string output =
      GetOperandNameById(logical_binary.output_operand_id);
  CHECK_EQ(output_data_type, OperandDataType::kUint8);
  InsertCastNode(bool_output, output, WebnnToOnnxDataType(output_data_type));
}

void GraphBuilderOrt::AddLogicalNotOperation(
    const mojom::ElementWiseUnary& logical_not) {
  const std::string node_name = GenerateNodeName(logical_not.label);
  // ONNX logical not operation only supports bool input.
  CHECK_EQ(GetOperand(logical_not.input_operand_id).descriptor.data_type(),
           OperandDataType::kUint8);
  std::string input =
      CreateCastNode(GetOperandNameById(logical_not.input_operand_id),
                     ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
  std::vector<const char*> inputs = {input.c_str()};

  const std::string bool_output = GenerateOperandName();
  std::array<const char*, 1> outputs = {bool_output.c_str()};
  model_editor_.AddNode(kOpTypeLogicalNot, node_name, inputs, outputs);

  // ONNX `Not` operator only supports bool output, while WebNN `logicalNot`
  // operator supports uint8 output. Insert a `Cast` operator for type
  // conversion.
  const OperandDataType output_data_type =
      GetOperand(logical_not.output_operand_id).descriptor.data_type();
  const std::string output = GetOperandNameById(logical_not.output_operand_id);
  CHECK_EQ(output_data_type, OperandDataType::kUint8);
  InsertCastNode(bool_output, output, WebnnToOnnxDataType(output_data_type));
}

void GraphBuilderOrt::AddLogicalNotEqualOperation(
    const mojom::ElementWiseBinary& not_equal) {
  // Step 1: calculate `equal(a, b)`.
  const std::string equal_node_name = GenerateNodeName(base::JoinString(
      {kInserted, kOpTypeEqual, kToEmulate, not_equal.label}, kUnderscore));
  std::string lhs = GetOperandNameById(not_equal.lhs_operand_id);
  std::string rhs = GetOperandNameById(not_equal.rhs_operand_id);
  const std::string equal_output = GenerateOperandName();

  std::array<const char*, 1> equal_outputs = {equal_output.c_str()};
  std::array<const char*, 2> equal_inputs = {lhs.c_str(), rhs.c_str()};
  model_editor_.AddNode(kOpTypeEqual, equal_node_name, equal_inputs,
                        equal_outputs);

  // Step 2: calculate `logicalNot(equal_output)`
  const std::string not_output = GenerateOperandName();
  std::array<const char*, 1> not_outputs = {not_output.c_str()};
  const std::string not_node_name = GenerateNodeName(base::JoinString(
      {kInserted, kOpTypeLogicalNot, kToEmulate, not_equal.label},
      kUnderscore));
  model_editor_.AddNode(kOpTypeLogicalNot, not_node_name, equal_outputs,
                        not_outputs);

  // ONNX logical operators only support bool output. To support output with the
  // WebNN data type, it is necessary to insert a cast operator after a logical
  // operator.
  OperandId output_operand_id = not_equal.output_operand_id;
  const OperandDataType output_data_type =
      GetOperand(output_operand_id).descriptor.data_type();
  std::string output = GetOperandNameById(output_operand_id);
  CHECK_EQ(output_data_type, OperandDataType::kUint8);
  InsertCastNode(not_output, output, WebnnToOnnxDataType(output_data_type));
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
    case mojom::ElementWiseBinary::Kind::kEqual: {
      CHECK(data_type_limits.equal_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalBinaryOperation(element_wise_binary, kOpTypeEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kNotEqual: {
      CHECK(data_type_limits.not_equal_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalNotEqualOperation(element_wise_binary);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      CHECK(data_type_limits.greater_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalBinaryOperation(element_wise_binary, kOpTypeGreater);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual: {
      CHECK(data_type_limits.greater_or_equal_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalBinaryOperation(element_wise_binary, kOpTypeGreaterOrEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      CHECK(data_type_limits.lesser_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalBinaryOperation(element_wise_binary, kOpTypeLesser);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual: {
      CHECK(data_type_limits.lesser_or_equal_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalBinaryOperation(element_wise_binary, kOpTypeLesserOrEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalAnd: {
      CHECK(data_type_limits.logical_and_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalBinaryOperation(element_wise_binary, kOpTypeLogicalAnd);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalOr: {
      CHECK(data_type_limits.logical_or_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalBinaryOperation(element_wise_binary, kOpTypeLogicalOr);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalXor: {
      CHECK(data_type_limits.logical_xor_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      AddLogicalBinaryOperation(element_wise_binary, kOpTypeLogicalXor);
      break;
    }
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
    case mojom::ElementWiseUnary::Kind::kLogicalNot: {
      CHECK(data_type_limits.logical_not_input.Supports(input_descriptor));
      AddLogicalNotOperation(element_wise_unary);
      break;
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
  }
}

void GraphBuilderOrt::AddClampOperation(const mojom::Clamp& clamp) {
  const std::string node_name = GenerateNodeName(clamp.label);
  const std::string input = GetOperandNameById(clamp.input_operand_id);
  const std::string output = GetOperandNameById(clamp.output_operand_id);

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDescriptor& input_descriptor =
      GetOperand(clamp.input_operand_id).descriptor;
  CHECK(data_type_limits.clamp_input.Supports(input_descriptor));

  const OperandDataType input_data_type = input_descriptor.data_type();

  // Min and max are 0-D operands with the same data type of input.
  const std::string min =
      CreateScalarInitializerForFloat(input_data_type, clamp.min_value);
  const std::string max =
      CreateScalarInitializerForFloat(input_data_type, clamp.max_value);

  std::array<const char*, 3> inputs = {input.c_str(), min.c_str(), max.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeClamp, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddExpandOperation(const mojom::Expand& expand) {
  const std::string node_name = GenerateNodeName(expand.label);
  const std::string input = GetOperandNameById(expand.input_operand_id);
  const std::string output = GetOperandNameById(expand.output_operand_id);

  CHECK(context_properties_.data_type_limits.expand_input.Supports(
      GetOperand(expand.input_operand_id).descriptor));

  const std::vector<uint32_t>& output_shape =
      GetOperand(expand.output_operand_id).descriptor.shape();

  AddExpandNode(node_name, input, output, output_shape);
}

void GraphBuilderOrt::AddConcatOperation(const mojom::Concat& concat) {
  const std::string node_name = GenerateNodeName(concat.label);

  size_t input_count = concat.input_operand_ids.size();
  base::FixedArray<std::string> inputs_string(input_count);
  base::FixedArray<const char*> inputs(input_count);
  for (size_t i = 0; i < input_count; i++) {
    CHECK(context_properties_.data_type_limits.concat_inputs.Supports(
        GetOperand(concat.input_operand_ids[i]).descriptor));
    inputs_string[i] = GetOperandNameById(concat.input_operand_ids[i]);
    inputs[i] = inputs_string[i].c_str();
  }

  const std::string output = GetOperandNameById(concat.output_operand_id);
  std::array<const char*, 1> outputs = {output.c_str()};

  constexpr base::cstring_view kAxis = "axis";
  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAxis, base::checked_cast<int64_t>(concat.axis))};

  model_editor_.AddNode(kOpTypeConcat, node_name, inputs, outputs, attributes);
}

template <typename T>
void GraphBuilderOrt::AddGatherOperation(const T& operation,
                                         base::cstring_view op_type) {
  const std::string node_name = GenerateNodeName(operation.label);
  const std::string input = GetOperandNameById(operation.input_operand_id);
  const std::string indices = GetOperandNameById(operation.indices_operand_id);
  const std::string output = GetOperandNameById(operation.output_operand_id);

  // Clamp the indices operand to prevent out-of-bounds reading which will cause
  // ORT CPU EP to throw a runtime error.
  std::string clamped_indices = ClampIndices(
      indices, GetOperand(operation.indices_operand_id).descriptor.data_type(),
      GetOperand(operation.input_operand_id)
          .descriptor.shape()
          .at(operation.axis));

  std::array<const char*, 2> inputs = {input.c_str(), clamped_indices.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  constexpr base::cstring_view kAttrAxis = "axis";
  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAttrAxis, static_cast<int64_t>(operation.axis))};

  model_editor_.AddNode(op_type, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddGatherNDOperation(const mojom::GatherND& gather_nd) {
  const std::string node_name = GenerateNodeName(gather_nd.label);
  const std::string input = GetOperandNameById(gather_nd.input_operand_id);
  const std::string indices = GetOperandNameById(gather_nd.indices_operand_id);
  const std::string output = GetOperandNameById(gather_nd.output_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(gather_nd.input_operand_id).descriptor;
  const OperandDescriptor& indices_descriptor =
      GetOperand(gather_nd.indices_operand_id).descriptor;
  CHECK(context_properties_.data_type_limits.gather_nd_input.Supports(
      input_descriptor));
  CHECK(context_properties_.data_type_limits.gather_nd_indices.Supports(
      indices_descriptor));

  // ONNX GatherND only supports int64 indices.
  std::string int64_indices =
      indices_descriptor.data_type() == OperandDataType::kInt64
          ? indices
          : CreateCastNode(indices,
                           WebnnToOnnxDataType(OperandDataType::kInt64));

  // Clamp the indices operand to prevent out-of-bounds reading which will cause
  // ORT CPU EP to throw a runtime error.
  std::string clamped_indices = ClampGatherNDIndices(
      int64_indices, input_descriptor.shape(), indices_descriptor.shape());

  std::array<const char*, 2> inputs = {input.c_str(), clamped_indices.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeGatherND, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddGemmOperation(const mojom::Gemm& gemm) {
  const std::string node_name = GenerateNodeName(gemm.label);
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

  model_editor_.AddNode(kOpTypeGemm, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddLeakyReluOperation(
    const mojom::LeakyRelu& leaky_relu) {
  const std::string node_name = GenerateNodeName(leaky_relu.label);
  const std::string input = GetOperandNameById(leaky_relu.input_operand_id);
  const std::string output = GetOperandNameById(leaky_relu.output_operand_id);

  CHECK(context_properties_.data_type_limits.leaky_relu_input.Supports(
      GetOperand(leaky_relu.input_operand_id).descriptor));

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  constexpr base::cstring_view kAttrAlpha = "alpha";
  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrAlpha, leaky_relu.alpha)};
  model_editor_.AddNode(kOpTypeLeakyRelu, node_name, inputs, outputs,
                        attributes);
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

  const std::string node_name = GenerateNodeName(pool2d.label);
  const std::string input = GetOperandNameById(pool2d.input_operand_id);
  const std::string output = GetOperandNameById(pool2d.output_operand_id);
  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddReshapeOperation(const mojom::Reshape& reshape) {
  const std::string node_name = GenerateNodeName(reshape.label);
  const std::string input = GetOperandNameById(reshape.input_operand_id);
  const std::string output = GetOperandNameById(reshape.output_operand_id);

  CHECK(context_properties_.data_type_limits.reshape_input.Supports(
      GetOperand(reshape.input_operand_id).descriptor));

  const std::vector<uint32_t>& output_shape =
      GetOperand(reshape.output_operand_id).descriptor.shape();
  // `new_shape` should be the name of an int64 tensor that specifies the
  // output's shape.
  const std::string new_shape =
      CreateInt64InitializerForUint32Array(output_shape);

  std::array<const char*, 2> inputs = {input.c_str(), new_shape.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeReshape, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddReverseOperation(const mojom::Reverse& reverse) {
  const std::string node_name = GenerateNodeName(reverse.label);
  const std::string input = GetOperandNameById(reverse.input_operand_id);
  const std::string output = GetOperandNameById(reverse.output_operand_id);

  CHECK(context_properties_.data_type_limits.reverse_input.Supports(
      GetOperand(reverse.input_operand_id).descriptor));

  // Axes can be empty, which means no dimensions are reversed.
  base::FixedArray<int64_t> axes(reverse.axes.begin(), reverse.axes.end());
  size_t axes_size = axes.size();

  // Emulate reverse operation using backward slice with negative steps.
  // For each axis to be reversed:
  // - start = -1 (last element)
  // - end = min_int64 (goes to the beginning)
  // - step = -1 (backward direction)
  base::FixedArray<int64_t> starts(axes_size, -1);
  base::FixedArray<int64_t> ends(axes_size,
                                 std::numeric_limits<int64_t>::min());
  base::FixedArray<int64_t> steps(axes_size, -1);

  return AddSliceNode(node_name, input, output, axes, starts, ends, steps);
}

void GraphBuilderOrt::AddScatterElementsOperation(
    const mojom::ScatterElements& scatter_elements) {
  const std::string node_name = GenerateNodeName(scatter_elements.label);
  const std::string input =
      GetOperandNameById(scatter_elements.input_operand_id);
  const std::string indices =
      GetOperandNameById(scatter_elements.indices_operand_id);
  const std::string updates =
      GetOperandNameById(scatter_elements.updates_operand_id);
  const std::string output =
      GetOperandNameById(scatter_elements.output_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(scatter_elements.input_operand_id).descriptor;
  const OperandDescriptor& updates_descriptor =
      GetOperand(scatter_elements.updates_operand_id).descriptor;
  const OperandDescriptor& indices_descriptor =
      GetOperand(scatter_elements.indices_operand_id).descriptor;
  CHECK(context_properties_.data_type_limits.scatter_elements_input.SupportsAll(
      {input_descriptor, updates_descriptor}));
  CHECK(context_properties_.data_type_limits.scatter_elements_indices.Supports(
      indices_descriptor));

  // Clamp the indices operand to prevent out-of-bounds writing which will cause
  // ORT CPU EP to throw a runtime error.
  std::string clamped_indices =
      ClampIndices(indices, indices_descriptor.data_type(),
                   input_descriptor.shape().at(scatter_elements.axis));

  std::array<const char*, 3> inputs = {input.c_str(), clamped_indices.c_str(),
                                       updates.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  constexpr base::cstring_view kAttrAxis = "axis";
  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAttrAxis, static_cast<int64_t>(scatter_elements.axis))};

  model_editor_.AddNode(kOpTypeScatterElements, node_name, inputs, outputs,
                        attributes);
}

void GraphBuilderOrt::AddScatterNDOperation(
    const mojom::ScatterND& scatter_nd) {
  const std::string node_name = GenerateNodeName(scatter_nd.label);
  const std::string input = GetOperandNameById(scatter_nd.input_operand_id);
  const std::string indices = GetOperandNameById(scatter_nd.indices_operand_id);
  const std::string updates = GetOperandNameById(scatter_nd.updates_operand_id);
  const std::string output = GetOperandNameById(scatter_nd.output_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(scatter_nd.input_operand_id).descriptor;
  const OperandDescriptor& updates_descriptor =
      GetOperand(scatter_nd.updates_operand_id).descriptor;
  const OperandDescriptor& indices_descriptor =
      GetOperand(scatter_nd.indices_operand_id).descriptor;
  CHECK(context_properties_.data_type_limits.scatter_nd_input.Supports(
      input_descriptor));
  CHECK(context_properties_.data_type_limits.scatter_nd_updates.Supports(
      updates_descriptor));
  CHECK(context_properties_.data_type_limits.scatter_nd_indices.Supports(
      indices_descriptor));

  // ONNX ScatterND only supports int64 indices.
  std::string int64_indices =
      indices_descriptor.data_type() == OperandDataType::kInt64
          ? indices
          : CreateCastNode(indices,
                           WebnnToOnnxDataType(OperandDataType::kInt64));

  // Clamp the indices operand to prevent out-of-bounds writing which will cause
  // ORT CPU EP to throw a runtime error.
  std::string clamped_indices = ClampGatherNDIndices(
      int64_indices, input_descriptor.shape(), indices_descriptor.shape());

  std::array<const char*, 3> inputs = {input.c_str(), clamped_indices.c_str(),
                                       updates.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeScatterND, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddSliceOperation(const mojom::Slice& slice) {
  const std::string node_name = GenerateNodeName(slice.label);
  const std::string input = GetOperandNameById(slice.input_operand_id);
  const std::string output = GetOperandNameById(slice.output_operand_id);

  CHECK(context_properties_.data_type_limits.slice_input.Supports(
      GetOperand(slice.input_operand_id).descriptor));

  base::FixedArray<int64_t> starts(slice.ranges.size());
  base::FixedArray<int64_t> ends(slice.ranges.size());
  base::FixedArray<int64_t> steps(slice.ranges.size());
  for (size_t i = 0; i < slice.ranges.size(); ++i) {
    starts[i] = base::checked_cast<int64_t>(slice.ranges[i].start);
    ends[i] = base::checked_cast<int64_t>(slice.ranges[i].start +
                                          slice.ranges[i].size);
    steps[i] = base::checked_cast<int64_t>(slice.ranges[i].stride);
  }

  // Explicitly provide axes to avoid validation failure of DirectML EP.
  // https://github.com/microsoft/onnxruntime/issues/25252
  base::FixedArray<int64_t> axes(slice.ranges.size());
  std::iota(axes.begin(), axes.end(), 0);

  AddSliceNode(node_name, input, output, axes, starts, ends, steps);
}

void GraphBuilderOrt::AddSoftmaxOperation(const mojom::Softmax& softmax) {
  const std::string node_name = GenerateNodeName(softmax.label);
  const std::string input = GetOperandNameById(softmax.input_operand_id);
  const std::string output = GetOperandNameById(softmax.output_operand_id);

  CHECK(context_properties_.data_type_limits.softmax_input.Supports(
      GetOperand(softmax.input_operand_id).descriptor));

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  constexpr base::cstring_view kAttrAxis = "axis";
  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAttrAxis, static_cast<int64_t>(softmax.axis))};

  model_editor_.AddNode(kOpTypeSoftmax, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddPadOperation(const mojom::Pad& pad) {
  const std::string node_name = GenerateNodeName(pad.label);
  const std::string input = GetOperandNameById(pad.input_operand_id);
  const std::string output = GetOperandNameById(pad.output_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(pad.input_operand_id).descriptor;
  CHECK(context_properties_.data_type_limits.pad_input.Supports(
      input_descriptor));

  std::vector<const char*> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  size_t paddings_size =
      pad.beginning_padding.size() + pad.ending_padding.size();
  CHECK_EQ(paddings_size, input_descriptor.Rank() * 2);
  std::vector<uint32_t> paddings_value;
  paddings_value.reserve(paddings_size);
  std::ranges::copy(pad.beginning_padding, std::back_inserter(paddings_value));
  std::ranges::copy(pad.ending_padding, std::back_inserter(paddings_value));
  const std::string paddings =
      CreateInt64InitializerForUint32Array(paddings_value);
  inputs.push_back(paddings.c_str());

  std::string mode;
  std::string constant;
  switch (pad.mode->which()) {
    case mojom::PaddingMode::Tag::kConstant: {
      mode = "constant";
      constant = CreateScalarInitializerForFloat(
          input_descriptor.data_type(), pad.mode->get_constant()->value);
      inputs.push_back(constant.c_str());
      break;
    }
    case mojom::PaddingMode::Tag::kEdge:
      mode = "edge";
      break;
    case mojom::PaddingMode::Tag::kReflection:
      mode = "reflect";
      break;
  }

  constexpr base::cstring_view kAttrMode = "mode";
  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrMode, mode)};
  model_editor_.AddNode(kOpTypePad, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddPreluOperation(const mojom::Prelu& prelu) {
  const std::string node_name = GenerateNodeName(prelu.label);
  std::string input = GetOperandNameById(prelu.input_operand_id);
  const std::string slope = GetOperandNameById(prelu.slope_operand_id);
  const std::string output = GetOperandNameById(prelu.output_operand_id);

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDescriptor& input_descriptor =
      GetOperand(prelu.input_operand_id).descriptor;
  CHECK(data_type_limits.prelu_input.Supports(input_descriptor));
  const OperandDescriptor& slope_descriptor =
      GetOperand(prelu.slope_operand_id).descriptor;
  CHECK(data_type_limits.prelu_input.Supports(slope_descriptor));

  const std::vector<uint32_t>& input_shape = input_descriptor.shape();
  const std::vector<uint32_t>& slope_shape = slope_descriptor.shape();
  // ONNX Prelu requires slope's shape to be unidirectionally broadcastable to
  // input when the shape of slope is smaller than the input. While WebNN allows
  // input and slope to be bidirectionally broadcastable.
  if (!BroadcastShapes(slope_shape, input_shape, /*bidirectional=*/false)) {
    input = CreateExpandNode(input, slope_shape);
  }
  std::array<const char*, 2> inputs = {input.c_str(), slope.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypePRelu, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddSplitOperation(const mojom::Split& split) {
  const std::string node_name = GenerateNodeName(split.label);
  const std::string input = GetOperandNameById(split.input_operand_id);

  CHECK(context_properties_.data_type_limits.split_input.Supports(
      GetOperand(split.input_operand_id).descriptor));

  const auto output_count = split.output_operand_ids.size();
  // 'split' is a 1-D tensor which specifies the length of each output. The sum
  // of the values must be equal to the input size along 'axis'.
  // https://onnx.ai/onnx/operators/onnx__Split.html#inputs
  base::FixedArray<uint32_t> split_sizes(output_count);
  for (size_t i = 0; i < output_count; i++) {
    const std::vector<uint32_t>& output_shape =
        GetOperand(split.output_operand_ids[i]).descriptor.shape();
    CHECK_LT(split.axis, output_shape.size());
    split_sizes[i] = output_shape[split.axis];
  }
  const std::string split_input =
      CreateInt64InitializerForUint32Array(split_sizes);
  std::array<const char*, 2> inputs = {input.c_str(), split_input.c_str()};

  base::FixedArray<std::string> output_names(output_count);
  base::FixedArray<const char*> outputs(output_count);
  for (size_t i = 0; i < output_count; i++) {
    output_names[i] = GetOperandNameById(split.output_operand_ids[i]);
    outputs[i] = output_names[i].c_str();
  }

  constexpr base::cstring_view kAttrAxis = "axis";
  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAttrAxis, base::checked_cast<int64_t>(split.axis))};

  model_editor_.AddNode(kOpTypeSplit, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddTileOperation(const mojom::Tile& tile) {
  const std::string node_name = GenerateNodeName(tile.label);
  const std::string input = GetOperandNameById(tile.input_operand_id);
  const std::string output = GetOperandNameById(tile.output_operand_id);

  CHECK(context_properties_.data_type_limits.tile_input.Supports(
      GetOperand(tile.input_operand_id).descriptor));

  const std::string repeats =
      CreateInt64InitializerForUint32Array(tile.repetitions);

  std::array<const char*, 2> inputs = {input.data(), repeats.data()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeTile, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddTransposeOperation(const mojom::Transpose& transpose) {
  const std::string node_name = GenerateNodeName(transpose.label);
  const std::string input = GetOperandNameById(transpose.input_operand_id);
  const std::string output = GetOperandNameById(transpose.output_operand_id);

  CHECK(context_properties_.data_type_limits.transpose_input.Supports(
      GetOperand(transpose.input_operand_id).descriptor));

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  constexpr base::cstring_view kAttrPerm = "perm";
  std::vector<int64_t> perm_value(transpose.permutation.begin(),
                                  transpose.permutation.end());
  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrPerm, perm_value)};

  model_editor_.AddNode(kOpTypeTranspose, node_name, inputs, outputs,
                        attributes);
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
      case mojom::Operation::Tag::kArgMinMax: {
        AddArgMinMaxOperation(*operation->get_arg_min_max());
        break;
      }
      case mojom::Operation::Tag::kClamp: {
        AddClampOperation(*operation->get_clamp());
        break;
      }
      case mojom::Operation::Tag::kConcat: {
        AddConcatOperation(*operation->get_concat());
        break;
      }
      case mojom::Operation::Tag::kConv2d: {
        AddConv2dOperation(*operation->get_conv2d());
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
      case mojom::Operation::Tag::kExpand: {
        AddExpandOperation(*operation->get_expand());
        break;
      }
      case mojom::Operation::Tag::kGather: {
        CHECK(data_type_limits.gather_input.Supports(
            GetOperand(operation->get_gather()->input_operand_id).descriptor));
        CHECK(data_type_limits.gather_indices.Supports(
            GetOperand(operation->get_gather()->indices_operand_id)
                .descriptor));
        AddGatherOperation(*operation->get_gather(), kOpTypeGather);
        break;
      }
      case mojom::Operation::Tag::kGatherElements: {
        CHECK(data_type_limits.gather_elements_input.Supports(
            GetOperand(operation->get_gather_elements()->input_operand_id)
                .descriptor));
        CHECK(data_type_limits.gather_elements_indices.Supports(
            GetOperand(operation->get_gather_elements()->indices_operand_id)
                .descriptor));
        AddGatherOperation(*operation->get_gather_elements(),
                           kOpTypeGatherElements);
        break;
      }
      case mojom::Operation::Tag::kGatherNd: {
        AddGatherNDOperation(*operation->get_gather_nd());
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
      case mojom::Operation::Tag::kLeakyRelu: {
        AddLeakyReluOperation(*operation->get_leaky_relu());
        break;
      }
      case mojom::Operation::Tag::kPad: {
        AddPadOperation(*operation->get_pad());
        break;
      }
      case mojom::Operation::Tag::kPool2d: {
        AddPool2dOperation(*operation->get_pool2d());
        break;
      }
      case mojom::Operation::Tag::kPrelu: {
        AddPreluOperation(*operation->get_prelu());
        break;
      }
      case mojom::Operation::Tag::kRelu: {
        CHECK(data_type_limits.relu_input.Supports(
            GetOperand(operation->get_relu()->input_operand_id).descriptor));
        AddUnaryOperation(*operation->get_relu(), kOpTypeRelu);
        break;
      }
      case mojom::Operation::Tag::kReshape: {
        AddReshapeOperation(*operation->get_reshape());
        break;
      }
      case mojom::Operation::Tag::kReverse: {
        AddReverseOperation(*operation->get_reverse());
        break;
      }
      case mojom::Operation::Tag::kScatterElements: {
        AddScatterElementsOperation(*operation->get_scatter_elements());
        break;
      }
      case mojom::Operation::Tag::kScatterNd: {
        AddScatterNDOperation(*operation->get_scatter_nd());
        break;
      }
      case mojom::Operation::Tag::kSlice: {
        AddSliceOperation(*operation->get_slice());
        break;
      }
      case mojom::Operation::Tag::kSigmoid: {
        CHECK(data_type_limits.sigmoid_input.Supports(
            GetOperand(operation->get_sigmoid()->input_operand_id).descriptor));
        AddUnaryOperation(*operation->get_sigmoid(), kOpTypeSigmoid);
        break;
      }
      case mojom::Operation::Tag::kSoftmax: {
        AddSoftmaxOperation(*operation->get_softmax());
        break;
      }
      case mojom::Operation::Tag::kSoftsign: {
        CHECK(data_type_limits.softsign_input.Supports(
            GetOperand(operation->get_softsign()->input_operand_id)
                .descriptor));
        AddUnaryOperation(*operation->get_softsign(), kOpTypeSoftsign);
        break;
      }
      case mojom::Operation::Tag::kSplit: {
        AddSplitOperation(*operation->get_split());
        break;
      }
      case mojom::Operation::Tag::kTanh: {
        CHECK(data_type_limits.tanh_input.Supports(
            GetOperand(operation->get_tanh()->input_operand_id).descriptor));
        AddUnaryOperation(*operation->get_tanh(), kOpTypeTanh);
        break;
      }
      case mojom::Operation::Tag::kTile: {
        AddTileOperation(*operation->get_tile());
        break;
      }
      case mojom::Operation::Tag::kTranspose: {
        AddTransposeOperation(*operation->get_transpose());
        break;
      }
      case mojom::Operation::Tag::kBatchNormalization:
      case mojom::Operation::Tag::kCumulativeSum:
      case mojom::Operation::Tag::kDequantizeLinear:
      case mojom::Operation::Tag::kElu:
      case mojom::Operation::Tag::kGru:
      case mojom::Operation::Tag::kGruCell:
      case mojom::Operation::Tag::kHardSigmoid:
      case mojom::Operation::Tag::kInstanceNormalization:
      case mojom::Operation::Tag::kLayerNormalization:
      case mojom::Operation::Tag::kLinear:
      case mojom::Operation::Tag::kLstm:
      case mojom::Operation::Tag::kLstmCell:
      case mojom::Operation::Tag::kMatmul:
      case mojom::Operation::Tag::kQuantizeLinear:
      case mojom::Operation::Tag::kReduce:
      case mojom::Operation::Tag::kResample2d:
      case mojom::Operation::Tag::kSoftplus:
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
