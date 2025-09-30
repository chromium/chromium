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
#include "base/types/expected_macros.h"
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
constexpr base::cstring_view kOpTypeIsNaN = "IsNaN";
constexpr base::cstring_view kOpTypeIsInfinite = "IsInf";
constexpr base::cstring_view kOpTypeLogicalNot = "Not";
constexpr base::cstring_view kOpTypeNeg = "Neg";
constexpr base::cstring_view kOpTypeRoundEven = "Round";
constexpr base::cstring_view kOpTypeSign = "Sign";
constexpr base::cstring_view kOpTypeSin = "Sin";
constexpr base::cstring_view kOpTypeTan = "Tan";
constexpr base::cstring_view kOpTypeIdentity = "Identity";
constexpr base::cstring_view kOpTypeSqrt = "Sqrt";
constexpr base::cstring_view kOpTypeErf = "Erf";
constexpr base::cstring_view kOpTypeReciprocal = "Reciprocal";
constexpr base::cstring_view kOpTypeCast = "Cast";

constexpr base::cstring_view kOpTypeBatchNormalization = "BatchNormalization";
constexpr base::cstring_view kOpTypeClamp = "Clip";
constexpr base::cstring_view kOpTypeConcat = "Concat";
constexpr base::cstring_view kOpTypeConv2d = "Conv";
constexpr base::cstring_view kOpTypeConvTranspose2d = "ConvTranspose";
constexpr base::cstring_view kOpTypeCumulativeSum = "CumSum";
constexpr base::cstring_view kOpTypeDequantizeLinear = "DequantizeLinear";
constexpr base::cstring_view kOpTypeElu = "Elu";
constexpr base::cstring_view kOpTypeExpand = "Expand";
constexpr base::cstring_view kOpTypeGather = "Gather";
constexpr base::cstring_view kOpTypeGatherElements = "GatherElements";
constexpr base::cstring_view kOpTypeGatherND = "GatherND";
constexpr base::cstring_view kOpTypeGelu = "Gelu";
constexpr base::cstring_view kOpTypeGemm = "Gemm";
constexpr base::cstring_view kOpTypeGru = "GRU";
constexpr base::cstring_view kOpTypeHardSigmoid = "HardSigmoid";
constexpr base::cstring_view kOpTypeHardSwish = "HardSwish";
constexpr base::cstring_view kOpTypeInstanceNormalization =
    "InstanceNormalization";
constexpr base::cstring_view kOpTypeLayerNormalization = "LayerNormalization";
constexpr base::cstring_view kOpTypeLeakyRelu = "LeakyRelu";
constexpr base::cstring_view kOpTypeLstm = "LSTM";
constexpr base::cstring_view kOpTypeMatMul = "MatMul";
constexpr base::cstring_view kOpTypePad = "Pad";
constexpr base::cstring_view kOpTypePRelu = "PRelu";
constexpr base::cstring_view kOpTypeQuantizeLinear = "QuantizeLinear";
constexpr base::cstring_view kOpTypeRelu = "Relu";
constexpr base::cstring_view kOpTypeResize = "Resize";
constexpr base::cstring_view kOpTypeReshape = "Reshape";
constexpr base::cstring_view kOpTypeScatterElements = "ScatterElements";
constexpr base::cstring_view kOpTypeScatterND = "ScatterND";
constexpr base::cstring_view kOpTypeSigmoid = "Sigmoid";
constexpr base::cstring_view kOpTypeSlice = "Slice";
constexpr base::cstring_view kOpTypeSoftmax = "Softmax";
constexpr base::cstring_view kOpTypeSoftplus = "Softplus";
constexpr base::cstring_view kOpTypeSoftsign = "Softsign";
constexpr base::cstring_view kOpTypeSplit = "Split";
constexpr base::cstring_view kOpTypeTanh = "Tanh";
constexpr base::cstring_view kOpTypeTile = "Tile";
constexpr base::cstring_view kOpTypeTranspose = "Transpose";
constexpr base::cstring_view kOpTypeTriangular = "Trilu";
constexpr base::cstring_view kOpTypeWhere = "Where";

// Pooling operations
constexpr base::cstring_view kOpTypeAveragePool2d = "AveragePool";
constexpr base::cstring_view kOpTypeMaxPool2d = "MaxPool";
constexpr base::cstring_view kOpTypeLpPool2d = "LpPool";

// Reduction operations
constexpr base::cstring_view kOpTypeReduceL1 = "ReduceL1";
constexpr base::cstring_view kOpTypeReduceL2 = "ReduceL2";
constexpr base::cstring_view kOpTypeReduceLogSum = "ReduceLogSum";
constexpr base::cstring_view kOpTypeReduceLogSumExp = "ReduceLogSumExp";
constexpr base::cstring_view kOpTypeReduceMax = "ReduceMax";
constexpr base::cstring_view kOpTypeReduceMean = "ReduceMean";
constexpr base::cstring_view kOpTypeReduceMin = "ReduceMin";
constexpr base::cstring_view kOpTypeReduceProd = "ReduceProd";
constexpr base::cstring_view kOpTypeReduceSum = "ReduceSum";
constexpr base::cstring_view kOpTypeReduceSumSquare = "ReduceSumSquare";

// Attributes
constexpr base::cstring_view kAttrActivations = "activations";
constexpr base::cstring_view kAttrAlpha = "alpha";
constexpr base::cstring_view kAttrAxis = "axis";
constexpr base::cstring_view kAttrBeta = "beta";
constexpr base::cstring_view kAttrBlockSize = "block_size";
constexpr base::cstring_view kAttrCeilMode = "ceil_mode";
constexpr base::cstring_view kAttrDilations = "dilations";
constexpr base::cstring_view kAttrDirection = "direction";
constexpr base::cstring_view kAttrEpsilon = "epsilon";
constexpr base::cstring_view kAttrExclusive = "exclusive";
constexpr base::cstring_view kAttrGroup = "group";
constexpr base::cstring_view kAttrHiddenSize = "hidden_size";
constexpr base::cstring_view kAttrKeepDims = "keepdims";
constexpr base::cstring_view kAttrKernelShape = "kernel_shape";
constexpr base::cstring_view kAttrLinearBeforeReset = "linear_before_reset";
constexpr base::cstring_view kAttrMode = "mode";
constexpr base::cstring_view kAttrNoopWithEmptyAxes = "noop_with_empty_axes";
constexpr base::cstring_view kAttrNumOutputs = "num_outputs";
constexpr base::cstring_view kAttrOutputPadding = "output_padding";
constexpr base::cstring_view kAttrP = "p";
constexpr base::cstring_view kAttrPads = "pads";
constexpr base::cstring_view kAttrPerm = "perm";
constexpr base::cstring_view kAttrReverse = "reverse";
constexpr base::cstring_view kAttrStrides = "strides";
constexpr base::cstring_view kAttrTo = "to";
constexpr base::cstring_view kAttrTransA = "transA";
constexpr base::cstring_view kAttrTransB = "transB";
constexpr base::cstring_view kAttrUpper = "upper";

constexpr base::cstring_view kInserted = "Inserted";
constexpr base::cstring_view kToEmulate = "ToEmulate";
constexpr base::cstring_view kUnderscore = "_";

std::string GetOperandName(std::string_view name, OperandId id) {
  return base::JoinString({name, base::NumberToString(id.value())},
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

void CheckReduceInputSupported(const DataTypeLimits& data_type_limits,
                               mojom::Reduce::Kind kind,
                               const OperandDescriptor& input_descriptor) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      CHECK(data_type_limits.reduce_l1_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kL2:
      CHECK(data_type_limits.reduce_l2_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kLogSum:
      CHECK(data_type_limits.reduce_log_sum_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kLogSumExp:
      CHECK(
          data_type_limits.reduce_log_sum_exp_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kMax:
      CHECK(data_type_limits.reduce_max_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kMean:
      CHECK(data_type_limits.reduce_mean_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kMin:
      CHECK(data_type_limits.reduce_min_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kProduct:
      CHECK(data_type_limits.reduce_product_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kSum:
      CHECK(data_type_limits.reduce_sum_input.Supports(input_descriptor));
      break;
    case mojom::Reduce::Kind::kSumSquare:
      CHECK(
          data_type_limits.reduce_sum_square_input.Supports(input_descriptor));
      break;
  }
}

base::cstring_view MapReduceKindToOrtOpType(mojom::Reduce::Kind kind) {
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

const std::vector<base::cstring_view> GetRecurrentNetworkActivations(
    std::vector<mojom::RecurrentNetworkActivation> activations,
    bool is_bidirectional) {
  std::vector<base::cstring_view> activation_list;
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

const base::cstring_view GetRecurrentNetworkDirection(
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

}  // namespace

// static
std::unique_ptr<ModelEditor::ModelInfo> GraphBuilderOrt::CreateAndBuild(
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
      context_properties_(std::move(context_properties)) {}

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

std::string GraphBuilderOrt::GenerateEmulatedOpLabel(
    base::cstring_view op_type,
    std::string_view original_label,
    std::string_view additional_tag) {
  if (additional_tag.empty()) {
    return base::JoinString({kInserted, op_type, kToEmulate, original_label},
                            kUnderscore);
  } else {
    return base::JoinString(
        {kInserted, op_type, additional_tag, kToEmulate, original_label},
        kUnderscore);
  }
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

std::string GraphBuilderOrt::CreateInitializerForFloat(
    OperandDataType data_type,
    base::span<const uint32_t> shape,
    float value) {
  base::CheckedNumeric<size_t> checked_operand_size =
      std::accumulate(shape.begin(), shape.end(),
                      base::CheckedNumeric<size_t>(1), std::multiplies());
  size_t operand_size = checked_operand_size.ValueOrDie();
  base::FixedArray<int64_t> int64_shape(shape.begin(), shape.end());
  switch (data_type) {
    case OperandDataType::kFloat32: {
      base::FixedArray<float> data(operand_size, value);
      return CreateInitializer<float>(int64_shape, data);
    }
    case OperandDataType::kFloat16: {
      base::FixedArray<uint16_t> data(operand_size,
                                      fp16_ieee_from_fp32_value(value));
      return CreateInitializer<uint16_t>(int64_shape, data);
    }
    case OperandDataType::kInt32: {
      base::FixedArray<int32_t> data(operand_size,
                                     base::saturated_cast<int32_t>(value));
      return CreateInitializer<int32_t>(int64_shape, data);
    }
    case OperandDataType::kUint32: {
      base::FixedArray<uint32_t> data(operand_size,
                                      base::saturated_cast<uint32_t>(value));
      return CreateInitializer<uint32_t>(int64_shape, data);
    }
    case OperandDataType::kInt64: {
      base::FixedArray<int64_t> data(operand_size,
                                     base::saturated_cast<int64_t>(value));
      return CreateInitializer<int64_t>(int64_shape, data);
    }
    case OperandDataType::kUint64: {
      base::FixedArray<uint64_t> data(operand_size,
                                      base::saturated_cast<uint64_t>(value));
      return CreateInitializer<uint64_t>(int64_shape, data);
    }
    case OperandDataType::kInt8: {
      base::FixedArray<int8_t> data(operand_size,
                                    base::saturated_cast<int8_t>(value));
      return CreateInitializer<int8_t>(int64_shape, data);
    }
    case OperandDataType::kUint8: {
      base::FixedArray<uint8_t> data(operand_size,
                                     base::saturated_cast<uint8_t>(value));
      return CreateInitializer<uint8_t>(int64_shape, data);
    }
    case OperandDataType::kInt4:
    case OperandDataType::kUint4: {
      NOTREACHED();
    }
  }
}

std::string GraphBuilderOrt::CreateScalarInitializer(OperandDataType data_type,
                                                     const MLNumber& value) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return CreateScalarInitializer(value.AsFloat32());
    case OperandDataType::kFloat16:
      return CreateScalarInitializer(value.AsFloat16());
    case OperandDataType::kInt32:
      return CreateScalarInitializer(value.AsInt32());
    case OperandDataType::kUint32:
      return CreateScalarInitializer(value.AsUint32());
    case OperandDataType::kInt64:
      return CreateScalarInitializer(value.AsInt64());
    case OperandDataType::kUint64:
      return CreateScalarInitializer(value.AsUint64());
    case OperandDataType::kInt8:
      return CreateScalarInitializer(value.AsInt8());
    case OperandDataType::kUint8:
      return CreateScalarInitializer(value.AsUint8());
    case OperandDataType::kInt4:
    case OperandDataType::kUint4: {
      NOTREACHED();
    }
  }
}

std::string GraphBuilderOrt::CreateOneInitializer(
    OperandDataType data_type,
    base::span<const uint32_t> shape) {
  return CreateInitializerForFloat(data_type, shape, 1.0f);
}

std::string GraphBuilderOrt::CreateZeroInitializer(
    OperandDataType data_type,
    base::span<const uint32_t> shape) {
  return CreateInitializerForFloat(data_type, shape, 0.0f);
}

std::string GraphBuilderOrt::TransposeRnnWeightOrBiasLayout(
    base::cstring_view weight_or_bias,
    base::span<const uint32_t> permutation) {
  size_t num_gates = permutation.size();

  // Use Split operator to split the weight/bias into num_gates slices.
  std::vector<std::string> gate_names;
  gate_names.reserve(num_gates);
  for (size_t i = 0; i < num_gates; i++) {
    gate_names.push_back(GenerateOperandName());
  }
  constexpr int64_t axis = 1;
  std::array<ScopedOrtOpAttr, 2> split_attrs = {
      model_editor_.CreateAttribute(kAttrAxis, axis),
      model_editor_.CreateAttribute(kAttrNumOutputs,
                                    static_cast<int64_t>(num_gates))};
  std::array<const char*, 1> split_inputs = {weight_or_bias.c_str()};
  std::vector<const char*> split_outputs;
  split_outputs.reserve(num_gates);
  for (const auto& gate_name : gate_names) {
    split_outputs.push_back(gate_name.c_str());
  }
  std::string split_node_name = GenerateNodeName(
      base::JoinString({kInserted, kOpTypeSplit}, kUnderscore));
  model_editor_.AddNode(kOpTypeSplit, split_node_name, split_inputs,
                        split_outputs, split_attrs);

  // Use Concat operator to concatenate the slices in the order of permutation.
  std::vector<const char*> concat_inputs;
  concat_inputs.reserve(num_gates);
  for (uint32_t index : permutation) {
    concat_inputs.push_back(gate_names[index].c_str());
  }
  std::string concat_output = GenerateOperandName();
  std::array<const char*, 1> concat_outputs = {concat_output.c_str()};
  std::array<ScopedOrtOpAttr, 1> concat_attrs = {
      model_editor_.CreateAttribute(kAttrAxis, axis)};
  std::string concat_node_name = GenerateNodeName(
      base::JoinString({kInserted, kOpTypeConcat}, kUnderscore));
  model_editor_.AddNode(kOpTypeConcat, concat_node_name, concat_inputs,
                        concat_outputs, concat_attrs);

  return concat_output;
}

void GraphBuilderOrt::AddCastNode(base::cstring_view node_name,
                                  base::cstring_view input,
                                  base::cstring_view output,
                                  ONNXTensorElementDataType to_data_type) {
  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};
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

void GraphBuilderOrt::AddResizeNode(base::cstring_view node_name,
                                    base::cstring_view input,
                                    base::cstring_view scales,
                                    base::cstring_view sizes,
                                    base::cstring_view mode,
                                    base::cstring_view output) {
  // Skip the input roi, which only takes effect when the coordinate
  // transformation mode is set to "tf_crop_and_resize". Currently WebNN only
  // supports "half_pixel", which is the default mode.
  const std::string roi;
  std::array<const char*, 4> inputs = {input.c_str(), roi.c_str(),
                                       scales.c_str(), sizes.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrMode, mode)};

  model_editor_.AddNode(kOpTypeResize, node_name, inputs, outputs, attributes);
}

std::string GraphBuilderOrt::BlockwiseExpand(base::cstring_view input,
                                             base::span<const uint32_t> shape) {
  const std::string sizes = CreateInt64InitializerForUint32Array(shape);
  const std::string node_name = GenerateNodeName(
      base::JoinString({kInserted, kOpTypeResize}, kUnderscore));
  const std::string output = GenerateOperandName();
  AddResizeNode(node_name, input, /*scales=*/"", sizes,
                /*mode=*/"nearest", output);

  return output;
}

void GraphBuilderOrt::AddReshapeNode(base::cstring_view node_name,
                                     base::cstring_view input,
                                     base::cstring_view output,
                                     base::span<const uint32_t> shape) {
  // `new_shape` should be the name of an int64 tensor that specifies the
  // output's shape.
  const std::string new_shape = CreateInt64InitializerForUint32Array(shape);

  std::array<const char*, 2> inputs = {input.c_str(), new_shape.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeReshape, node_name, inputs, outputs);
}

std::string GraphBuilderOrt::CreateReshapeNode(
    base::cstring_view input,
    base::span<const uint32_t> shape) {
  const std::string output = GenerateOperandName();
  InsertReshapeNode(input, output, shape);
  return output;
}

void GraphBuilderOrt::InsertReshapeNode(base::cstring_view input,
                                        base::cstring_view output,
                                        base::span<const uint32_t> shape) {
  const std::string node_name = GenerateNodeName(
      base::JoinString({kInserted, kOpTypeReshape}, kUnderscore));
  AddReshapeNode(node_name, input, output, shape);
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

void GraphBuilderOrt::AddTransposeNode(base::cstring_view node_name,
                                       base::cstring_view input,
                                       base::cstring_view output,
                                       base::span<const uint32_t> perm_value) {
  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  base::FixedArray<int64_t> perm(perm_value.begin(), perm_value.end());
  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrPerm, perm)};
  model_editor_.AddNode(kOpTypeTranspose, node_name, inputs, outputs,
                        attributes);
}

std::string GraphBuilderOrt::CreateTransposeNode(
    base::cstring_view input,
    base::span<const uint32_t> perm_value) {
  const std::string node_name = GenerateNodeName(
      base::JoinString({kInserted, kOpTypeTranspose}, kUnderscore));
  const std::string output = GenerateOperandName();

  AddTransposeNode(node_name, input, output, perm_value);
  return output;
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
  CHECK(context_properties_.data_type_limits.arg_min_max_output.Supports(
      GetOperand(arg_min_max.output_operand_id).descriptor));

  std::array<ScopedOrtOpAttr, 2> attributes = {
      model_editor_.CreateAttribute(kAttrAxis,
                                    static_cast<int64_t>(arg_min_max.axis)),
      model_editor_.CreateAttribute(
          kAttrKeepDims, static_cast<int64_t>(arg_min_max.keep_dimensions))};

  // ONNX ArgMin/Max only supports int64 output.
  OperandDataType output_data_type =
      GetOperand(arg_min_max.output_operand_id).descriptor.data_type();
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

void GraphBuilderOrt::AddBatchNormalizationOperation(
    const mojom::BatchNormalization& batch_normalization) {
  const std::string node_name = GenerateNodeName(batch_normalization.label);
  const std::string input =
      GetOperandNameById(batch_normalization.input_operand_id);
  const std::string mean =
      GetOperandNameById(batch_normalization.mean_operand_id);
  const std::string variance =
      GetOperandNameById(batch_normalization.variance_operand_id);
  const std::string output =
      GetOperandNameById(batch_normalization.output_operand_id);

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  CHECK(data_type_limits.batch_normalization_input.Supports(
      GetOperand(batch_normalization.input_operand_id).descriptor));
  CHECK(data_type_limits.batch_normalization_mean.Supports(
      GetOperand(batch_normalization.mean_operand_id).descriptor));
  // TODO(crbug.com/431952809): Rename DataTypeLimits fields to be more generic
  // or encompassing.
  CHECK(data_type_limits.batch_normalization_mean.Supports(
      GetOperand(batch_normalization.variance_operand_id).descriptor));

  const OperandDescriptor& input_descriptor =
      GetOperand(batch_normalization.input_operand_id).descriptor;
  const OperandDataType input_data_type = input_descriptor.data_type();
  const std::vector<uint32_t>& input_shape = input_descriptor.shape();
  // ONNX BatchNormalization expects NCHW layout, channel is at index 1. In
  // addition it also accepts single dimension input of size N in which case C
  // is assumed to be 1.
  // https://onnx.ai/onnx/operators/onnx__BatchNormalization.html#inputs
  uint32_t input_channels = 1;
  if (input_shape.size() > 1) {
    input_channels = input_shape[1];
  }
  std::vector<uint32_t> scale_and_bias_shape = {input_channels};

  // ONNX BatchNormalization requires 5 inputs: input, scale, bias, mean and
  // variance. WebNN allows optional scale/bias, so create default ones if not
  // provided. Default scale = 1.0 (no scaling), default bias = 0.0 (no offset).
  std::string scale, bias;
  if (batch_normalization.scale_operand_id) {
    CHECK(data_type_limits.batch_normalization_mean.Supports(
        GetOperand(batch_normalization.scale_operand_id.value()).descriptor));
    scale = GetOperandNameById(batch_normalization.scale_operand_id.value());
  } else {
    scale = CreateOneInitializer(input_data_type, scale_and_bias_shape);
  }
  if (batch_normalization.bias_operand_id) {
    CHECK(data_type_limits.batch_normalization_mean.Supports(
        GetOperand(batch_normalization.bias_operand_id.value()).descriptor));
    bias = GetOperandNameById(batch_normalization.bias_operand_id.value());
  } else {
    bias = CreateZeroInitializer(input_data_type, scale_and_bias_shape);
  }

  std::array<const char*, 5> inputs = {input.c_str(), scale.c_str(),
                                       bias.c_str(), mean.c_str(),
                                       variance.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};
  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrEpsilon, batch_normalization.epsilon)};
  model_editor_.AddNode(kOpTypeBatchNormalization, node_name, inputs, outputs,
                        attributes);
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
  attributes.push_back(
      model_editor_.CreateAttribute(kAttrDilations, dilations));

  int64_t group = base::checked_cast<int64_t>(conv2d.groups);
  attributes.push_back(model_editor_.CreateAttribute(kAttrGroup, group));

  std::array<int64_t, 4> pads = {
      base::checked_cast<int64_t>(conv2d.padding->beginning->height),
      base::checked_cast<int64_t>(conv2d.padding->beginning->width),
      base::checked_cast<int64_t>(conv2d.padding->ending->height),
      base::checked_cast<int64_t>(conv2d.padding->ending->width)};
  attributes.push_back(model_editor_.CreateAttribute(kAttrPads, pads));

  std::array<int64_t, 2> strides = {
      base::checked_cast<int64_t>(conv2d.strides->height),
      base::checked_cast<int64_t>(conv2d.strides->width)};
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

      attributes.push_back(
          model_editor_.CreateAttribute(kAttrOutputPadding, output_padding));

      model_editor_.AddNode(kOpTypeConvTranspose2d, node_name, inputs, outputs,
                            attributes);
      break;
  }
}

void GraphBuilderOrt::AddCumulativeSumOperation(
    const mojom::CumulativeSum& cumulative_sum) {
  const std::string node_name = GenerateNodeName(cumulative_sum.label);
  const std::string input = GetOperandNameById(cumulative_sum.input_operand_id);
  const std::string output =
      GetOperandNameById(cumulative_sum.output_operand_id);

  CHECK(context_properties_.data_type_limits.cumulative_sum_input.Supports(
      GetOperand(cumulative_sum.input_operand_id).descriptor));

  const std::string axis =
      CreateScalarInitializer(base::checked_cast<int64_t>(cumulative_sum.axis));

  std::array<ScopedOrtOpAttr, 2> attributes = {
      model_editor_.CreateAttribute(
          kAttrExclusive,
          base::checked_cast<int64_t>(cumulative_sum.exclusive)),
      model_editor_.CreateAttribute(
          kAttrReverse, base::checked_cast<int64_t>(cumulative_sum.reversed))};

  std::array<const char*, 2> inputs = {input.c_str(), axis.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeCumulativeSum, node_name, inputs, outputs,
                        attributes);
}

template <typename T>
  requires(std::is_same_v<T, mojom::DequantizeLinear> ||
           std::is_same_v<T, mojom::QuantizeLinear>)
void GraphBuilderOrt::AddDequantizeOrQuantizeLinearOperation(
    const T& operation,
    base::cstring_view op_type) {
  const std::string node_name = GenerateNodeName(operation.label);
  std::string input = GetOperandNameById(operation.input_operand_id);
  std::string scale = GetOperandNameById(operation.scale_operand_id);
  std::string zero_point = GetOperandNameById(operation.zero_point_operand_id);
  std::string output = GetOperandNameById(operation.output_operand_id);

  const std::vector<uint32_t>& input_shape =
      GetOperand(operation.input_operand_id).descriptor.shape();
  // ZeroPoint has the same shape as the scale.
  const std::vector<uint32_t>& scale_zero_point_shape =
      GetOperand(operation.scale_operand_id).descriptor.shape();
  CHECK_EQ(scale_zero_point_shape.size(), input_shape.size());

  std::optional<int64_t> axis;
  uint32_t scale_not_size_one_dimension_count = 0;
  for (size_t i = 0; i < scale_zero_point_shape.size(); i++) {
    if (scale_zero_point_shape[i] != 1) {
      scale_not_size_one_dimension_count++;
      if (scale_zero_point_shape[i] == input_shape[i]) {
        axis = i;
      }
    }
  }

  bool is_per_axis =
      axis.has_value() && scale_not_size_one_dimension_count == 1;

  std::optional<int64_t> block_size;
  if (scale_not_size_one_dimension_count == 0) {
    // For per-tensor(per-layer) quantization and dequantization, scale should
    // be a scalar.
    if (!scale_zero_point_shape.empty()) {
      // The numbers in scale shape are all 1, scale and zeroPoint should be
      // reshaped to a scalar.
      scale = CreateReshapeNode(scale, {});
      zero_point = CreateReshapeNode(zero_point, {});
    }
  } else if (is_per_axis) {
    // For per-axis quantization and dequantization, scale and zeroPoint should
    // be a 1-D Tensor.
    if (scale_zero_point_shape.size() != 1) {
      scale = CreateReshapeNode(scale, {input_shape[axis.value()]});
      zero_point = CreateReshapeNode(zero_point, {input_shape[axis.value()]});
    }
  } else {
    // For blockwise quantization and dequantization, scale should has the same
    // shape as the input or except for one dimension in which blocking is
    // performed.
    // The default values are used if scale has the same shape as the input.
    axis = 0;
    block_size = 1;
    uint32_t blockwise_axis_count = 0;
    for (size_t i = 0; i < scale_zero_point_shape.size(); i++) {
      if (scale_zero_point_shape[i] != input_shape[i]) {
        CHECK_EQ(input_shape[i] % scale_zero_point_shape[i], 0u);
        block_size = input_shape[i] / scale_zero_point_shape[i];
        axis = i;
        blockwise_axis_count++;
      }
    }

    if (blockwise_axis_count > 1) {
      // The data type of zero point can be int4/uint4, which is not
      // supported by `resize` operator. So cast it to int8/uint8 before
      // `resize` and cast back to int4/uint4 after `resize`.
      const OperandDataType zero_point_data_type =
          GetOperand(operation.zero_point_operand_id).descriptor.data_type();
      if (zero_point_data_type == OperandDataType::kInt4) {
        zero_point =
            CreateCastNode(zero_point, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8);
      } else if (zero_point_data_type == OperandDataType::kUint4) {
        zero_point =
            CreateCastNode(zero_point, ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8);
      }

      scale = BlockwiseExpand(scale, input_shape);
      zero_point = BlockwiseExpand(zero_point, input_shape);

      if (zero_point_data_type == OperandDataType::kInt4) {
        zero_point =
            CreateCastNode(zero_point, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4);
      } else if (zero_point_data_type == OperandDataType::kUint4) {
        zero_point =
            CreateCastNode(zero_point, ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4);
      }

      // Reset the axis and block_size back to default values, because scale and
      // zeroPoint now have the same shape as input.
      axis = 0;
      block_size = 1;
    }
  }

  std::array<const char*, 3> inputs = {input.c_str(), scale.c_str(),
                                       zero_point.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  std::vector<ScopedOrtOpAttr> attributes;
  if (axis.has_value()) {
    attributes.push_back(
        model_editor_.CreateAttribute(kAttrAxis, axis.value()));
  }

  if (block_size.has_value()) {
    attributes.push_back(
        model_editor_.CreateAttribute(kAttrBlockSize, block_size.value()));
  }

  model_editor_.AddNode(op_type, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddEluOperation(const mojom::Elu& elu) {
  const std::string node_name = GenerateNodeName(elu.label);
  const std::string input = GetOperandNameById(elu.input_operand_id);
  const std::string output = GetOperandNameById(elu.output_operand_id);

  CHECK(context_properties_.data_type_limits.elu_input.Supports(
      GetOperand(elu.input_operand_id).descriptor));

  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrAlpha, elu.alpha)};

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeElu, node_name, inputs, outputs, attributes);
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

void GraphBuilderOrt::AddLogicalUnaryOperation(
    const mojom::ElementWiseUnary& logical_unary,
    base::cstring_view op_type) {
  const std::string node_name = GenerateNodeName(logical_unary.label);

  std::string input = GetOperandNameById(logical_unary.input_operand_id);

  // LogicalNot operation in ONNX only supports bool input.
  if (op_type == kOpTypeLogicalNot) {
    CHECK_EQ(GetOperand(logical_unary.input_operand_id).descriptor.data_type(),
             OperandDataType::kUint8);
    input = CreateCastNode(input, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);
  }

  const std::string bool_output = GenerateOperandName();

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {bool_output.c_str()};
  model_editor_.AddNode(op_type, node_name, inputs, outputs);

  // ONNX logical operators only support bool output, while WebNN logical
  // operators support uint8 output. Insert a `Cast` operator for type
  // conversion.
  const OperandDataType output_data_type =
      GetOperand(logical_unary.output_operand_id).descriptor.data_type();
  const std::string output =
      GetOperandNameById(logical_unary.output_operand_id);
  CHECK_EQ(output_data_type, OperandDataType::kUint8);
  InsertCastNode(bool_output, output, WebnnToOnnxDataType(output_data_type));
}

void GraphBuilderOrt::AddLogicalNotEqualOperation(
    const mojom::ElementWiseBinary& not_equal) {
  // Step 1: calculate `equal(a, b)`.
  const std::string equal_node_name =
      GenerateNodeName(GenerateEmulatedOpLabel(kOpTypeEqual, not_equal.label));
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
  const std::string not_node_name = GenerateNodeName(
      GenerateEmulatedOpLabel(kOpTypeLogicalNot, not_equal.label));
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
      return AddLogicalBinaryOperation(element_wise_binary, kOpTypeEqual);
    }
    case mojom::ElementWiseBinary::Kind::kNotEqual: {
      CHECK(data_type_limits.not_equal_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddLogicalNotEqualOperation(element_wise_binary);
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      CHECK(data_type_limits.greater_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddLogicalBinaryOperation(element_wise_binary, kOpTypeGreater);
    }
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual: {
      CHECK(data_type_limits.greater_or_equal_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddLogicalBinaryOperation(element_wise_binary,
                                       kOpTypeGreaterOrEqual);
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      CHECK(data_type_limits.lesser_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddLogicalBinaryOperation(element_wise_binary, kOpTypeLesser);
    }
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual: {
      CHECK(data_type_limits.lesser_or_equal_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddLogicalBinaryOperation(element_wise_binary,
                                       kOpTypeLesserOrEqual);
    }
    case mojom::ElementWiseBinary::Kind::kLogicalAnd: {
      CHECK(data_type_limits.logical_and_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddLogicalBinaryOperation(element_wise_binary, kOpTypeLogicalAnd);
    }
    case mojom::ElementWiseBinary::Kind::kLogicalOr: {
      CHECK(data_type_limits.logical_or_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddLogicalBinaryOperation(element_wise_binary, kOpTypeLogicalOr);
    }
    case mojom::ElementWiseBinary::Kind::kLogicalXor: {
      CHECK(data_type_limits.logical_xor_input.SupportsAll(
          {lhs_descriptor, rhs_descriptor}));
      return AddLogicalBinaryOperation(element_wise_binary, kOpTypeLogicalXor);
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
    case mojom::ElementWiseUnary::Kind::kIsNaN: {
      CHECK(data_type_limits.is_nan_input.Supports(input_descriptor));
      return AddLogicalUnaryOperation(element_wise_unary, kOpTypeIsNaN);
    }
    case mojom::ElementWiseUnary::Kind::kIsInfinite: {
      CHECK(data_type_limits.is_infinite_input.Supports(input_descriptor));
      return AddLogicalUnaryOperation(element_wise_unary, kOpTypeIsInfinite);
    }
    case mojom::ElementWiseUnary::Kind::kLogicalNot: {
      CHECK(data_type_limits.logical_not_input.Supports(input_descriptor));
      return AddLogicalUnaryOperation(element_wise_unary, kOpTypeLogicalNot);
    }
    case mojom::ElementWiseUnary::Kind::kNeg: {
      CHECK(data_type_limits.neg_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeNeg);
    }
    case mojom::ElementWiseUnary::Kind::kRoundEven: {
      CHECK(data_type_limits.round_even_input.Supports(input_descriptor));
      return AddUnaryOperation(element_wise_unary, kOpTypeRoundEven);
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
      CreateScalarInitializer(input_data_type, clamp.min_value);
  const std::string max =
      CreateScalarInitializer(input_data_type, clamp.max_value);

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

  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAttrAxis, base::checked_cast<int64_t>(concat.axis))};

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
          : CreateCastNode(indices, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);

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

  std::array<ScopedOrtOpAttr, 4> attributes = {
      model_editor_.CreateAttribute(kAttrAlpha, gemm.alpha),
      model_editor_.CreateAttribute(kAttrBeta, gemm.beta),
      model_editor_.CreateAttribute(kAttrTransA,
                                    static_cast<int64_t>(gemm.a_transpose)),
      model_editor_.CreateAttribute(kAttrTransB,
                                    static_cast<int64_t>(gemm.b_transpose))};

  model_editor_.AddNode(kOpTypeGemm, node_name, inputs, outputs, attributes);
}

// `GruType` must be `mojom::Gru` or `mojom::GruCell`.
template <typename GruType>
  requires(std::is_same_v<GruType, mojom::Gru> ||
           std::is_same_v<GruType, mojom::GruCell>)
void GraphBuilderOrt::AddGruOperation(const GruType& gru) {
  const std::string node_name = GenerateNodeName(gru.label);
  std::string input = GetOperandNameById(gru.input_operand_id);
  std::string weight = GetOperandNameById(gru.weight_operand_id);
  std::string recurrent_weight =
      GetOperandNameById(gru.recurrent_weight_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(gru.input_operand_id).descriptor;
  const OperandDescriptor& weight_descriptor =
      GetOperand(gru.weight_operand_id).descriptor;
  const OperandDescriptor& recurrent_weight_descriptor =
      GetOperand(gru.recurrent_weight_operand_id).descriptor;

  uint32_t num_directions = 1;
  if constexpr (std::is_same_v<GruType, mojom::Gru>) {
    CHECK(context_properties_.data_type_limits.gru_input.Supports(
        input_descriptor));
    CHECK(context_properties_.data_type_limits.gru_input.Supports(
        weight_descriptor));
    CHECK(context_properties_.data_type_limits.gru_input.Supports(
        recurrent_weight_descriptor));
    num_directions =
        gru.direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;
  } else {
    CHECK(context_properties_.data_type_limits.gru_cell_input.Supports(
        input_descriptor));
    CHECK(context_properties_.data_type_limits.gru_cell_input.Supports(
        weight_descriptor));
    CHECK(context_properties_.data_type_limits.gru_cell_input.Supports(
        recurrent_weight_descriptor));

    // Reshape the input into a 3-D tensor, since the GRU of ONNX requires
    // the input shape to be [seq_length, batch_size, input_size]. For
    // gruCell, `seq_length` is equal to 1.
    const std::vector<uint32_t>& input_shape = input_descriptor.shape();
    CHECK_EQ(input_shape.size(), 2u);
    input = CreateReshapeNode(input, {1, input_shape[0], input_shape[1]});

    // Reshape the weight into a 3-D tensor, since the GRU of ONNX requires
    // the weight shape to be [num_directions, 3*hidden_size, input_size].
    // For gruCell, `num_directions` is equal to 1.
    const std::vector<uint32_t>& weight_shape = weight_descriptor.shape();
    CHECK_EQ(weight_shape.size(), 2u);
    weight = CreateReshapeNode(weight, {1, weight_shape[0], weight_shape[1]});

    // Reshape the recurrent weight into a 3-D tensor, since the GRU of ONNX
    // requires the recurrent weight shape to be [num_directions,
    // 3*hidden_size, hidden_size]. For gruCell, `num_directions` is equal to 1.
    const std::vector<uint32_t>& recurrent_weight_shape =
        recurrent_weight_descriptor.shape();
    CHECK_EQ(recurrent_weight_shape.size(), 2u);
    recurrent_weight = CreateReshapeNode(
        recurrent_weight,
        {1, recurrent_weight_shape[0], recurrent_weight_shape[1]});
  }

  constexpr std::array<uint32_t, 3> kRznToZrnPermutation = {1, 0, 2};
  if (gru.layout == mojom::GruWeightLayout::kRzn) {
    weight = TransposeRnnWeightOrBiasLayout(weight, kRznToZrnPermutation);
    recurrent_weight =
        TransposeRnnWeightOrBiasLayout(recurrent_weight, kRznToZrnPermutation);
  }

  std::vector<const char*> inputs = {input.c_str(), weight.c_str(),
                                     recurrent_weight.c_str()};

  const uint32_t hidden_size = gru.hidden_size;
  // Graph validation already checked that hidden_size * 3 would not overflow.
  std::array<uint32_t, 2> bias_dims = {num_directions, hidden_size * 3};
  std::string bias, recurrent_bias, concatenated_bias;
  if (!gru.bias_operand_id.has_value() &&
      !gru.recurrent_bias_operand_id.has_value()) {
    // When both bias and recurrentBias are not present, set ONNX GRU input "B"
    // as not specified.
    inputs.push_back("");
  } else {
    if (gru.bias_operand_id.has_value()) {
      bias = GetOperandNameById(*gru.bias_operand_id);
      if constexpr (std::is_same_v<GruType, mojom::Gru>) {
        CHECK(context_properties_.data_type_limits.gru_bias.Supports(
            GetOperand(*gru.bias_operand_id).descriptor));
      } else {
        CHECK(context_properties_.data_type_limits.gru_cell_bias.Supports(
            GetOperand(*gru.bias_operand_id).descriptor));
        bias = CreateReshapeNode(bias, bias_dims);
      }
      if (gru.layout == mojom::GruWeightLayout::kRzn) {
        bias = TransposeRnnWeightOrBiasLayout(bias, kRznToZrnPermutation);
      }
    } else {
      bias = CreateZeroInitializer(input_descriptor.data_type(), bias_dims);
    }

    if (gru.recurrent_bias_operand_id.has_value()) {
      recurrent_bias = GetOperandNameById(*gru.recurrent_bias_operand_id);
      if constexpr (std::is_same_v<GruType, mojom::Gru>) {
        CHECK(context_properties_.data_type_limits.gru_bias.Supports(
            GetOperand(*gru.recurrent_bias_operand_id).descriptor));
      } else {
        CHECK(context_properties_.data_type_limits.gru_cell_bias.Supports(
            GetOperand(*gru.recurrent_bias_operand_id).descriptor));
        recurrent_bias = CreateReshapeNode(recurrent_bias, bias_dims);
      }
      if (gru.layout == mojom::GruWeightLayout::kRzn) {
        recurrent_bias = TransposeRnnWeightOrBiasLayout(recurrent_bias,
                                                        kRznToZrnPermutation);
      }
    } else {
      recurrent_bias =
          CreateZeroInitializer(input_descriptor.data_type(), bias_dims);
    }

    // Concat bias and recurrent_bias.
    concatenated_bias = GenerateOperandName();
    std::array<const char*, 2> bias_inputs = {bias.c_str(),
                                              recurrent_bias.c_str()};
    std::array<const char*, 1> bias_outputs = {concatenated_bias.c_str()};
    std::array<ScopedOrtOpAttr, 1> concat_attributes = {
        model_editor_.CreateAttribute(kAttrAxis, static_cast<int64_t>(1))};
    std::string concat_node_name = GenerateNodeName(
        base::JoinString({kInserted, kOpTypeConcat}, kUnderscore));
    model_editor_.AddNode(kOpTypeConcat, concat_node_name, bias_inputs,
                          bias_outputs, concat_attributes);
    inputs.push_back(concatenated_bias.c_str());
  }

  // "sequence_lens" is an optional tensor specifying lengths of the sequences
  // in a batch.
  inputs.push_back("");

  std::string hidden_state;
  if constexpr (std::is_same_v<GruType, mojom::Gru>) {
    if (gru.initial_hidden_state_operand_id.has_value()) {
      hidden_state =
          GetOperandNameById(gru.initial_hidden_state_operand_id.value());
      CHECK(context_properties_.data_type_limits.gru_input.Supports(
          GetOperand(gru.initial_hidden_state_operand_id.value()).descriptor));
    }
  } else {
    hidden_state = GetOperandNameById(gru.hidden_state_operand_id);
    const std::vector<uint32_t>& hidden_state_shape =
        GetOperand(gru.hidden_state_operand_id).descriptor.shape();
    CHECK_EQ(hidden_state_shape.size(), 2u);
    // Reshape the hiddenState into a 3-D tensor, since the GRU of ONNX requires
    // the "initial_h" shape to be [num_directions, batch_size, hidden_size].
    // For gruCell, `num_directions` is equal to 1.
    hidden_state = CreateReshapeNode(
        hidden_state, {1, hidden_state_shape[0], hidden_state_shape[1]});
  }
  inputs.push_back(hidden_state.c_str());

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(4);
  base::cstring_view direction = "forward";
  if constexpr (std::is_same_v<GruType, mojom::Gru>) {
    direction = GetRecurrentNetworkDirection(gru.direction);
  }
  attributes.push_back(
      model_editor_.CreateAttribute(kAttrDirection, direction));

  const std::vector<base::cstring_view> activations =
      GetRecurrentNetworkActivations(gru.activations,
                                     direction == "bidirectional");
  std::vector<const char*> activations_c_str;
  for (const auto& activation : activations) {
    activations_c_str.push_back(activation.c_str());
  }
  attributes.push_back(
      model_editor_.CreateAttribute(kAttrActivations, activations_c_str));

  attributes.push_back(model_editor_.CreateAttribute(
      kAttrHiddenSize, base::checked_cast<int64_t>(hidden_size)));
  attributes.push_back(model_editor_.CreateAttribute(
      kAttrLinearBeforeReset, static_cast<int64_t>(gru.reset_after)));

  std::string output, output_hidden;
  if constexpr (std::is_same_v<GruType, mojom::Gru>) {
    output_hidden = GetOperandNameById(gru.output_operand_ids[0]);
    if (gru.return_sequence) {
      output = GetOperandNameById(gru.output_operand_ids[1]);
    }
  } else {
    output_hidden = GenerateOperandName();
  }
  std::array<const char*, 2> outputs = {output.c_str(), output_hidden.c_str()};
  model_editor_.AddNode(kOpTypeGru, node_name, inputs, outputs, attributes);

  if constexpr (std::is_same_v<GruType, mojom::GruCell>) {
    // Reshape the ONNX GRU output "Y_h" of shape [num_directions, batch_size,
    // hidden_size] back to a 2-D tensor, since the gruCell of WebNN requires
    // the output shape to be [batchSize, hiddenSize].
    const std::vector<uint32_t>& output_shape =
        GetOperand(gru.output_operand_id).descriptor.shape();
    CHECK_EQ(output_shape.size(), 2u);
    InsertReshapeNode(output_hidden, GetOperandNameById(gru.output_operand_id),
                      output_shape);
  }
}

template void GraphBuilderOrt::AddGruOperation<mojom::Gru>(const mojom::Gru&);

template void GraphBuilderOrt::AddGruOperation<mojom::GruCell>(
    const mojom::GruCell&);

void GraphBuilderOrt::AddHardSigmoidOperation(
    const mojom::HardSigmoid& hard_sigmoid) {
  const std::string node_name = GenerateNodeName(hard_sigmoid.label);
  const std::string input = GetOperandNameById(hard_sigmoid.input_operand_id);
  const std::string output = GetOperandNameById(hard_sigmoid.output_operand_id);

  CHECK(context_properties_.data_type_limits.hard_sigmoid_input.Supports(
      GetOperand(hard_sigmoid.input_operand_id).descriptor));

  std::array<const char*, 1> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  std::array<ScopedOrtOpAttr, 2> attributes = {
      model_editor_.CreateAttribute(kAttrAlpha, hard_sigmoid.alpha),
      model_editor_.CreateAttribute(kAttrBeta, hard_sigmoid.beta)};
  model_editor_.AddNode(kOpTypeHardSigmoid, node_name, inputs, outputs,
                        attributes);
}

void GraphBuilderOrt::AddInstanceNormalizationOperation(
    const mojom::InstanceNormalization& instance_normalization) {
  const std::string node_name = GenerateNodeName(instance_normalization.label);
  const std::string input =
      GetOperandNameById(instance_normalization.input_operand_id);
  const std::string output =
      GetOperandNameById(instance_normalization.output_operand_id);

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  CHECK(data_type_limits.instance_normalization_input.Supports(
      GetOperand(instance_normalization.input_operand_id).descriptor));
  const OperandDescriptor& input_descriptor =
      GetOperand(instance_normalization.input_operand_id).descriptor;
  const OperandDataType input_data_type = input_descriptor.data_type();
  const std::vector<uint32_t>& input_shape = input_descriptor.shape();
  // ONNX InstanceNormalization expects NCHW layout, channel is at index 1.
  CHECK_EQ(input_shape.size(), 4u);
  uint32_t input_channels = input_shape[1];
  std::vector<uint32_t> scale_and_bias_shape = {input_channels};

  // ONNX InstanceNormalization requires 3 inputs: input, scale and bias.
  // WebNN allows optional scale/bias, so create default ones if not provided.
  // Default scale = 1.0 (no scaling), default bias = 0.0 (no offset).
  std::string scale, bias;
  if (instance_normalization.scale_operand_id) {
    CHECK(data_type_limits.instance_normalization_scale.Supports(
        GetOperand(instance_normalization.scale_operand_id.value())
            .descriptor));
    scale = GetOperandNameById(instance_normalization.scale_operand_id.value());
  } else {
    scale = CreateOneInitializer(input_data_type, scale_and_bias_shape);
  }
  if (instance_normalization.bias_operand_id) {
    CHECK(data_type_limits.instance_normalization_scale.Supports(
        GetOperand(instance_normalization.bias_operand_id.value()).descriptor));
    bias = GetOperandNameById(instance_normalization.bias_operand_id.value());
  } else {
    bias = CreateZeroInitializer(input_data_type, scale_and_bias_shape);
  }

  std::array<const char*, 3> inputs = {input.c_str(), scale.c_str(),
                                       bias.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};
  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAttrEpsilon, instance_normalization.epsilon)};
  model_editor_.AddNode(kOpTypeInstanceNormalization, node_name, inputs,
                        outputs, attributes);
}

void GraphBuilderOrt::AddLayerNormalizationOperation(
    const mojom::LayerNormalization& layer_normalization) {
  const std::string node_name = GenerateNodeName(layer_normalization.label);
  const std::string input =
      GetOperandNameById(layer_normalization.input_operand_id);
  const std::string output =
      GetOperandNameById(layer_normalization.output_operand_id);

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDescriptor& input_descriptor =
      GetOperand(layer_normalization.input_operand_id).descriptor;
  CHECK(data_type_limits.layer_normalization_input.Supports(input_descriptor));

  std::string scale, bias;
  if (layer_normalization.scale_operand_id) {
    CHECK(data_type_limits.layer_normalization_input.Supports(
        GetOperand(layer_normalization.scale_operand_id.value()).descriptor));
    scale = GetOperandNameById(layer_normalization.scale_operand_id.value());
  }
  if (layer_normalization.bias_operand_id) {
    CHECK(data_type_limits.layer_normalization_input.Supports(
        GetOperand(layer_normalization.bias_operand_id.value()).descriptor));
    bias = GetOperandNameById(layer_normalization.bias_operand_id.value());
  }

  std::vector<const char*> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};
  const OperandDataType input_data_type = input_descriptor.data_type();
  auto axes = layer_normalization.axes;
  const std::vector<uint32_t>& input_shape = input_descriptor.shape();
  // ONNX LayerNormalization doesn't support empty axes because it requires to
  // set the first normalization dimension.
  // https://onnx.ai/onnx/operators/onnx__LayerNormalization.html#attributes
  // For WebNN layerNormalization, if axes is empty, no dimensions are reduced
  // and the emulation can be simplified to `output = bias + (scale * 0).
  // https://www.w3.org/TR/webnn/#dom-mllayernormalizationoptions-axes
  if (axes.empty()) {
    if (layer_normalization.bias_operand_id) {
      const std::string zero =
          CreateZeroInitializer(input_data_type, input_shape);
      std::array<const char*, 2> add_inputs = {bias.c_str(), zero.c_str()};
      return model_editor_.AddNode(kOpTypeAdd, node_name, add_inputs, outputs);
    } else {
      std::array<const char*, 2> sub_inputs = {input.c_str(), input.c_str()};
      return model_editor_.AddNode(kOpTypeSub, node_name, sub_inputs, outputs);
    }
  }

  const size_t axes_size = axes.size();
  // Sort the indexes of the elements in the axes array based on their values
  // and return the sorted index array for adding a transpose operation if
  // needed. For example input shape is [2, 1, 4, 3], the shape of the scale and
  // bias is [3, 1, 4] if axes is [3, 1, 2], the sorted axes would be [1, 2, 3],
  // then the permutation would be (sorted indices array) [1, 2, 0].
  std::optional<std::vector<uint32_t>> permutation;
  if (!std::ranges::is_sorted(axes)) {
    std::vector<uint32_t> sorted_indices(axes_size);
    std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
    std::ranges::sort(sorted_indices, std::ranges::less(),
                      [&axes](uint32_t index) { return axes[index]; });
    permutation = std::move(sorted_indices);
    std::ranges::sort(axes);
  }

  std::vector<uint32_t> scale_shape;
  scale_shape.reserve(axes_size);
  std::ranges::transform(
      axes, std::back_inserter(scale_shape),
      [&input_shape](uint32_t axis) { return input_shape[axis]; });
  // Because ONNX LayerNormalization only accepts the first normalization
  // dimension, it can only support WebNN layerNormalization whose axes are
  // consecutive til the last dimension. Here we only check beginning and ending
  // of the ascending sorted axes, because the blink validation code ensures
  // axes not having duplicated values.
  if (axes[axes_size - 1] == input_shape.size() - 1 &&
      axes[0] == input_shape.size() - axes_size) {
    if (layer_normalization.scale_operand_id) {
      if (permutation.has_value()) {
        scale = CreateTransposeNode(scale, permutation.value());
      }
    } else {
      scale = CreateOneInitializer(input_data_type, scale_shape);
    }
    inputs.push_back(scale.c_str());

    if (layer_normalization.bias_operand_id) {
      if (permutation.has_value()) {
        bias = CreateTransposeNode(bias, permutation.value());
      }
      inputs.push_back(bias.c_str());
    }

    std::array<ScopedOrtOpAttr, 2> attributes = {
        model_editor_.CreateAttribute(kAttrAxis,
                                      base::checked_cast<int64_t>(axes[0])),
        model_editor_.CreateAttribute(kAttrEpsilon,
                                      layer_normalization.epsilon)};

    model_editor_.AddNode(kOpTypeLayerNormalization, node_name, inputs, outputs,
                          attributes);
  } else {
    // Emulate layerNormalization by scale * ((input - mean) / sqrt(variance +
    // epsilon)) + bias. Calculate mean as follows:
    // reduceOptions = {axes, keepDimensions: true};
    // mean = builder.reduceMean(input, reduceOptions).
    const std::string reduce_mean_1_label = GenerateEmulatedOpLabel(
        kOpTypeReduceMean, layer_normalization.label, "1");
    const std::string reduce_mean_1_node_name =
        GenerateNodeName(reduce_mean_1_label);
    const std::string mean_output = GenerateOperandName();
    std::string axes_name = CreateInt64InitializerForUint32Array(axes);
    std::array<const char*, 2> reduce_mean_1_inputs = {input.c_str(),
                                                       axes_name.c_str()};
    std::array<const char*, 1> reduce_mean_1_outputs = {mean_output.c_str()};
    std::array<ScopedOrtOpAttr, 2> reduce_mean_1_attributes = {
        model_editor_.CreateAttribute(kAttrKeepDims, 1),
        model_editor_.CreateAttribute(kAttrNoopWithEmptyAxes, 1)};
    model_editor_.AddNode(kOpTypeReduceMean, reduce_mean_1_node_name,
                          reduce_mean_1_inputs, reduce_mean_1_outputs,
                          reduce_mean_1_attributes);

    // Calculate variance as follows:
    // powValue = builder.constant(input.dataType, 2);
    // variance = builder.reduceMean(builder.pow(builder.sub(input, mean),
    // powValue), reduceOptions);
    const std::string sub_label =
        GenerateEmulatedOpLabel(kOpTypeSub, layer_normalization.label);
    const std::string sub_node_name = GenerateNodeName(sub_label);
    const std::string sub_output = GenerateOperandName();

    std::array<const char*, 2> sub_inputs = {input.c_str(),
                                             mean_output.c_str()};
    std::array<const char*, 1> sub_outputs = {sub_output.c_str()};
    model_editor_.AddNode(kOpTypeSub, sub_node_name, sub_inputs, sub_outputs);

    const std::string pow_label =
        GenerateEmulatedOpLabel(kOpTypePow, layer_normalization.label);
    std::string pow_node_name = GenerateNodeName(pow_label);
    const std::string pow_output = GenerateOperandName();
    std::string pow_value =
        CreateScalarInitializer(input_data_type, MLNumber::FromFloat64(2.0f));
    std::array<const char*, 2> pow_inputs = {sub_output.c_str(),
                                             pow_value.c_str()};
    std::array<const char*, 1> pow_outputs = {pow_output.c_str()};
    model_editor_.AddNode(kOpTypePow, pow_node_name, pow_inputs, pow_outputs);

    const std::string reduce_mean_2_label = GenerateEmulatedOpLabel(
        kOpTypeReduceMean, layer_normalization.label, "2");
    const std::string reduce_mean_2_node_name =
        GenerateNodeName(reduce_mean_2_label);
    const std::string variance_output = GenerateOperandName();
    std::array<const char*, 2> reduce_mean_2_inputs = {pow_output.c_str(),
                                                       axes_name.c_str()};
    std::array<const char*, 1> reduce_mean_2_outputs = {
        variance_output.c_str()};
    std::array<ScopedOrtOpAttr, 2> reduce_mean_2_attributes = {
        model_editor_.CreateAttribute(kAttrKeepDims, 1),
        model_editor_.CreateAttribute(kAttrNoopWithEmptyAxes, 1)};
    model_editor_.AddNode(kOpTypeReduceMean, reduce_mean_2_node_name,
                          reduce_mean_2_inputs, reduce_mean_2_outputs,
                          reduce_mean_2_attributes);

    const std::string add_label =
        GenerateEmulatedOpLabel(kOpTypeAdd, layer_normalization.label);
    const std::string add_node_name = GenerateNodeName(add_label);
    const std::string add_output = GenerateOperandName();
    std::string epsilon_value = CreateScalarInitializer(
        input_data_type, MLNumber::FromFloat64(layer_normalization.epsilon));
    std::array<const char*, 2> add_inputs = {variance_output.c_str(),
                                             epsilon_value.c_str()};
    std::array<const char*, 1> add_outputs = {add_output.c_str()};
    model_editor_.AddNode(kOpTypeAdd, add_node_name, add_inputs, add_outputs);

    const std::string sqrt_label =
        GenerateEmulatedOpLabel(kOpTypeSqrt, layer_normalization.label);
    const std::string sqrt_node_name = GenerateNodeName(sqrt_label);
    const std::string sqrt_output = GenerateOperandName();
    std::array<const char*, 1> sqrt_inputs = {add_output.c_str()};
    std::array<const char*, 1> sqrt_outputs = {sqrt_output.c_str()};
    model_editor_.AddNode(kOpTypeSqrt, sqrt_node_name, sqrt_inputs,
                          sqrt_outputs);

    const std::string div_label =
        GenerateEmulatedOpLabel(kOpTypeDiv, layer_normalization.label);
    const std::string div_node_name = GenerateNodeName(div_label);
    const std::string div_output = GenerateOperandName();
    std::array<const char*, 2> div_inputs = {sub_output.c_str(),
                                             sqrt_output.c_str()};
    std::array<const char*, 1> div_outputs = {div_output.c_str()};
    model_editor_.AddNode(kOpTypeDiv, div_node_name, div_inputs, div_outputs);

    // Create compatible_shape for broadcasting scale and bias with intermediate
    // results sach as `div_output` and `mul_output`. Initialize all dimensions
    // to 1, then set normalization axes to match input dimensions for
    // element-wise operations.
    // Example: input_shape=[2,3,4,5], axes=[1,3] -> compatible_shape=[1,3,1,5].
    std::vector<uint32_t> compatible_shape(input_shape.size(), 1);
    for (auto axis : axes) {
      compatible_shape[axis] = input_shape[axis];
    }
    if (layer_normalization.scale_operand_id) {
      if (permutation.has_value()) {
        scale = CreateTransposeNode(scale, permutation.value());
      }
      if (scale_shape.size() != input_shape.size()) {
        scale = CreateReshapeNode(scale, compatible_shape);
      }
    } else {
      scale = CreateOneInitializer(input_data_type, compatible_shape);
    }

    const std::string mul_label =
        GenerateEmulatedOpLabel(kOpTypeMul, layer_normalization.label);
    const std::string mul_node_name = GenerateNodeName(mul_label);
    std::array<const char*, 2> mul_inputs = {scale.c_str(), div_output.c_str()};
    if (layer_normalization.bias_operand_id) {
      const std::string mul_output = GenerateOperandName();
      std::array<const char*, 1> mul_outputs = {mul_output.c_str()};
      model_editor_.AddNode(kOpTypeMul, mul_node_name, mul_inputs, mul_outputs);
      if (permutation.has_value()) {
        bias = CreateTransposeNode(bias, permutation.value());
      }
      if (scale_shape.size() != input_shape.size()) {
        bias = CreateReshapeNode(bias, compatible_shape);
      }

      const std::string add_2_label =
          GenerateEmulatedOpLabel(kOpTypeAdd, layer_normalization.label, "2");
      const std::string add_2_node_name = GenerateNodeName(add_2_label);
      std::array<const char*, 2> add_2_inputs = {mul_output.c_str(),
                                                 bias.c_str()};
      model_editor_.AddNode(kOpTypeAdd, add_2_node_name, add_2_inputs, outputs);
    } else {
      model_editor_.AddNode(kOpTypeMul, mul_node_name, mul_inputs, outputs);
    }
  }
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

  std::array<ScopedOrtOpAttr, 1> attributes = {
      model_editor_.CreateAttribute(kAttrAlpha, leaky_relu.alpha)};
  model_editor_.AddNode(kOpTypeLeakyRelu, node_name, inputs, outputs,
                        attributes);
}

void GraphBuilderOrt::AddLinearOperation(const mojom::Linear& linear) {
  const OperandDescriptor& input_descriptor =
      GetOperand(linear.input_operand_id).descriptor;
  CHECK(context_properties_.data_type_limits.linear_input.Supports(
      input_descriptor));

  // Emulate a linear operation using two ONNX nodes for expression `alpha * x +
  // beta`.
  const OperandDataType input_data_type = input_descriptor.data_type();
  std::string alpha = CreateScalarInitializer(
      input_data_type, MLNumber::FromFloat64(linear.alpha));
  std::string beta = CreateScalarInitializer(
      input_data_type, MLNumber::FromFloat64(linear.beta));

  // Step 1: Create 'Mul' node (alpha * x)
  const std::string mul_node_label =
      GenerateEmulatedOpLabel(kOpTypeMul, linear.label);
  const std::string mul_node_name = GenerateNodeName(mul_node_label);
  const std::string input = GetOperandNameById(linear.input_operand_id);
  std::array<const char*, 2> mul_inputs = {input.c_str(), alpha.c_str()};
  const std::string mul_output = GenerateOperandName();
  std::array<const char*, 1> mul_outputs = {mul_output.c_str()};
  model_editor_.AddNode(kOpTypeMul, mul_node_name, mul_inputs, mul_outputs);

  // Step 2: Create 'Add' node (mul_output + beta)
  const std::string add_node_label =
      GenerateEmulatedOpLabel(kOpTypeAdd, linear.label);
  const std::string add_node_name = GenerateNodeName(add_node_label);
  std::array<const char*, 2> add_inputs = {mul_output.c_str(), beta.c_str()};
  const std::string output = GetOperandNameById(linear.output_operand_id);
  std::array<const char*, 1> add_outputs = {output.c_str()};
  model_editor_.AddNode(kOpTypeAdd, add_node_name, add_inputs, add_outputs);
}

// `LstmType` must be `mojom::Lstm` or `mojom::LstmCell`.
template <typename LstmType>
  requires(std::is_same_v<LstmType, mojom::Lstm> ||
           std::is_same_v<LstmType, mojom::LstmCell>)
void GraphBuilderOrt::AddLstmOperation(const LstmType& lstm) {
  const std::string node_name = GenerateNodeName(lstm.label);
  std::string input = GetOperandNameById(lstm.input_operand_id);
  std::string weight = GetOperandNameById(lstm.weight_operand_id);
  std::string recurrent_weight =
      GetOperandNameById(lstm.recurrent_weight_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(lstm.input_operand_id).descriptor;
  const OperandDescriptor& weight_descriptor =
      GetOperand(lstm.weight_operand_id).descriptor;
  const OperandDescriptor& recurrent_weight_descriptor =
      GetOperand(lstm.recurrent_weight_operand_id).descriptor;

  uint32_t num_directions = 1;
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    CHECK(context_properties_.data_type_limits.lstm_input.Supports(
        input_descriptor));
    CHECK(context_properties_.data_type_limits.lstm_input.Supports(
        weight_descriptor));
    CHECK(context_properties_.data_type_limits.lstm_input.Supports(
        recurrent_weight_descriptor));
    num_directions =
        lstm.direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;
  } else {
    CHECK(context_properties_.data_type_limits.lstm_cell_input.Supports(
        input_descriptor));
    CHECK(context_properties_.data_type_limits.lstm_cell_input.Supports(
        weight_descriptor));
    CHECK(context_properties_.data_type_limits.lstm_cell_input.Supports(
        recurrent_weight_descriptor));

    // Reshape the input into a 3-D tensor, since the LSTM of ONNX requires
    // the input shape to be [seq_length, batch_size, input_size]. For
    // lstmCell, `seq_length` is equal to 1.
    const std::vector<uint32_t>& input_shape = input_descriptor.shape();
    CHECK_EQ(input_shape.size(), 2u);
    input = CreateReshapeNode(input, {1, input_shape[0], input_shape[1]});

    // Reshape the weight into a 3-D tensor, since the LSTM of ONNX requires
    // the weight shape to be [num_directions, 4*hidden_size, input_size].
    // For lstmCell, `num_directions` is equal to 1.
    const std::vector<uint32_t>& weight_shape = weight_descriptor.shape();
    CHECK_EQ(weight_shape.size(), 2u);
    weight = CreateReshapeNode(weight, {1, weight_shape[0], weight_shape[1]});

    // Reshape the recurrent weight into a 3-D tensor, since the LSTM of ONNX
    // requires the recurrent weight shape to be [num_directions,
    // 4*hidden_size, hidden_size]. For lstmCell, `num_directions` is equal
    // to 1.
    const std::vector<uint32_t>& recurrent_weight_shape =
        recurrent_weight_descriptor.shape();
    CHECK_EQ(recurrent_weight_shape.size(), 2u);
    recurrent_weight = CreateReshapeNode(
        recurrent_weight,
        {1, recurrent_weight_shape[0], recurrent_weight_shape[1]});
  }

  constexpr std::array<uint32_t, 4> kIfgoToIofgPermutation = {0, 3, 1, 2};
  if (lstm.layout == mojom::LstmWeightLayout::kIfgo) {
    weight = TransposeRnnWeightOrBiasLayout(weight, kIfgoToIofgPermutation);
    recurrent_weight = TransposeRnnWeightOrBiasLayout(recurrent_weight,
                                                      kIfgoToIofgPermutation);
  }

  std::vector<const char*> inputs = {input.c_str(), weight.c_str(),
                                     recurrent_weight.c_str()};

  const uint32_t hidden_size = lstm.hidden_size;
  // Graph validation already checked that hidden_size * 4 would not overflow.
  std::array<uint32_t, 2> bias_dims = {num_directions, hidden_size * 4};
  std::string bias, recurrent_bias, concatenated_bias;
  if (!lstm.bias_operand_id.has_value() &&
      !lstm.recurrent_bias_operand_id.has_value()) {
    // When both bias and recurrentBias are not present, set ONNX LSTM input "B"
    // as not specified.
    inputs.push_back("");
  } else {
    if (lstm.bias_operand_id.has_value()) {
      bias = GetOperandNameById(*lstm.bias_operand_id);
      if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
        CHECK(context_properties_.data_type_limits.lstm_bias.Supports(
            GetOperand(*lstm.bias_operand_id).descriptor));
      } else {
        CHECK(context_properties_.data_type_limits.lstm_cell_bias.Supports(
            GetOperand(*lstm.bias_operand_id).descriptor));
        // Reshape the bias into a 2-D tensor, since the LSTM of ONNX requires
        // the bias shape to be [num_directions, 4*hidden_size]. For lstmCell,
        // `num_directions` is equal to 1.
        bias = CreateReshapeNode(bias, bias_dims);
      }
      if (lstm.layout == mojom::LstmWeightLayout::kIfgo) {
        bias = TransposeRnnWeightOrBiasLayout(bias, kIfgoToIofgPermutation);
      }
    } else {
      bias = CreateZeroInitializer(input_descriptor.data_type(), bias_dims);
    }

    if (lstm.recurrent_bias_operand_id.has_value()) {
      recurrent_bias = GetOperandNameById(*lstm.recurrent_bias_operand_id);
      if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
        CHECK(context_properties_.data_type_limits.lstm_bias.Supports(
            GetOperand(*lstm.recurrent_bias_operand_id).descriptor));
      } else {
        CHECK(context_properties_.data_type_limits.lstm_cell_bias.Supports(
            GetOperand(*lstm.recurrent_bias_operand_id).descriptor));
        // Reshape the recurrentBias into a 2-D tensor, since the LSTM of ONNX
        // requires the recurrentBias shape to be [num_directions,
        // 4*hidden_size]. For lstmCell, `num_directions` is equal to 1.
        recurrent_bias = CreateReshapeNode(recurrent_bias, bias_dims);
      }
      if (lstm.layout == mojom::LstmWeightLayout::kIfgo) {
        recurrent_bias = TransposeRnnWeightOrBiasLayout(recurrent_bias,
                                                        kIfgoToIofgPermutation);
      }
    } else {
      recurrent_bias =
          CreateZeroInitializer(input_descriptor.data_type(), bias_dims);
    }

    // Concatenate bias and recurrent_bias to create the ONNX LSTM input "B".
    concatenated_bias = GenerateOperandName();
    std::array<const char*, 2> bias_inputs = {bias.c_str(),
                                              recurrent_bias.c_str()};
    std::array<const char*, 1> bias_outputs = {concatenated_bias.c_str()};
    std::array<ScopedOrtOpAttr, 1> bias_attributes = {
        model_editor_.CreateAttribute(kAttrAxis, static_cast<int64_t>(1))};
    std::string concat_node_name = GenerateNodeName(
        base::JoinString({kInserted, kOpTypeConcat}, kUnderscore));
    model_editor_.AddNode(kOpTypeConcat, concat_node_name, bias_inputs,
                          bias_outputs, bias_attributes);
    inputs.push_back(concatenated_bias.c_str());
  }

  // "sequence_lens" is an optional tensor specifying lengths of the sequences
  // in a batch.
  inputs.push_back("");

  std::string hidden_state, cell_state;
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    if (lstm.initial_hidden_state_operand_id.has_value()) {
      hidden_state = GetOperandNameById(*lstm.initial_hidden_state_operand_id);
      CHECK(context_properties_.data_type_limits.lstm_input.Supports(
          GetOperand(*lstm.initial_hidden_state_operand_id).descriptor));
    }
    if (lstm.initial_cell_state_operand_id.has_value()) {
      cell_state = GetOperandNameById(*lstm.initial_cell_state_operand_id);
      CHECK(context_properties_.data_type_limits.lstm_input.Supports(
          GetOperand(*lstm.initial_cell_state_operand_id).descriptor));
    }
  } else {
    hidden_state = GetOperandNameById(lstm.hidden_state_operand_id);
    const OperandDescriptor& hidden_state_descriptor =
        GetOperand(lstm.hidden_state_operand_id).descriptor;
    CHECK(context_properties_.data_type_limits.lstm_cell_input.Supports(
        hidden_state_descriptor));
    cell_state = GetOperandNameById(lstm.cell_state_operand_id);
    const OperandDescriptor& cell_state_descriptor =
        GetOperand(lstm.cell_state_operand_id).descriptor;
    CHECK(context_properties_.data_type_limits.lstm_cell_input.Supports(
        cell_state_descriptor));

    // Reshape the hidden/cell_state into a 3-D tensor, since the LSTM of ONNX
    // requires the "initial_h"/"initial_c" shape to be [num_directions,
    // batch_size, hidden_size]. For lstmCell, `num_directions` is equal to 1.
    const std::vector<uint32_t>& hidden_state_shape =
        hidden_state_descriptor.shape();
    const std::vector<uint32_t>& cell_state_shape =
        cell_state_descriptor.shape();
    hidden_state = CreateReshapeNode(
        hidden_state, {1, hidden_state_shape[0], hidden_state_shape[1]});
    cell_state = CreateReshapeNode(
        cell_state, {1, cell_state_shape[0], cell_state_shape[1]});
  }
  inputs.push_back(hidden_state.c_str());
  inputs.push_back(cell_state.c_str());

  std::string peephole_weight;
  if (lstm.peephole_weight_operand_id.has_value()) {
    peephole_weight = GetOperandNameById(*lstm.peephole_weight_operand_id);
    if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
      CHECK(context_properties_.data_type_limits.lstm_bias.Supports(
          GetOperand(*lstm.peephole_weight_operand_id).descriptor));
    } else {
      const OperandDescriptor& peephole_weight_descriptor =
          GetOperand(*lstm.peephole_weight_operand_id).descriptor;
      CHECK(context_properties_.data_type_limits.lstm_cell_bias.Supports(
          peephole_weight_descriptor));
      // Reshape the peephole_weight into a 2-D tensor, since the LSTM of ONNX
      // requires the peephole_weight shape to be [num_directions,
      // 3*hidden_size]. For lstmCell, `num_directions` is equal to 1.
      const std::vector<uint32_t>& peephole_weight_shape =
          peephole_weight_descriptor.shape();
      peephole_weight =
          CreateReshapeNode(peephole_weight, {1, peephole_weight_shape[0]});
    }
  }
  inputs.push_back(peephole_weight.c_str());

  std::vector<ScopedOrtOpAttr> attributes;
  attributes.reserve(3);
  base::cstring_view direction = "forward";
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    direction = GetRecurrentNetworkDirection(lstm.direction);
  }
  attributes.push_back(
      model_editor_.CreateAttribute(kAttrDirection, direction));

  const std::vector<base::cstring_view> activations =
      GetRecurrentNetworkActivations(lstm.activations,
                                     direction == "bidirectional");
  std::vector<const char*> activations_c_str;
  for (const auto& activation : activations) {
    activations_c_str.push_back(activation.c_str());
  }
  attributes.push_back(
      model_editor_.CreateAttribute(kAttrActivations, activations_c_str));

  attributes.push_back(model_editor_.CreateAttribute(
      kAttrHiddenSize, base::checked_cast<int64_t>(hidden_size)));

  std::string output, output_hidden, output_cell;
  if constexpr (std::is_same_v<LstmType, mojom::Lstm>) {
    output_hidden = GetOperandNameById(lstm.output_operand_ids[0]);
    output_cell = GetOperandNameById(lstm.output_operand_ids[1]);
    if (lstm.return_sequence) {
      output = GetOperandNameById(lstm.output_operand_ids[2]);
    }
  } else {
    output_hidden = GenerateOperandName();
    output_cell = GenerateOperandName();
  }
  std::array<const char*, 3> outputs = {output.c_str(), output_hidden.c_str(),
                                        output_cell.c_str()};
  model_editor_.AddNode(kOpTypeLstm, node_name, inputs, outputs, attributes);

  if constexpr (std::is_same_v<LstmType, mojom::LstmCell>) {
    // Reshape the output_hidden and output_cell back to 2-D tensors, since the
    // LSTM of WebNN requires the "output_h"/"output_c" shape to be
    // [batch_size, hidden_size].
    const std::vector<uint32_t>& output_shape =
        GetOperand(lstm.output_operand_ids[0]).descriptor.shape();
    CHECK_EQ(output_shape.size(), 2u);
    InsertReshapeNode(output_hidden,
                      GetOperandNameById(lstm.output_operand_ids[0]),
                      output_shape);
    InsertReshapeNode(output_cell,
                      GetOperandNameById(lstm.output_operand_ids[1]),
                      output_shape);
  }
}

template void GraphBuilderOrt::AddLstmOperation(const mojom::Lstm& lstm);

template void GraphBuilderOrt::AddLstmOperation(
    const mojom::LstmCell& lstm_cell);

void GraphBuilderOrt::AddMatMulOperation(const mojom::Matmul& matmul) {
  const std::string node_name = GenerateNodeName(matmul.label);
  const std::string input_a = GetOperandNameById(matmul.a_operand_id);
  const std::string input_b = GetOperandNameById(matmul.b_operand_id);
  const std::string output = GetOperandNameById(matmul.output_operand_id);

  CHECK(context_properties_.data_type_limits.matmul_input.SupportsAll(
      {GetOperand(matmul.a_operand_id).descriptor,
       GetOperand(matmul.b_operand_id).descriptor}));

  std::array<const char*, 2> inputs = {input_a.c_str(), input_b.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeMatMul, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddPool2dOperation(const mojom::Pool2d& pool2d) {
  std::vector<ScopedOrtOpAttr> attributes;

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

void GraphBuilderOrt::AddReduceOperation(const mojom::Reduce& reduce) {
  const std::string input = GetOperandNameById(reduce.input_operand_id);
  const std::string output = GetOperandNameById(reduce.output_operand_id);
  std::vector<const char*> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  CheckReduceInputSupported(context_properties_.data_type_limits, reduce.kind,
                            GetOperand(reduce.input_operand_id).descriptor);

  // According to
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-reduce,
  // if axes is empty, WebNN applies reduction function to each value in the
  // tensor individually with no dimensions reduced, but the ONNX reduction
  // operations either reduce all dimensions or act as a no-op. So we need to
  // emulate the behavior of reducing each value individually:
  // 1. Element-wise log for reduceLogSum
  // 2. Element-wise pow of 2 for reduceSumSquare
  // 3. Element-wise abs for reduceL1 and reduceL2
  // 4. No-op for other reduction operations e.g. reduceMin and reduceSum
  //
  // TODO(crbug.com/429272269): Remove the workaround for reduction operations
  // when ORT issue is fixed.
  // https://github.com/onnx/onnx/issues/6103
  if (reduce.axes.empty()) {
    switch (reduce.kind) {
      case mojom::Reduce::Kind::kLogSum: {
        const std::string node_name =
            GenerateNodeName(GenerateEmulatedOpLabel(kOpTypeLog, reduce.label));
        model_editor_.AddNode(kOpTypeLog, node_name, inputs, outputs);
        return;
      }
      case mojom::Reduce::Kind::kSumSquare: {
        const std::string node_name =
            GenerateNodeName(GenerateEmulatedOpLabel(kOpTypePow, reduce.label));
        const std::string pow = CreateScalarInitializer<int64_t>(2);
        inputs.push_back(pow.c_str());
        model_editor_.AddNode(kOpTypePow, node_name, inputs, outputs);
        return;
      }
      case mojom::Reduce::Kind::kL1:
      case mojom::Reduce::Kind::kL2: {
        const std::string node_name =
            GenerateNodeName(GenerateEmulatedOpLabel(kOpTypeAbs, reduce.label));
        model_editor_.AddNode(kOpTypeAbs, node_name, inputs, outputs);
        return;
      }
      case mojom::Reduce::Kind::kLogSumExp:
      case mojom::Reduce::Kind::kMax:
      case mojom::Reduce::Kind::kMean:
      case mojom::Reduce::Kind::kMin:
      case mojom::Reduce::Kind::kProduct:
      case mojom::Reduce::Kind::kSum:
        // Setting the `noop_with_empty_axes` attribute to 1 will make them act
        // as a no-op.
        break;
    }
  }

  const std::string axes = CreateInt64InitializerForUint32Array(reduce.axes);
  inputs.push_back(axes.c_str());

  int64_t keepdims = reduce.keep_dimensions ? 1 : 0;
  int64_t noop_with_empty_axes = 1;
  std::array<ScopedOrtOpAttr, 2> attributes = {
      model_editor_.CreateAttribute(kAttrKeepDims, keepdims),
      model_editor_.CreateAttribute(kAttrNoopWithEmptyAxes,
                                    noop_with_empty_axes),
  };

  const std::string node_name = GenerateNodeName(reduce.label);
  base::cstring_view reduce_op_type = MapReduceKindToOrtOpType(reduce.kind);
  model_editor_.AddNode(reduce_op_type, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddResample2dOperation(
    const mojom::Resample2d& resample2d) {
  const std::string node_name = GenerateNodeName(resample2d.label);
  const std::string input = GetOperandNameById(resample2d.input_operand_id);
  const std::string output = GetOperandNameById(resample2d.output_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(resample2d.input_operand_id).descriptor;
  CHECK(context_properties_.data_type_limits.resample2d_input.Supports(
      input_descriptor));

  std::string scales;
  std::string sizes;
  if (resample2d.scales) {
    // Each element of scales applies to a dimension of the input.
    CHECK_EQ(input_descriptor.Rank(), 4u);
    std::array<float, 4> scales_data = {1.f, 1.f, 1.f, 1.f};
    CHECK_EQ(resample2d.axes.size(), 2u);
    CHECK_EQ(resample2d.scales->size(), 2u);
    scales_data.at(resample2d.axes[0]) = resample2d.scales->at(0);
    scales_data.at(resample2d.axes[1]) = resample2d.scales->at(1);
    scales = Create1DInitializer<float>(scales_data);
  } else {
    sizes = CreateInt64InitializerForUint32Array(
        GetOperand(resample2d.output_operand_id).descriptor.shape());
  }

  std::string mode;
  switch (resample2d.mode) {
    case mojom::Resample2d::InterpolationMode::kLinear:
      mode = "linear";
      break;
    case mojom::Resample2d::InterpolationMode::kNearestNeighbor:
      mode = "nearest";
      break;
  }

  AddResizeNode(node_name, input, scales, sizes, mode, output);
}

void GraphBuilderOrt::AddReshapeOperation(const mojom::Reshape& reshape) {
  const std::string node_name = GenerateNodeName(reshape.label);
  const std::string input = GetOperandNameById(reshape.input_operand_id);
  const std::string output = GetOperandNameById(reshape.output_operand_id);

  CHECK(context_properties_.data_type_limits.reshape_input.Supports(
      GetOperand(reshape.input_operand_id).descriptor));

  const std::vector<uint32_t>& output_shape =
      GetOperand(reshape.output_operand_id).descriptor.shape();

  AddReshapeNode(node_name, input, output, output_shape);
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
          : CreateCastNode(indices, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);

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
      constant = CreateScalarInitializer(input_descriptor.data_type(),
                                         pad.mode->get_constant()->value);
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

  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAttrAxis, base::checked_cast<int64_t>(split.axis))};

  model_editor_.AddNode(kOpTypeSplit, node_name, inputs, outputs, attributes);
}

void GraphBuilderOrt::AddTileOperation(const mojom::Tile& tile) {
  const std::string input = GetOperandNameById(tile.input_operand_id);
  const std::string output = GetOperandNameById(tile.output_operand_id);

  const OperandDescriptor& input_descriptor =
      GetOperand(tile.input_operand_id).descriptor;
  CHECK(context_properties_.data_type_limits.tile_input.Supports(
      input_descriptor));

  std::vector<const char*> inputs = {input.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  // Emulate the tile operation with identity operation for unsupported scalar
  // input.
  // TODO(crbug.com/433414906): Remove the workaround for unsupported scalar
  // input when the ORT tile operation issue is fixed.
  // https://github.com/microsoft/onnxruntime/issues/11523
  if (input_descriptor.Rank() == 0) {
    const std::string node_name = GenerateNodeName(base::JoinString(
        {kInserted, kOpTypeIdentity, kToEmulate, tile.label}, kUnderscore));
    model_editor_.AddNode(kOpTypeIdentity, node_name, inputs, outputs);
    return;
  }

  const std::string repeats =
      CreateInt64InitializerForUint32Array(tile.repetitions);
  inputs.push_back(repeats.c_str());
  const std::string node_name = GenerateNodeName(tile.label);
  model_editor_.AddNode(kOpTypeTile, node_name, inputs, outputs);
}

void GraphBuilderOrt::AddTransposeOperation(const mojom::Transpose& transpose) {
  const std::string node_name = GenerateNodeName(transpose.label);
  const std::string input = GetOperandNameById(transpose.input_operand_id);
  const std::string output = GetOperandNameById(transpose.output_operand_id);

  CHECK(context_properties_.data_type_limits.transpose_input.Supports(
      GetOperand(transpose.input_operand_id).descriptor));

  AddTransposeNode(node_name, input, output, transpose.permutation);
}

void GraphBuilderOrt::AddTriangularOperation(
    const mojom::Triangular& triangular) {
  const std::string node_name = GenerateNodeName(triangular.label);
  const std::string input = GetOperandNameById(triangular.input_operand_id);
  const std::string output = GetOperandNameById(triangular.output_operand_id);

  CHECK(context_properties_.data_type_limits.triangular_input.Supports(
      GetOperand(triangular.input_operand_id).descriptor));

  const std::string diagonal =
      CreateScalarInitializer(static_cast<int64_t>(triangular.diagonal));
  std::array<const char*, 2> inputs = {input.c_str(), diagonal.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  std::array<ScopedOrtOpAttr, 1> attributes = {model_editor_.CreateAttribute(
      kAttrUpper, static_cast<int64_t>(triangular.upper))};

  model_editor_.AddNode(kOpTypeTriangular, node_name, inputs, outputs,
                        attributes);
}

void GraphBuilderOrt::AddWhereOperation(const mojom::Where& where) {
  const std::string node_name = GenerateNodeName(where.label);
  std::string condition = GetOperandNameById(where.condition_operand_id);
  const std::string true_value =
      GetOperandNameById(where.true_value_operand_id);
  const std::string false_value =
      GetOperandNameById(where.false_value_operand_id);
  const std::string output = GetOperandNameById(where.output_operand_id);

  const OperandDescriptor& condition_descriptor =
      GetOperand(where.condition_operand_id).descriptor;
  const OperandDescriptor& true_value_descriptor =
      GetOperand(where.true_value_operand_id).descriptor;
  const OperandDescriptor& false_value_descriptor =
      GetOperand(where.false_value_operand_id).descriptor;

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  CHECK(data_type_limits.where_condition.Supports(condition_descriptor));
  CHECK(data_type_limits.where_value.Supports(true_value_descriptor));
  CHECK(data_type_limits.where_value.Supports(false_value_descriptor));

  // ONNX where operation only supports bool condition input.
  CHECK_EQ(condition_descriptor.data_type(), OperandDataType::kUint8);
  condition = CreateCastNode(condition, ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL);

  std::array<const char*, 3> inputs = {condition.c_str(), true_value.c_str(),
                                       false_value.c_str()};
  std::array<const char*, 1> outputs = {output.c_str()};

  model_editor_.AddNode(kOpTypeWhere, node_name, inputs, outputs);
}

std::unique_ptr<ModelEditor::ModelInfo> GraphBuilderOrt::BuildModel() {
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
      case mojom::Operation::Tag::kBatchNormalization: {
        AddBatchNormalizationOperation(*operation->get_batch_normalization());
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
      case mojom::Operation::Tag::kCumulativeSum: {
        AddCumulativeSumOperation(*operation->get_cumulative_sum());
        break;
      }
      case mojom::Operation::Tag::kDequantizeLinear: {
        CHECK(data_type_limits.dequantize_linear_input.SupportsAll(
            {GetOperand(operation->get_dequantize_linear()->input_operand_id)
                 .descriptor,
             GetOperand(
                 operation->get_dequantize_linear()->zero_point_operand_id)
                 .descriptor}));
        CHECK(data_type_limits.dequantize_linear_scale.Supports(
            GetOperand(operation->get_dequantize_linear()->scale_operand_id)
                .descriptor));
        AddDequantizeOrQuantizeLinearOperation(
            *operation->get_dequantize_linear(), kOpTypeDequantizeLinear);
        break;
      }
      case mojom::Operation::Tag::kElu: {
        AddEluOperation(*operation->get_elu());
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
      case mojom::Operation::Tag::kGru: {
        AddGruOperation(*operation->get_gru());
        break;
      }
      case mojom::Operation::Tag::kGruCell: {
        AddGruOperation(*operation->get_gru_cell());
        break;
      }
      case mojom::Operation::Tag::kHardSigmoid: {
        AddHardSigmoidOperation(*operation->get_hard_sigmoid());
        break;
      }
      case mojom::Operation::Tag::kHardSwish: {
        CHECK(data_type_limits.hard_swish_input.Supports(
            GetOperand(operation->get_hard_swish()->input_operand_id)
                .descriptor));
        AddUnaryOperation(*operation->get_hard_swish(), kOpTypeHardSwish);
        break;
      }
      case mojom::Operation::Tag::kInstanceNormalization: {
        AddInstanceNormalizationOperation(
            *operation->get_instance_normalization());
        break;
      }
      case mojom::Operation::Tag::kLayerNormalization: {
        AddLayerNormalizationOperation(*operation->get_layer_normalization());
        break;
      }
      case mojom::Operation::Tag::kLeakyRelu: {
        AddLeakyReluOperation(*operation->get_leaky_relu());
        break;
      }
      case mojom::Operation::Tag::kLinear: {
        AddLinearOperation(*operation->get_linear());
        break;
      }
      case mojom::Operation::Tag::kLstm: {
        AddLstmOperation(*operation->get_lstm());
        break;
      }
      case mojom::Operation::Tag::kLstmCell: {
        AddLstmOperation(*operation->get_lstm_cell());
        break;
      }
      case mojom::Operation::Tag::kMatmul: {
        AddMatMulOperation(*operation->get_matmul());
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
      case mojom::Operation::Tag::kQuantizeLinear: {
        CHECK(data_type_limits.quantize_linear_input.SupportsAll(
            {GetOperand(operation->get_quantize_linear()->input_operand_id)
                 .descriptor,
             GetOperand(operation->get_quantize_linear()->scale_operand_id)
                 .descriptor}));
        CHECK(data_type_limits.quantize_linear_zero_point.Supports(
            GetOperand(operation->get_quantize_linear()->zero_point_operand_id)
                .descriptor));
        AddDequantizeOrQuantizeLinearOperation(
            *operation->get_quantize_linear(), kOpTypeQuantizeLinear);
        break;
      }
      case mojom::Operation::Tag::kRelu: {
        CHECK(data_type_limits.relu_input.Supports(
            GetOperand(operation->get_relu()->input_operand_id).descriptor));
        AddUnaryOperation(*operation->get_relu(), kOpTypeRelu);
        break;
      }
      case mojom::Operation::Tag::kReduce: {
        AddReduceOperation(*operation->get_reduce());
        break;
      }
      case mojom::Operation::Tag::kResample2d: {
        AddResample2dOperation(*operation->get_resample2d());
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
      case mojom::Operation::Tag::kSoftplus: {
        CHECK(data_type_limits.softplus_input.Supports(
            GetOperand(operation->get_softplus()->input_operand_id)
                .descriptor));
        AddUnaryOperation(*operation->get_softplus(), kOpTypeSoftplus);
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
      case mojom::Operation::Tag::kTriangular: {
        AddTriangularOperation(*operation->get_triangular());
        break;
      }
      case mojom::Operation::Tag::kWhere: {
        AddWhereOperation(*operation->get_where());
        break;
      }
    }
  }

  for (OperandId output_id : graph_info_->output_operands) {
    model_editor_.AddOutput(GetOperandNameById(output_id),
                            GetOperand(output_id));
  }

  return model_editor_.BuildAndTakeModelInfo();
}

}  // namespace webnn::ort
