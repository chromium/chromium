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
constexpr char kOpTypeHardSwish[] = "HardSwish";
constexpr char kOpTypeHardSigmoid[] = "HardSigmoid";
constexpr char kOpTypeInstanceNormalization[] = "InstanceNormalization";
constexpr char kOpTypeLayerNormalization[] = "LayerNormalization";
constexpr char kOpTypeLeakyRelu[] = "LeakyRelu";
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
struct TensorTypeMap<int64_t> {
  static constexpr ONNXTensorElementDataType value =
      ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
};

#define ADD_CAST_NODE(node_name, input_name, output_name, to_data_type)      \
  do {                                                                       \
    std::array<const char*, 1> input_names = {input_name.data()};            \
    std::array<const char*, 1> output_names = {output_name.data()};          \
    int64_t attr_to = static_cast<int64_t>(to_data_type);                    \
    std::array<OrtOpAttr*, 1> attributes = {                                 \
        model_editor_.CreateAttribute(/*name=*/"to", attr_to).Release()};    \
    model_editor_.AddNode(kOpTypeCast, node_name, input_names, output_names, \
                          attributes);                                       \
  } while (0)

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

const mojom::Operand& GraphBuilderOrt::GetOperand(uint64_t operand_id) {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

std::string GraphBuilderOrt::GetOperandNameById(uint64_t operand_id) {
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

  ScopedOrtStatusPtr status_ptr;
  // TODO(https://github.com/shiyi9801/chromium/issues/70): Remove this
  // workaround for OpenVINO EP once the invalid external data issue is fixed.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtUseOpenvino)) {
    status_ptr = model_editor_.AddInitializer(name, int64_shape, byte_span,
                                              TensorTypeMap<DataType>::value);

  } else {
    status_ptr = model_editor_.AddInitializerAsRawData(
        name, int64_shape, byte_span, TensorTypeMap<DataType>::value);
  }

  if (status_ptr) {
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

std::string GraphBuilderOrt::PrependCast(
    std::string_view input_name,
    ONNXTensorElementDataType to_data_type) {
  const std::string node_name = GenerateNextOperationName("inserted_cast");
  const std::string output_name = GenerateNextOperandName();
  ADD_CAST_NODE(node_name, input_name, output_name, to_data_type);
  return output_name;
}

void GraphBuilderOrt::AppendCast(std::string_view input_name,
                                 std::string_view output_name,
                                 ONNXTensorElementDataType to_data_type) {
  const std::string node_name = GenerateNextOperationName("inserted_cast");
  ADD_CAST_NODE(node_name, input_name, output_name, to_data_type);
}

std::string GraphBuilderOrt::PrependTranspose(
    std::string_view input_name,
    base::span<const uint32_t> permutation) {
  const std::string node_name = GenerateNextOperationName("inserted_transpose");
  const std::string output_name = GenerateNextOperandName();

  std::array<const char*, 1> input_names = {input_name.data()};
  std::array<const char*, 1> output_names = {output_name.data()};

  std::vector<int64_t> perm(permutation.begin(), permutation.end());
  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"perm", perm).Release()};

  model_editor_.AddNode(kOpTypeTranspose, node_name, input_names, output_names,
                        attributes);
  return output_name;
}

void GraphBuilderOrt::AppendTranspose(std::string_view input_name,
                                      std::string_view output_name,
                                      base::span<const uint32_t> permutation) {
  const std::string node_name = GenerateNextOperationName("inserted_transpose");
  std::array<const char*, 1> input_names = {input_name.data()};
  std::array<const char*, 1> output_names = {output_name.data()};

  std::vector<int64_t> perm(permutation.begin(), permutation.end());
  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"perm", perm).Release()};

  model_editor_.AddNode(kOpTypeTranspose, node_name, input_names, output_names,
                        attributes);
}

[[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
GraphBuilderOrt::PrependReshape(std::string_view input_name,
                                base::span<const int64_t> new_shape) {
  const std::string node_name = GenerateNextOperationName("inserted_reshape");
  const std::string output_name = GenerateNextOperandName();

  // Shape is an operand with data type int64, not an attribute.
  std::vector<uint32_t> new_shape_dims = {
      base::checked_cast<uint32_t>(new_shape.size())};
  ASSIGN_OR_RETURN(const std::string shape_name,
                   CreateInitializer<int64_t>(new_shape_dims, new_shape));

  std::array<const char*, 2> input_names = {input_name.data(),
                                            shape_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeReshape, node_name, input_names, output_names);

  return output_name;
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddSliceNode(std::string_view node_name,
                              std::string_view input_name,
                              std::string_view output_name,
                              std::string_view axes_name,
                              base::span<const int64_t> starts,
                              base::span<const int64_t> ends,
                              base::span<const int64_t> steps) {
  // Starts is an operand with data type int64, not an attribute.
  std::vector<uint32_t> starts_shape = {
      base::checked_cast<uint32_t>(starts.size())};
  ASSIGN_OR_RETURN(const std::string starts_name,
                   CreateInitializer<int64_t>(starts_shape, starts));

  // Ends is an operand with data type int64, not an attribute.
  std::vector<uint32_t> ends_shape = {
      base::checked_cast<uint32_t>(ends.size())};
  ASSIGN_OR_RETURN(const std::string ends_name,
                   CreateInitializer<int64_t>(ends_shape, ends));

  // Steps is an operand with data type int64, not an attribute.
  std::vector<uint32_t> steps_shape = {
      base::checked_cast<uint32_t>(steps.size())};
  ASSIGN_OR_RETURN(const std::string steps_name,
                   CreateInitializer<int64_t>(steps_shape, steps));

  std::array<const char*, 5> input_names = {
      input_name.data(), starts_name.data(), ends_name.data(), axes_name.data(),
      steps_name.data()};
  std::array<const char*, 1> output_names = {output_name.data()};

  model_editor_.AddNode(kOpTypeSlice, node_name, input_names, output_names);

  return base::ok();
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
  ScopedOrtStatusPtr status_ptr;
  // TODO(https://github.com/shiyi9801/chromium/issues/70): Remove this
  // workaround for OpenVINO EP once the invalid external data issue is fixed.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtUseOpenvino)) {
    status_ptr = model_editor_.AddInitializer(
        name, int64_shape, operand.ByteSpan(), onnx_data_type);
  } else {
    status_ptr = model_editor_.AddInitializerAsRawData(
        name, int64_shape, operand.ByteSpan(), onnx_data_type);
  }

  if (status_ptr) {
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

  const std::string input_name =
      GetOperandNameById(batch_normalization.input_operand_id);
  std::vector<const char*> input_names = {input_name.c_str()};

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

  std::string scale_name, bias_name;
  // ONNX requires scale and bias inputs.
  if (batch_normalization.scale_operand_id) {
    scale_name =
        GetOperandNameById(batch_normalization.scale_operand_id.value());
    input_names.push_back(scale_name.c_str());
  } else {
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> scale_data_fp16(input_channel,
                                              fp16_ieee_from_fp32_value(1.0f));
        ASSIGN_OR_RETURN(scale_name, CreateInitializer<uint16_t>(
                                         constant_dims, scale_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        std::vector<float> scale_data(input_channel, 1.0f);
        ASSIGN_OR_RETURN(scale_name,
                         CreateInitializer<float>(constant_dims, scale_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] BatchNormalization only supports float32 "
                        "and float16 data type.";
    }

    input_names.push_back(scale_name.c_str());
  }

  if (batch_normalization.bias_operand_id) {
    bias_name = GetOperandNameById(batch_normalization.bias_operand_id.value());
    input_names.push_back(bias_name.c_str());
  } else {
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> bias_data_fp16(input_channel,
                                             fp16_ieee_from_fp32_value(0.0f));
        ASSIGN_OR_RETURN(bias_name, CreateInitializer<uint16_t>(
                                        constant_dims, bias_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        std::vector<float> bias_data(input_channel, 0.0f);
        ASSIGN_OR_RETURN(bias_name,
                         CreateInitializer<float>(constant_dims, bias_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] BatchNormalization only supports float32 "
                        "and float16 data type.";
    }

    input_names.push_back(bias_name.c_str());
  }

  const std::string mean_name =
      GetOperandNameById(batch_normalization.mean_operand_id);
  input_names.push_back(mean_name.c_str());

  const std::string variance_name =
      GetOperandNameById(batch_normalization.variance_operand_id);
  input_names.push_back(variance_name.c_str());

  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_
          .CreateAttribute(/*name=*/"epsilon", batch_normalization.epsilon)
          .Release()};

  const std::string node_name =
      GenerateNextOperationName(batch_normalization.label);
  const std::string output_name =
      GetOperandNameById(batch_normalization.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};
  model_editor_.AddNode(kOpTypeBatchNormalization, node_name, input_names,
                        output_names, attributes);

  return base::ok();
}

template <typename T>
void GraphBuilderOrt::AddBinaryOperation(const T& operation,
                                         std::string_view op_type) {
  const std::string node_name = GenerateNextOperationName(operation.label);
  const std::string lhs_name = GetOperandNameById(operation.lhs_operand_id);
  const std::string rhs_name = GetOperandNameById(operation.rhs_operand_id);
  const std::string output_name =
      GetOperandNameById(operation.output_operand_id);

  std::array<const char*, 2> input_names = {lhs_name.c_str(), rhs_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(op_type, node_name, input_names, output_names);
}

void GraphBuilderOrt::AddElementWiseLogicalOperation(
    absl::variant<const mojom::ElementWiseBinary*,
                  const mojom::ElementWiseUnary*> operation,
    std::string_view op_type) {
  const std::string node_name = absl::visit(
      [this](const auto* op) { return GenerateNextOperationName(op->label); },
      operation);

  std::vector<const char*> input_names;
  std::string lhs_name;
  std::string rhs_name;
  if (absl::holds_alternative<const mojom::ElementWiseBinary*>(operation)) {
    const mojom::ElementWiseBinary* element_wise_binary =
        absl::get<const mojom::ElementWiseBinary*>(operation);
    lhs_name = GetOperandNameById(element_wise_binary->lhs_operand_id);
    rhs_name = GetOperandNameById(element_wise_binary->rhs_operand_id);

    // Some ONNX logical operators only support bool input.
    if (element_wise_binary->kind ==
            mojom::ElementWiseBinary::Kind::kLogicalAnd ||
        element_wise_binary->kind ==
            mojom::ElementWiseBinary::Kind::kLogicalOr ||
        element_wise_binary->kind ==
            mojom::ElementWiseBinary::Kind::kLogicalXor) {
      lhs_name = PrependCast(lhs_name, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
      rhs_name = PrependCast(rhs_name, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
    }

    input_names = {lhs_name.c_str(), rhs_name.c_str()};
  } else {
    const mojom::ElementWiseUnary* element_wise_unary =
        absl::get<const mojom::ElementWiseUnary*>(operation);
    lhs_name = GetOperandNameById(element_wise_unary->input_operand_id);

    // Some ONNX logical operators only support bool input.
    if (element_wise_unary->kind ==
        mojom::ElementWiseUnary::Kind::kLogicalNot) {
      lhs_name = PrependCast(lhs_name, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
    }

    input_names = {lhs_name.c_str()};
  }

  const std::string bool_output_name = GenerateNextOperandName();
  std::array<const char*, 1> output_names = {bool_output_name.c_str()};

  model_editor_.AddNode(op_type, node_name, input_names, output_names);

  // ONNX logical operators only support bool output. To support output with the
  // WebNN data type, it is necessary to insert a cast operator after a logical
  // operator.
  uint64_t output_operand_id = absl::visit(
      [](const auto* op) { return op->output_operand_id; }, operation);
  OperandDataType output_data_type =
      GetOperand(output_operand_id).descriptor.data_type();
  std::string output_name = GetOperandNameById(output_operand_id);
  AppendCast(bool_output_name, output_name,
             OperandTypeToONNXTensorElementDataType(output_data_type));
}

// ONNX doesn't support `notEqual`, emulate it by `logicalNot(equal(a, b))`.
void GraphBuilderOrt::AddElementWiseLogicalNotEqualOperation(
    const mojom::ElementWiseBinary& not_equal) {
  // Step 1: calculate `equal(a, b)`.
  const std::string equal_node_name =
      GenerateNextOperationName("inserted_equal_to_emulate_" + not_equal.label);
  std::string lhs_name = GetOperandNameById(not_equal.lhs_operand_id);
  std::string rhs_name = GetOperandNameById(not_equal.rhs_operand_id);
  const std::string equal_output_name = GenerateNextOperandName();

  std::array<const char*, 1> equal_output_names = {equal_output_name.c_str()};
  std::array<const char*, 2> equal_input_names = {lhs_name.c_str(),
                                                  rhs_name.c_str()};
  model_editor_.AddNode(kOpTypeEqual, equal_node_name, equal_input_names,
                        equal_output_names);

  // Step 2: calculate `logicalNot(equal_output)`
  const std::string not_output_name = GenerateNextOperandName();
  std::array<const char*, 1> not_output_names = {not_output_name.c_str()};
  const std::string not_node_name =
      GenerateNextOperationName("inserted_not_to_emulate_" + not_equal.label);
  model_editor_.AddNode(kOpTypeLogicalNot, not_node_name, equal_output_names,
                        not_output_names);

  // ONNX logical operators only support bool output. To support output with the
  // WebNN data type, it is necessary to insert a cast operator after a logical
  // operator.
  uint64_t output_operand_id = not_equal.output_operand_id;
  OperandDataType output_data_type =
      GetOperand(output_operand_id).descriptor.data_type();
  std::string output_name = GetOperandNameById(output_operand_id);
  AppendCast(not_output_name, output_name,
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
  const std::string node_name = GenerateNextOperationName(operation.label);
  const std::string input_name = GetOperandNameById(operation.input_operand_id);
  const std::string output_name =
      GetOperandNameById(operation.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(op_type, node_name, input_names, output_names);
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
  const std::string node_name = GenerateNextOperationName(arg_min_max.label);
  const std::string input_name =
      GetOperandNameById(arg_min_max.input_operand_id);
  const std::string output_name =
      GetOperandNameById(arg_min_max.output_operand_id);

  int64_t axis = static_cast<int64_t>(arg_min_max.axis);
  ScopedOrtOpAttrPtr attr_axis =
      model_editor_.CreateAttribute(/*name=*/"axis", axis);

  int64_t keep_dimensions = static_cast<int64_t>(arg_min_max.keep_dimensions);
  ScopedOrtOpAttrPtr attr_keepdims =
      model_editor_.CreateAttribute(/*name=*/"keepdims", keep_dimensions);

  std::array<OrtOpAttr*, 2> attributes = {attr_axis.Release(),
                                          attr_keepdims.Release()};

  // Onnx ArgMin/Max only supports int64 output. To support int32 output, it is
  // necessary to insert a cast operator after ArgMin/Max. To cast Argmin/Max
  // output from int64 to int32 is safe since a valid operand dimension is
  // greater than zero and in the range of int32.
  // https://www.w3.org/TR/webnn/#valid-dimension
  OperandDataType output_data_type =
      GetOperand(arg_min_max.output_operand_id).descriptor.data_type();
  bool need_cast = output_data_type != OperandDataType::kInt64;

  const std::string int64_output_name =
      need_cast ? GenerateNextOperandName() : output_name;

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {int64_output_name.c_str()};

  switch (arg_min_max.kind) {
    case mojom::ArgMinMax::Kind::kMax: {
      model_editor_.AddNode(kOpTypeArgMax, node_name, input_names, output_names,
                            attributes);
      break;
    }
    case mojom::ArgMinMax::Kind::kMin: {
      model_editor_.AddNode(kOpTypeArgMin, node_name, input_names, output_names,
                            attributes);
      break;
    }
  }

  if (need_cast) {
    AppendCast(int64_output_name, output_name,
               OperandTypeToONNXTensorElementDataType(output_data_type));
  }
}

void GraphBuilderOrt::AddCastOperation(const mojom::ElementWiseUnary& cast) {
  const std::string node_name = GenerateNextOperationName(cast.label);
  const std::string input_name = GetOperandNameById(cast.input_operand_id);
  const std::string output_name = GetOperandNameById(cast.output_operand_id);
  const OperandDataType output_data_type =
      GetOperand(cast.output_operand_id).descriptor.data_type();
  int64_t to_data_type = static_cast<int64_t>(
      OperandTypeToONNXTensorElementDataType(output_data_type));
  ADD_CAST_NODE(node_name, input_name, output_name, to_data_type);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddClampOperation(const mojom::Clamp& clamp) {
  const std::string node_name = GenerateNextOperationName(clamp.label);
  const std::string input_name = GetOperandNameById(clamp.input_operand_id);
  const std::string output_name = GetOperandNameById(clamp.output_operand_id);

  const OperandDataType input_data_type =
      GetOperand(clamp.output_operand_id).descriptor.data_type();

  // Min and max are 0-D operands with the same data type of input.

  std::string min_name;
  std::string max_name;
  switch (input_data_type) {
    case OperandDataType::kFloat32: {
      ASSIGN_OR_RETURN(min_name, CreateScalarInitializer(clamp.min_value));
      ASSIGN_OR_RETURN(max_name, CreateScalarInitializer(clamp.max_value));
      break;
    }
    case OperandDataType::kFloat16: {
      ASSIGN_OR_RETURN(
          min_name,
          CreateScalarInitializer(fp16_ieee_from_fp32_value(clamp.min_value)));
      ASSIGN_OR_RETURN(
          max_name,
          CreateScalarInitializer(fp16_ieee_from_fp32_value(clamp.max_value)));
      break;
    }
    // TODO(https://github.com/shiyi9801/chromium/issues/60): Add other data
    // types support. https://onnx.ai/onnx/operators/onnx__Clip.html
    default:
      NOTREACHED()
          << "[WebNN] Clamp only supports float32 and float16 data type.";
  }

  std::array<const char*, 3> input_names = {input_name.c_str(),
                                            min_name.c_str(), max_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeClamp, node_name, input_names, output_names);

  return base::ok();
}

void GraphBuilderOrt::AddConcatOperation(const mojom::Concat& concat) {
  const std::string node_name = GenerateNextOperationName(concat.label);

  std::vector<std::string> input_names_string;
  input_names_string.reserve(concat.input_operand_ids.size());
  std::vector<const char*> input_names;
  input_names.reserve(concat.input_operand_ids.size());
  for (uint64_t input_operand_id : concat.input_operand_ids) {
    input_names_string.push_back(GetOperandNameById(input_operand_id));
    input_names.push_back(input_names_string.back().c_str());
  }

  const std::string output_name = GetOperandNameById(concat.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtOpAttrPtr attr_axis = model_editor_.CreateAttribute(
      /*name=*/"axis", base::checked_cast<int64_t>(concat.axis));
  std::array<OrtOpAttr*, 1> attributes = {attr_axis.Release()};

  model_editor_.AddNode(kOpTypeConcat, node_name, input_names, output_names,
                        attributes);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddConv2dOperation(const mojom::Conv2d& conv2d) {
  const std::string node_name = GenerateNextOperationName(conv2d.label);
  const std::string input_name = GetOperandNameById(conv2d.input_operand_id);
  const std::string filter_name = GetOperandNameById(conv2d.filter_operand_id);
  const std::string output_name = GetOperandNameById(conv2d.output_operand_id);
  std::vector<const char*> input_names;
  std::string bias_name;
  if (conv2d.bias_operand_id) {
    bias_name = GetOperandNameById(conv2d.bias_operand_id.value());
    input_names = {input_name.c_str(), filter_name.c_str(), bias_name.c_str()};
  } else {
    input_names = {input_name.c_str(), filter_name.c_str()};
  }
  std::array<const char*, 1> output_names = {output_name.c_str()};

  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(conv2d.dilations->height),
      base::checked_cast<int64_t>(conv2d.dilations->width)};
  ScopedOrtOpAttrPtr attr_dilations =
      model_editor_.CreateAttribute(/*name=*/"dilations", dilations);

  int64_t group = base::checked_cast<int64_t>(conv2d.groups);
  ScopedOrtOpAttrPtr attr_group =
      model_editor_.CreateAttribute(/*name=*/"group", group);

  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(conv2d.padding->beginning->height),
      base::checked_cast<int64_t>(conv2d.padding->beginning->width),
      base::checked_cast<int64_t>(conv2d.padding->ending->height),
      base::checked_cast<int64_t>(conv2d.padding->ending->width)};
  ScopedOrtOpAttrPtr attr_pads =
      model_editor_.CreateAttribute(/*name=*/"pads", pads);

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(conv2d.strides->height),
      base::checked_cast<int64_t>(conv2d.strides->width)};
  ScopedOrtOpAttrPtr attr_strides =
      model_editor_.CreateAttribute(/*name=*/"strides", strides);

  std::vector<OrtOpAttr*> attributes = {
      attr_dilations.Release(),
      attr_group.Release(),
      attr_pads.Release(),
      attr_strides.Release(),
  };

  switch (conv2d.kind) {
    case mojom::Conv2d::Kind::kDirect:
      model_editor_.AddNode(kOpTypeConv2d, node_name, input_names, output_names,
                            attributes);
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
      ScopedOrtOpAttrPtr attr_output_shape =
          model_editor_.CreateAttribute(/*name=*/"output_shape", output_size);
      attributes.push_back(attr_output_shape.Release());

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

      // According to the ONNX ConvTranspose2d documentation, since pads will be
      // auto generated if output_shape is specified, and output_shape is
      // determined by pads and output_padding, we need to calculate the actual
      // output_padding to ensure that the pads value automatically calculated
      // is correct.
      // https://onnx.ai/onnx/operators/onnx__ConvTranspose.html#attributes
      ScopedOrtOpAttrPtr attr_output_padding = model_editor_.CreateAttribute(
          /*name=*/"output_padding", output_padding);
      attributes.push_back(attr_output_padding.Release());

      model_editor_.AddNode(kOpTypeConvTranspose2d, node_name, input_names,
                            output_names, attributes);
      break;
  }

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddCumulativeSumOperation(
    const mojom::CumulativeSum& cumulative_sum) {
  const std::string node_name = GenerateNextOperationName(cumulative_sum.label);
  const std::string input_name =
      GetOperandNameById(cumulative_sum.input_operand_id);
  const std::string output_name =
      GetOperandNameById(cumulative_sum.output_operand_id);

  std::string axis_name;
  ASSIGN_OR_RETURN(axis_name,
                   CreateScalarInitializer<int64_t>(
                       base::checked_cast<int64_t>(cumulative_sum.axis)));

  ScopedOrtOpAttrPtr attr_exclusive = model_editor_.CreateAttribute(
      /*name=*/"exclusive",
      base::checked_cast<int64_t>(cumulative_sum.exclusive));
  ScopedOrtOpAttrPtr attr_reverse = model_editor_.CreateAttribute(
      /*name=*/"reverse", base::checked_cast<int64_t>(cumulative_sum.reversed));
  std::array<OrtOpAttr*, 2> attributes = {attr_exclusive.Release(),
                                          attr_reverse.Release()};

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            axis_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};
  model_editor_.AddNode(kOpTypeCumulativeSum, node_name, input_names,
                        output_names, attributes);

  return base::ok();
}

void GraphBuilderOrt::AddEluOperation(const mojom::Elu& elu) {
  const std::string node_name = GenerateNextOperationName(elu.label);
  const std::string input_name = GetOperandNameById(elu.input_operand_id);
  const std::string output_name = GetOperandNameById(elu.output_operand_id);

  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"alpha", elu.alpha).Release()};

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeElu, node_name, input_names, output_names,
                        attributes);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddExpandOperation(const mojom::Expand& expand) {
  const std::string node_name = GenerateNextOperationName(expand.label);
  const std::string input_name = GetOperandNameById(expand.input_operand_id);
  const std::string output_name = GetOperandNameById(expand.output_operand_id);

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
  ASSIGN_OR_RETURN(const std::string shape_name,
                   CreateInitializer<int64_t>(shape_dims, shape_values));

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            shape_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeExpand, node_name, input_names, output_names);

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
  const std::string node_name = GenerateNextOperationName(operation.label);
  std::string input_name = GetOperandNameById(operation.input_operand_id);
  std::string scale_name = GetOperandNameById(operation.scale_operand_id);
  std::string zero_point_name =
      GetOperandNameById(operation.zero_point_operand_id);
  std::string output_name = GetOperandNameById(operation.output_operand_id);

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
    ASSIGN_OR_RETURN(scale_name, PrependReshape(scale_name, {}));
    ASSIGN_OR_RETURN(zero_point_name, PrependReshape(zero_point_name, {}));
  } else if (is_per_axis) {
    // For per-axis dequantization, scale and zeroPoint must be a 1-D
    // Tensor.
    CHECK(axis.has_value());
    ASSIGN_OR_RETURN(scale_name,
                     PrependReshape(scale_name, {input_shape[axis.value()]}));
    ASSIGN_OR_RETURN(
        zero_point_name,
        PrependReshape(zero_point_name, {input_shape[axis.value()]}));
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
        input_name = PrependTranspose(input_name, {1, 0});
        scale_name = PrependTranspose(scale_name, {1, 0});
        zero_point_name = PrependTranspose(zero_point_name, {1, 0});
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

  const std::string transposed_output_name =
      need_transpose ? GenerateNextOperandName() : output_name;

  base::FixedArray<const char*> input_names = {
      input_name.c_str(), scale_name.c_str(), zero_point_name.c_str()};
  base::FixedArray<const char*> output_names = {transposed_output_name.c_str()};

  std::vector<OrtOpAttr*> attributes;
  if (axis.has_value()) {
    attributes.push_back(
        model_editor_
            .CreateAttribute(/*name=*/"axis",
                             base::checked_cast<int64_t>(axis.value()))
            .Release());
  }

  if (block_size.has_value()) {
    attributes.push_back(
        model_editor_
            .CreateAttribute(/*name=*/"block_size",
                             base::checked_cast<int64_t>(block_size.value()))
            .Release());
  }

  model_editor_.AddNode(op_type, node_name, input_names, output_names,
                        attributes);

  if (need_transpose) {
    AppendTranspose(transposed_output_name, output_name, {1, 0});
  }
  return base::ok();
}

void GraphBuilderOrt::AddGatherOperation(const mojom::Gather& gather) {
  const std::string node_name = GenerateNextOperationName(gather.label);
  const std::string input_name = GetOperandNameById(gather.input_operand_id);
  const std::string indices_name =
      GetOperandNameById(gather.indices_operand_id);
  const std::string output_name = GetOperandNameById(gather.output_operand_id);

  // TODO(https://github.com/shiyi9801/chromium/issues/82): Clamp the indices
  // operand to ensure it won't be out-of-bound.

  int64_t axis = static_cast<int64_t>(gather.axis);
  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"axis", axis).Release()};

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            indices_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeGather, node_name, input_names, output_names,
                        attributes);
}

void GraphBuilderOrt::AddGatherElementsOperation(
    const mojom::GatherElements& gather_elements) {
  const std::string node_name =
      GenerateNextOperationName(gather_elements.label);
  const std::string input_name =
      GetOperandNameById(gather_elements.input_operand_id);
  const std::string indices_name =
      GetOperandNameById(gather_elements.indices_operand_id);
  const std::string output_name =
      GetOperandNameById(gather_elements.output_operand_id);

  // TODO(https://github.com/shiyi9801/chromium/issues/149): Clamp the indices
  // operand to ensure it won't be out-of-bound.

  int64_t axis = static_cast<int64_t>(gather_elements.axis);
  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"axis", axis).Release()};

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            indices_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeGatherElements, node_name, input_names,
                        output_names, attributes);
}

void GraphBuilderOrt::AddGatherNDOperation(const mojom::GatherND& gather_nd) {
  const std::string node_name = GenerateNextOperationName(gather_nd.label);
  const std::string input_name = GetOperandNameById(gather_nd.input_operand_id);
  const std::string indices_name =
      GetOperandNameById(gather_nd.indices_operand_id);
  const std::string output_name =
      GetOperandNameById(gather_nd.output_operand_id);

  std::string int64_indices_name;
  const OperandDataType indices_data_type =
      GetOperand(gather_nd.indices_operand_id).descriptor.data_type();

  // TODO(https://github.com/shiyi9801/chromium/issues/141): Clamp the indices
  // operand to ensure it won't be out-of-bound.

  // ONNX GatherND only supports int64 indices.
  switch (indices_data_type) {
    case OperandDataType::kInt64: {
      int64_indices_name = indices_name;
      break;
    }
    case OperandDataType::kInt32:
    case OperandDataType::kUint32: {
      int64_indices_name = GenerateNextOperandName();
      AppendCast(
          indices_name, int64_indices_name,
          ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
      break;
    }
    default:
      NOTREACHED()
          << "[WebNN] GatherND only supports int32, uint32 and int64 indices.";
  }

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            int64_indices_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeGatherND, node_name, input_names, output_names);
}

void GraphBuilderOrt::AddGemmOperation(const mojom::Gemm& gemm) {
  const std::string node_name = GenerateNextOperationName(gemm.label);
  const std::string input_a_name = GetOperandNameById(gemm.a_operand_id);
  const std::string input_b_name = GetOperandNameById(gemm.b_operand_id);
  const std::string output_name = GetOperandNameById(gemm.output_operand_id);

  std::vector<const char*> input_names;
  std::string input_c_name;
  if (gemm.c_operand_id.has_value()) {
    input_c_name = GetOperandNameById(gemm.c_operand_id.value());
    input_names = {input_a_name.c_str(), input_b_name.c_str(),
                   input_c_name.c_str()};
  } else {
    input_names = {input_a_name.c_str(), input_b_name.c_str()};
  }
  std::array<const char*, 1> output_names = {output_name.c_str()};

  ScopedOrtOpAttrPtr attr_alpha =
      model_editor_.CreateAttribute(/*name=*/"alpha", gemm.alpha);
  ScopedOrtOpAttrPtr attr_beta =
      model_editor_.CreateAttribute(/*name=*/"beta", gemm.beta);
  ScopedOrtOpAttrPtr attr_trans_a = model_editor_.CreateAttribute(
      /*name=*/"transA", static_cast<int64_t>(gemm.a_transpose));
  ScopedOrtOpAttrPtr attr_trans_b = model_editor_.CreateAttribute(
      /*name=*/"transB", static_cast<int64_t>(gemm.b_transpose));

  std::array<OrtOpAttr*, 4> attributes = {
      attr_alpha.Release(), attr_beta.Release(), attr_trans_a.Release(),
      attr_trans_b.Release()};

  model_editor_.AddNode(kOpTypeGemm, node_name, input_names, output_names,
                        attributes);
}

void GraphBuilderOrt::AddHardSigmoidOperation(
    const mojom::HardSigmoid& hard_sigmoid) {
  const std::string node_name = GenerateNextOperationName(hard_sigmoid.label);
  const std::string input_name =
      GetOperandNameById(hard_sigmoid.input_operand_id);
  const std::string output_name =
      GetOperandNameById(hard_sigmoid.output_operand_id);
  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  std::array<OrtOpAttr*, 2> attributes = {
      model_editor_
          .CreateAttribute(
              /*name=*/"alpha", hard_sigmoid.alpha)
          .Release(),
      model_editor_
          .CreateAttribute(
              /*name=*/"beta", hard_sigmoid.beta)
          .Release()};

  model_editor_.AddNode(kOpTypeHardSigmoid, node_name, input_names,
                        output_names, attributes);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddInstanceNormalizationOperation(
    const mojom::InstanceNormalization& instance_normalization) {
  const OperandDataType input_data_type =
      GetOperand(instance_normalization.output_operand_id)
          .descriptor.data_type();

  const std::string input_name =
      GetOperandNameById(instance_normalization.input_operand_id);
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
  // ONNX requires scale and bias inputs.
  if (instance_normalization.scale_operand_id) {
    scale_name =
        GetOperandNameById(instance_normalization.scale_operand_id.value());
    input_names.push_back(scale_name.c_str());
  } else {
    std::vector<float> scale_data(input_channel, 1.0f);
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> scale_data_fp16(input_channel,
                                              fp16_ieee_from_fp32_value(1.0f));
        ASSIGN_OR_RETURN(scale_name, CreateInitializer<uint16_t>(
                                         constant_dims, scale_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        ASSIGN_OR_RETURN(scale_name,
                         CreateInitializer<float>(constant_dims, scale_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] InstanceNormalization only supports float32 "
                        "and float16 data type.";
    }

    input_names.push_back(scale_name.c_str());
  }

  if (instance_normalization.bias_operand_id) {
    bias_name =
        GetOperandNameById(instance_normalization.bias_operand_id.value());
    input_names.push_back(bias_name.c_str());
  } else {
    std::vector<float> bias_data(input_channel, 0.0f);
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> bias_data_fp16(input_channel,
                                             fp16_ieee_from_fp32_value(0.0f));
        ASSIGN_OR_RETURN(bias_name, CreateInitializer<uint16_t>(
                                        constant_dims, bias_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        ASSIGN_OR_RETURN(bias_name,
                         CreateInitializer<float>(constant_dims, bias_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] InstanceNormalization only supports float32 "
                        "and float16 data type.";
    }

    input_names.push_back(bias_name.c_str());
  }

  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_
          .CreateAttribute(
              /*name=*/"epsilon", instance_normalization.epsilon)
          .Release()};

  const std::string node_name =
      GenerateNextOperationName(instance_normalization.label);
  const std::string output_name =
      GetOperandNameById(instance_normalization.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};
  model_editor_.AddNode(kOpTypeInstanceNormalization, node_name, input_names,
                        output_names, attributes);

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddLayerNormalizationOperation(
    const mojom::LayerNormalization& layer_normalization) {
  const OperandDataType input_data_type =
      GetOperand(layer_normalization.input_operand_id).descriptor.data_type();

  const std::string input_name =
      GetOperandNameById(layer_normalization.input_operand_id);
  std::vector<const char*> input_names = {input_name.c_str()};
  const std::vector<uint32_t>& input_shape =
      GetOperand(layer_normalization.input_operand_id).descriptor.shape();

  const std::string node_name =
      GenerateNextOperationName(layer_normalization.label);
  const std::string output_name =
      GetOperandNameById(layer_normalization.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};

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
      std::string zero_name;
      switch (input_data_type) {
        case OperandDataType::kFloat16: {
          std::vector<uint16_t> zero_data_fp16(checked_input_size.ValueOrDie(),
                                               fp16_ieee_from_fp32_value(0.0f));
          ASSIGN_OR_RETURN(zero_name, CreateInitializer<uint16_t>(
                                          input_shape, zero_data_fp16));
          break;
        }
        case OperandDataType::kFloat32: {
          std::vector<float> zero_data(checked_input_size.ValueOrDie(), 0.0f);
          ASSIGN_OR_RETURN(zero_name,
                           CreateInitializer<float>(input_shape, zero_data));
          break;
        }
        default:
          NOTREACHED() << "[WebNN] InstanceNormalization only supports float32 "
                          "and float16 data type.";
      }
      const std::string bias_name =
          GetOperandNameById(layer_normalization.bias_operand_id.value());
      std::array<const char*, 2> binary_input_names = {bias_name.c_str(),
                                                       zero_name.c_str()};
      model_editor_.AddNode(kOpTypeAdd, node_name, binary_input_names,
                            output_names);
    } else {
      std::array<const char*, 2> binary_input_names = {input_name.c_str(),
                                                       input_name.c_str()};
      model_editor_.AddNode(kOpTypeSub, node_name, binary_input_names,
                            output_names);
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
  std::string scale_name;
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
    scale_name =
        GetOperandNameById(layer_normalization.scale_operand_id.value());
    input_names.push_back(scale_name.c_str());
  } else {
    switch (input_data_type) {
      case OperandDataType::kFloat16: {
        std::vector<uint16_t> scale_data_fp16(checked_scale_size.ValueOrDie(),
                                              fp16_ieee_from_fp32_value(1.0f));
        ASSIGN_OR_RETURN(scale_name, CreateInitializer<uint16_t>(
                                         scale_dims, scale_data_fp16));
        break;
      }
      case OperandDataType::kFloat32: {
        std::vector<float> scale_data(checked_scale_size.ValueOrDie(), 1.0f);
        ASSIGN_OR_RETURN(scale_name,
                         CreateInitializer<float>(scale_dims, scale_data));
        break;
      }
      default:
        NOTREACHED() << "[WebNN] LayerNormalization only supports float32 "
                        "and float16 data type.";
    }

    input_names.push_back(scale_name.c_str());
  }

  std::string bias_name;
  if (layer_normalization.bias_operand_id) {
    bias_name = GetOperandNameById(layer_normalization.bias_operand_id.value());
    input_names.push_back(bias_name.c_str());
  }

  ScopedOrtOpAttrPtr attr_axis = model_editor_.CreateAttribute(
      /*name=*/"axis", base::checked_cast<int64_t>(axis));
  ScopedOrtOpAttrPtr attr_epsilon = model_editor_.CreateAttribute(
      /*name=*/"epsilon", layer_normalization.epsilon);

  std::array<OrtOpAttr*, 2> attributes = {attr_axis.Release(),
                                          attr_epsilon.Release()};

  model_editor_.AddNode(kOpTypeLayerNormalization, node_name, input_names,
                        output_names, attributes);

  return base::ok();
}

void GraphBuilderOrt::AddLeakyReluOperation(
    const mojom::LeakyRelu& leaky_relu) {
  const std::string node_name = GenerateNextOperationName(leaky_relu.label);
  const std::string input_name =
      GetOperandNameById(leaky_relu.input_operand_id);
  const std::string output_name =
      GetOperandNameById(leaky_relu.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"alpha", leaky_relu.alpha)
          .Release()};
  model_editor_.AddNode(kOpTypeLeakyRelu, node_name, input_names, output_names,
                        attributes);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddLinearOperation(const mojom::Linear& linear) {
  // Emulate a linear operation whose calculation follows the expression `alpha
  // * x + beta`.
  const mojom::Operand& input_operand = GetOperand(linear.input_operand_id);
  const OperandDataType input_data_type = input_operand.descriptor.data_type();

  std::string alpha_name;
  switch (input_data_type) {
    case OperandDataType::kFloat16: {
      ASSIGN_OR_RETURN(alpha_name,
                       CreateScalarInitializer<uint16_t>(
                           fp16_ieee_from_fp32_value(linear.alpha)));
      break;
    }
    case OperandDataType::kFloat32: {
      ASSIGN_OR_RETURN(alpha_name,
                       CreateScalarInitializer<float>(linear.alpha));
      break;
    }
    default:
      NOTREACHED()
          << "[WebNN] Linear only supports float32 and float16 data type.";
  }
  const std::string linear_mul_label =
      base::JoinString({linear.label, "mul"}, kUnderscore);
  const std::string mul_node_name = GenerateNextOperationName(linear_mul_label);
  const std::string input_name = GetOperandNameById(linear.input_operand_id);
  std::array<const char*, 2> mul_input_names = {input_name.c_str(),
                                                alpha_name.c_str()};
  const std::string mul_output_name = GenerateNextOperandName();
  std::array<const char*, 1> mul_output_names = {mul_output_name.c_str()};
  model_editor_.AddNode(kOpTypeMul, mul_node_name, mul_input_names,
                        mul_output_names);

  std::string beta_name;
  switch (input_data_type) {
    case OperandDataType::kFloat16: {
      ASSIGN_OR_RETURN(beta_name, CreateScalarInitializer<uint16_t>(
                                      fp16_ieee_from_fp32_value(linear.beta)));
      break;
    }
    case OperandDataType::kFloat32: {
      ASSIGN_OR_RETURN(beta_name, CreateScalarInitializer<float>(linear.beta));
      break;
    }
    default:
      NOTREACHED()
          << "[WebNN] Linear only supports float32 and float16 data type.";
  }
  const std::string linear_add_label =
      base::JoinString({linear.label, "add"}, kUnderscore);
  const std::string add_node_name = GenerateNextOperationName(linear_add_label);
  std::array<const char*, 2> add_input_names = {mul_output_name.c_str(),
                                                beta_name.c_str()};
  const std::string output_name = GetOperandNameById(linear.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};
  model_editor_.AddNode(kOpTypeAdd, add_node_name, add_input_names,
                        output_names);

  return base::ok();
}

void GraphBuilderOrt::AddMatMulOperation(const mojom::Matmul& matmul) {
  const std::string node_name = GenerateNextOperationName(matmul.label);
  const std::string input_a_name = GetOperandNameById(matmul.a_operand_id);
  const std::string input_b_name = GetOperandNameById(matmul.b_operand_id);
  const std::string output_name = GetOperandNameById(matmul.output_operand_id);

  std::array<const char*, 2> input_names = {input_a_name.c_str(),
                                            input_b_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeMatMul, node_name, input_names, output_names);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddPadOperation(const mojom::Pad& pad) {
  const std::string node_name = GenerateNextOperationName(pad.label);
  const std::string input_name = GetOperandNameById(pad.input_operand_id);
  const OperandDataType input_data_type =
      GetOperand(pad.output_operand_id).descriptor.data_type();
  std::vector<const char*> input_names = {input_name.c_str()};

  CHECK_EQ(pad.beginning_padding.size(), pad.ending_padding.size());
  auto padding_length =
      pad.beginning_padding.size() + pad.ending_padding.size();

  // paddings is an operand with data type int64, not an attribute.
  std::vector<int64_t> paddings;
  paddings.reserve(padding_length);
  std::ranges::transform(
      pad.beginning_padding, std::back_inserter(paddings),
      [](uint32_t value) { return base::checked_cast<int64_t>(value); });
  std::ranges::transform(
      pad.ending_padding, std::back_inserter(paddings),
      [](uint32_t value) { return base::checked_cast<int64_t>(value); });

  std::vector<uint32_t> paddings_dims = {
      base::checked_cast<uint32_t>(padding_length)};
  ASSIGN_OR_RETURN(const std::string paddings_name,
                   CreateInitializer<int64_t>(paddings_dims, paddings));
  input_names.push_back(paddings_name.c_str());

  std::string mode;
  std::string constant_name;
  switch (pad.mode->which()) {
    case mojom::PaddingMode::Tag::kConstant: {
      mode = "constant";
      auto constant = pad.mode->get_constant()->value;
      switch (input_data_type) {
        case OperandDataType::kFloat32: {
          ASSIGN_OR_RETURN(constant_name, CreateScalarInitializer(constant));
          break;
        }
        case OperandDataType::kFloat16: {
          ASSIGN_OR_RETURN(
              constant_name,
              CreateScalarInitializer(fp16_ieee_from_fp32_value(constant)));
          break;
        }
        default:
          NOTREACHED() << "[WebNN] Pad only supports float32 "
                          "and float16 data type.";
      }
      input_names.push_back(constant_name.c_str());
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

  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"mode", mode).Release()};

  const std::string output_name = GetOperandNameById(pad.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypePad, node_name, input_names, output_names,
                        attributes);

  return base::ok();
}

void GraphBuilderOrt::AddPool2dOperation(const mojom::Pool2d& pool2d) {
  std::array<int64_t, 2> dilations = {
      base::checked_cast<int64_t>(pool2d.dilations->height),
      base::checked_cast<int64_t>(pool2d.dilations->width)};
  ScopedOrtOpAttrPtr attr_dilations =
      model_editor_.CreateAttribute(/*name=*/"dilations", dilations);

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(pool2d.strides->height),
      base::checked_cast<int64_t>(pool2d.strides->width)};
  ScopedOrtOpAttrPtr attr_strides =
      model_editor_.CreateAttribute(/*name=*/"strides", strides);

  std::array<int64_t, 2> window_dimensions = {
      base::checked_cast<int64_t>(pool2d.window_dimensions->height),
      base::checked_cast<int64_t>(pool2d.window_dimensions->width)};
  ScopedOrtOpAttrPtr attr_kernel_shape = model_editor_.CreateAttribute(
      /*name=*/"kernel_shape", window_dimensions);

  // ONNX's pads are [beginning_height, beginning_width, ending_height,
  // ending_width]
  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(pool2d.padding->beginning->height),
      base::checked_cast<int64_t>(pool2d.padding->beginning->width),
      base::checked_cast<int64_t>(pool2d.padding->ending->height),
      base::checked_cast<int64_t>(pool2d.padding->ending->width)};
  ScopedOrtOpAttrPtr attr_pads =
      model_editor_.CreateAttribute(/*name=*/"pads", pads);

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
  ScopedOrtOpAttrPtr attr_ceil_mode =
      model_editor_.CreateAttribute(/*name=*/"ceil_mode", ceil_mode);

  std::vector<OrtOpAttr*> attributes = {
      attr_dilations.Release(), attr_strides.Release(),
      attr_kernel_shape.Release(), attr_pads.Release(),
      attr_ceil_mode.Release()};

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
          model_editor_.CreateAttribute(/*name=*/"p", static_cast<int64_t>(2))
              .Release());
      break;
    }
  }

  const std::string node_name = GenerateNextOperationName(pool2d.label);
  const std::string input_name = GetOperandNameById(pool2d.input_operand_id);
  const std::string output_name = GetOperandNameById(pool2d.output_operand_id);
  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(op_type, node_name, input_names, output_names,
                        attributes);
}

void GraphBuilderOrt::AddPreluOperation(const mojom::Prelu& prelu) {
  const std::string node_name = GenerateNextOperationName(prelu.label);
  const std::string input_name = GetOperandNameById(prelu.input_operand_id);
  // ONNX Prelu requires slope's shape must be unidirectional broadcastable to
  // input when the shape of slope is smaller than the input. While WebNN allows
  // input and slope to be bidirectionally broadcastable.
  // TODO(https://github.com/shiyi9801/chromium/issues/153): Consider to emulate
  // if slope is not unidirectional broadcastable to input.
  const std::string slope_name = GetOperandNameById(prelu.slope_operand_id);
  const std::string output_name = GetOperandNameById(prelu.output_operand_id);

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            slope_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypePRelu, node_name, input_names, output_names);
}

// TODO(https://github.com/shiyi9801/chromium/issues/53): 'reduceSumSquare
// float32 1D tensor with empty axes' test case fails
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddReduceOperation(const mojom::Reduce& reduce) {
  const std::string input_name = GetOperandNameById(reduce.input_operand_id);
  std::vector<const char*> input_names = {input_name.c_str()};

  std::vector<int64_t> axes(reduce.axes.begin(), reduce.axes.end());
  std::string axes_name;
  if (!axes.empty()) {
    // axes is an operand with data type int64, not an attribute.
    std::vector<uint32_t> axes_dims = {
        base::checked_cast<uint32_t>(axes.size())};
    ASSIGN_OR_RETURN(axes_name, CreateInitializer<int64_t>(axes_dims, axes));
    input_names.push_back(axes_name.c_str());
  }

  int64_t keepdims = reduce.keep_dimensions ? 1 : 0;
  ScopedOrtOpAttrPtr attr_keepdims =
      model_editor_.CreateAttribute(/*name=*/"keepdims", keepdims);

  // According to
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-reduce, if
  // axes is empty, the operation is a noop, no dimensions are reduced.
  int64_t noop_with_empty_axes = 1;
  ScopedOrtOpAttrPtr attr_noop_with_empty_axes = model_editor_.CreateAttribute(
      /*name=*/"noop_with_empty_axes", noop_with_empty_axes);

  const std::string node_name = GenerateNextOperationName(reduce.label);
  const std::string output_name = GetOperandNameById(reduce.output_operand_id);
  std::array<const char*, 1> output_names = {output_name.c_str()};
  std::string reduce_op_type = MapReduceKindToOrtOpType(reduce.kind);
  std::array<OrtOpAttr*, 2> attributes = {attr_keepdims.Release(),
                                          attr_noop_with_empty_axes.Release()};
  model_editor_.AddNode(reduce_op_type, node_name, input_names, output_names,
                        attributes);

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddResample2dOperation(const mojom::Resample2d& resample2d) {
  const std::string node_name = GenerateNextOperationName(resample2d.label);
  const std::string input_name =
      GetOperandNameById(resample2d.input_operand_id);
  const std::string output_name =
      GetOperandNameById(resample2d.output_operand_id);
  const std::vector<uint32_t>& input_shape =
      GetOperand(resample2d.input_operand_id).descriptor.shape();
  const std::vector<uint32_t>& output_shape =
      GetOperand(resample2d.output_operand_id).descriptor.shape();
  std::vector<const char*> input_names = {input_name.c_str()};

  // ROI only takes effect when ONNX Resize op's attribute
  // coordinate_transformation_mode is “tf_crop_and_resize” and the default
  // value of coordinate_transformation_mode is "half_pixel". Currently, WebNN
  // only supports "half_pixel".
  const std::string roi_name = "";
  input_names.push_back(roi_name.c_str());

  // When axes != [2, 3], webnn blink side will insert transpose before and
  // after resample2d -
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.cc;l=1438.
  CHECK_EQ(resample2d.axes.size(), 2u);
  CHECK_EQ(resample2d.axes[0], 2u);
  CHECK_EQ(resample2d.axes[1], 3u);

  CHECK_EQ(input_shape.size(), 4u);
  std::string scales_name;
  std::string sizes_name;
  // Here we using default axes([0,..., R-1]) due to this issue-
  // https://github.com/shiyi9801/chromium/issues/92.
  if (resample2d.scales) {
    // The number of elements of scales should be the same as the rank of input
    // or axes.
    std::array<float, 4> scales_data = {1, 1, resample2d.scales->at(0),
                                        resample2d.scales->at(1)};
    ASSIGN_OR_RETURN(scales_name, CreateInitializer<float>({4}, scales_data));
    sizes_name = "";
  } else {
    // The number of elements of sizes should be the same as the rank of input
    // or axes.
    CHECK_EQ(output_shape.size(), 4u);
    std::array<int64_t, 4> sizes_data = {
        base::checked_cast<int64_t>(output_shape[0]),
        base::checked_cast<int64_t>(output_shape[1]),
        base::checked_cast<int64_t>(output_shape[2]),
        base::checked_cast<int64_t>(output_shape[3])};
    ASSIGN_OR_RETURN(sizes_name, CreateInitializer<int64_t>({4}, sizes_data));
    scales_name = "";
  }
  input_names.push_back(scales_name.c_str());
  input_names.push_back(sizes_name.c_str());

  std::string mode;
  switch (resample2d.mode) {
    case mojom::Resample2d::InterpolationMode::kLinear:
      mode = "linear";
      break;
    case mojom::Resample2d::InterpolationMode::kNearestNeighbor:
      mode = "nearest";
      break;
  }
  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"mode", mode).Release()};

  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeResample2d, node_name, input_names, output_names,
                        attributes);

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddReshapeOperation(const mojom::Reshape& reshape) {
  const std::string node_name = GenerateNextOperationName(reshape.label);
  const std::string input_name = GetOperandNameById(reshape.input_operand_id);
  const std::string output_name = GetOperandNameById(reshape.output_operand_id);

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
  ASSIGN_OR_RETURN(const std::string shape_name,
                   CreateInitializer<int64_t>(shape_dims, shape_values));

  std::array<const char*, 2> input_names = {input_name.c_str(),
                                            shape_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeReshape, node_name, input_names, output_names);

  return base::ok();
}

// Emulate it by slice operation since there is no reserve operation in ONNX.
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddReverseOperation(const mojom::Reverse& reverse) {
  const std::string node_name = GenerateNextOperationName(reverse.label);
  const std::string input_name = GetOperandNameById(reverse.input_operand_id);
  const std::string output_name = GetOperandNameById(reverse.output_operand_id);

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
  ASSIGN_OR_RETURN(std::string axes_name,
                   CreateInitializer<int64_t>(axes_dims, reverse_axes));

  return AddSliceNode(node_name, input_name, output_name, axes_name, starts,
                      ends, steps);
}

void GraphBuilderOrt::AddScatterElementsOperation(
    const mojom::ScatterElements& scatter_elements) {
  const std::string node_name =
      GenerateNextOperationName(scatter_elements.label);
  const std::string input_name =
      GetOperandNameById(scatter_elements.input_operand_id);
  const std::string indices_name =
      GetOperandNameById(scatter_elements.indices_operand_id);
  const std::string updates_name =
      GetOperandNameById(scatter_elements.updates_operand_id);
  const std::string output_name =
      GetOperandNameById(scatter_elements.output_operand_id);

  std::string cast_indices_name;
  const OperandDataType indices_data_type =
      GetOperand(scatter_elements.indices_operand_id).descriptor.data_type();

  // ONNX ScatterElements only supports int32 and int64 indices.
  switch (indices_data_type) {
    case OperandDataType::kInt32:
    case OperandDataType::kInt64: {
      cast_indices_name = indices_name;
      break;
    }
    case OperandDataType::kUint32: {
      cast_indices_name = GenerateNextOperandName();
      AppendCast(
          indices_name, cast_indices_name,
          ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
      break;
    }
    default:
      NOTREACHED() << "[WebNN] ScatterElements only supports int32, uint32 and "
                      "int64 indices.";
  }

  ScopedOrtOpAttrPtr attr_axis = model_editor_.CreateAttribute(
      /*name=*/"axis", base::checked_cast<int64_t>(scatter_elements.axis));
  std::array<OrtOpAttr*, 1> attributes = {attr_axis.Release()};

  std::array<const char*, 3> input_names = {
      input_name.c_str(), cast_indices_name.c_str(), updates_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeScatterElements, node_name, input_names,
                        output_names, attributes);
}

void GraphBuilderOrt::AddScatterNDOperation(
    const mojom::ScatterND& scatter_nd) {
  const std::string node_name = GenerateNextOperationName(scatter_nd.label);
  const std::string input_name =
      GetOperandNameById(scatter_nd.input_operand_id);
  const std::string indices_name =
      GetOperandNameById(scatter_nd.indices_operand_id);
  const std::string updates_name =
      GetOperandNameById(scatter_nd.updates_operand_id);
  const std::string output_name =
      GetOperandNameById(scatter_nd.output_operand_id);

  std::string int64_indices_name;
  const OperandDataType indices_data_type =
      GetOperand(scatter_nd.indices_operand_id).descriptor.data_type();

  // ONNX ScatterND only supports int64 indices.
  switch (indices_data_type) {
    case OperandDataType::kInt64: {
      int64_indices_name = indices_name;
      break;
    }
    case OperandDataType::kInt32:
    case OperandDataType::kUint32: {
      int64_indices_name = GenerateNextOperandName();
      AppendCast(
          indices_name, int64_indices_name,
          ONNXTensorElementDataType::ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
      break;
    }
    default:
      NOTREACHED()
          << "[WebNN] ScatterND only supports int32, uint32 and int64 indices.";
  }

  std::array<const char*, 3> input_names = {
      input_name.c_str(), int64_indices_name.c_str(), updates_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeScatterND, node_name, input_names, output_names);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddSliceOperation(const mojom::Slice& slice) {
  const std::string node_name = GenerateNextOperationName(slice.label);
  const std::string input_name = GetOperandNameById(slice.input_operand_id);
  const std::string output_name = GetOperandNameById(slice.output_operand_id);

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
  const std::string axes_name = "";
  return AddSliceNode(node_name, input_name, output_name, axes_name, starts,
                      ends, steps);
}

void GraphBuilderOrt::AddSoftmaxOperation(const mojom::Softmax& softmax) {
  const std::string node_name = GenerateNextOperationName(softmax.label);
  const std::string input_name = GetOperandNameById(softmax.input_operand_id);
  const std::string output_name = GetOperandNameById(softmax.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_
          .CreateAttribute(/*name=*/"axis", static_cast<int64_t>(softmax.axis))
          .Release()};

  model_editor_.AddNode(kOpTypeSoftmax, node_name, input_names, output_names,
                        attributes);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddTileOperation(const mojom::Tile& tile) {
  const std::string node_name = GenerateNextOperationName(tile.label);
  const std::string input_name = GetOperandNameById(tile.input_operand_id);
  const std::string output_name = GetOperandNameById(tile.output_operand_id);

  std::vector<int64_t> repetitions(tile.repetitions.begin(),
                                   tile.repetitions.end());
  ASSIGN_OR_RETURN(
      const std::string repeats_name,
      CreateInitializer<int64_t>(
          {base::checked_cast<uint32_t>(repetitions.size())}, repetitions));

  std::array<const char*, 2> input_names = {input_name.data(),
                                            repeats_name.data()};
  std::array<const char*, 1> output_names = {output_name.c_str()};
  model_editor_.AddNode(kOpTypeTile, node_name, input_names, output_names);

  return base::ok();
}

void GraphBuilderOrt::AddTransposeOperation(const mojom::Transpose& transpose) {
  const std::string node_name = GenerateNextOperationName(transpose.label);
  const std::string input_name = GetOperandNameById(transpose.input_operand_id);
  const std::string output_name =
      GetOperandNameById(transpose.output_operand_id);

  std::array<const char*, 1> input_names = {input_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  std::vector<int64_t> permutation(transpose.permutation.begin(),
                                   transpose.permutation.end());
  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_.CreateAttribute(/*name=*/"perm", permutation).Release()};

  model_editor_.AddNode(kOpTypeTranspose, node_name, input_names, output_names,
                        attributes);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddSplitOperation(const mojom::Split& split) {
  const std::string node_name = GenerateNextOperationName(split.label);
  const std::string input_name = GetOperandNameById(split.input_operand_id);

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
      const std::string split_name,
      CreateInitializer<int64_t>(
          {base::checked_cast<uint32_t>(split_sizes.size())}, split_sizes));
  base::FixedArray<const char*> input_names = {input_name.c_str(),
                                               split_name.c_str()};

  base::FixedArray<std::string> output_names_string(output_nums);
  base::FixedArray<const char*> output_names(output_nums);
  for (size_t i = 0; i < output_nums; i++) {
    output_names_string[i] = GetOperandNameById(split.output_operand_ids[i]);
    output_names[i] = output_names_string[i].c_str();
  }

  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_
          .CreateAttribute(/*name=*/"axis",
                           base::checked_cast<int64_t>(split.axis))
          .Release()};

  model_editor_.AddNode(kOpTypeSplit, node_name, input_names, output_names,
                        attributes);

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderOrt::AddTriangularOperation(const mojom::Triangular& triangular) {
  const std::string node_name = GenerateNextOperationName(triangular.label);
  const std::string input_name =
      GetOperandNameById(triangular.input_operand_id);
  const std::string output_name =
      GetOperandNameById(triangular.output_operand_id);
  std::vector<const char*> input_names = {input_name.c_str()};

  // K is an operand with data type int64, not an attribute.;
  ASSIGN_OR_RETURN(const std::string k_name,
                   CreateScalarInitializer<int64_t>(
                       static_cast<int64_t>(triangular.diagonal)));
  input_names.push_back(k_name.c_str());

  std::array<const char*, 1> output_names = {output_name.c_str()};

  std::array<OrtOpAttr*, 1> attributes = {
      model_editor_
          .CreateAttribute(/*name=*/"upper",
                           static_cast<int64_t>(triangular.upper))
          .Release()};

  model_editor_.AddNode(kOpTypeTriangular, node_name, input_names, output_names,
                        attributes);

  return base::ok();
}

void GraphBuilderOrt::AddWhereOperation(const mojom::Where& where) {
  const std::string node_name = GenerateNextOperationName(where.label);
  // ONNX only supports bool data type for the condition input of Where, insert
  // a Cast node to convert the condition input to bool.
  std::string condition_name = GetOperandNameById(where.condition_operand_id);
  condition_name =
      PrependCast(condition_name, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);

  const std::string true_value_name =
      GetOperandNameById(where.true_value_operand_id);
  const std::string false_value_name =
      GetOperandNameById(where.false_value_operand_id);
  const std::string output_name = GetOperandNameById(where.output_operand_id);
  std::array<const char*, 3> input_names = {condition_name.c_str(),
                                            true_value_name.c_str(),
                                            false_value_name.c_str()};
  std::array<const char*, 1> output_names = {output_name.c_str()};

  model_editor_.AddNode(kOpTypeWhere, node_name, input_names, output_names);
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
        AddEluOperation(*operation->get_elu());
        break;
      }
      case mojom::Operation::Tag::kExpand: {
        RETURN_IF_ERROR(AddExpandOperation(*operation->get_expand()));
        break;
      }
      case mojom::Operation::Tag::kGather: {
        AddGatherOperation(*operation->get_gather());
        break;
      }
      case mojom::Operation::Tag::kGatherElements: {
        AddGatherElementsOperation(*operation->get_gather_elements());
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
      case mojom::Operation::Tag::kGru:
      case mojom::Operation::Tag::kGruCell:
      case mojom::Operation::Tag::kLstm:
      case mojom::Operation::Tag::kLstmCell:
        return NewNotSupportedError("op is not supported.");
    }
  }
  // Add outputs.
  for (uint64_t output_id : graph_info_->output_operands) {
    AddOutput(output_id);
  }

  return model_editor_.BuildAndTakeModelInfo();
}

}  // namespace webnn::ort
