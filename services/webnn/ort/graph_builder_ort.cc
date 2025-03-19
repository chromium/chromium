// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_builder_ort.h"

#include <limits>
#include <numeric>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected_macros.h"
#include "base/types/fixed_array.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn::ort {

namespace {

constexpr char kOpTypeArgMax[] = "ArgMax";
constexpr char kOpTypeArgMin[] = "ArgMin";
constexpr char kOpTypeBatchNormalization[] = "BatchNormalization";

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
constexpr char kOpTypeConcat[] = "Concat";
constexpr char kOpTypeConv2d[] = "Conv";
constexpr char kOpTypeConvTranspose2d[] = "ConvTranspose";
constexpr char kOpTypeCumulativeSum[] = "CumSum";
constexpr char kOpTypeDequantizeLinear[] = "DequantizeLinear";
constexpr char kOpTypeElu[] = "Elu";
constexpr char kOpTypeExpand[] = "Expand";
constexpr char kOpTypeGather[] = "Gather";
constexpr char kOpTypeGatherElements[] = "GatherElements";
constexpr char kOpTypeGatherND[] = "GatherND";
constexpr char kOpTypeGelu[] = "Gelu";
constexpr char kOpTypeGemm[] = "Gemm";
constexpr char kOpTypeGru[] = "GRU";
constexpr char kOpTypeHardSwish[] = "HardSwish";
constexpr char kOpTypeHardSigmoid[] = "HardSigmoid";
constexpr char kOpTypeInstanceNormalization[] = "InstanceNormalization";
constexpr char kOpTypeLayerNormalization[] = "LayerNormalization";
constexpr char kOpTypeLeakyRelu[] = "LeakyRelu";
constexpr char kOpTypeLstm[] = "LSTM";
constexpr char kOpTypeMatMul[] = "MatMul";
constexpr char kOpTypePad[] = "Pad";

// Pooling operations
constexpr char kOpTypeAveragePool2d[] = "AveragePool";
constexpr char kOpTypeMaxPool2d[] = "MaxPool";
constexpr char kOpTypeLpPool2d[] = "LpPool";

constexpr char kOpTypePRelu[] = "PRelu";
constexpr char kOpTypeQuantizeLinear[] = "QuantizeLinear";

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
constexpr char kOpTypeResample2d[] = "Resize";
constexpr char kOpTypeReshape[] = "Reshape";
constexpr char kOpTypeScatterElements[] = "ScatterElements";
constexpr char kOpTypeScatterND[] = "ScatterND";
constexpr char kOpTypeSigmoid[] = "Sigmoid";
constexpr char kOpTypeSlice[] = "Slice";
constexpr char kOpTypeSoftmax[] = "Softmax";
constexpr char kOpTypeSoftplus[] = "Softplus";
constexpr char kOpTypeSoftsign[] = "Softsign";
constexpr char kOpTypeSplit[] = "Split";
constexpr char kOpTypeTanh[] = "Tanh";
constexpr char kOpTypeTile[] = "Tile";
constexpr char kOpTypeTranspose[] = "Transpose";
constexpr char kOpTypeTriangular[] = "Trilu";
constexpr char kOpTypeWhere[] = "Where";

constexpr char kInserted[] = "Inserted";
constexpr char kUnderscore[] = "_";

base::unexpected<mojom::ErrorPtr> NewNotSupportedError(std::string message) {
  return base::unexpected(mojom::Error::New(
      mojom::Error::Code::kNotSupportedError, std::move(message)));
}

base::unexpected<mojom::ErrorPtr> NewUnknownError(std::string message) {
  return base::unexpected(
      mojom::Error::New(mojom::Error::Code::kUnknownError, std::move(message)));
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

}  // namespace

std::string GetOperandName(std::string_view label, uint64_t id) {
  return base::JoinString({label, base::NumberToString(id)}, kUnderscore);
}

// static
base::expected<std::unique_ptr<OrtModelEditor::ModelInfo>, mojom::ErrorPtr>
GraphBuilderOrt::CreateAndBuild(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands) {
  GraphBuilderOrt graph_builder(graph_info, std::move(context_properties),
                                std::move(constant_operands));

  return graph_builder.BuildModel();
}

GraphBuilderOrt::GraphBuilderOrt(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands)
    : graph_info_(graph_info),
      constant_operands_(std::move(constant_operands)),
      context_properties_(std::move(context_properties)),
      model_editor_(OrtModelEditor()) {
  for (const auto& [id, _] : graph_info.id_to_operand_map) {
    next_operand_id_ = std::max(next_operand_id_, id + 1);
  }
}

GraphBuilderOrt::~GraphBuilderOrt() = default;

const mojom::Operand& GraphBuilderOrt::GetOperand(uint64_t operand_id) const {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

std::string GraphBuilderOrt::GetOperandNameById(uint64_t operand_id) const {
  const mojom::Operand& operand = GetOperand(operand_id);
  std::string operand_label =
      operand.name.has_value() ? operand.name.value() : "";
  return GetOperandName(operand_label, operand_id);
}

std::string GraphBuilderOrt::GenerateNextOperandName() {
  return GetOperandName(kInserted, next_operand_id_++);
}

std::string GraphBuilderOrt::GenerateNextOperationName(std::string_view label) {
  return base::JoinString({label, base::NumberToString(next_operation_id_++)},
                          kUnderscore);
}

const std::vector<std::string> GetRecurrentNetworkActivations(
    std::vector<mojom::RecurrentNetworkActivation> activations,
    bool is_bidirectional) {
  std::vector<std::string> activation_list;
  for (const auto& activation : activations) {
    switch (activation) {
      case mojom::RecurrentNetworkActivation::kRelu:
        activation_list.push_back("relu");
        break;
      case mojom::RecurrentNetworkActivation::kSigmoid:
        activation_list.push_back("sigmoid");
        break;
      case mojom::RecurrentNetworkActivation::kTanh:
        activation_list.push_back("tanh");
        break;
      default:
        NOTREACHED() << "Unsupported recurrent network activation function.";
    }
  }
  if (is_bidirectional) {
    activation_list.insert(activation_list.end(), activation_list.begin(),
                           activation_list.end());
  }
  return activation_list;
}

const std::string GetRecurrentNetworkDirection(
    mojom::RecurrentNetworkDirection direction) {
  switch (direction) {
    case mojom::RecurrentNetworkDirection::kForward:
      return "forward";
    case mojom::RecurrentNetworkDirection::kBackward:
      return "reverse";
    case mojom::RecurrentNetworkDirection::kBoth:
      return "bidirectional";
    default:
      NOTREACHED() << "Unsupported recurrent network activation direction.";
  }
}

[[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
GraphBuilderOrt::CreateOrReshapeBias(const std::optional<uint32_t>& bias_id,
                                     OperandDataType input_data_type,
                                     const std::vector<uint32_t>& bias_dims) {
  std::string bias;
  if (bias_id.has_value()) {
    bias = GetOperandNameById(bias_id.value());
    // Reshape only when needed. (gruCell, lstmCell)
    const std::vector<uint32_t>& bias_shape =
        GetOperand(bias_id.value()).descriptor.shape();
    if (bias_shape.size() != bias_dims.size()) {
      ASSIGN_OR_RETURN(bias, PrependReshape(bias, bias_dims));
    }
  } else {
    // Create all zero bias.
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::array<uint16_t, 1> bias_value = {fp16_ieee_from_fp32_value(0.0f)};
        ASSIGN_OR_RETURN(const std::string bias_scalar,
                         CreateInitializer<uint16_t>({}, bias_value));
        ASSIGN_OR_RETURN(bias, PrependExpand(bias_scalar, bias_dims));
        break;
      }
      case OperandDataType::kFloat32: {
        std::array<float, 1> bias_value = {0.0f};
        ASSIGN_OR_RETURN(const std::string bias_scalar,
                         CreateInitializer<float>({}, bias_value));
        ASSIGN_OR_RETURN(bias, PrependExpand(bias_scalar, bias_dims));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] Recurrent network operators only support "
                        "float32 and float16 data type.";
    }
  }
  return bias;
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
base::expected<std::string, mojom::ErrorPtr> GraphBuilderOrt::CreateInitializer(
    base::span<const uint32_t> shape,
    base::span<const DataType> data) {
  std::string name = GenerateNextOperandName();
  std::vector<int64_t> int64_shape(shape.begin(), shape.end());

  base::span<const uint8_t> byte_span;
  if constexpr (std::floating_point<DataType>) {
    // Floating point types do not have unique object representations, but
    // this code appears to be using a byte span to type-erase, which is fine.
    byte_span = base::as_byte_span(base::allow_nonunique_obj, data);
  } else {
    byte_span = base::as_byte_span(data);
  }

  ScopedOrtStatus status;
  // TODO(https://github.com/shiyi9801/chromium/issues/70): Remove this
  // workaround for OpenVINO EP once the invalid external data issue is fixed.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtUseOpenvino)) {
    status = model_editor_.AddInitializer(name, int64_shape, byte_span,
                                          TensorTypeMap<DataType>::value);

  } else {
    status = model_editor_.AddInitializerAsRawData(
        name, int64_shape, byte_span, TensorTypeMap<DataType>::value);
  }

  if (status.is_valid()) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create initializer."));
  } else {
    return name;
  }
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
[[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
GraphBuilderOrt::CreateScalarInitializer(const DataType& value) {
  return CreateInitializer<DataType>(
      /*shape=*/{}, base::span_from_ref(value));
}

void GraphBuilderOrt::AddCastNode(std::string_view node,
                                  std::string_view input,
                                  std::string_view output,
                                  ONNXTensorElementDataType to_data_type) {
  std::array<const char*, 1> inputs = {input.data()};
  std::array<const char*, 1> outputs = {output.data()};
  int64_t attr_to = static_cast<int64_t>(to_data_type);
  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"to", attr_to));
  model_editor_.AddNode(kOpTypeCast, node, inputs, outputs,
                        std::move(attributes));
}

std::string GraphBuilderOrt::PrependCast(
    std::string_view input,
    ONNXTensorElementDataType to_data_type) {
  const std::string node = GenerateNextOperationName("inserted_cast");
  const std::string output = GenerateNextOperandName();
  AddCastNode(node, input, output, to_data_type);
  return output;
}

void GraphBuilderOrt::AppendCast(std::string_view input,
                                 std::string_view output,
                                 ONNXTensorElementDataType to_data_type) {
  const std::string node = GenerateNextOperationName("inserted_cast");
  AddCastNode(node, input, output, to_data_type);
}

std::string GraphBuilderOrt::PrependTranspose(
    std::string_view input,
    base::span<const uint32_t> permutation) {
  const std::string node = GenerateNextOperationName("inserted_transpose");
  const std::string output = GenerateNextOperandName();

  std::array<const char*, 1> inputs = {input.data()};
  std::array<const char*, 1> outputs = {output.data()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  std::vector<int64_t> perm(permutation.begin(), permutation.end());
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"perm", perm));

  model_editor_.AddNode(kOpTypeTranspose, node, inputs, outputs,
                        std::move(attributes));
  return output;
}

void GraphBuilderOrt::AppendTranspose(std::string_view input,
                                      std::string_view output,
                                      base::span<const uint32_t> permutation) {
  const std::string node = GenerateNextOperationName("inserted_transpose");
  std::array<const char*, 1> inputs = {input.data()};
  std::array<const char*, 1> outputs = {output.data()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  std::vector<int64_t> perm(permutation.begin(), permutation.end());
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"perm", perm));

  model_editor_.AddNode(kOpTypeTranspose, node, inputs, outputs,
                        std::move(attributes));
}

[[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
GraphBuilderOrt::PrependReshape(std::string_view input,
                                base::span<const uint32_t> new_shape) {
  const std::string node = GenerateNextOperationName("inserted_reshape");
  const std::string output = GenerateNextOperandName();

  // Shape is an operand with data type int64, not an attribute.
  std::vector<uint32_t> shape_dims = {
      base::checked_cast<uint32_t>(new_shape.size())};
  std::vector<int64_t> shape_value(new_shape.begin(), new_shape.end());
  ASSIGN_OR_RETURN(const std::string shape,
                   CreateInitializer<int64_t>(shape_dims, shape_value));

  std::array<const char*, 2> inputs = {input.data(), shape.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeReshape, node, inputs, outputs);

  return output;
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AppendReshape(std::string_view input,
                               std::string_view output,
                               base::span<const uint32_t> new_shape) {
  const std::string node = GenerateNextOperationName("inserted_reshape");

  // Shape is an operand with data type int64, not an attribute.
  std::vector<uint32_t> shape_dims = {
      base::checked_cast<uint32_t>(new_shape.size())};
  std::vector<int64_t> shape_value(new_shape.begin(), new_shape.end());
  ASSIGN_OR_RETURN(const std::string shape,
                   CreateInitializer<int64_t>(shape_dims, shape_value));

  std::array<const char*, 2> inputs = {input.data(), shape.c_str()};
  std::array<const char*, 1> outputs = {output.data()};

  model_editor_.AddNode(kOpTypeReshape, node, inputs, outputs);

  return base::ok();
}

[[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
GraphBuilderOrt::PrependExpand(std::string_view input,
                               base::span<const uint32_t> shape) {
  const std::string node = GenerateNextOperationName("inserted_expand");
  const std::string output = GenerateNextOperandName();

  // Shape is an operand with data type int64, not an attribute.
  std::vector<uint32_t> shape_dims = {
      base::checked_cast<uint32_t>(shape.size())};
  std::vector<int64_t> shape_value(shape.begin(), shape.end());
  ASSIGN_OR_RETURN(const std::string new_shape,
                   CreateInitializer<int64_t>(shape_dims, shape_value));

  std::array<const char*, 2> inputs = {input.data(), new_shape.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeExpand, node, inputs, outputs);

  return output;
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddSliceNode(std::string_view node,
                              std::string_view input,
                              std::string_view output,
                              std::string_view axes,
                              base::span<const int64_t> starts_value,
                              base::span<const int64_t> ends_value,
                              base::span<const int64_t> steps_value) {
  // Starts is an operand with data type int64, not an attribute.
  std::vector<uint32_t> starts_shape = {
      base::checked_cast<uint32_t>(starts_value.size())};
  ASSIGN_OR_RETURN(const std::string starts,
                   CreateInitializer<int64_t>(starts_shape, starts_value));

  // Ends is an operand with data type int64, not an attribute.
  std::vector<uint32_t> ends_shape = {
      base::checked_cast<uint32_t>(ends_value.size())};
  ASSIGN_OR_RETURN(const std::string ends,
                   CreateInitializer<int64_t>(ends_shape, ends_value));

  // Steps is an operand with data type int64, not an attribute.
  std::vector<uint32_t> steps_shape = {
      base::checked_cast<uint32_t>(steps_value.size())};
  ASSIGN_OR_RETURN(const std::string steps,
                   CreateInitializer<int64_t>(steps_shape, steps_value));

  std::array<const char*, 5> inputs = {input.data(), starts.data(), ends.data(),
                                       axes.data(), steps.data()};
  std::array<const char*, 1> outputs = {output.data()};

  model_editor_.AddNode(kOpTypeSlice, node, inputs, outputs);

  return base::ok();
}

[[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
GraphBuilderOrt::ClampIndices(std::string_view indices,
                              OperandDataType indices_data_type,
                              uint32_t dim_size) {
  const std::string node = GenerateNextOperationName("inserted_clamp");
  const std::string output = GenerateNextOperandName();

  // The dimension size must be greater than 0.
  CHECK_GT(dim_size, 0u);

  std::string min;
  std::string max;
  switch (indices_data_type) {
    case OperandDataType::kInt32: {
      int32_t min_value = -base::saturated_cast<int32_t>(dim_size);
      int32_t max_value = base::saturated_cast<int32_t>(dim_size - 1);
      ASSIGN_OR_RETURN(min, CreateScalarInitializer(min_value));
      ASSIGN_OR_RETURN(max, CreateScalarInitializer(max_value));
      break;
    }
    case OperandDataType::kUint32: {
      uint32_t min_value = 0;
      uint32_t max_value = dim_size - 1;
      ASSIGN_OR_RETURN(min, CreateScalarInitializer(min_value));
      ASSIGN_OR_RETURN(max, CreateScalarInitializer(max_value));
      break;
    }
    case OperandDataType::kInt64: {
      int64_t min_value = -static_cast<int64_t>(dim_size);
      int64_t max_value = static_cast<int64_t>(dim_size - 1);
      ASSIGN_OR_RETURN(min, CreateScalarInitializer(min_value));
      ASSIGN_OR_RETURN(max, CreateScalarInitializer(max_value));
      break;
    }
    default:
      NOTREACHED() << "[WebNN] Indices can only be one of the int32, uint32 "
                      "and int64 data types.";
  }

  std::array<const char*, 3> inputs = {indices.data(), min.c_str(),
                                       max.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeClamp, node, inputs, outputs);

  return output;
}

void GraphBuilderOrt::AddInput(uint64_t input_id) {
  const mojom::Operand& operand = GetOperand(input_id);
  std::string name = GetOperandNameById(input_id);

  std::vector<int64_t> int64_shape(operand.descriptor.shape().begin(),
                                   operand.descriptor.shape().end());

  model_editor_.AddInput(
      name, int64_shape,
      OperandTypeToONNXTensorElementDataType(operand.descriptor.data_type()));
}

void GraphBuilderOrt::AddOutput(uint64_t output_id) {
  const mojom::Operand& operand = GetOperand(output_id);
  std::string name = GetOperandNameById(output_id);

  std::vector<int64_t> int64_shape(operand.descriptor.shape().begin(),
                                   operand.descriptor.shape().end());

  model_editor_.AddOutput(
      name, int64_shape,
      OperandTypeToONNXTensorElementDataType(operand.descriptor.data_type()));
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddInitializer(uint64_t constant_id) {
  const WebNNConstantOperand& operand = *constant_operands_.at(constant_id);
  std::string name = GetOperandNameById(constant_id);

  std::vector<int64_t> int64_shape(operand.descriptor().shape().begin(),
                                   operand.descriptor().shape().end());
  ONNXTensorElementDataType onnx_data_type =
      OperandTypeToONNXTensorElementDataType(operand.descriptor().data_type());
  ScopedOrtStatus status;
  // TODO(https://github.com/shiyi9801/chromium/issues/70): Remove this
  // workaround for OpenVINO EP once the invalid external data issue is fixed.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtUseOpenvino)) {
    status = model_editor_.AddInitializer(name, int64_shape, operand.ByteSpan(),
                                          onnx_data_type);
  } else {
    status = model_editor_.AddInitializerAsRawData(
        name, int64_shape, operand.ByteSpan(), onnx_data_type);
  }

  if (status.is_valid()) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to add initializer."));
  } else {
    return base::ok();
  }
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddBatchNormalizationOperation(
    const mojom::BatchNormalization& batch_normalization) {
  const OperandDataType input_data_type =
      GetOperand(batch_normalization.output_operand_id).descriptor.data_type();

  const std::string input =
      GetOperandNameById(batch_normalization.input_operand_id);
  std::vector<const char*> inputs = {input.c_str()};

  const std::vector<uint32_t>& input_shape =
      GetOperand(batch_normalization.input_operand_id).descriptor.shape();
  // TODO: Support NHWC layout-
  // https://github.com/shiyi9801/chromium/issues/77
  if (batch_normalization.axis != 1) {
    return NewNotSupportedError(
        "Unsupported axis since BatchNormalization only supports NCHW layout "
        "currently. ");
  }
  uint32_t input_channel = input_shape[1];
  std::vector<uint32_t> constant_dims = {input_channel};

  std::string scale, bias;
  // ONNX requires scale and bias inputs.
  if (batch_normalization.scale_operand_id) {
    scale = GetOperandNameById(batch_normalization.scale_operand_id.value());
    inputs.push_back(scale.c_str());
  } else {
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> scale_data_fp16(input_channel,
                                              fp16_ieee_from_fp32_value(1.0f));
        ASSIGN_OR_RETURN(
            scale, CreateInitializer<uint16_t>(constant_dims, scale_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        std::vector<float> scale_data(input_channel, 1.0f);
        ASSIGN_OR_RETURN(scale,
                         CreateInitializer<float>(constant_dims, scale_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] BatchNormalization only supports float32 "
                        "and float16 data type.";
    }

    inputs.push_back(scale.c_str());
  }

  if (batch_normalization.bias_operand_id) {
    bias = GetOperandNameById(batch_normalization.bias_operand_id.value());
    inputs.push_back(bias.c_str());
  } else {
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> bias_data_fp16(input_channel,
                                             fp16_ieee_from_fp32_value(0.0f));
        ASSIGN_OR_RETURN(
            bias, CreateInitializer<uint16_t>(constant_dims, bias_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        std::vector<float> bias_data(input_channel, 0.0f);
        ASSIGN_OR_RETURN(bias,
                         CreateInitializer<float>(constant_dims, bias_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] BatchNormalization only supports float32 "
                        "and float16 data type.";
    }

    inputs.push_back(bias.c_str());
  }

  const std::string mean =
      GetOperandNameById(batch_normalization.mean_operand_id);
  inputs.push_back(mean.c_str());

  const std::string variance =
      GetOperandNameById(batch_normalization.variance_operand_id);
  inputs.push_back(variance.c_str());

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"epsilon", batch_normalization.epsilon));

  const std::string node = GenerateNextOperationName(batch_normalization.label);
  const std::string output =
      GetOperandNameById(batch_normalization.output_operand_id);
  std::array<const char*, 1> outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeBatchNormalization, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

template <typename T>
void GraphBuilderOrt::AddBinaryOperation(const T& operation,
                                         std::string_view op_type) {
  const std::string node = GenerateNextOperationName(operation.label);
  const std::string lhs = GetOperandNameById(operation.lhs_operand_id);
  const std::string rhs = GetOperandNameById(operation.rhs_operand_id);
  const std::string output = GetOperandNameById(operation.output_operand_id);

  std::array<const char*, 2> inputs = {lhs.c_str(), rhs.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node, inputs, outputs);
}

void GraphBuilderOrt::AddElementWiseLogicalOperation(
    absl::variant<const mojom::ElementWiseBinary*,
                  const mojom::ElementWiseUnary*> operation,
    std::string_view op_type) {
  const std::string node = absl::visit(
      [this](const auto* op) { return GenerateNextOperationName(op->label); },
      operation);

  std::vector<const char*> inputs;
  std::string lhs;
  std::string rhs;
  if (absl::holds_alternative<const mojom::ElementWiseBinary*>(operation)) {
    const mojom::ElementWiseBinary* element_wise_binary =
        absl::get<const mojom::ElementWiseBinary*>(operation);
    lhs = GetOperandNameById(element_wise_binary->lhs_operand_id);
    rhs = GetOperandNameById(element_wise_binary->rhs_operand_id);

    // Some ONNX logical operators only support bool input.
    if (element_wise_binary->kind ==
            mojom::ElementWiseBinary::Kind::kLogicalAnd ||
        element_wise_binary->kind ==
            mojom::ElementWiseBinary::Kind::kLogicalOr ||
        element_wise_binary->kind ==
            mojom::ElementWiseBinary::Kind::kLogicalXor) {
      lhs = PrependCast(lhs, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
      rhs = PrependCast(rhs, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
    }

    inputs = {lhs.c_str(), rhs.c_str()};
  } else {
    const mojom::ElementWiseUnary* element_wise_unary =
        absl::get<const mojom::ElementWiseUnary*>(operation);
    lhs = GetOperandNameById(element_wise_unary->input_operand_id);

    // Some ONNX logical operators only support bool input.
    if (element_wise_unary->kind ==
        mojom::ElementWiseUnary::Kind::kLogicalNot) {
      lhs = PrependCast(lhs, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
    }

    inputs = {lhs.c_str()};
  }

  const std::string bool_output = GenerateNextOperandName();
  std::array<const char*, 1> outputs = {bool_output.c_str()};

  model_editor_.AddNode(op_type, node, inputs, outputs);

  // ONNX logical operators only support bool output. To support output with the
  // WebNN data type, it is necessary to insert a cast operator after a logical
  // operator.
  uint64_t output_operand_id = absl::visit(
      [](const auto* op) { return op->output_operand_id; }, operation);
  OperandDataType output_data_type =
      GetOperand(output_operand_id).descriptor.data_type();
  std::string output = GetOperandNameById(output_operand_id);
  AppendCast(bool_output, output,
             OperandTypeToONNXTensorElementDataType(output_data_type));
}

// ONNX doesn't support `notEqual`, emulate it by `logicalNot(equal(a, b))`.
void GraphBuilderOrt::AddElementWiseLogicalNotEqualOperation(
    const mojom::ElementWiseBinary& not_equal) {
  // Step 1: calculate `equal(a, b)`.
  const std::string equal_node =
      GenerateNextOperationName("inserted_equal_to_emulate_" + not_equal.label);
  std::string lhs = GetOperandNameById(not_equal.lhs_operand_id);
  std::string rhs = GetOperandNameById(not_equal.rhs_operand_id);
  const std::string equal_output = GenerateNextOperandName();

  std::array<const char*, 1> equal_outputs = {equal_output.c_str()};
  std::array<const char*, 2> equal_inputs = {lhs.c_str(), rhs.c_str()};
  model_editor_.AddNode(kOpTypeEqual, equal_node, equal_inputs, equal_outputs);

  // Step 2: calculate `logicalNot(equal_output)`
  const std::string not_output = GenerateNextOperandName();
  std::array<const char*, 1> not_outputs = {not_output.c_str()};
  const std::string not_node =
      GenerateNextOperationName("inserted_not_to_emulate_" + not_equal.label);
  model_editor_.AddNode(kOpTypeLogicalNot, not_node, equal_outputs,
                        not_outputs);

  // ONNX logical operators only support bool output. To support output with the
  // WebNN data type, it is necessary to insert a cast operator after a logical
  // operator.
  uint64_t output_operand_id = not_equal.output_operand_id;
  OperandDataType output_data_type =
      GetOperand(output_operand_id).descriptor.data_type();
  std::string output = GetOperandNameById(output_operand_id);
  AppendCast(not_output, output,
             OperandTypeToONNXTensorElementDataType(output_data_type));
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddElementWiseBinaryOperation(
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
      AddElementWiseLogicalOperation(&element_wise_binary, kOpTypeEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kNotEqual: {
      AddElementWiseLogicalNotEqualOperation(element_wise_binary);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      AddElementWiseLogicalOperation(&element_wise_binary, kOpTypeGreater);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual: {
      AddElementWiseLogicalOperation(&element_wise_binary,
                                     kOpTypeGreaterOrEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      AddElementWiseLogicalOperation(&element_wise_binary, kOpTypeLesser);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual: {
      AddElementWiseLogicalOperation(&element_wise_binary,
                                     kOpTypeLesserOrEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalAnd: {
      AddElementWiseLogicalOperation(&element_wise_binary, kOpTypeLogicalAnd);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalOr: {
      AddElementWiseLogicalOperation(&element_wise_binary, kOpTypeLogicalOr);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalXor: {
      AddElementWiseLogicalOperation(&element_wise_binary, kOpTypeLogicalXor);
      break;
    }
  }

  return base::ok();
}

template <typename T>
void GraphBuilderOrt::AddUnaryOperation(const T& operation,
                                        std::string_view op_type) {
  const std::string node = GenerateNextOperationName(operation.label);
  const std::string input = GetOperandNameById(operation.input_operand_id);
  const std::string output = GetOperandNameById(operation.output_operand_id);

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node, inputs, outputs);
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
      AddElementWiseLogicalOperation(&element_wise_unary, kOpTypeLogicalNot);
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

void GraphBuilderOrt::AddArgMinMaxOperation(
    const mojom::ArgMinMax& arg_min_max) {
  const std::string node = GenerateNextOperationName(arg_min_max.label);
  const std::string input = GetOperandNameById(arg_min_max.input_operand_id);
  const std::string output = GetOperandNameById(arg_min_max.output_operand_id);

  std::vector<ScopedOrtOpAttr> attributes;

  int64_t axis = static_cast<int64_t>(arg_min_max.axis);
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"axis", axis));

  int64_t keep_dimensions = static_cast<int64_t>(arg_min_max.keep_dimensions);
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"keepdims", keep_dimensions));

  // Onnx ArgMin/Max only supports int64 output. To support int32 output, it is
  // necessary to insert a cast operator after ArgMin/Max. To cast Argmin/Max
  // output from int64 to int32 is safe since a valid operand dimension is
  // greater than zero and in the range of int32.
  // https://www.w3.org/TR/webnn/#valid-dimension
  OperandDataType output_data_type =
      GetOperand(arg_min_max.output_operand_id).descriptor.data_type();
  bool need_cast = output_data_type != OperandDataType::kInt64;

  const std::string int64_output =
      need_cast ? GenerateNextOperandName() : output;

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {int64_output.c_str()};

  switch (arg_min_max.kind) {
    case mojom::ArgMinMax::Kind::kMax: {
      model_editor_.AddNode(kOpTypeArgMax, node, inputs, outputs,
                            std::move(attributes));
      break;
    }
    case mojom::ArgMinMax::Kind::kMin: {
      model_editor_.AddNode(kOpTypeArgMin, node, inputs, outputs,
                            std::move(attributes));
      break;
    }
  }

  if (need_cast) {
    AppendCast(int64_output, output,
               OperandTypeToONNXTensorElementDataType(output_data_type));
  }
}

void GraphBuilderOrt::AddCastOperation(const mojom::ElementWiseUnary& cast) {
  const std::string node = GenerateNextOperationName(cast.label);
  const std::string input = GetOperandNameById(cast.input_operand_id);
  const std::string output = GetOperandNameById(cast.output_operand_id);
  const OperandDataType output_data_type =
      GetOperand(cast.output_operand_id).descriptor.data_type();
  AddCastNode(node, input, output,
              OperandTypeToONNXTensorElementDataType(output_data_type));
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddClampOperation(const mojom::Clamp& clamp) {
  const std::string node = GenerateNextOperationName(clamp.label);
  const std::string input = GetOperandNameById(clamp.input_operand_id);
  const std::string output = GetOperandNameById(clamp.output_operand_id);

  const OperandDataType input_data_type =
      GetOperand(clamp.output_operand_id).descriptor.data_type();

  // Min and max are 0-D operands with the same data type of input.

  std::string min;
  std::string max;
  switch (input_data_type) {
    case OperandDataType::kFloat32: {
      ASSIGN_OR_RETURN(min, CreateScalarInitializer(clamp.min_value));
      ASSIGN_OR_RETURN(max, CreateScalarInitializer(clamp.max_value));
      break;
    }
    case OperandDataType::kFloat16: {
      ASSIGN_OR_RETURN(min, CreateScalarInitializer(
                                fp16_ieee_from_fp32_value(clamp.min_value)));
      ASSIGN_OR_RETURN(max, CreateScalarInitializer(
                                fp16_ieee_from_fp32_value(clamp.max_value)));
      break;
    }
    // TODO(https://github.com/shiyi9801/chromium/issues/60): Add other data
    // types support. https://onnx.ai/onnx/operators/onnx__Clip.html
    default:
      NOTREACHED()
          << "[WebNN] Clamp only supports float32 and float16 data type.";
  }

  std::array<const char*, 3> inputs = {input.c_str(), min.c_str(), max.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeClamp, node, inputs, outputs);

  return base::ok();
}

void GraphBuilderOrt::AddConcatOperation(const mojom::Concat& concat) {
  const std::string node = GenerateNextOperationName(concat.label);

  std::vector<std::string> inputs_string;
  inputs_string.reserve(concat.input_operand_ids.size());
  std::vector<const char*> inputs;
  inputs.reserve(concat.input_operand_ids.size());
  for (uint64_t input_operand_id : concat.input_operand_ids) {
    inputs_string.push_back(GetOperandNameById(input_operand_id));
    inputs.push_back(inputs_string.back().c_str());
  }

  const std::string output = GetOperandNameById(concat.output_operand_id);
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"axis", base::checked_cast<int64_t>(concat.axis)));

  model_editor_.AddNode(kOpTypeConcat, node, inputs, outputs,
                        std::move(attributes));
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddConv2dOperation(const mojom::Conv2d& conv2d) {
  const std::string node = GenerateNextOperationName(conv2d.label);
  const std::string input = GetOperandNameById(conv2d.input_operand_id);
  const std::string filter = GetOperandNameById(conv2d.filter_operand_id);
  const std::string output = GetOperandNameById(conv2d.output_operand_id);
  std::vector<const char*> inputs;
  std::string bias;
  if (conv2d.bias_operand_id) {
    bias = GetOperandNameById(conv2d.bias_operand_id.value());
    inputs = {input.c_str(), filter.c_str(), bias.c_str()};
  } else {
    inputs = {input.c_str(), filter.c_str()};
  }
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;

  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(conv2d.dilations->height),
      base::checked_cast<int64_t>(conv2d.dilations->width)};
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"dilations", dilations));

  int64_t group = base::checked_cast<int64_t>(conv2d.groups);
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"group", group));

  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(conv2d.padding->beginning->height),
      base::checked_cast<int64_t>(conv2d.padding->beginning->width),
      base::checked_cast<int64_t>(conv2d.padding->ending->height),
      base::checked_cast<int64_t>(conv2d.padding->ending->width)};
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"pads", pads));

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(conv2d.strides->height),
      base::checked_cast<int64_t>(conv2d.strides->width)};
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"strides", strides));

  switch (conv2d.kind) {
    case mojom::Conv2d::Kind::kDirect:
      model_editor_.AddNode(kOpTypeConv2d, node, inputs, outputs,
                            std::move(attributes));
      break;
    case mojom::Conv2d::Kind::kTransposed:
      const OperandDescriptor& output_descriptor =
          GetOperand(conv2d.output_operand_id).descriptor;
      const std::vector<uint32_t>& output_shape = output_descriptor.shape();
      // Since ONNX Runtime uses the NCHW format， output_shape[2] and
      // output_shape[3] are used here to access the height and width dimensions
      // of the output tensor shape.
      std::array<int64_t, 2> output_size = {
          base::checked_cast<int64_t>(output_shape[2]),
          base::checked_cast<int64_t>(output_shape[3])};
      // According to the ONNX ConvTranspose2d documentation, the shape of the
      // output_padding is calculated as:
      // output_padding[i] = output_shape[i] - stride[i] * (input_size[i] - 1) -
      // ((kernel_shape[i] - 1) * dilations[i] + 1) + pads[start_i] +
      // pads[end_i]
      // https://onnx.ai/onnx/operators/onnx__ConvTranspose.html#summary
      const std::vector<uint32_t>& input_shape =
          GetOperand(conv2d.input_operand_id).descriptor.shape();
      const std::vector<uint32_t>& filter_shape =
          GetOperand(conv2d.filter_operand_id).descriptor.shape();

      const auto output_padding_height =
          base::MakeCheckedNum(output_size[0]) -
          strides[0] * (base::checked_cast<int64_t>(input_shape[2]) - 1) -
          ((base::checked_cast<int64_t>(filter_shape[2]) - 1) * dilations[0] +
           1) +
          pads[0] + pads[2];
      if (!output_padding_height.IsValid()) {
        return NewUnknownError(
            "[WebNN] Failed to calculate the height of output_padding.");
      }

      const auto output_padding_width =
          base::MakeCheckedNum(output_size[1]) -
          strides[1] * (base::checked_cast<int64_t>(input_shape[3]) - 1) -
          ((base::checked_cast<int64_t>(filter_shape[3]) - 1) * dilations[1] +
           1) +
          pads[1] + pads[3];
      if (!output_padding_width.IsValid()) {
        return NewUnknownError(
            "[WebNN] Failed to calculate the width of output_padding.");
      }
      std::array<int64_t, 2> output_padding = {
          output_padding_height.ValueOrDie(),
          output_padding_width.ValueOrDie()};

      // According to the ONNX ConvTranspose2d documentation, `output_padding`
      // is a zero vector if not specified and `pads` will be auto generated if
      // `output_shape` is specified. So we need to calculate the
      // `output_padding` and explicitly set it to ensure that the attributes
      // information is not missing. Since the `pads` attribute has already been
      // set, there is no need to set `output_size` attribute.
      // https://onnx.ai/onnx/operators/onnx__ConvTranspose.html#attributes
      attributes.push_back(model_editor_.CreateAttribute(
          /*name=*/"output_padding", output_padding));

      model_editor_.AddNode(kOpTypeConvTranspose2d, node, inputs, outputs,
                            std::move(attributes));
      break;
  }

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddCumulativeSumOperation(
    const mojom::CumulativeSum& cumulative_sum) {
  const std::string node = GenerateNextOperationName(cumulative_sum.label);
  const std::string input = GetOperandNameById(cumulative_sum.input_operand_id);
  const std::string output =
      GetOperandNameById(cumulative_sum.output_operand_id);

  std::string axis;
  ASSIGN_OR_RETURN(axis, CreateScalarInitializer<int64_t>(
                             base::checked_cast<int64_t>(cumulative_sum.axis)));

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(2);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"exclusive",
      base::checked_cast<int64_t>(cumulative_sum.exclusive)));
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"reverse",
      base::checked_cast<int64_t>(cumulative_sum.reversed)));

  std::array<const char*, 2> inputs = {input.c_str(), axis.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeCumulativeSum, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddEluOperation(const mojom::Elu& elu) {
  const std::string node = GenerateNextOperationName(elu.label);
  std::string input = GetOperandNameById(elu.input_operand_id);
  const std::string output = GetOperandNameById(elu.output_operand_id);

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"alpha", elu.alpha));

  const OperandDescriptor& input_descriptor =
      GetOperand(elu.input_operand_id).descriptor;
  std::vector<uint32_t> input_shape = input_descriptor.shape();
  // Elu only supports 1-D input tensor, so we need to prepend a reshape node to
  // convert the input tensor to 1-D tensor.
  bool need_reshape = input_descriptor.Rank() != 1;
  std::string elu_output = output;
  if (need_reshape) {
    base::CheckedNumeric<uint32_t> checked_input_size =
        std::accumulate(input_shape.begin(), input_shape.end(),
                        base::CheckedNumeric<uint32_t>(1), std::multiplies());
    std::array<uint32_t, 1> new_shape = {checked_input_size.ValueOrDie()};
    ASSIGN_OR_RETURN(input, PrependReshape(input, new_shape));
    elu_output = GenerateNextOperandName();
  }

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {elu_output.c_str()};
  model_editor_.AddNode(kOpTypeElu, node, inputs, outputs,
                        std::move(attributes));

  // Append a reshape node to convert the output tensor back to the original
  // shape.
  if (need_reshape) {
    return AppendReshape(elu_output, output, input_shape);
  }

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddExpandOperation(const mojom::Expand& expand) {
  const std::string node = GenerateNextOperationName(expand.label);
  const std::string input = GetOperandNameById(expand.input_operand_id);
  const std::string output = GetOperandNameById(expand.output_operand_id);

  const OperandDescriptor& output_descriptor =
      GetOperand(expand.output_operand_id).descriptor;
  const std::vector<uint32_t>& output_shape = output_descriptor.shape();
  // Shape is an operand with data type int64, not an attribute.
  std::vector<uint32_t> shape_dims = {
      base::checked_cast<uint32_t>(output_shape.size())};
  std::vector<int64_t> shape_values;
  std::ranges::transform(
      output_shape, std::back_inserter(shape_values),
      [](uint32_t dim) { return static_cast<int64_t>(dim); });
  ASSIGN_OR_RETURN(const std::string shape,
                   CreateInitializer<int64_t>(shape_dims, shape_values));

  std::array<const char*, 2> inputs = {input.c_str(), shape.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeExpand, node, inputs, outputs);

  return base::ok();
}

template <typename DequantizeOrQuantizeLinear>
  requires(
      std::is_same_v<DequantizeOrQuantizeLinear, mojom::DequantizeLinear> ||
      std::is_same_v<DequantizeOrQuantizeLinear, mojom::QuantizeLinear>)
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddDequantizeOrQuantizeLinearOperation(
    std::string_view op_type,
    const DequantizeOrQuantizeLinear& operation) {
  const std::string node = GenerateNextOperationName(operation.label);
  std::string input = GetOperandNameById(operation.input_operand_id);
  std::string scale = GetOperandNameById(operation.scale_operand_id);
  std::string zero_point = GetOperandNameById(operation.zero_point_operand_id);
  std::string output = GetOperandNameById(operation.output_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(operation.input_operand_id).descriptor;
  std::vector<uint32_t> input_shape = input_descriptor.shape();

  const OperandDescriptor& scale_descriptor =
      GetOperand(operation.scale_operand_id).descriptor;
  // ZeroPoint has the same shape as the scale.
  std::vector<uint32_t> scale_shape = scale_descriptor.shape();

  std::optional<int64_t> axis;
  uint32_t not_one_value_dim_count = 0;
  bool found_same_size = false;
  CHECK_LE(scale_shape.size(), input_shape.size());
  for (size_t i = 0; i < scale_shape.size(); i++) {
    if (scale_shape[scale_shape.size() - i - 1] != 1) {
      not_one_value_dim_count++;
      if (scale_shape[scale_shape.size() - i - 1] ==
          input_shape[input_shape.size() - i - 1]) {
        axis = input_shape.size() - i - 1;
        found_same_size = true;
      }
    }
  }
  // TODO(https://github.com/shiyi9801/chromium/issues/139): Consider to add
  // emulation to support multiple axes case, e.g. input shape is [2, 3, 4, 5]
  // and scale shape is [1, 3, 4, 1].
  bool is_per_axis = found_same_size && not_one_value_dim_count == 1;

  std::optional<int64_t> block_size;
  bool need_transpose = false;
  if (scale_shape.empty()) {
    // For per-tensor/layer dequantization the scale is a scalar.
  } else if (not_one_value_dim_count == 0) {
    // The numbers in scale shape are all 1., scale and zeroPoint should be
    // reshaped to a scalar.
    ASSIGN_OR_RETURN(scale, PrependReshape(scale, {}));
    ASSIGN_OR_RETURN(zero_point, PrependReshape(zero_point, {}));
  } else if (is_per_axis) {
    // For per-axis dequantization, scale and zeroPoint must be a 1-D
    // Tensor.
    CHECK(axis.has_value());
    ASSIGN_OR_RETURN(scale, PrependReshape(scale, {input_shape[axis.value()]}));
    ASSIGN_OR_RETURN(zero_point,
                     PrependReshape(zero_point, {input_shape[axis.value()]}));
  } else if (scale_shape.size() == input_shape.size()) {
    // For blocked dequantization it has the same shape as the input, except for
    // one dimension in which blocking is performed.
    uint32_t blocked_axis_count = 0;
    axis = 0;
    block_size = 1;
    for (size_t i = 0; i < input_shape.size(); i++) {
      if (scale_shape[i] != input_shape[i]) {
        CHECK_EQ(input_shape[i] % scale_shape[i], 0u);
        block_size = input_shape[i] / scale_shape[i];
        axis = i;
        blocked_axis_count++;
        // TODO(https://github.com/shiyi9801/chromium/issues/135): Consider to
        // add emulation to support multi-dimensions blockwise.
        if (blocked_axis_count > 1) {
          return NewNotSupportedError(
              "For blocked dequantization scale has the same shape as the "
              "input or except for one dimension in which blocking is "
              "performed");
        }
      }
    }

    // Currently, OpenVINO only supports axis == 0 when scale.size == 2.
    // https://github.com/openvinotoolkit/openvino/blob/master/src/frontends/onnx/frontend/src/op/dequantize_linear.cpp#L228.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWebNNOrtUseOpenvino) &&
        std::is_same_v<DequantizeOrQuantizeLinear, mojom::DequantizeLinear>) {
      if (scale_shape.size() != 2) {
        // https://github.com/openvinotoolkit/openvino/blob/master/src/frontends/onnx/frontend/src/op/dequantize_linear.cpp#L220
        return NewNotSupportedError(
            "Currently ORT OpenVINO only support 2D scale for block_wise "
            "dequantizeLinear.");
      } else if (axis == 1) {
        input = PrependTranspose(input, {1, 0});
        scale = PrependTranspose(scale, {1, 0});
        zero_point = PrependTranspose(zero_point, {1, 0});
        axis = 0;
        need_transpose = true;
      }
    }
  } else {
    // The proposal of requiring scale and zeroPoint to be the same rank as
    // the input is under discussion-
    // https://github.com/webmachinelearning/webnn/pull/805#discussion_r1919498405
    return NewNotSupportedError(
        "Currently, ONNX only supports per-tensor, per-axis and block-wise "
        "dequantizeLinear and quantizeLinear");
  }

  const std::string transposed_output =
      need_transpose ? GenerateNextOperandName() : output;

  base::FixedArray<const char*> inputs = {input.c_str(), scale.c_str(),
                                          zero_point.c_str()};
  base::FixedArray<const char*> outputs = {transposed_output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  if (axis.has_value()) {
    attributes.push_back(model_editor_.CreateAttribute(
        /*name=*/"axis", base::checked_cast<int64_t>(axis.value())));
  }

  if (block_size.has_value()) {
    attributes.push_back(model_editor_.CreateAttribute(
        /*name=*/"block_size",
        base::checked_cast<int64_t>(block_size.value())));
  }

  model_editor_.AddNode(op_type, node, inputs, outputs, std::move(attributes));

  if (need_transpose) {
    AppendTranspose(transposed_output, output, {1, 0});
  }
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddGatherOperation(const mojom::Gather& gather) {
  const std::string node = GenerateNextOperationName(gather.label);
  const std::string input = GetOperandNameById(gather.input_operand_id);
  const std::string indices = GetOperandNameById(gather.indices_operand_id);
  const std::string output = GetOperandNameById(gather.output_operand_id);

  // Clamp the indices operand to ensure it won't be out-of-bound.
  base::span<const uint32_t> input_shape =
      GetOperand(gather.input_operand_id).descriptor.shape();
  const OperandDataType indices_data_type =
      GetOperand(gather.indices_operand_id).descriptor.data_type();
  ASSIGN_OR_RETURN(
      std::string clamped_indices,
      ClampIndices(indices, indices_data_type, input_shape.at(gather.axis)));

  int64_t axis = static_cast<int64_t>(gather.axis);
  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"axis", axis));

  std::array<const char*, 2> inputs = {input.c_str(), clamped_indices.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeGather, node, inputs, outputs,
                        std::move(attributes));
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddGatherElementsOperation(
    const mojom::GatherElements& gather_elements) {
  const std::string node = GenerateNextOperationName(gather_elements.label);
  const std::string input =
      GetOperandNameById(gather_elements.input_operand_id);
  const std::string indices =
      GetOperandNameById(gather_elements.indices_operand_id);
  const std::string output =
      GetOperandNameById(gather_elements.output_operand_id);

  // Clamp the indices operand to ensure it won't be out-of-bound.
  base::span<const uint32_t> input_shape =
      GetOperand(gather_elements.input_operand_id).descriptor.shape();
  const OperandDataType indices_data_type =
      GetOperand(gather_elements.indices_operand_id).descriptor.data_type();
  ASSIGN_OR_RETURN(std::string clamped_indices,
                   ClampIndices(indices, indices_data_type,
                                input_shape.at(gather_elements.axis)));

  int64_t axis = static_cast<int64_t>(gather_elements.axis);
  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"axis", axis));

  std::array<const char*, 2> inputs = {input.c_str(), clamped_indices.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeGatherElements, node, inputs, outputs,
                        std::move(attributes));
  return base::ok();
}

void GraphBuilderOrt::AddGatherNDOperation(const mojom::GatherND& gather_nd) {
  const std::string node = GenerateNextOperationName(gather_nd.label);
  const std::string input = GetOperandNameById(gather_nd.input_operand_id);
  const std::string indices = GetOperandNameById(gather_nd.indices_operand_id);
  const std::string output = GetOperandNameById(gather_nd.output_operand_id);

  std::string int64_indices;
  const OperandDataType indices_data_type =
      GetOperand(gather_nd.indices_operand_id).descriptor.data_type();

  // TODO(https://github.com/shiyi9801/chromium/issues/141): Clamp the indices
  // operand to ensure it won't be out-of-bound.

  // ONNX GatherND only supports int64 indices.
  switch (indices_data_type) {
    case OperandDataType::kInt64: {
      int64_indices = indices;
      break;
    }
    case OperandDataType::kInt32:
    case OperandDataType::kUint32: {
      int64_indices = GenerateNextOperandName();
      AppendCast(
          indices, int64_indices,
          ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
      break;
    }
    default:
      NOTREACHED()
          << "[WebNN] GatherND only supports int32, uint32 and int64 indices.";
  }

  std::array<const char*, 2> inputs = {input.c_str(), int64_indices.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeGatherND, node, inputs, outputs);
}

void GraphBuilderOrt::AddGemmOperation(const mojom::Gemm& gemm) {
  const std::string node = GenerateNextOperationName(gemm.label);
  const std::string input_a = GetOperandNameById(gemm.a_operand_id);
  const std::string input_b = GetOperandNameById(gemm.b_operand_id);
  const std::string output = GetOperandNameById(gemm.output_operand_id);

  std::vector<const char*> inputs;
  std::string input_c;
  if (gemm.c_operand_id.has_value()) {
    input_c = GetOperandNameById(gemm.c_operand_id.value());
    inputs = {input_a.c_str(), input_b.c_str(), input_c.c_str()};
  } else {
    inputs = {input_a.c_str(), input_b.c_str()};
  }
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(4);
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"alpha", gemm.alpha));
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"beta", gemm.beta));
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"transA", static_cast<int64_t>(gemm.a_transpose)));
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"transB", static_cast<int64_t>(gemm.b_transpose)));

  model_editor_.AddNode(kOpTypeGemm, node, inputs, outputs,
                        std::move(attributes));
}

// `GruType` must be `mojom::Gru` or `mojom::GruCell`.
template <typename GruType>
  requires(std::is_same_v<GruType, mojom::Gru> ||
           std::is_same_v<GruType, mojom::GruCell>)
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddGruOperation(const GruType& gru) {
  const std::string node = GenerateNextOperationName(gru.label);
  std::string input = GetOperandNameById(gru.input_operand_id);
  std::string weight = GetOperandNameById(gru.weight_operand_id);
  std::string recurrent_weight =
      GetOperandNameById(gru.recurrent_weight_operand_id);
  if constexpr (std::is_same_v<GruType, mojom::GruCell>) {
    const std::vector<uint32_t>& input_shape =
        GetOperand(gru.input_operand_id).descriptor.shape();
    CHECK_EQ(input_shape.size(), 2u);
    ASSIGN_OR_RETURN(
        input,
        // Reshape the input into a 3-D tensor, since the GRU of ONNX requires
        // the input shape to be [seq_length, batch_size, input_size]. For
        // gruCell, `seq_length` is equal to 1.
        PrependReshape(input, {1, input_shape[0], input_shape[1]}));
    const std::vector<uint32_t>& weight_shape =
        GetOperand(gru.weight_operand_id).descriptor.shape();
    CHECK_EQ(weight_shape.size(), 2u);
    ASSIGN_OR_RETURN(
        weight,
        // Reshape the weight into a 3-D tensor, since the GRU of ONNX requires
        // the weight shape to be [num_directions, 3*hidden_size, input_size].
        // For gruCell, `num_directions` is equal to 1.
        PrependReshape(weight, {1, weight_shape[0], weight_shape[1]}));
    const std::vector<uint32_t>& recurrent_weight_shape =
        GetOperand(gru.recurrent_weight_operand_id).descriptor.shape();
    CHECK_EQ(recurrent_weight_shape.size(), 2u);
    ASSIGN_OR_RETURN(
        recurrent_weight,
        // Reshape the recurrentWeight into a 3-D tensor, since the GRU of ONNX
        // requires the recurrent weight shape to be [num_directions,
        // 3*hidden_size, hidden_size]. For gruCell, `num_directions` is equal
        // to 1.
        PrependReshape(recurrent_weight, {1, recurrent_weight_shape[0],
                                          recurrent_weight_shape[1]}));
  }
  std::vector<const char*> inputs = {input.c_str(), weight.c_str(),
                                     recurrent_weight.c_str()};

  uint32_t num_directions = 1;
  if constexpr (std::is_same_v<GruType, mojom::Gru>) {
    num_directions =
        gru.direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;
  }
  const uint32_t hidden_size = gru.hidden_size;
  // Already checked that hidden_size * 3 is safe in the model validation.
  uint32_t checked_three_times_hidden_size = hidden_size * 3;
  std::vector<uint32_t> bias_dims = {num_directions,
                                     checked_three_times_hidden_size};
  if (!gru.bias_operand_id.has_value() &&
      !gru.recurrent_bias_operand_id.has_value()) {
    // When both bias and currentBias are not present, set ONNX gru input "B" as
    // not specified.
    inputs.push_back("");
  } else {
    const OperandDataType input_data_type =
        GetOperand(gru.input_operand_id).descriptor.data_type();
    ASSIGN_OR_RETURN(
        const std::string bias,
        CreateOrReshapeBias(gru.bias_operand_id, input_data_type, bias_dims));
    ASSIGN_OR_RETURN(const std::string recurrent_bias,
                     CreateOrReshapeBias(gru.recurrent_bias_operand_id,
                                         input_data_type, bias_dims));
    // Concat bias and recurrent_bias
    std::string concatenated_bias = GenerateNextOperandName();
    std::array<const char*, 2> bias_inputs = {bias.c_str(),
                                              recurrent_bias.c_str()};
    std::array<const char*, 1> bias_outputs = {concatenated_bias.c_str()};
    // The bias tensor of ONNX has shape [num_directions, 6*hidden_size]
    std::vector<ScopedOrtOpAttr> concat_attributes;
    concat_attributes.reserve(1);
    concat_attributes.push_back({model_editor_.CreateAttribute(
        /*name=*/"axis", static_cast<int64_t>(1))});
    model_editor_.AddNode(
        kOpTypeConcat, GenerateNextOperationName("inserted_concat"),
        bias_inputs, bias_outputs, std::move(concat_attributes));
    inputs.push_back(concatenated_bias.c_str());
  }

  // `sequence_lens` is an optional tensor specifying lengths of the sequences
  // in a batch.
  inputs.push_back("");

  std::string hidden_state;
  if constexpr (std::is_same_v<GruType, mojom::Gru>) {
    if (gru.initial_hidden_state_operand_id.has_value()) {
      hidden_state =
          GetOperandNameById(gru.initial_hidden_state_operand_id.value());
    }
  } else {
    hidden_state = GetOperandNameById(gru.hidden_state_operand_id);
    const std::vector<uint32_t>& hidden_state_shape =
        GetOperand(gru.hidden_state_operand_id).descriptor.shape();
    CHECK_EQ(hidden_state_shape.size(), 2u);
    // Reshape the hiddenState into a 3-D tensor, since the GRU of ONNX requires
    // the initial_h shape to be [num_directions, batch_size, hidden_size]. For
    // gruCell, `num_directions` is equal to 1.
    ASSIGN_OR_RETURN(hidden_state,
                     PrependReshape(hidden_state, {1, hidden_state_shape[0],
                                                   hidden_state_shape[1]}));
  }
  inputs.push_back(hidden_state.c_str());

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(4);
  std::string direction = "forward";
  if constexpr (std::is_same_v<GruType, mojom::Gru>) {
    direction = GetRecurrentNetworkDirection(gru.direction);
  }
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"direction", direction.c_str()));

  const std::vector<std::string> activations = GetRecurrentNetworkActivations(
      gru.activations, direction == "bidirectional");
  std::vector<const char*> activations_c_str;
  for (const auto& activation : activations) {
    activations_c_str.push_back(activation.c_str());
  }
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"activations", activations_c_str));

  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"hidden_size", base::checked_cast<int64_t>(hidden_size)));

  // TODO(https://github.com/shiyi9801/chromium/issues/190): Support rzn layout.
  if (gru.layout != mojom::GruWeightLayout::kZrn) {
    return NewNotSupportedError(
        "[WebNN] The gru weight layout (rzn) is not supported.");
  }

  int64_t linear_before_reset = static_cast<int64_t>(gru.reset_after);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"linear_before_reset", linear_before_reset));

  std::string output, output_hidden;
  if constexpr (std::is_same_v<GruType, mojom::Gru>) {
    output_hidden = GetOperandNameById(gru.output_operand_ids[0]);
    if (gru.return_sequence) {
      output = GetOperandNameById(gru.output_operand_ids[1]);
    }
  } else {
    output_hidden = GenerateNextOperandName();
  }
  std::array<const char*, 2> outputs = {output.c_str(), output_hidden.c_str()};
  model_editor_.AddNode(kOpTypeGru, node, inputs, outputs,
                        std::move(attributes));
  if constexpr (std::is_same_v<GruType, mojom::GruCell>) {
    // Reshape the Y_h of shape [num_directions, batch_size, hidden_size] back
    // to a 2-D tensor, since the gruCell of WebNN requires the output shape to
    // be [batchSize, hiddenSize].
    const std::vector<uint32_t>& output_shape =
        GetOperand(gru.output_operand_id).descriptor.shape();
    CHECK_EQ(output_shape.size(), 2u);
    RETURN_IF_ERROR(AppendReshape(output_hidden,
                                  GetOperandNameById(gru.output_operand_id),
                                  output_shape));
  }

  return base::ok();
}

template base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddGruOperation<mojom::Gru>(const mojom::Gru&);

template base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddGruOperation<mojom::GruCell>(const mojom::GruCell&);

void GraphBuilderOrt::AddHardSigmoidOperation(
    const mojom::HardSigmoid& hard_sigmoid) {
  const std::string node = GenerateNextOperationName(hard_sigmoid.label);
  const std::string input = GetOperandNameById(hard_sigmoid.input_operand_id);
  const std::string output = GetOperandNameById(hard_sigmoid.output_operand_id);
  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(2);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"alpha", hard_sigmoid.alpha));
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"beta", hard_sigmoid.beta));

  model_editor_.AddNode(kOpTypeHardSigmoid, node, inputs, outputs,
                        std::move(attributes));
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddInstanceNormalizationOperation(
    const mojom::InstanceNormalization& instance_normalization) {
  const OperandDataType input_data_type =
      GetOperand(instance_normalization.output_operand_id)
          .descriptor.data_type();

  const std::string input =
      GetOperandNameById(instance_normalization.input_operand_id);
  std::vector<const char*> inputs = {input.c_str()};

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

  std::string scale, bias;
  // ONNX requires scale and bias inputs.
  if (instance_normalization.scale_operand_id) {
    scale = GetOperandNameById(instance_normalization.scale_operand_id.value());
    inputs.push_back(scale.c_str());
  } else {
    std::vector<float> scale_data(input_channel, 1.0f);
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> scale_data_fp16(input_channel,
                                              fp16_ieee_from_fp32_value(1.0f));
        ASSIGN_OR_RETURN(
            scale, CreateInitializer<uint16_t>(constant_dims, scale_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        ASSIGN_OR_RETURN(scale,
                         CreateInitializer<float>(constant_dims, scale_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] InstanceNormalization only supports float32 "
                        "and float16 data type.";
    }

    inputs.push_back(scale.c_str());
  }

  if (instance_normalization.bias_operand_id) {
    bias = GetOperandNameById(instance_normalization.bias_operand_id.value());
    inputs.push_back(bias.c_str());
  } else {
    std::vector<float> bias_data(input_channel, 0.0f);
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> bias_data_fp16(input_channel,
                                             fp16_ieee_from_fp32_value(0.0f));
        ASSIGN_OR_RETURN(
            bias, CreateInitializer<uint16_t>(constant_dims, bias_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        ASSIGN_OR_RETURN(bias,
                         CreateInitializer<float>(constant_dims, bias_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] InstanceNormalization only supports float32 "
                        "and float16 data type.";
    }

    inputs.push_back(bias.c_str());
  }

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"epsilon", instance_normalization.epsilon));

  const std::string node =
      GenerateNextOperationName(instance_normalization.label);
  const std::string output =
      GetOperandNameById(instance_normalization.output_operand_id);
  std::array<const char*, 1> outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeInstanceNormalization, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddLayerNormalizationOperation(
    const mojom::LayerNormalization& layer_normalization) {
  const OperandDataType input_data_type =
      GetOperand(layer_normalization.input_operand_id).descriptor.data_type();

  const std::string input =
      GetOperandNameById(layer_normalization.input_operand_id);
  std::vector<const char*> inputs = {input.c_str()};
  const std::vector<uint32_t>& input_shape =
      GetOperand(layer_normalization.input_operand_id).descriptor.shape();

  const std::string node = GenerateNextOperationName(layer_normalization.label);
  const std::string output =
      GetOperandNameById(layer_normalization.output_operand_id);
  std::array<const char*, 1> outputs = {output.c_str()};

  auto axes = layer_normalization.axes;
  // ONNX doesn't support empty axes. When axes is empty, the mean equals to
  // input, output = bias + (scale * 0)
  if (axes.empty()) {
    if (layer_normalization.bias_operand_id) {
      base::CheckedNumeric<uint32_t> checked_input_size =
          std::accumulate(input_shape.begin(), input_shape.end(),
                          base::CheckedNumeric<uint32_t>(1), std::multiplies());
      if (!checked_input_size.IsValid()) {
        return NewNotSupportedError("The size of input is too large.");
      }
      std::string zero;
      switch (input_data_type) {
        case OperandDataType::kFloat16: {
          std::vector<uint16_t> zero_data_fp16(checked_input_size.ValueOrDie(),
                                               fp16_ieee_from_fp32_value(0.0f));
          ASSIGN_OR_RETURN(
              zero, CreateInitializer<uint16_t>(input_shape, zero_data_fp16));
          break;
        }
        case OperandDataType::kFloat32: {
          std::vector<float> zero_data(checked_input_size.ValueOrDie(), 0.0f);
          ASSIGN_OR_RETURN(zero,
                           CreateInitializer<float>(input_shape, zero_data));
          break;
        }
        default:
          NOTREACHED() << "[WebNN] InstanceNormalization only supports float32 "
                          "and float16 data type.";
      }
      const std::string bias =
          GetOperandNameById(layer_normalization.bias_operand_id.value());
      std::array<const char*, 2> binary_inputs = {bias.c_str(), zero.c_str()};
      model_editor_.AddNode(kOpTypeAdd, node, binary_inputs, outputs);
    } else {
      std::array<const char*, 2> binary_inputs = {input.c_str(), input.c_str()};
      model_editor_.AddNode(kOpTypeSub, node, binary_inputs, outputs);
    }
    return base::ok();
  }

  // TODO: crbug.com/356905058: Figure out if unordered axes should be allowed.
  if (!std::ranges::is_sorted(axes)) {
    return NewNotSupportedError("Axes must be ordered for layerNormalization.");
  }
  const auto axes_size = axes.size();
  // Here we only check beginning and ending of the ascending sorted axes,
  // because the blink validation code ensures axes not having duplicated
  // values.
  // TODO: support inconsecutive axes by emulation -
  // https://github.com/shiyi9801/chromium/issues/69.
  if (axes[axes_size - 1] != input_shape.size() - 1 ||
      axes[0] != input_shape.size() - axes_size) {
    return NewNotSupportedError(
        "ONNX LayerNormalization only supports last consecutive dimensions "
        "as axes.");
  }
  uint32_t axis = axes[0];
  std::string scale;
  base::CheckedNumeric<uint32_t> checked_scale_size =
      std::accumulate(input_shape.begin() + axis, input_shape.end(),
                      base::CheckedNumeric<uint32_t>(1), std::multiplies());
  if (!checked_scale_size.IsValid()) {
    return NewNotSupportedError("The size of scale is too large.");
  }

  std::vector<uint32_t> scale_dims;
  scale_dims.reserve(axes_size);
  std::ranges::transform(
      axes, std::back_inserter(scale_dims),
      [&input_shape](uint32_t axis) { return input_shape[axis]; });

  if (layer_normalization.scale_operand_id) {
    scale = GetOperandNameById(layer_normalization.scale_operand_id.value());
    inputs.push_back(scale.c_str());
  } else {
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> scale_data_fp16(checked_scale_size.ValueOrDie(),
                                              fp16_ieee_from_fp32_value(1.0f));
        ASSIGN_OR_RETURN(
            scale, CreateInitializer<uint16_t>(scale_dims, scale_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        std::vector<float> scale_data(checked_scale_size.ValueOrDie(), 1.0f);
        ASSIGN_OR_RETURN(scale,
                         CreateInitializer<float>(scale_dims, scale_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] LayerNormalization only supports float32 "
                        "and float16 data type.";
    }

    inputs.push_back(scale.c_str());
  }

  std::string bias;
  if (layer_normalization.bias_operand_id) {
    bias = GetOperandNameById(layer_normalization.bias_operand_id.value());
    inputs.push_back(bias.c_str());
  }

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(2);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"axis", base::checked_cast<int64_t>(axis)));
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"epsilon", layer_normalization.epsilon));

  model_editor_.AddNode(kOpTypeLayerNormalization, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

void GraphBuilderOrt::AddLeakyReluOperation(
    const mojom::LeakyRelu& leaky_relu) {
  const std::string node = GenerateNextOperationName(leaky_relu.label);
  const std::string input = GetOperandNameById(leaky_relu.input_operand_id);
  const std::string output = GetOperandNameById(leaky_relu.output_operand_id);

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"alpha", leaky_relu.alpha));
  model_editor_.AddNode(kOpTypeLeakyRelu, node, inputs, outputs,
                        std::move(attributes));
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddLinearOperation(const mojom::Linear& linear) {
  // Emulate a linear operation whose calculation follows the expression `alpha
  // * x + beta`.
  const mojom::Operand& input_operand = GetOperand(linear.input_operand_id);
  const OperandDataType input_data_type = input_operand.descriptor.data_type();

  std::string alpha;
  switch (input_data_type) {
    case OperandDataType::kFloat16: {
      ASSIGN_OR_RETURN(alpha, CreateScalarInitializer<uint16_t>(
                                  fp16_ieee_from_fp32_value(linear.alpha)));
      break;
    }
    case OperandDataType::kFloat32: {
      ASSIGN_OR_RETURN(alpha, CreateScalarInitializer<float>(linear.alpha));
      break;
    }
    default:
      NOTREACHED()
          << "[WebNN] Linear only supports float32 and float16 data type.";
  }
  const std::string linear_mul_label =
      base::JoinString({linear.label, "mul"}, kUnderscore);
  const std::string mul_node = GenerateNextOperationName(linear_mul_label);
  const std::string input = GetOperandNameById(linear.input_operand_id);
  std::array<const char*, 2> mul_inputs = {input.c_str(), alpha.c_str()};
  const std::string mul_output = GenerateNextOperandName();
  std::array<const char*, 1> mul_outputs = {mul_output.c_str()};
  model_editor_.AddNode(kOpTypeMul, mul_node, mul_inputs, mul_outputs);

  std::string beta;
  switch (input_data_type) {
    case OperandDataType::kFloat16: {
      ASSIGN_OR_RETURN(beta, CreateScalarInitializer<uint16_t>(
                                 fp16_ieee_from_fp32_value(linear.beta)));
      break;
    }
    case OperandDataType::kFloat32: {
      ASSIGN_OR_RETURN(beta, CreateScalarInitializer<float>(linear.beta));
      break;
    }
    default:
      NOTREACHED()
          << "[WebNN] Linear only supports float32 and float16 data type.";
  }
  const std::string linear_add_label =
      base::JoinString({linear.label, "add"}, kUnderscore);
  const std::string add_node = GenerateNextOperationName(linear_add_label);
  std::array<const char*, 2> add_inputs = {mul_output.c_str(), beta.c_str()};
  const std::string output = GetOperandNameById(linear.output_operand_id);
  std::array<const char*, 1> outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeAdd, add_node, add_inputs, outputs);

  return base::ok();
}

template <typename LstmType>
  requires(std::is_same_v<LstmType, mojom::Lstm> ||
           std::is_same_v<LstmType, mojom::LstmCell>)
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddLstmOperation(const LstmType& lstm) {
  const std::string node = GenerateNextOperationName(lstm.label);
  std::string input = GetOperandNameById(lstm.input_operand_id);
  std::string weight = GetOperandNameById(lstm.weight_operand_id);
  std::string recurrent_weight =
      GetOperandNameById(lstm.recurrent_weight_operand_id);
  if constexpr (std::is_same_v<LstmType, mojom::LstmCell>) {
    const std::vector<uint32_t>& input_shape =
        GetOperand(lstm.input_operand_id).descriptor.shape();
    CHECK_EQ(input_shape.size(), 2u);
    ASSIGN_OR_RETURN(
        input,
        // Reshape the input into a 3-D tensor, since the LSTM of ONNX requires
        // the input shape to be [seq_length, batch_size, input_size]. For
        // lstmCell, `seq_length` is equal to 1.
        PrependReshape(input, {1, input_shape[0], input_shape[1]}));
    const std::vector<uint32_t>& weight_shape =
        GetOperand(lstm.weight_operand_id).descriptor.shape();
    CHECK_EQ(weight_shape.size(), 2u);
    ASSIGN_OR_RETURN(
        weight,
        // Reshape the weight into a 3-D tensor, since the LSTM of ONNX requires
        // the weight shape to be [num_directions, 4*hidden_size, input_size].
        // For lstmCell, `num_directions` is equal to 1.
        PrependReshape(weight, {1, weight_shape[0], weight_shape[1]}));
    const std::vector<uint32_t>& recurrent_weight_shape =
        GetOperand(lstm.recurrent_weight_operand_id).descriptor.shape();
    CHECK_EQ(recurrent_weight_shape.size(), 2u);
    ASSIGN_OR_RETURN(
        recurrent_weight,
        // Reshape the recurrentWeight into a 3-D tensor, since the LSTM of ONNX
        // requires the recurrent weight shape to be [num_directions,
        // 4*hidden_size, hidden_size]. For lstmCell, `num_directions` is equal
        // to 1.
        PrependReshape(recurrent_weight, {1, recurrent_weight_shape[0],
                                          recurrent_weight_shape[1]}));
  }
  std::vector<const char*> inputs = {input.c_str(), weight.c_str(),
                                     recurrent_weight.c_str()};
  uint32_t num_directions = 1;
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    num_directions =
        lstm.direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;
  }
  const uint32_t hidden_size = lstm.hidden_size;
  // Already checked that hidden_size * 4 is safe in the model validation.
  uint32_t checked_four_times_hidden_size = hidden_size * 4;
  std::vector<uint32_t> bias_dims = {num_directions,
                                     checked_four_times_hidden_size};
  if (!lstm.bias_operand_id.has_value() &&
      !lstm.recurrent_bias_operand_id.has_value()) {
    // When both bias and currentBias are not present, set ONNX LSTM input "B"
    // as not specified.
    inputs.push_back("");
  } else {
    const OperandDataType input_data_type =
        GetOperand(lstm.input_operand_id).descriptor.data_type();
    ASSIGN_OR_RETURN(
        const std::string bias,
        CreateOrReshapeBias(lstm.bias_operand_id, input_data_type, bias_dims));
    ASSIGN_OR_RETURN(const std::string recurrent_bias,
                     CreateOrReshapeBias(lstm.recurrent_bias_operand_id,
                                         input_data_type, bias_dims));
    // Concat bias and recurrent_bias
    std::string concatenated_bias = GenerateNextOperandName();
    std::array<const char*, 2> bias_inputs = {bias.c_str(),
                                              recurrent_bias.c_str()};
    std::array<const char*, 1> bias_outputs = {concatenated_bias.c_str()};
    // The bias tensor of ONNX has shape [num_directions, 8*hidden_size]
    std::vector<ScopedOrtOpAttr> concat_attributes;
    concat_attributes.reserve(1);
    concat_attributes.push_back({model_editor_.CreateAttribute(
        /*name=*/"axis", static_cast<int64_t>(1))});
    model_editor_.AddNode(
        kOpTypeConcat, GenerateNextOperationName("inserted_concat"),
        bias_inputs, bias_outputs, std::move(concat_attributes));
    inputs.push_back(concatenated_bias.c_str());
  }

  // `sequence_lens` is an optional tensor specifying lengths of the sequences
  // in a batch.
  inputs.push_back("");

  std::string hidden_state, cell_state;
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    if (lstm.initial_hidden_state_operand_id.has_value()) {
      hidden_state =
          GetOperandNameById(lstm.initial_hidden_state_operand_id.value());
    }
    if (lstm.initial_cell_state_operand_id.has_value()) {
      cell_state =
          GetOperandNameById(lstm.initial_cell_state_operand_id.value());
    }
  } else {
    hidden_state = GetOperandNameById(lstm.hidden_state_operand_id);
    const std::vector<uint32_t>& hidden_state_shape =
        GetOperand(lstm.hidden_state_operand_id).descriptor.shape();
    CHECK_EQ(hidden_state_shape.size(), 2u);
    // Reshape the hiddenState into a 3-D tensor, since the LSTM of ONNX
    // requires the initial_h shape to be [num_directions, batch_size,
    // hidden_size]. For lstmCell, `num_directions` is equal to 1.
    ASSIGN_OR_RETURN(hidden_state,
                     PrependReshape(hidden_state, {1, hidden_state_shape[0],
                                                   hidden_state_shape[1]}));

    cell_state = GetOperandNameById(lstm.cell_state_operand_id);
    const std::vector<uint32_t>& cell_state_shape =
        GetOperand(lstm.cell_state_operand_id).descriptor.shape();
    CHECK_EQ(cell_state_shape.size(), 2u);
    // Reshape the cellState into a 3-D tensor, since the LSTM of ONNX
    // requires the initial_c shape to be [num_directions, batch_size,
    // hidden_size]. For lstmCell, `num_directions` is equal to 1.
    ASSIGN_OR_RETURN(cell_state,
                     PrependReshape(cell_state, {1, cell_state_shape[0],
                                                 cell_state_shape[1]}));
  }
  inputs.push_back(hidden_state.c_str());
  inputs.push_back(cell_state.c_str());

  std::string peephole_weight;
  if (lstm.peephole_weight_operand_id.has_value()) {
    peephole_weight =
        GetOperandNameById(lstm.peephole_weight_operand_id.value());
    if constexpr (std::is_same_v<LstmType, mojom::LstmCell>) {
      // Reshape the peepholeWeight into a 2-D tensor, since the LSTM of ONNX
      // requires the peephole shape to be [num_directions, 3*hidden_size]. For
      // lstmCell, `num_directions` is equal to 1.
      const std::vector<uint32_t>& peephole_weight_shape =
          GetOperand(lstm.peephole_weight_operand_id.value())
              .descriptor.shape();
      CHECK_EQ(peephole_weight_shape.size(), 1u);
      ASSIGN_OR_RETURN(
          peephole_weight,
          PrependReshape(peephole_weight, {1, peephole_weight_shape[0]}));
    }
  }
  inputs.push_back(peephole_weight.c_str());

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(3);
  std::string direction = "forward";
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    direction = GetRecurrentNetworkDirection(lstm.direction);
  }
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"direction", direction.c_str()));

  const std::vector<std::string> activations = GetRecurrentNetworkActivations(
      lstm.activations, direction == "bidirectional");
  std::vector<const char*> activations_c_str;
  for (const auto& activation : activations) {
    activations_c_str.push_back(activation.c_str());
  }
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"activations", activations_c_str));

  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"hidden_size", base::checked_cast<int64_t>(hidden_size)));

  // TODO(https://github.com/shiyi9801/chromium/issues/195): Support ifgo
  // layout.
  if (lstm.layout != mojom::LstmWeightLayout::kIofg) {
    return NewNotSupportedError(
        "[WebNN] The lstm weight layout (ifgo) is not supported.");
  }

  std::string output, output_hidden, output_cell;
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    CHECK_GE(lstm.output_operand_ids.size(), 2u);
    output_hidden = GetOperandNameById(lstm.output_operand_ids[0]);
    output_cell = GetOperandNameById(lstm.output_operand_ids[1]);
    if (lstm.return_sequence) {
      CHECK_EQ(lstm.output_operand_ids.size(), 3u);
      output = GetOperandNameById(lstm.output_operand_ids[2]);
    }
  } else {
    output_hidden = GenerateNextOperandName();
    output_cell = GenerateNextOperandName();
  }
  std::array<const char*, 3> outputs = {output.c_str(), output_hidden.c_str(),
                                        output_cell.c_str()};
  model_editor_.AddNode(kOpTypeLstm, node, inputs, outputs,
                        std::move(attributes));
  if constexpr (std::is_same_v<LstmType, mojom::LstmCell>) {
    // Reshape the Y_h and Y_c of shape [num_directions, batch_size,
    // hidden_size] back to a 2-D tensor, since the lstmCell of WebNN requires
    // the output shapes to be [batchSize, hiddenSize].
    const std::vector<uint32_t>& output_shape =
        GetOperand(lstm.output_operand_ids[0]).descriptor.shape();
    CHECK_EQ(output_shape.size(), 2u);
    CHECK_EQ(lstm.output_operand_ids.size(), 2u);
    RETURN_IF_ERROR(AppendReshape(
        output_hidden, GetOperandNameById(lstm.output_operand_ids[0]),
        output_shape));
    RETURN_IF_ERROR(AppendReshape(
        output_cell, GetOperandNameById(lstm.output_operand_ids[1]),
        output_shape));
  }

  return base::ok();
}

void GraphBuilderOrt::AddMatMulOperation(const mojom::Matmul& matmul) {
  const std::string node = GenerateNextOperationName(matmul.label);
  const std::string input_a = GetOperandNameById(matmul.a_operand_id);
  const std::string input_b = GetOperandNameById(matmul.b_operand_id);
  const std::string output = GetOperandNameById(matmul.output_operand_id);

  std::array<const char*, 2> inputs = {input_a.c_str(), input_b.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeMatMul, node, inputs, outputs);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddPadOperation(const mojom::Pad& pad) {
  const std::string node = GenerateNextOperationName(pad.label);
  const std::string input = GetOperandNameById(pad.input_operand_id);
  const OperandDataType input_data_type =
      GetOperand(pad.output_operand_id).descriptor.data_type();
  std::vector<const char*> inputs = {input.c_str()};

  CHECK_EQ(pad.beginning_padding.size(), pad.ending_padding.size());
  auto padding_length =
      pad.beginning_padding.size() + pad.ending_padding.size();

  // paddings is an operand with data type int64, not an attribute.
  std::vector<int64_t> paddings_value;
  paddings_value.reserve(padding_length);
  std::ranges::transform(
      pad.beginning_padding, std::back_inserter(paddings_value),
      [](uint32_t value) { return base::checked_cast<int64_t>(value); });
  std::ranges::transform(
      pad.ending_padding, std::back_inserter(paddings_value),
      [](uint32_t value) { return base::checked_cast<int64_t>(value); });

  std::vector<uint32_t> paddings_dims = {
      base::checked_cast<uint32_t>(padding_length)};
  ASSIGN_OR_RETURN(const std::string paddings,
                   CreateInitializer<int64_t>(paddings_dims, paddings_value));
  inputs.push_back(paddings.c_str());

  std::string mode;
  std::string constant;
  switch (pad.mode->which()) {
    case mojom::PaddingMode::Tag::kConstant: {
      mode = "constant";
      auto constant_value = pad.mode->get_constant()->value;
      switch (input_data_type) {
        case OperandDataType::kFloat32: {
          ASSIGN_OR_RETURN(constant, CreateScalarInitializer(constant_value));
          break;
        }
        case OperandDataType::kFloat16: {
          ASSIGN_OR_RETURN(constant,
                           CreateScalarInitializer(
                               fp16_ieee_from_fp32_value(constant_value)));
          break;
        }
        default:
          NOTREACHED() << "[WebNN] Pad only supports float32 "
                          "and float16 data type.";
      }
      inputs.push_back(constant.c_str());
      break;
    }
    case mojom::PaddingMode::Tag::kSymmetric:
      // TODO: Support Symmetric mode-
      // https://github.com/shiyi9801/chromium/issues/80.
      return NewNotSupportedError("Unsupported mode symmetric for pad.");
    case mojom::PaddingMode::Tag::kEdge:
      mode = "edge";
      break;
    case mojom::PaddingMode::Tag::kReflection:
      mode = "reflect";
      break;
  }

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"mode", mode));

  const std::string output = GetOperandNameById(pad.output_operand_id);
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypePad, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

void GraphBuilderOrt::AddPool2dOperation(const mojom::Pool2d& pool2d) {
  std::vector<ScopedOrtOpAttr> attributes;

  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(pool2d.dilations->height),
      base::checked_cast<int64_t>(pool2d.dilations->width)};
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"dilations", dilations));

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(pool2d.strides->height),
      base::checked_cast<int64_t>(pool2d.strides->width)};
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"strides", strides));

  std::array<int64_t, 2> window_dimensions = {
      base::checked_cast<int64_t>(pool2d.window_dimensions->height),
      base::checked_cast<int64_t>(pool2d.window_dimensions->width)};
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"kernel_shape", window_dimensions));

  // ONNX's pads are [beginning_height, beginning_width, ending_height,
  // ending_width]
  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(pool2d.padding->beginning->height),
      base::checked_cast<int64_t>(pool2d.padding->beginning->width),
      base::checked_cast<int64_t>(pool2d.padding->ending->height),
      base::checked_cast<int64_t>(pool2d.padding->ending->width)};
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"pads", pads));

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
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"ceil_mode", ceil_mode));

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
      attributes.push_back(
          model_editor_.CreateAttribute(/*name=*/"p", static_cast<int64_t>(2)));
      break;
    }
  }

  const std::string node = GenerateNextOperationName(pool2d.label);
  const std::string input = GetOperandNameById(pool2d.input_operand_id);
  const std::string output = GetOperandNameById(pool2d.output_operand_id);
  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(op_type, node, inputs, outputs, std::move(attributes));
}

void GraphBuilderOrt::AddPreluOperation(const mojom::Prelu& prelu) {
  const std::string node = GenerateNextOperationName(prelu.label);
  const std::string input = GetOperandNameById(prelu.input_operand_id);
  // ONNX Prelu requires slope's shape must be unidirectional broadcastable to
  // input when the shape of slope is smaller than the input. While WebNN allows
  // input and slope to be bidirectionally broadcastable.
  // TODO(https://github.com/shiyi9801/chromium/issues/153): Consider to emulate
  // if slope is not unidirectional broadcastable to input.
  const std::string slope = GetOperandNameById(prelu.slope_operand_id);
  const std::string output = GetOperandNameById(prelu.output_operand_id);

  std::array<const char*, 2> inputs = {input.c_str(), slope.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypePRelu, node, inputs, outputs);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddReduceOperation(const mojom::Reduce& reduce) {
  const std::string input = GetOperandNameById(reduce.input_operand_id);
  const std::string output = GetOperandNameById(reduce.output_operand_id);
  std::vector<const char*> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  // According to
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-reduce,
  // if axes is empty,
  // WebNN applies reduction function to each value in the tensor individually
  // with no dimensions are reduced, but ONNX reduction operations either
  // reduces all dimensions or act as noop. So we need to emulate the behavior
  // of reducing each value individually:
  // 1. insert a log operation for reduceLogSum
  // 2. insert a pow operation for reduceSumSquare
  // 3. insert a abs operation for reduceL1, reduceL2
  // For other reduction operations like reuduceMin, reuduceSum, reducing each
  // value individually is equivalent to noop.
  std::vector<int64_t> axes_value(reduce.axes.begin(), reduce.axes.end());
  if (axes_value.empty()) {
    if (reduce.kind == mojom::Reduce::Kind::kLogSum) {
      const std::string log_node = GenerateNextOperationName("inserted_log");
      model_editor_.AddNode(kOpTypeLog, log_node, inputs, outputs);
      return base::ok();
    } else if (reduce.kind == mojom::Reduce::Kind::kSumSquare) {
      const std::string pow_node = GenerateNextOperationName("inserted_pow");
      ASSIGN_OR_RETURN(const std::string pow,
                       CreateScalarInitializer<int64_t>(2));
      std::array<const char*, 2> pow_inputs = {input.c_str(), pow.c_str()};
      model_editor_.AddNode(kOpTypePow, pow_node, pow_inputs, outputs);
      return base::ok();
    } else if (reduce.kind == mojom::Reduce::Kind::kL1 ||
               reduce.kind == mojom::Reduce::Kind::kL2) {
      const std::string abs_node = GenerateNextOperationName("inserted_abs");
      model_editor_.AddNode(kOpTypeAbs, abs_node, inputs, outputs);
      return base::ok();
    }
  }

  std::string axes;
  // axes is an operand with data type int64, not an attribute.
  std::vector<uint32_t> axes_dims = {
      base::checked_cast<uint32_t>(axes_value.size())};
  ASSIGN_OR_RETURN(axes, CreateInitializer<int64_t>(axes_dims, axes_value));
  inputs.push_back(axes.c_str());

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(2);

  int64_t keepdims = reduce.keep_dimensions ? 1 : 0;
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"keepdims", keepdims));

  int64_t noop_with_empty_axes = 1;
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"noop_with_empty_axes", noop_with_empty_axes));

  const std::string node = GenerateNextOperationName(reduce.label);
  std::string reduce_op_type = MapReduceKindToOrtOpType(reduce.kind);
  model_editor_.AddNode(reduce_op_type, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddResample2dOperation(const mojom::Resample2d& resample2d) {
  const std::string node = GenerateNextOperationName(resample2d.label);
  const std::string input = GetOperandNameById(resample2d.input_operand_id);
  const std::string output = GetOperandNameById(resample2d.output_operand_id);
  const std::vector<uint32_t>& input_shape =
      GetOperand(resample2d.input_operand_id).descriptor.shape();
  const std::vector<uint32_t>& output_shape =
      GetOperand(resample2d.output_operand_id).descriptor.shape();
  std::vector<const char*> inputs = {input.c_str()};

  // ROI only takes effect when ONNX Resize op's attribute
  // coordinate_transformation_mode is “tf_crop_and_resize” and the default
  // value of coordinate_transformation_mode is "half_pixel". Currently, WebNN
  // only supports "half_pixel".
  const std::string roi = "";
  inputs.push_back(roi.c_str());

  // When axes != [2, 3], webnn blink side will insert transpose before and
  // after resample2d -
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.cc;l=1438.
  CHECK_EQ(resample2d.axes.size(), 2u);
  CHECK_EQ(resample2d.axes[0], 2u);
  CHECK_EQ(resample2d.axes[1], 3u);

  CHECK_EQ(input_shape.size(), 4u);
  std::string scales;
  std::string sizes;
  // Here we using default axes([0,..., R-1]) due to this issue-
  // https://github.com/shiyi9801/chromium/issues/92.
  if (resample2d.scales) {
    // The number of elements of scales should be the same as the rank of input
    // or axes.
    std::array<float, 4> scales_data = {1, 1, resample2d.scales->at(0),
                                        resample2d.scales->at(1)};
    ASSIGN_OR_RETURN(scales, CreateInitializer<float>({4}, scales_data));
    sizes = "";
  } else {
    // The number of elements of sizes should be the same as the rank of input
    // or axes.
    CHECK_EQ(output_shape.size(), 4u);
    std::array<int64_t, 4> sizes_data = {
        base::checked_cast<int64_t>(output_shape[0]),
        base::checked_cast<int64_t>(output_shape[1]),
        base::checked_cast<int64_t>(output_shape[2]),
        base::checked_cast<int64_t>(output_shape[3])};
    ASSIGN_OR_RETURN(sizes, CreateInitializer<int64_t>({4}, sizes_data));
    scales = "";
  }
  inputs.push_back(scales.c_str());
  inputs.push_back(sizes.c_str());

  std::string mode;
  switch (resample2d.mode) {
    case mojom::Resample2d::InterpolationMode::kLinear:
      mode = "linear";
      break;
    case mojom::Resample2d::InterpolationMode::kNearestNeighbor:
      mode = "nearest";
      break;
  }
  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(/*name=*/"mode", mode));

  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeResample2d, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddReshapeOperation(const mojom::Reshape& reshape) {
  const std::string node = GenerateNextOperationName(reshape.label);
  const std::string input = GetOperandNameById(reshape.input_operand_id);
  const std::string output = GetOperandNameById(reshape.output_operand_id);

  const OperandDescriptor& output_descriptor =
      GetOperand(reshape.output_operand_id).descriptor;
  const std::vector<uint32_t>& output_shape = output_descriptor.shape();
  // Shape is an operand with data type int64, not an attribute.
  std::vector<uint32_t> shape_dims = {
      base::checked_cast<uint32_t>(output_shape.size())};
  std::vector<int64_t> shape_values;
  std::ranges::transform(
      output_shape, std::back_inserter(shape_values),
      [](uint32_t dim) { return static_cast<int64_t>(dim); });
  ASSIGN_OR_RETURN(const std::string shape,
                   CreateInitializer<int64_t>(shape_dims, shape_values));

  std::array<const char*, 2> inputs = {input.c_str(), shape.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeReshape, node, inputs, outputs);

  return base::ok();
}

// Emulate it by slice operation since there is no reserve operation in ONNX.
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddReverseOperation(const mojom::Reverse& reverse) {
  const std::string node = GenerateNextOperationName(reverse.label);
  const std::string input = GetOperandNameById(reverse.input_operand_id);
  const std::string output = GetOperandNameById(reverse.output_operand_id);

  // Axes can be empty, which means no dimensions are reversed.
  std::vector<int64_t> reverse_axes(reverse.axes.begin(), reverse.axes.end());
  size_t reverse_axes_size = reverse_axes.size();

  base::FixedArray<int64_t> starts(reverse_axes_size, -1);
  base::FixedArray<int64_t> ends(reverse_axes_size,
                                 std::numeric_limits<int64_t>::min());
  base::FixedArray<int64_t> steps(reverse_axes_size, -1);

  // Axes is an operand with data type int64, not an attribute.
  std::vector<uint32_t> axes_dims = {
      base::checked_cast<uint32_t>(reverse_axes_size)};
  ASSIGN_OR_RETURN(std::string axes,
                   CreateInitializer<int64_t>(axes_dims, reverse_axes));

  return AddSliceNode(node, input, output, axes, starts, ends, steps);
}

void GraphBuilderOrt::AddScatterElementsOperation(
    const mojom::ScatterElements& scatter_elements) {
  const std::string node = GenerateNextOperationName(scatter_elements.label);
  const std::string input =
      GetOperandNameById(scatter_elements.input_operand_id);
  const std::string indices =
      GetOperandNameById(scatter_elements.indices_operand_id);
  const std::string updates =
      GetOperandNameById(scatter_elements.updates_operand_id);
  const std::string output =
      GetOperandNameById(scatter_elements.output_operand_id);

  std::string cast_indices;
  const OperandDataType indices_data_type =
      GetOperand(scatter_elements.indices_operand_id).descriptor.data_type();

  // ONNX ScatterElements only supports int32 and int64 indices.
  switch (indices_data_type) {
    case OperandDataType::kInt32:
    case OperandDataType::kInt64: {
      cast_indices = indices;
      break;
    }
    case OperandDataType::kUint32: {
      cast_indices = GenerateNextOperandName();
      AppendCast(
          indices, cast_indices,
          ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
      break;
    }
    default:
      NOTREACHED() << "[WebNN] ScatterElements only supports int32, uint32 and "
                      "int64 indices.";
  }

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"axis", base::checked_cast<int64_t>(scatter_elements.axis)));

  std::array<const char*, 3> inputs = {input.c_str(), cast_indices.c_str(),
                                       updates.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeScatterElements, node, inputs, outputs,
                        std::move(attributes));
}

void GraphBuilderOrt::AddScatterNDOperation(
    const mojom::ScatterND& scatter_nd) {
  const std::string node = GenerateNextOperationName(scatter_nd.label);
  const std::string input = GetOperandNameById(scatter_nd.input_operand_id);
  const std::string indices = GetOperandNameById(scatter_nd.indices_operand_id);
  const std::string updates = GetOperandNameById(scatter_nd.updates_operand_id);
  const std::string output = GetOperandNameById(scatter_nd.output_operand_id);

  std::string int64_indices;
  const OperandDataType indices_data_type =
      GetOperand(scatter_nd.indices_operand_id).descriptor.data_type();

  // ONNX ScatterND only supports int64 indices.
  switch (indices_data_type) {
    case OperandDataType::kInt64: {
      int64_indices = indices;
      break;
    }
    case OperandDataType::kInt32:
    case OperandDataType::kUint32: {
      int64_indices = GenerateNextOperandName();
      AppendCast(
          indices, int64_indices,
          ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
      break;
    }
    default:
      NOTREACHED()
          << "[WebNN] ScatterND only supports int32, uint32 and int64 indices.";
  }

  std::array<const char*, 3> inputs = {input.c_str(), int64_indices.c_str(),
                                       updates.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeScatterND, node, inputs, outputs);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddSliceOperation(const mojom::Slice& slice) {
  const std::string node = GenerateNextOperationName(slice.label);
  const std::string input = GetOperandNameById(slice.input_operand_id);
  const std::string output = GetOperandNameById(slice.output_operand_id);

  auto range = slice.ranges;
  base::FixedArray<int64_t> starts(slice.ranges.size());
  base::FixedArray<int64_t> ends(slice.ranges.size());
  base::FixedArray<int64_t> steps(slice.ranges.size());
  for (size_t i = 0; i < slice.ranges.size(); ++i) {
    starts[i] = base::checked_cast<int64_t>(slice.ranges[i].start);
    ends[i] = base::checked_cast<int64_t>(slice.ranges[i].start +
                                          slice.ranges[i].size);
    steps[i] = base::checked_cast<int64_t>(slice.ranges[i].stride);
  }

  // Axes is an optional input, if not provided, it is an empty string and will
  // be treated as [0, 1, …, len(starts) - 1]:
  // https://onnx.ai/onnx/operators/onnx__Slice.html#inputs
  const std::string axes = "";
  return AddSliceNode(node, input, output, axes, starts, ends, steps);
}

void GraphBuilderOrt::AddSoftmaxOperation(const mojom::Softmax& softmax) {
  const std::string node = GenerateNextOperationName(softmax.label);
  const std::string input = GetOperandNameById(softmax.input_operand_id);
  const std::string output = GetOperandNameById(softmax.output_operand_id);

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"axis", static_cast<int64_t>(softmax.axis)));

  model_editor_.AddNode(kOpTypeSoftmax, node, inputs, outputs,
                        std::move(attributes));
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddTileOperation(const mojom::Tile& tile) {
  const std::string node = GenerateNextOperationName(tile.label);
  const std::string input = GetOperandNameById(tile.input_operand_id);
  const std::string output = GetOperandNameById(tile.output_operand_id);

  std::vector<int64_t> repetitions(tile.repetitions.begin(),
                                   tile.repetitions.end());
  ASSIGN_OR_RETURN(
      const std::string repeats,
      CreateInitializer<int64_t>(
          {base::checked_cast<uint32_t>(repetitions.size())}, repetitions));

  std::array<const char*, 2> inputs = {input.data(), repeats.data()};
  std::array<const char*, 1> outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeTile, node, inputs, outputs);

  return base::ok();
}

void GraphBuilderOrt::AddTransposeOperation(const mojom::Transpose& transpose) {
  const std::string node = GenerateNextOperationName(transpose.label);
  const std::string input = GetOperandNameById(transpose.input_operand_id);
  const std::string output = GetOperandNameById(transpose.output_operand_id);

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  std::vector<int64_t> permutation(transpose.permutation.begin(),
                                   transpose.permutation.end());
  attributes.push_back(
      model_editor_.CreateAttribute(/*name=*/"perm", permutation));

  model_editor_.AddNode(kOpTypeTranspose, node, inputs, outputs,
                        std::move(attributes));
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddSplitOperation(const mojom::Split& split) {
  const std::string node = GenerateNextOperationName(split.label);
  const std::string input = GetOperandNameById(split.input_operand_id);

  const auto output_nums = split.output_operand_ids.size();
  // 'split' is a optional input which specifies the length of each output. Sum
  // of the values must be equal to the dim value at 'axis' specified. Notes
  // that either input 'split' or the attribute 'num_outputs' should be
  // specified, but not both.
  base::FixedArray<int64_t> split_sizes(output_nums);
  for (size_t i = 0; i < output_nums; i++) {
    const std::vector<uint32_t>& output_shape =
        GetOperand(split.output_operand_ids[i]).descriptor.shape();
    CHECK_LT(split.axis, output_shape.size());
    split_sizes[i] = base::checked_cast<int64_t>(output_shape[split.axis]);
  }
  ASSIGN_OR_RETURN(
      const std::string split_input,
      CreateInitializer<int64_t>(
          {base::checked_cast<uint32_t>(split_sizes.size())}, split_sizes));
  base::FixedArray<const char*> inputs = {input.c_str(), split_input.c_str()};

  base::FixedArray<std::string> outputs_string(output_nums);
  base::FixedArray<const char*> outputs(output_nums);
  for (size_t i = 0; i < output_nums; i++) {
    outputs_string[i] = GetOperandNameById(split.output_operand_ids[i]);
    outputs[i] = outputs_string[i].c_str();
  }

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"axis", base::checked_cast<int64_t>(split.axis)));

  model_editor_.AddNode(kOpTypeSplit, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddTriangularOperation(const mojom::Triangular& triangular) {
  const std::string node = GenerateNextOperationName(triangular.label);
  const std::string input = GetOperandNameById(triangular.input_operand_id);
  const std::string output = GetOperandNameById(triangular.output_operand_id);
  std::vector<const char*> inputs = {input.c_str()};

  // K is an operand with data type int64, not an attribute.;
  ASSIGN_OR_RETURN(const std::string k,
                   CreateScalarInitializer<int64_t>(
                       static_cast<int64_t>(triangular.diagonal)));
  inputs.push_back(k.c_str());

  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(1);
  attributes.push_back(model_editor_.CreateAttribute(
      /*name=*/"upper", static_cast<int64_t>(triangular.upper)));

  model_editor_.AddNode(kOpTypeTriangular, node, inputs, outputs,
                        std::move(attributes));

  return base::ok();
}

void GraphBuilderOrt::AddWhereOperation(const mojom::Where& where) {
  const std::string node = GenerateNextOperationName(where.label);
  // ONNX only supports bool data type for the condition input of Where, insert
  // a Cast node to convert the condition input to bool.
  std::string condition = GetOperandNameById(where.condition_operand_id);
  condition = PrependCast(condition, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);

  const std::string true_value =
      GetOperandNameById(where.true_value_operand_id);
  const std::string false_value =
      GetOperandNameById(where.false_value_operand_id);
  const std::string output = GetOperandNameById(where.output_operand_id);
  std::array<const char*, 3> inputs = {condition.c_str(), true_value.c_str(),
                                       false_value.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeWhere, node, inputs, outputs);
}

[[nodiscard]] base::expected<std::unique_ptr<OrtModelEditor::ModelInfo>,
                             mojom::ErrorPtr>
GraphBuilderOrt::BuildModel() {
  // Add inputs.
  for (uint64_t input_id : graph_info_->input_operands) {
    AddInput(input_id);
  }

  // Add initializers.
  for (const auto& [constant_id, _] : constant_operands_) {
    RETURN_IF_ERROR(AddInitializer(constant_id));
  }

  // Add operations.
  for (const mojom::OperationPtr& operation : graph_info_->operations) {
    switch (operation->which()) {
      case mojom::Operation::Tag::kArgMinMax: {
        AddArgMinMaxOperation(*operation->get_arg_min_max());
        break;
      }
      case mojom::Operation::Tag::kBatchNormalization: {
        RETURN_IF_ERROR(AddBatchNormalizationOperation(
            *operation->get_batch_normalization()));
        break;
      }
      case mojom::Operation::Tag::kClamp: {
        RETURN_IF_ERROR(AddClampOperation(*operation->get_clamp()));
        break;
      }
      case mojom::Operation::Tag::kElementWiseBinary: {
        RETURN_IF_ERROR(AddElementWiseBinaryOperation(
            *operation->get_element_wise_binary()));
        break;
      }
      case mojom::Operation::Tag::kElementWiseUnary: {
        AddElementWiseUnaryOperation(*operation->get_element_wise_unary());
        break;
      }
      case mojom::Operation::Tag::kConcat: {
        AddConcatOperation(*operation->get_concat());
        break;
      }
      case mojom::Operation::Tag::kConv2d: {
        RETURN_IF_ERROR(AddConv2dOperation(*operation->get_conv2d()));
        break;
      }
      case mojom::Operation::Tag::kCumulativeSum: {
        RETURN_IF_ERROR(
            AddCumulativeSumOperation(*operation->get_cumulative_sum()));
        break;
      }
      case mojom::Operation::Tag::kDequantizeLinear: {
        RETURN_IF_ERROR(AddDequantizeOrQuantizeLinearOperation(
            kOpTypeDequantizeLinear, *operation->get_dequantize_linear()));
        break;
      }
      case mojom::Operation::Tag::kElu: {
        RETURN_IF_ERROR(AddEluOperation(*operation->get_elu()));
        break;
      }
      case mojom::Operation::Tag::kExpand: {
        RETURN_IF_ERROR(AddExpandOperation(*operation->get_expand()));
        break;
      }
      case mojom::Operation::Tag::kGather: {
        RETURN_IF_ERROR(AddGatherOperation(*operation->get_gather()));
        break;
      }
      case mojom::Operation::Tag::kGatherElements: {
        RETURN_IF_ERROR(
            AddGatherElementsOperation(*operation->get_gather_elements()));
        break;
      }
      case mojom::Operation::Tag::kGatherNd: {
        AddGatherNDOperation(*operation->get_gather_nd());
        break;
      }
      case mojom::Operation::Tag::kGelu: {
        AddUnaryOperation(*operation->get_gelu(), kOpTypeGelu);
        break;
      }
      case mojom::Operation::Tag::kGemm: {
        AddGemmOperation(*operation->get_gemm());
        break;
      }
      case mojom::Operation::Tag::kGru: {
        RETURN_IF_ERROR(AddGruOperation(*operation->get_gru()));
        break;
      }
      case mojom::Operation::Tag::kGruCell: {
        RETURN_IF_ERROR(AddGruOperation(*operation->get_gru_cell()));
        break;
      }
      case mojom::Operation::Tag::kHardSigmoid: {
        AddHardSigmoidOperation(*operation->get_hard_sigmoid());
        break;
      }
      case mojom::Operation::Tag::kHardSwish: {
        AddUnaryOperation(*operation->get_hard_swish(), kOpTypeHardSwish);
        break;
      }
      case mojom::Operation::Tag::kInstanceNormalization: {
        RETURN_IF_ERROR(AddInstanceNormalizationOperation(
            *operation->get_instance_normalization()));
        break;
      }
      case mojom::Operation::Tag::kLayerNormalization: {
        RETURN_IF_ERROR(AddLayerNormalizationOperation(
            *operation->get_layer_normalization()));
        break;
      }
      case mojom::Operation::Tag::kLinear: {
        RETURN_IF_ERROR(AddLinearOperation(*operation->get_linear()));
        break;
      }
      case mojom::Operation::Tag::kLstm: {
        RETURN_IF_ERROR(AddLstmOperation(*operation->get_lstm()));
        break;
      }
      case mojom::Operation::Tag::kLstmCell: {
        RETURN_IF_ERROR(AddLstmOperation(*operation->get_lstm_cell()));
        break;
      }
      case mojom::Operation::Tag::kMatmul: {
        AddMatMulOperation(*operation->get_matmul());
        break;
      }
      case mojom::Operation::Tag::kLeakyRelu: {
        AddLeakyReluOperation(*operation->get_leaky_relu());
        break;
      }
      case mojom::Operation::Tag::kPad: {
        RETURN_IF_ERROR(AddPadOperation(*operation->get_pad()));
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
      case mojom::Operation::Tag::kQuantizeLinear: {
        RETURN_IF_ERROR(AddDequantizeOrQuantizeLinearOperation(
            kOpTypeQuantizeLinear, *operation->get_quantize_linear()));
        break;
      }
      case mojom::Operation::Tag::kReduce: {
        RETURN_IF_ERROR(AddReduceOperation(*operation->get_reduce()));
        break;
      }
      case mojom::Operation::Tag::kRelu: {
        AddUnaryOperation(*operation->get_relu(), kOpTypeRelu);
        break;
      }
      case mojom::Operation::Tag::kResample2d: {
        RETURN_IF_ERROR(AddResample2dOperation(*operation->get_resample2d()));
        break;
      }
      case mojom::Operation::Tag::kReshape: {
        RETURN_IF_ERROR(AddReshapeOperation(*operation->get_reshape()));
        break;
      }
      case mojom::Operation::Tag::kReverse: {
        RETURN_IF_ERROR(AddReverseOperation(*operation->get_reverse()));
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
      case mojom::Operation::Tag::kSigmoid: {
        AddUnaryOperation(*operation->get_sigmoid(), kOpTypeSigmoid);
        break;
      }
      case mojom::Operation::Tag::kSlice: {
        RETURN_IF_ERROR(AddSliceOperation(*operation->get_slice()));
        break;
      }
      case mojom::Operation::Tag::kSoftmax: {
        AddSoftmaxOperation(*operation->get_softmax());
        break;
      }
      case mojom::Operation::Tag::kSoftplus: {
        AddUnaryOperation(*operation->get_softplus(), kOpTypeSoftplus);
        break;
      }
      case mojom::Operation::Tag::kSoftsign: {
        AddUnaryOperation(*operation->get_softsign(), kOpTypeSoftsign);
        break;
      }
      case mojom::Operation::Tag::kSplit: {
        RETURN_IF_ERROR(AddSplitOperation(*operation->get_split()));
        break;
      }
      case mojom::Operation::Tag::kTanh: {
        AddUnaryOperation(*operation->get_tanh(), kOpTypeTanh);
        break;
      }
      case mojom::Operation::Tag::kTile: {
        RETURN_IF_ERROR(AddTileOperation(*operation->get_tile()));
        break;
      }
      case mojom::Operation::Tag::kTranspose: {
        AddTransposeOperation(*operation->get_transpose());
        break;
      }
      case mojom::Operation::Tag::kTriangular: {
        RETURN_IF_ERROR(AddTriangularOperation(*operation->get_triangular()));
        break;
      }
      case mojom::Operation::Tag::kWhere: {
        AddWhereOperation(*operation->get_where());
        break;
      }
      default:
        NOTREACHED() << "[WebNN] Unsupported operation.";
    }
  }
  // Add outputs.
  for (uint64_t output_id : graph_info_->output_operands) {
    AddOutput(output_id);
  }

  return model_editor_.BuildAndTakeModelInfo();
}

}  // namespace webnn::ort
