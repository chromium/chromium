// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/graph_impl.h"

#include <algorithm>
#include <array>
#include <limits>

#include "base/bits.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/graph_builder.h"
#include "services/webnn/dml/tensor_desc.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/error.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {
namespace {

using Microsoft::WRL::ComPtr;
using mojom::ComputeResult;
using mojom::Operand;
using mojom::OperandPtr;
using mojom::Operation;

// A map of all mojom operands in `mojom::GraphInfo` using the mojom operand id
// as key.
using IdToOperandMap = base::flat_map<uint64_t, OperandPtr>;
// A map of all node outputs in `dml::GraphBuilder` using the mojom operand id
// as key.
using IdToNodeOutputMap = std::map<uint64_t, const NodeOutput*>;

constexpr const uint32_t kNhwcToNchwPermutation[] = {0, 3, 1, 2};
constexpr const uint32_t kNchwToNhwcPermutation[] = {0, 2, 3, 1};

DML_TENSOR_DATA_TYPE GetTensorDataType(Operand::DataType type) {
  switch (type) {
    case Operand::DataType::kFloat32:
      return DML_TENSOR_DATA_TYPE_FLOAT32;
    case Operand::DataType::kFloat16:
      return DML_TENSOR_DATA_TYPE_FLOAT16;
    case Operand::DataType::kInt8:
      return DML_TENSOR_DATA_TYPE_INT8;
    case Operand::DataType::kUint8:
      return DML_TENSOR_DATA_TYPE_UINT8;
    case Operand::DataType::kInt64:
      return DML_TENSOR_DATA_TYPE_INT64;
    case Operand::DataType::kUint64:
      return DML_TENSOR_DATA_TYPE_UINT64;
    case Operand::DataType::kInt32:
      return DML_TENSOR_DATA_TYPE_INT32;
    case Operand::DataType::kUint32:
      return DML_TENSOR_DATA_TYPE_UINT32;
    default:
      DLOG(ERROR) << "This data type is not supported.";
      NOTREACHED_NORETURN();
  }
}

std::string OpKindToString(mojom::ElementWiseBinary::Kind kind) {
  switch (kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      return "add";
    case mojom::ElementWiseBinary::Kind::kSub:
      return "sub";
    case mojom::ElementWiseBinary::Kind::kMul:
      return "mul";
    case mojom::ElementWiseBinary::Kind::kDiv:
      return "div";
    case mojom::ElementWiseBinary::Kind::kMax:
      return "max";
    case mojom::ElementWiseBinary::Kind::kMin:
      return "min";
    case mojom::ElementWiseBinary::Kind::kPow:
      return "pow";
    case mojom::ElementWiseBinary::Kind::kEqual:
      return "equal";
    case mojom::ElementWiseBinary::Kind::kGreater:
      return "greater";
    case mojom::ElementWiseBinary::Kind::kLesser:
      return "lesser";
  }
  NOTREACHED_NORETURN();
}

std::string ReduceOpKindToString(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return "ReduceL1";
    case mojom::Reduce::Kind::kL2:
      return "ReduceL2";
    case mojom::Reduce::Kind::kLogSum:
      return "ReduceLogSum";
    case mojom::Reduce::Kind::kLogSumExp:
      return "ReduceLogSumExp";
    case mojom::Reduce::Kind::kMax:
      return "ReduceMax";
    case mojom::Reduce::Kind::kMean:
      return "ReduceMean";
    case mojom::Reduce::Kind::kMin:
      return "ReduceMin";
    case mojom::Reduce::Kind::kProduct:
      return "ReduceProduct";
    case mojom::Reduce::Kind::kSum:
      return "ReduceSum";
    case mojom::Reduce::Kind::kSumSquare:
      return "ReduceSumSquare";
  }
  NOTREACHED_NORETURN();
}

DML_REDUCE_FUNCTION MapReduceKindToReduceFuntion(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return DML_REDUCE_FUNCTION_L1;
    case mojom::Reduce::Kind::kL2:
      return DML_REDUCE_FUNCTION_L2;
    case mojom::Reduce::Kind::kLogSum:
      return DML_REDUCE_FUNCTION_LOG_SUM;
    case mojom::Reduce::Kind::kLogSumExp:
      return DML_REDUCE_FUNCTION_LOG_SUM_EXP;
    case mojom::Reduce::Kind::kMax:
      return DML_REDUCE_FUNCTION_MAX;
    case mojom::Reduce::Kind::kMean:
      return DML_REDUCE_FUNCTION_AVERAGE;
    case mojom::Reduce::Kind::kMin:
      return DML_REDUCE_FUNCTION_MIN;
    case mojom::Reduce::Kind::kProduct:
      return DML_REDUCE_FUNCTION_MULTIPLY;
    case mojom::Reduce::Kind::kSum:
      return DML_REDUCE_FUNCTION_SUM;
    case mojom::Reduce::Kind::kSumSquare:
      return DML_REDUCE_FUNCTION_SUM_SQUARE;
  }
  NOTREACHED_NORETURN();
}
std::string OpTagToString(Operation::Tag tag) {
  switch (tag) {
    case Operation::Tag::kBatchNormalization:
      return "batchNormalization";
    case Operation::Tag::kClamp:
      return "clamp";
    case Operation::Tag::kConcat:
      return "concat";
    case Operation::Tag::kConv2d:
      return "conv2d";
    case Operation::Tag::kElementWiseBinary:
      return "element-wise binary";
    case Operation::Tag::kElu:
      return "elu";
    case Operation::Tag::kElementWiseUnary:
      return "element-wise unary";
    case Operation::Tag::kExpand:
      return "expand";
    case Operation::Tag::kGather:
      return "gather";
    case Operation::Tag::kGemm:
      return "gemm";
    case Operation::Tag::kLeakyRelu:
      return "leakyRelu";
    case Operation::Tag::kMatmul:
      return "matmul";
    case Operation::Tag::kPad:
      return "pad";
    case Operation::Tag::kPool2d:
      return "pool2d";
    case Operation::Tag::kPrelu:
      return "prelu";
    case Operation::Tag::kReduce:
      return "reduce";
    case Operation::Tag::kRelu:
      return "relu";
    case Operation::Tag::kResample2d:
      return "resample2d";
    case Operation::Tag::kReshape:
      return "reshape";
    case Operation::Tag::kSigmoid:
      return "sigmoid";
    case Operation::Tag::kSlice:
      return "slice";
    case Operation::Tag::kSoftmax:
      return "softmax";
    case Operation::Tag::kSplit:
      return "split";
    case Operation::Tag::kTanh:
      return "tanh";
    case Operation::Tag::kTranspose:
      return "transpose";
    case Operation::Tag::kWhere:
      return "where";
  }
  NOTREACHED_NORETURN();
}

// Calculate the total byte length of buffers and the D3D12_RANGE for each
// buffer, all with the required alignment.
template <typename Map>
absl::optional<AlignedByteLength<typename Map::key_type>>
CalculateAlignedByteLength(const Map& buffer_to_byte_length_map) {
  base::CheckedNumeric<size_t> total_byte_length(0);
  std::map<typename Map::key_type, D3D12_RANGE> key_to_d3d12_range_map;

  for (auto& [buffer, byte_length] : buffer_to_byte_length_map) {
    auto& d3d12_range = key_to_d3d12_range_map[buffer];
    d3d12_range.Begin = total_byte_length.ValueOrDie();

    // The buffer has a minimum base address alignment requirement of 16 bytes
    // in the macro `DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT`:
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-directml-constants
    total_byte_length += base::bits::AlignUp<size_t>(
        byte_length, DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT);
    if (!total_byte_length.IsValid()) {
      DLOG(ERROR) << "Failed to calculate the total byte length.";
      return absl::nullopt;
    }

    // The aligned byte length calculated with `End` sub `Begin` attribute is
    // used to set the `SizeInBytes` field of `DML_BUFFER_BINDING`.
    d3d12_range.End = total_byte_length.ValueOrDie();
  }

  return AlignedByteLength<typename Map::key_type>{
      .total_byte_length = total_byte_length.ValueOrDie(),
      .key_to_d3d12_range_map = std::move(key_to_d3d12_range_map)};
}

// Upload constants/inputs buffers in one Direct3D 12 committed resource, the
// DML_BUFFER_BINDING specifies a resource binding described by a range of bytes
// in the single buffer.
template <typename Key>
absl::optional<std::map<Key, DML_BUFFER_BINDING>> UploadAndCreateBufferBinding(
    CommandRecorder* command_recorder,
    const base::flat_map<Key, mojo_base::BigBuffer>& key_to_buffer_map,
    const AlignedByteLength<Key>& aligned_byte_length,
    ComPtr<ID3D12Resource> upload_buffer,
    ComPtr<ID3D12Resource> default_buffer) {
  // Map entire resource to copy the array buffer of constant/input one by one
  // with byte offset.
  void* mapped_upload_buffer = nullptr;
  HRESULT hr = upload_buffer->Map(0, nullptr, &mapped_upload_buffer);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to map upload buffer for inputs: "
                << logging::SystemErrorCodeToString(hr);
    return absl::nullopt;
  }

  std::map<Key, DML_BUFFER_BINDING> key_to_buffer_binding_map;
  for (auto& [key, buffer] : key_to_buffer_map) {
    // Copy the input data to the upload heap with byte offset
    const auto& d3d12_range =
        aligned_byte_length.key_to_d3d12_range_map.at(key);
    memcpy(static_cast<uint8_t*>(mapped_upload_buffer) + d3d12_range.Begin,
           buffer.data(), buffer.size());
    // Create the buffer binding for each constant/input and push back into the
    // DML_BUFFER_BINDING array.
    auto size_in_bytes = d3d12_range.End - d3d12_range.Begin;
    key_to_buffer_binding_map[key] =
        DML_BUFFER_BINDING{.Buffer = default_buffer.Get(),
                           .Offset = d3d12_range.Begin,
                           .SizeInBytes = size_in_bytes};
  }
  upload_buffer->Unmap(0, nullptr);

  UploadBufferWithBarrier(command_recorder, std::move(default_buffer),
                          std::move(upload_buffer),
                          aligned_byte_length.total_byte_length);

  return key_to_buffer_binding_map;
}

// Define some methods like CreateInputNode and CreateOperatorNodeForRelu here
// to focus on converting the mojo graph struct to corresponding DML graph node
// by using dml::GraphBuilder as a helper. dml::GraphBuilder should be decoupled
// from mojo graph structs and focus on manipulating DML graph structs.
//
// Create the input node of graph for computation with the default tensor flag,
// specifying the DML_TENSOR_FLAG_OWNED_BY_DML is to create input node for
// constant weight data.
//
// The return value is the GraphInputIndex assigned by graph builder.
uint32_t CreateInputNode(const IdToOperandMap& id_to_operand_map,
                         uint64_t input_id,
                         GraphBuilder& graph_builder,
                         IdToNodeOutputMap& id_to_node_output_map,
                         DML_TENSOR_FLAGS flags = DML_TENSOR_FLAG_NONE) {
  const OperandPtr& operand = id_to_operand_map.at(input_id);
  TensorDesc input_tensor_desc(GetTensorDataType(operand->data_type), flags,
                               operand->dimensions);
  const InputNode* input_node = graph_builder.CreateInputNode();
  CHECK(input_node);
  const NodeOutput* node_output =
      graph_builder.CreateNodeOutput(input_node, std::move(input_tensor_desc));
  CHECK(node_output);
  id_to_node_output_map[input_id] = std::move(node_output);
  return input_node->GetGraphInputIndex();
}

const NodeOutput* GetNodeOutputForOperand(
    const IdToNodeOutputMap& id_to_node_output_map,
    uint64_t operand_id) {
  const auto input_iterator = id_to_node_output_map.find(operand_id);
  CHECK(input_iterator != id_to_node_output_map.end());
  CHECK(input_iterator->second);
  return input_iterator->second;
}

const TensorDesc CreateOutputTensorDesc(const IdToOperandMap& id_to_operand_map,
                                        uint64_t output_id) {
  const OperandPtr& output_operand = id_to_operand_map.at(output_id);
  return TensorDesc(GetTensorDataType(output_operand->data_type),
                    output_operand->dimensions);
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForClamp(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ClampPtr& clamp,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, clamp->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  uint64_t output_id = clamp->output_operand_id;
  auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  DML_ELEMENT_WISE_CLIP_OPERATOR_DESC clamp_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      // No scale or bias applies to the input.
      .ScaleBias = nullptr,
      .Min = clamp->min_value,
      .Max = clamp->max_value};
  std::array<const NodeOutput*, 1> inputs = {input};
  const OperatorNode* clamp_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_CLIP, &clamp_operator_desc, inputs);
  if (!clamp_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create clamp operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      clamp_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForConcat(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ConcatPtr& concat,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_operand_ids = concat->input_operand_ids;
  size_t input_num = input_operand_ids.size();

  std::vector<const NodeOutput*> inputs;
  std::vector<DML_TENSOR_DESC> input_dml_tensor_descs;
  inputs.reserve(input_num);
  input_dml_tensor_descs.reserve(input_num);

  for (const auto& input_operand_id : input_operand_ids) {
    const NodeOutput* input =
        GetNodeOutputForOperand(id_to_node_output_map, input_operand_id);
    inputs.push_back(input);
    input_dml_tensor_descs.push_back(input->GetTensorDesc().GetDMLTensorDesc());
  }

  uint64_t output_id = concat->output_operand_id;
  auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  DML_JOIN_OPERATOR_DESC concat_operator_desc{
      .InputCount = base::checked_cast<uint32_t>(input_dml_tensor_descs.size()),
      .InputTensors = input_dml_tensor_descs.data(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Axis = concat->axis};

  const OperatorNode* concat_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_JOIN, &concat_operator_desc, inputs);
  if (!concat_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create concat operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      concat_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

struct ActivationOperatorDesc {
  absl::variant<DML_ACTIVATION_ELU_OPERATOR_DESC,
                DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC,
                DML_ACTIVATION_RELU_OPERATOR_DESC,
                DML_ACTIVATION_SIGMOID_OPERATOR_DESC,
                DML_ACTIVATION_TANH_OPERATOR_DESC>
      desc;

  DML_OPERATOR_DESC GetActivationDmlDesc() const {
    if (absl::holds_alternative<DML_ACTIVATION_ELU_OPERATOR_DESC>(desc)) {
      return {DML_OPERATOR_ACTIVATION_ELU,
              &absl::get<DML_ACTIVATION_ELU_OPERATOR_DESC>(desc)};
    } else if (absl::holds_alternative<DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_LEAKY_RELU,
              &absl::get<DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC>(desc)};
    } else if (absl::holds_alternative<DML_ACTIVATION_RELU_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_RELU,
              &absl::get<DML_ACTIVATION_RELU_OPERATOR_DESC>(desc)};
    } else if (absl::holds_alternative<DML_ACTIVATION_SIGMOID_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_SIGMOID,
              &absl::get<DML_ACTIVATION_SIGMOID_OPERATOR_DESC>(desc)};
    } else if (absl::holds_alternative<DML_ACTIVATION_TANH_OPERATOR_DESC>(
                   desc)) {
      return {DML_OPERATOR_ACTIVATION_TANH,
              &absl::get<DML_ACTIVATION_TANH_OPERATOR_DESC>(desc)};
    } else {
      NOTREACHED_NORETURN() << "The activation type is not supported.";
    }
  }
};

// DML_OPERATOR_ELEMENT_WISE_CLIP will be supported after the DirectML version
// upper than DML_FEATURE_LEVEL_6_0.
// https://learn.microsoft.com/en-us/windows/ai/directml/dml-feature-level-history#dml_feature_level_6_0
base::expected<ActivationOperatorDesc, mojom::ErrorPtr>
CreateActivationOperatorDesc(const mojom::ActivationPtr& activation) {
  CHECK(activation);
  switch (activation->which()) {
    case mojom::Activation::Tag::kElu:
      return ActivationOperatorDesc{.desc = DML_ACTIVATION_ELU_OPERATOR_DESC{
                                        .Alpha = activation->get_elu()->alpha}};
    case mojom::Activation::Tag::kLeakyRelu:
      return ActivationOperatorDesc{
          .desc = DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC{
              .Alpha = activation->get_leaky_relu()->alpha}};
    case mojom::Activation::Tag::kRelu:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_RELU_OPERATOR_DESC{}};
    case mojom::Activation::Tag::kSigmoid:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_SIGMOID_OPERATOR_DESC{}};
    case mojom::Activation::Tag::kTanh:
      return ActivationOperatorDesc{.desc =
                                        DML_ACTIVATION_TANH_OPERATOR_DESC{}};
    default:
      return base::unexpected(
          CreateError(mojom::Error::Code::kNotSupportedError,
                      "The fused activation type is not supported."));
  }
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForConv2d(
    const IdToOperandMap& id_to_operand_map,
    const mojom::Conv2dPtr& conv2d,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, conv2d->input_operand_id);
  // The input tensor description may be transposed.
  auto input_tensor_desc = input->GetTensorDesc();
  CHECK_EQ(input_tensor_desc.GetDimensions().size(), 4u);

  const NodeOutput* filter =
      GetNodeOutputForOperand(id_to_node_output_map, conv2d->filter_operand_id);
  const auto& filter_tensor_desc = filter->GetTensorDesc();

  uint64_t output_id = conv2d->output_operand_id;
  // The output tensor description may be transposed.
  auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  std::vector<const NodeOutput*> inputs = {input, filter};
  absl::optional<TensorDesc> reshaped_bias_tensor_desc;
  auto& bias_operand_id = conv2d->bias_operand_id;
  if (bias_operand_id) {
    const auto bias_node_output_iterator =
        id_to_node_output_map.find(bias_operand_id.value());
    CHECK(bias_node_output_iterator != id_to_node_output_map.end());
    const NodeOutput* bias_node_output = bias_node_output_iterator->second;
    CHECK(bias_node_output);
    const auto& bias_tensor_desc = bias_node_output->GetTensorDesc();
    const auto& bias_dims = bias_tensor_desc.GetDimensions();
    CHECK_EQ(bias_dims.size(), 1u);

    // In WebNN spec bias specifies the additional 1-D tensor with the shape of
    // {outputChannels}. But for DML the expected dimensions of the BiasTensor
    // are { 1, OutputChannelCount, 1, 1 } for 4D. So reshape the bias:
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_convolution_operator_desc
    std::vector<uint32_t> reshaped_bias_dims = {1, bias_dims[0], 1, 1};
    reshaped_bias_tensor_desc =
        TensorDesc(bias_tensor_desc.GetDataType(), bias_tensor_desc.GetFlags(),
                   std::move(reshaped_bias_dims));

    const NodeOutput* reshaped_bias_node_output =
        graph_builder.CreateNodeOutput(&bias_node_output->GetNode(),
                                       reshaped_bias_tensor_desc.value());
    inputs.push_back(reshaped_bias_node_output);
  }

  switch (conv2d->input_layout) {
    case mojom::InputOperandLayout::kChannelsFirst: {
      break;
    }
    // DML convolution operator only support nchw layout according to
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_convolution_operator_desc
    //
    // To support other layouts, we can transpose the input and output
    // tensors
    case mojom::InputOperandLayout::kChannelsLast: {
      input_tensor_desc.Transpose(kNhwcToNchwPermutation);
      output_tensor_desc.Transpose(kNhwcToNchwPermutation);
      break;
    }
  }

  std::array<uint32_t, 2> strides = {conv2d->strides->height,
                                     conv2d->strides->width};
  std::array<uint32_t, 2> dilations = {conv2d->dilations->height,
                                       conv2d->dilations->width};
  std::array<uint32_t, 2> start_padding = {conv2d->padding->beginning->height,
                                           conv2d->padding->beginning->width};
  std::array<uint32_t, 2> end_padding = {conv2d->padding->ending->height,
                                         conv2d->padding->ending->width};

  // The outputSizes of WebNN convTranspose2d specifies the sizes of the last
  // two dimensions of the output tensor but the outputPadding of DirectML
  // convolution applies a zero padding to the result of the operator. Since
  // graph builder will explicitly pass in the output tensor shape anyway. So,
  // there is no ambiguity of the output shape and we set the output_padding to
  // {0, 0}:
  // https://www.w3.org/TR/webnn/#dom-mlconvtranspose2doptions-outputpadding
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_convolution_operator_desc
  std::array<uint32_t, 2> default_out_padding = {0, 0};

  absl::optional<ActivationOperatorDesc> activation_operator_desc;
  absl::optional<DML_OPERATOR_DESC> activation_dml_desc;
  if (conv2d->activation) {
    auto create_activation_result =
        CreateActivationOperatorDesc(conv2d->activation);
    if (!create_activation_result.has_value()) {
      return base::unexpected(std::move(create_activation_result.error()));
    }

    activation_operator_desc = std::move(create_activation_result.value());
    activation_dml_desc = activation_operator_desc->GetActivationDmlDesc();
  }

  DML_CONVOLUTION_DIRECTION conv2d_direction;
  switch (conv2d->type) {
    case mojom::Conv2d_Type::kDirect:
      conv2d_direction =
          DML_CONVOLUTION_DIRECTION::DML_CONVOLUTION_DIRECTION_FORWARD;
      break;
    case mojom::Conv2d_Type::kTransposed:
      conv2d_direction =
          DML_CONVOLUTION_DIRECTION::DML_CONVOLUTION_DIRECTION_BACKWARD;
      break;
  }

  DML_CONVOLUTION_OPERATOR_DESC conv2d_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .FilterTensor = &filter_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = (reshaped_bias_tensor_desc.has_value())
                        ? &reshaped_bias_tensor_desc->GetDMLTensorDesc()
                        : nullptr,
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION,
      .Direction = conv2d_direction,
      .DimensionCount =
          2u, /*Determines the size of the Strides, Dilations, StartPadding,
                 EndPadding, and OutputPadding arrays.*/
      .Strides = strides.data(),
      .Dilations = dilations.data(),
      .StartPadding = start_padding.data(),
      .EndPadding = end_padding.data(),
      .OutputPadding = default_out_padding.data(),
      .GroupCount = conv2d->groups,
      .FusedActivation =
          activation_dml_desc ? &activation_dml_desc.value() : nullptr};

  const OperatorNode* conv2d_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_CONVOLUTION, &conv2d_operator_desc, inputs);
  if (!conv2d_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create conv2d operator."));
  }

  if (conv2d->input_layout == mojom::InputOperandLayout::kChannelsLast) {
    // Transpose the output tensor from nchw to nhwc layout.
    output_tensor_desc.Transpose(kNchwToNhwcPermutation);
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      conv2d_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

template <typename DML_OPERATOR_DESC>
const OperatorNode* CreateBinaryOperator(const TensorDesc& a_tensor,
                                         const TensorDesc& b_tensor,
                                         const TensorDesc& output_tensor,
                                         GraphBuilder& graph_builder,
                                         DML_OPERATOR_TYPE operator_type,
                                         base::span<const NodeOutput*> inputs) {
  DML_OPERATOR_DESC binary_operator_desc{
      .ATensor = &a_tensor.GetDMLTensorDesc(),
      .BTensor = &b_tensor.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor.GetDMLTensorDesc()};
  return graph_builder.CreateOperatorNode(operator_type, &binary_operator_desc,
                                          inputs);
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForBinary(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ElementWiseBinaryPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  // The input a and b tensor descriptions may be broadcasted.
  const NodeOutput* input_a =
      GetNodeOutputForOperand(id_to_node_output_map, operation->lhs_operand);
  auto input_a_tensor_desc = input_a->GetTensorDesc();
  const NodeOutput* input_b =
      GetNodeOutputForOperand(id_to_node_output_map, operation->rhs_operand);
  auto input_b_tensor_desc = input_b->GetTensorDesc();

  uint64_t output_id = operation->output_operand;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  auto output_dimensions = output_tensor_desc.GetDimensions();
  if (input_a_tensor_desc.GetDimensions() != output_dimensions) {
    input_a_tensor_desc.BroadcastTo(output_dimensions);
  }
  if (input_b_tensor_desc.GetDimensions() != output_dimensions) {
    input_b_tensor_desc.BroadcastTo(output_dimensions);
  }

  const OperatorNode* binary_node = nullptr;
  std::array<const NodeOutput*, 2> inputs = {input_a, input_b};
  switch (operation->kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_ADD_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_ADD, inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kDiv: {
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_DIVIDE_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_DIVIDE, inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMax: {
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_MAX_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_MAX, inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMin: {
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_MIN_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_MIN, inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMul: {
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_MULTIPLY, inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kSub: {
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_SUBTRACT_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_SUBTRACT, inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kPow: {
      DML_ELEMENT_WISE_POW_OPERATOR_DESC element_wise_operator_desc{
          .InputTensor = &input_a_tensor_desc.GetDMLTensorDesc(),
          .ExponentTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};
      binary_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_ELEMENT_WISE_POW, &element_wise_operator_desc, inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kEqual: {
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_LOGICAL_EQUALS_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_EQUALS, inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      binary_node = CreateBinaryOperator<
          DML_ELEMENT_WISE_LOGICAL_GREATER_THAN_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_GREATER_THAN,
          inputs);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      binary_node = CreateBinaryOperator<
          DML_ELEMENT_WISE_LOGICAL_LESS_THAN_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_LOGICAL_LESS_THAN, inputs);
      break;
    }
  }
  if (!binary_node) {
    std::string error_message =
        "Failed to create " + OpKindToString(operation->kind) + " operator.";
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        std::move(error_message)));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      binary_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForPad(
    const IdToOperandMap& id_to_operand_map,
    const mojom::PadPtr& pad,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, pad->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  uint64_t output_id = pad->output_operand_id;
  const auto& output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  DML_PADDING_MODE padding_mode;
  // This value is ignored for other padding modes.
  float padding_value = 0;
  switch (pad->mode->which()) {
    case mojom::PaddingMode::Tag::kConstant:
      padding_mode = DML_PADDING_MODE::DML_PADDING_MODE_CONSTANT;
      padding_value = pad->mode->get_constant()->value;
      break;
    case mojom::PaddingMode::Tag::kEdge:
      padding_mode = DML_PADDING_MODE::DML_PADDING_MODE_EDGE;
      break;
    case mojom::PaddingMode::Tag::kReflection:
      padding_mode = DML_PADDING_MODE::DML_PADDING_MODE_REFLECTION;
      break;
    case mojom::PaddingMode::Tag::kSymmetric:
      padding_mode = DML_PADDING_MODE::DML_PADDING_MODE_SYMMETRIC;
      break;
  }

  const auto& beginning_padding = pad->beginning_padding;
  const auto& ending_padding = pad->ending_padding;
  CHECK_EQ(beginning_padding.size(), ending_padding.size());

  DML_PADDING_OPERATOR_DESC pad_operator_desc = {
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .PaddingMode = padding_mode,
      .PaddingValue = padding_value,
      .DimensionCount = static_cast<uint32_t>(beginning_padding.size()),
      .StartPadding = beginning_padding.data(),
      .EndPadding = ending_padding.data()};

  std::array<const NodeOutput*, 1> inputs = {input};
  const OperatorNode* pad_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_PADDING, &pad_operator_desc, {inputs});
  if (!pad_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create pad operator."));
  }

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(pad_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForPool2d(
    const IdToOperandMap& id_to_operand_map,
    const mojom::Pool2dPtr& pool2d,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, pool2d->input_operand_id);
  // The input tensor description may be transposed.
  auto input_tensor_desc = input->GetTensorDesc();

  uint64_t output_id = pool2d->output_operand_id;
  // The output tensor description may be transposed.
  auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  switch (pool2d->layout) {
    case mojom::InputOperandLayout::kChannelsFirst: {
      break;
    }
    // DML pooling operators only support nchw layout according to
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_average_pooling_operator_desc
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_max_pooling2_operator_desc.
    //
    // To support other layouts, we can transpose the input and output tensors
    // to nchw without changing the physical arrangement by modifying the
    // descriptions of dimensions, and strides which determines the number of
    // elements to traverse to reach the next element in each dimension. E.g.,
    // for a tensor with nhwc layout, dimensions [1, 2, 3, 4] and strides [24,
    // 12, 4, 1], the new tensor with nchw layout should be with dimensions [1,
    // 4, 2, 3] and strides [24, 1, 12, 4]. See details in
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_buffer_tensor_desc.
    case mojom::InputOperandLayout::kChannelsLast: {
      input_tensor_desc.Transpose(kNhwcToNchwPermutation);

      // TODO(crbug.com/1476718): Figure out the optimal physical layout for
      // output tensor.
      output_tensor_desc.Transpose(kNhwcToNchwPermutation);
      break;
    }
  }

  std::array<uint32_t, 2> strides = {pool2d->strides->height,
                                     pool2d->strides->width};
  std::array<uint32_t, 2> dilations = {pool2d->dilations->height,
                                       pool2d->dilations->width};
  std::array<uint32_t, 2> window_dimensions = {
      pool2d->window_dimensions->height, pool2d->window_dimensions->width};
  std::array<uint32_t, 2> start_padding = {pool2d->padding->beginning->height,
                                           pool2d->padding->beginning->width};
  std::array<uint32_t, 2> end_padding = {pool2d->padding->ending->height,
                                         pool2d->padding->ending->width};
  std::array<const NodeOutput*, 1> inputs = {input};
  const OperatorNode* pool2d_node = nullptr;
  switch (pool2d->kind) {
      // TODO(crbug.com/1273291): Add L2Pool2d operator.

    case mojom::Pool2d::Kind::kAveragePool2d: {
      // TODO(crbug.com/1273291): Work around dilation support for L2 and
      // average pooling. According to WebNN spec:
      // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pool2d, dilations are
      // supported by pooling operations, while for DirectML AVERAGE_POOLING and
      // LP_POOLING don't support dilations.
      // Spec issue tracked on
      // https://github.com/webmachinelearning/webnn/issues/180.
      if (dilations[0] != 1 || dilations[1] != 1) {
        DLOG(ERROR)
            << "Dilations are not supported for average pooling operator.";
        return base::unexpected(CreateError(
            mojom::Error::Code::kNotSupportedError,
            "Dilations are not supported for average pooling operator."));
      }
      DML_AVERAGE_POOLING_OPERATOR_DESC average_pooling_desc = {
          .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
          .DimensionCount =
              base::checked_cast<uint32_t>(window_dimensions.size()),
          .Strides = strides.data(),
          .WindowSize = window_dimensions.data(),
          .StartPadding = start_padding.data(),
          .EndPadding = end_padding.data(),
          // The padding elements are not counted as part of the averaging
          // calculation.
          .IncludePadding = false};
      pool2d_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_AVERAGE_POOLING, &average_pooling_desc, inputs);
      break;
    }
    case mojom::Pool2d::Kind::kMaxPool2d: {
      DML_MAX_POOLING2_OPERATOR_DESC max_pooling_desc = {
          .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
          .OutputIndicesTensor = nullptr,
          .DimensionCount =
              base::checked_cast<uint32_t>(window_dimensions.size()),
          .Strides = strides.data(),
          .WindowSize = window_dimensions.data(),
          .StartPadding = start_padding.data(),
          .EndPadding = end_padding.data(),
          .Dilations = dilations.data()};
      pool2d_node = graph_builder.CreateOperatorNode(DML_OPERATOR_MAX_POOLING2,
                                                     &max_pooling_desc, inputs);
      break;
    }
    default:
      DLOG(ERROR) << "Invalid Pool2d operator type";
      NOTREACHED_NORETURN();
  }

  if (!pool2d_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create pooling operator."));
  }
  if (pool2d->layout == mojom::InputOperandLayout::kChannelsLast) {
    // Transpose the output tensor from nchw to nhwc layout.
    output_tensor_desc.Transpose(kNchwToNhwcPermutation);
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      pool2d_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForPrelu(
    const IdToOperandMap& id_to_operand_map,
    const mojom::PreluPtr& prelu,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, prelu->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  const NodeOutput* slope =
      GetNodeOutputForOperand(id_to_node_output_map, prelu->slope_operand_id);
  auto slope_tensor_desc = slope->GetTensorDesc();

  uint64_t output_id = prelu->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  const auto& output_dimensions = output_tensor_desc.GetDimensions();
  if (slope_tensor_desc.GetDimensions() != output_dimensions) {
    slope_tensor_desc.BroadcastTo(output_dimensions);
  }

  DML_ACTIVATION_PARAMETERIZED_RELU_OPERATOR_DESC prelu_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .SlopeTensor = &slope_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};

  std::array<const NodeOutput*, 2> inputs = {input, slope};
  const OperatorNode* prelu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_PARAMETERIZED_RELU, &prelu_desc, inputs);
  if (!prelu_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create prelu operator."));
  }

  const NodeOutput* node_output =
      graph_builder.CreateNodeOutput(prelu_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForSlice(
    const IdToOperandMap& id_to_operand_map,
    const mojom::SlicePtr& slice,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  base::expected<void, mojom::ErrorPtr> create_operator_result;
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, slice->input_operand_id);
  const TensorDesc& input_tensor_desc = input->GetTensorDesc();
  const auto& input_dimensions = input_tensor_desc.GetDimensions();

  // Start and size attributes must be unpacked from the mojo interface.
  std::vector<uint32_t> starts;
  std::vector<uint32_t> sizes;
  starts.reserve(slice->starts_and_sizes.size());
  sizes.reserve(slice->starts_and_sizes.size());
  for (size_t i = 0; i < slice->starts_and_sizes.size(); ++i) {
    starts.push_back(slice->starts_and_sizes[i]->start);
    sizes.push_back(slice->starts_and_sizes[i]->size);
  }
  CHECK_EQ(input_dimensions.size(), slice->starts_and_sizes.size());

  const TensorDesc& output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, slice->output_operand_id);

  // WebNN doesn't support the strides parameter, but DML expects one. Create
  // an appropriately sized array of 1s to produce the expected operation.
  std::vector<uint32_t> strides(input_dimensions.size(), 1u);

  DML_SLICE_OPERATOR_DESC slice_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .DimensionCount = static_cast<UINT>(input_dimensions.size()),
      .Offsets = starts.data(),
      .Sizes = sizes.data(),
      .Strides = strides.data(),
  };
  std::array<const NodeOutput*, 1> input_node_output = {input};
  const OperatorNode* slice_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_SLICE, &slice_operator_desc, input_node_output);
  if (!slice_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create slice operator."));
  }

  const auto* slice_output =
      graph_builder.CreateNodeOutput(slice_node, std::move(output_tensor_desc));
  id_to_node_output_map[slice->output_operand_id] = std::move(slice_output);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForSplit(
    const IdToOperandMap& id_to_operand_map,
    const mojom::SplitPtr& split,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, split->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  // Since TensorDesc stores dimensions and strides vectors, we need to keep
  // TensorDescs until create CreateOperatorNode is called.
  std::vector<TensorDesc> output_tensor_desc;
  output_tensor_desc.reserve(split->output_operand_ids.size());
  std::vector<DML_TENSOR_DESC> output_tensor_desc_dml;
  output_tensor_desc_dml.reserve(output_tensor_desc.size());
  for (uint64_t output_id : split->output_operand_ids) {
    output_tensor_desc.push_back(
        CreateOutputTensorDesc(id_to_operand_map, output_id));
    output_tensor_desc_dml.push_back(
        output_tensor_desc.back().GetDMLTensorDesc());
  }

  auto output_count =
      base::checked_cast<uint32_t>(output_tensor_desc_dml.size());
  DML_SPLIT_OPERATOR_DESC split_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputCount = output_count,
      .OutputTensors = output_tensor_desc_dml.data(),
      .Axis = split->axis};

  std::array<const NodeOutput*, 1> inputs = {input};
  const OperatorNode* split_node =
      graph_builder.CreateOperatorNode(DML_OPERATOR_SPLIT, &split_desc, inputs);

  if (!split_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create split operator."));
  }

  for (uint32_t i = 0; i < output_count; ++i) {
    uint64_t output_id = split->output_operand_ids[i];
    const auto* output = graph_builder.CreateNodeOutput(
        split_node, std::move(output_tensor_desc[i]), i);
    CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
  }

  return base::ok();
}

template <typename DML_OPERATOR_DESC, DML_OPERATOR_TYPE operator_type>
const OperatorNode* CreateUnaryOperator(const TensorDesc& input_tensor,
                                        const TensorDesc& output_tensor,
                                        const NodeOutput* input,
                                        GraphBuilder& graph_builder) {
  DML_OPERATOR_DESC unary_operator_desc{
      .InputTensor = &input_tensor.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor.GetDMLTensorDesc()};
  std::array<const NodeOutput*, 1> inputs = {input};
  return graph_builder.CreateOperatorNode(operator_type, &unary_operator_desc,
                                          inputs);
}

template <typename OperatorDesc,
          DML_OPERATOR_TYPE operator_type,
          typename Operation>
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForUnary(
    const IdToOperandMap& id_to_operand_map,
    const Operation& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, operation->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  uint64_t output_id = operation->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  const OperatorNode* unary_node =
      CreateUnaryOperator<OperatorDesc, operator_type>(
          input_tensor_desc, output_tensor_desc, input, graph_builder);
  if (!unary_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create unary operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      unary_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForNeg(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ElementWiseUnaryPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, operation->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  const uint64_t output_id = operation->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  // Set the values of scale and bias terms supplied to identity operator. Scale
  // and bias have the effect of applying the function g(x) = x * Scale + Bias.
  // When we set Scale to -1 and Bias to 0, we can simulate identity as negate
  // operator.
  DML_SCALE_BIAS scale_bias{.Scale = -1.f, .Bias = 0.f};
  DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC identity_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .ScaleBias = &scale_bias};

  std::array<const NodeOutput*, 1> inputs = {input};
  const OperatorNode* identity_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_IDENTITY, &identity_operator_desc, inputs);
  if (!identity_node) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Failed to create identity operator to implement "
                          "WebNN neg operation."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      identity_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForElementWiseUnary(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ElementWiseUnaryPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  switch (operation->kind) {
    case mojom::ElementWiseUnary::Kind::kAbs: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_ABS_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_ABS>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kCeil: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_CEIL_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_CEIL>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kCos: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_COS_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_COS>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kExp: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_EXP_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_EXP>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kFloor: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_FLOOR_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_FLOOR>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kLog: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_LOG_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_LOG>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    // TODO(crbug.com/1502731): Implement the negate operator directly by
    // DML_ELEMENT_WISE_NEGATE_OPERATOR_DESC which is available in
    // DML_FEATURE_LEVEL_5_0.
    // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_element_wise_negate_operator_desc#availability
    case mojom::ElementWiseUnary::Kind::kNeg: {
      return CreateOperatorNodeForNeg(id_to_operand_map, operation,
                                      graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kSin: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_SIN_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_SIN>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kTan: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_TAN_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_TAN>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kLogicalNot: {
      return CreateOperatorNodeForUnary<
          DML_ELEMENT_WISE_LOGICAL_NOT_OPERATOR_DESC,
          DML_OPERATOR_ELEMENT_WISE_LOGICAL_NOT>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kIdentity: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_IDENTITY>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kSqrt: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_SQRT_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_SQRT>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kErf: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_ERF_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_ERF>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kReciprocal: {
      return CreateOperatorNodeForUnary<DML_ELEMENT_WISE_RECIP_OPERATOR_DESC,
                                        DML_OPERATOR_ELEMENT_WISE_RECIP>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
    case mojom::ElementWiseUnary::Kind::kCast: {
      return CreateOperatorNodeForUnary<DML_CAST_OPERATOR_DESC,
                                        DML_OPERATOR_CAST>(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
    }
  }
  NOTREACHED_NORETURN();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForResample2d(
    const IdToOperandMap& id_to_operand_map,
    const mojom::Resample2dPtr& resample2d,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, resample2d->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  uint64_t output_id = resample2d->output_operand_id;
  const auto& output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  const auto& input_dimensions = input_tensor_desc.GetDimensions();
  const auto& output_dimensions = output_tensor_desc.GetDimensions();
  size_t input_rank = input_dimensions.size();
  CHECK_EQ(input_rank, output_dimensions.size());

  // Use explicit scales if given, otherwise, compute scales from output
  // dimensions / input dimensions. Then expand scales to full scales (same size
  // as input rank using axes).
  std::vector<float> full_scales(input_rank, 1);
  const auto& scales = resample2d->scales;
  const auto& axes = resample2d->axes;
  if (scales) {
    for (size_t i = 0; i < axes.size(); ++i) {
      auto axis = axes[i];
      CHECK_LT(axis, full_scales.size());
      full_scales[axis] = scales.value()[i];
    }
  } else {
    for (size_t i = 0; i < input_rank; ++i) {
      full_scales[i] =
          base::checked_cast<float>(output_dimensions[i]) / input_dimensions[i];
    }
  }

  DML_INTERPOLATION_MODE mode;
  switch (resample2d->mode) {
    case mojom::Resample2d::InterpolationMode::kNearestNeighbor:
      mode = DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
      break;
    case mojom::Resample2d::InterpolationMode::kLinear:
      mode = DML_INTERPOLATION_MODE_LINEAR;
      break;
  }

  DML_RESAMPLE_OPERATOR_DESC resample2d_operator_desc = {
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .InterpolationMode = mode,
      .ScaleCount = static_cast<uint32_t>(full_scales.size()),
      .Scales = full_scales.data()};

  std::array<const NodeOutput*, 1> inputs = {input};
  const OperatorNode* resample2d_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_RESAMPLE, &resample2d_operator_desc, inputs);
  if (!resample2d_node) {
    return base::unexpected(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to create resample2d operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      resample2d_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForReduce(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ReducePtr& reduce,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, reduce->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();
  uint64_t output_id = reduce->output_operand_id;
  const auto& output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);
  const auto& axes = reduce->axes;
  // Determine output sizes. Ignore output_desc->dimensions for the dimensions,
  // since DirectML expects the output dimensions to have the same rank as the
  // input, and output_desc->dimensions may have removed dimensions if
  // keepDimensions was false.
  std::vector<uint32_t> output_dimensions = input_tensor_desc.GetDimensions();
  for (uint32_t axis : axes) {
    CHECK_LT(axis, output_dimensions.size());
    output_dimensions[axis] = 1u;
  }
  TensorDesc new_output_tensor_desc(output_tensor_desc.GetDataType(),
                                    output_dimensions);

  std::array<const NodeOutput*, 1> inputs = {input};
  DML_REDUCE_OPERATOR_DESC operator_desc = {};
  operator_desc.Function = MapReduceKindToReduceFuntion(reduce->kind);
  operator_desc.InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
  operator_desc.OutputTensor = &new_output_tensor_desc.GetDMLTensorDesc(),
  operator_desc.AxisCount = static_cast<uint32_t>(axes.size());
  operator_desc.Axes = axes.data();
  const OperatorNode* reduce_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_REDUCE, &operator_desc, inputs);
  if (!reduce_node) {
    std::string error_message =
        "Failed to create " + ReduceOpKindToString(reduce->kind) + " operator.";
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        std::move(error_message)));
  }

  const NodeOutput* output =
      graph_builder.CreateNodeOutput(reduce_node, output_tensor_desc);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

// DirectML API does not have a real Reshape operator. The WebNN Reshape is
// implemented by creating a new NodeOutput for the input Node. The new
// NodeOutput has the reshaped dimensions and is used as the output of the WebNN
// Reshape operator. And if the input and output of the Reshape are exactly the
// input and output of the DirectML graph, we need to add another DirectML
// Identity operator to ensure that the DirectML graph can be compiled and
// calculated correctly.
void CreateNodeOutputForReshape(const IdToOperandMap& id_to_operand_map,
                                const mojom::ReshapePtr& reshape,
                                GraphBuilder& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, reshape->input_operand_id);
  uint64_t output_id = reshape->output_operand_id;
  auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  const Node& input_node = input->GetNode();

  // The output_index of this NodeOutput should be the same as the input
  // NodeOutput for creating correct intermediate edges of the graph.
  const NodeOutput* output = graph_builder.CreateNodeOutput(
      &input_node, std::move(output_tensor_desc), input->GetOutputIndex());

  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForElu(
    const IdToOperandMap& id_to_operand_map,
    const mojom::EluPtr& elu,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, elu->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  uint64_t output_id = elu->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  DML_ACTIVATION_ELU_OPERATOR_DESC elu_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Alpha = elu->alpha};

  std::array<const NodeOutput*, 1> inputs = {input};
  const OperatorNode* elu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_ELU, &elu_desc, inputs);
  if (!elu_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create elu operator."));
  }

  const NodeOutput* node_output =
      graph_builder.CreateNodeOutput(elu_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForExpand(
    const IdToOperandMap& id_to_operand_map,
    const mojom::ExpandPtr& expand,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, expand->input_operand_id);
  auto input_tensor_desc = input->GetTensorDesc();

  const uint64_t output_id = expand->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  // Use identity to implement the expand operation with broadcasting strides
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-strides#broadcasting-with-strides.
  const auto& output_dimensions = output_tensor_desc.GetDimensions();
  if (input_tensor_desc.GetDimensions() != output_dimensions) {
    input_tensor_desc.BroadcastTo(output_dimensions);
  }
  const OperatorNode* identity_node =
      CreateUnaryOperator<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC,
                          DML_OPERATOR_ELEMENT_WISE_IDENTITY>(
          input_tensor_desc, output_tensor_desc, input, graph_builder);
  if (!identity_node) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Failed to create identity dml operator to implement "
                          "expand operation."));
  }

  const NodeOutput* node_output = graph_builder.CreateNodeOutput(
      identity_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForGather(
    const IdToOperandMap& id_to_operand_map,
    const mojom::GatherPtr& gather,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input =
      GetNodeOutputForOperand(id_to_node_output_map, gather->input_operand_id);
  auto input_tensor_desc = input->GetTensorDesc();

  const NodeOutput* indices = GetNodeOutputForOperand(
      id_to_node_output_map, gather->indices_operand_id);
  auto indices_tensor_desc = indices->GetTensorDesc();
  size_t indices_rank = indices_tensor_desc.GetDimensions().size();
  if (base::MakeStrictNum(indices_rank) >
      std::numeric_limits<uint32_t>::max()) {
    return base::unexpected(
        CreateError(mojom::Error::Code::kUnknownError,
                    "The indices rank of gather operator is too large."));
  }

  uint64_t output_id = gather->output_operand_id;
  const auto original_output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);
  auto output_tensor_desc = original_output_tensor_desc;

  size_t input_rank = input_tensor_desc.GetDimensions().size();
  size_t output_rank = output_tensor_desc.GetDimensions().size();
  size_t expanded_rank = std::max(input_rank, output_rank);
  CHECK_GE(expanded_rank, indices_rank);

  // Expand all tensor ranks to expanded_rank, which DML_GATHER_OPERATOR_DESC
  // validation requires.
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_gather_operator_desc
  input_tensor_desc.EnsureMinimumRank(expanded_rank,
                                      TensorDesc::Alignment::kTrailing);
  indices_tensor_desc.EnsureMinimumRank(expanded_rank,
                                        TensorDesc::Alignment::kTrailing);
  output_tensor_desc.EnsureMinimumRank(expanded_rank,
                                       TensorDesc::Alignment::kTrailing);

  auto expanded_axis =
      base::MakeCheckedNum<size_t>(expanded_rank - input_rank) +
      base::checked_cast<size_t>(gather->axis);
  if (!expanded_axis.IsValid<uint32_t>()) {
    return base::unexpected(
        CreateError(mojom::Error::Code::kUnknownError,
                    "The axis of gather operator is too large."));
  }

  // TODO(crbug.com/1273291): Include a DirectML documentation link and a
  // Chromium test that validates the out-of-bounds indices handling.
  //
  // DirectML implementation for gather operator has already handled the
  // indices tensor by clamping it in the shader to prevent out-of-bounds
  // access.
  DML_GATHER_OPERATOR_DESC gather_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .IndicesTensor = &indices_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      // The axis dimension of InputTensor to gather on.
      .Axis = expanded_axis.ValueOrDie<uint32_t>(),
      // The number of actual index dimensions within the IndicesTensor.
      .IndexDimensions = base::checked_cast<uint32_t>(indices_rank)};

  std::array<const NodeOutput*, 2> inputs = {input, indices};
  const OperatorNode* gather_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GATHER, &gather_operator_desc, inputs);
  if (!gather_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create gather operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      gather_node, std::move(original_output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

// Creates a DirectML operator for the WebNN general matrix multiplication
// (GEMM) of the expression alpha * A * B + beta * C.
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForGemm(
    const IdToOperandMap& id_to_operand_map,
    const mojom::GemmPtr& gemm,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input_a_node_output =
      GetNodeOutputForOperand(id_to_node_output_map, gemm->a_operand_id);
  const auto& input_a_tensor_desc = input_a_node_output->GetTensorDesc();

  const NodeOutput* input_b_node_output =
      GetNodeOutputForOperand(id_to_node_output_map, gemm->b_operand_id);
  const auto& input_b_tensor_desc = input_b_node_output->GetTensorDesc();

  std::vector<const NodeOutput*> inputs{input_a_node_output,
                                        input_b_node_output};

  uint64_t output_id = gemm->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  // The input c tensor description may be broadcasted.
  absl::optional<TensorDesc> input_c_tensor_desc;
  auto& c_operand_id = gemm->c_operand_id;
  if (c_operand_id) {
    uint64_t input_c_id = c_operand_id.value();

    const auto input_c_node_output_iterator =
        id_to_node_output_map.find(input_c_id);
    CHECK(input_c_node_output_iterator != id_to_node_output_map.end());

    const NodeOutput* input_c_node_output =
        input_c_node_output_iterator->second;
    CHECK(input_c_node_output);
    input_c_tensor_desc = input_c_node_output->GetTensorDesc();

    // Ensure the graph edge for c operand will be created.
    inputs.push_back(input_c_node_output);

    auto output_dimensions = output_tensor_desc.GetDimensions();
    if (input_c_tensor_desc->GetDimensions() != output_dimensions) {
      input_c_tensor_desc->BroadcastTo(output_dimensions);
    }
  }

  DML_GEMM_OPERATOR_DESC gemm_operator_desc{
      .ATensor = &input_a_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
      .CTensor = input_c_tensor_desc.has_value()
                     ? &input_c_tensor_desc->GetDMLTensorDesc()
                     : nullptr,
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .TransA = (gemm->a_transpose) ? DML_MATRIX_TRANSFORM_TRANSPOSE
                                    : DML_MATRIX_TRANSFORM_NONE,
      .TransB = (gemm->b_transpose) ? DML_MATRIX_TRANSFORM_TRANSPOSE
                                    : DML_MATRIX_TRANSFORM_NONE,
      .Alpha = gemm->alpha,
      .Beta = gemm->beta,
      .FusedActivation = nullptr,  // Not supported
  };

  const OperatorNode* gemm_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GEMM, &gemm_operator_desc, inputs);
  if (!gemm_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create gemm operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      gemm_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForLeakyRelu(
    const IdToOperandMap& id_to_operand_map,
    const mojom::LeakyReluPtr& leaky_relu,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, leaky_relu->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  uint64_t output_id = leaky_relu->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);

  DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC leaky_relu_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Alpha = leaky_relu->alpha};

  std::array<const NodeOutput*, 1> inputs = {input};
  const OperatorNode* leaky_relu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_LEAKY_RELU, &leaky_relu_desc, inputs);
  if (!leaky_relu_node) {
    return base::unexpected(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to create leakyRelu operator."));
  }

  const NodeOutput* node_output = graph_builder.CreateNodeOutput(
      leaky_relu_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, node_output).second);

  return base::ok();
}

// Using DML_GEMM_OPERATOR_DESC to implement WebNN matmul.
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForMatmul(
    const IdToOperandMap& id_to_operand_map,
    const mojom::MatmulPtr& matmul,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input_a_node_output =
      GetNodeOutputForOperand(id_to_node_output_map, matmul->a_operand_id);
  auto input_a_tensor_desc = input_a_node_output->GetTensorDesc();
  const NodeOutput* input_b_node_output =
      GetNodeOutputForOperand(id_to_node_output_map, matmul->b_operand_id);
  auto input_b_tensor_desc = input_b_node_output->GetTensorDesc();

  uint64_t output_id = matmul->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);
  const auto output_tensor_dims = output_tensor_desc.GetDimensions();
  // Because DML_GEMM_OPERATOR_DESC restricts input_a_tensor and input_b_tensor,
  // output_tensor must have the same DimensionCount and can't support
  // broadcasting, input_a_tensor and input_b_tensor may need to be broadcasted.
  if (output_tensor_dims.size() > 2) {
    input_a_tensor_desc.BroadcastTo(output_tensor_dims, 2);
    input_b_tensor_desc.BroadcastTo(output_tensor_dims, 2);
  }

  CHECK_EQ(input_a_tensor_desc.GetDimensions().size(),
           input_b_tensor_desc.GetDimensions().size());
  CHECK_EQ(input_a_tensor_desc.GetDimensions().size(),
           output_tensor_dims.size());

  DML_GEMM_OPERATOR_DESC matmul_operator_desc{
      .ATensor = &input_a_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
      .CTensor = nullptr,
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .TransA = DML_MATRIX_TRANSFORM_NONE,
      .TransB = DML_MATRIX_TRANSFORM_NONE,
      .Alpha = 1.0f,
      .Beta = 0.0f,
      .FusedActivation = nullptr,
  };

  std::array<const NodeOutput*, 2> inputs{input_a_node_output,
                                          input_b_node_output};
  const OperatorNode* matmul_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GEMM, &matmul_operator_desc, inputs);
  if (!matmul_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create matmul operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      matmul_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

// Transpose is not a real DirectML operator. As for implementation, the input
// tensor is remapped for reading elements following the strides after the
// permutation, and an identity operator is appended to consume the remapped
// strides.
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForTranspose(
    const IdToOperandMap& id_to_operand_map,
    const mojom::TransposePtr& transpose,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* input = GetNodeOutputForOperand(
      id_to_node_output_map, transpose->input_operand_id);
  const auto& input_tensor_desc = input->GetTensorDesc();

  uint64_t output_id = transpose->output_operand_id;
  auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);
  CHECK_EQ(input_tensor_desc.GetDimensions().size(),
           output_tensor_desc.GetDimensions().size());

  TensorDesc remapped_input_tensor_desc = input_tensor_desc;
  remapped_input_tensor_desc.Transpose(transpose->permutation);

  // Append an identity node to consume the strides.
  const OperatorNode* identity_node =
      CreateUnaryOperator<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC,
                          DML_OPERATOR_ELEMENT_WISE_IDENTITY>(
          remapped_input_tensor_desc, output_tensor_desc, input, graph_builder);
  if (!identity_node) {
    return base::unexpected(CreateError(mojom::Error::Code::kUnknownError,
                                        "Failed to create identity operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      identity_node, std::move(output_tensor_desc));
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForWhere(
    const IdToOperandMap& id_to_operand_map,
    const mojom::WherePtr& where,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const NodeOutput* condition = GetNodeOutputForOperand(
      id_to_node_output_map, where->condition_operand_id);
  auto condition_tensor_desc = condition->GetTensorDesc();

  const NodeOutput* true_value = GetNodeOutputForOperand(
      id_to_node_output_map, where->true_value_operand_id);
  auto true_value_tensor_desc = true_value->GetTensorDesc();

  const NodeOutput* false_value = GetNodeOutputForOperand(
      id_to_node_output_map, where->false_value_operand_id);
  auto false_value_tensor_desc = false_value->GetTensorDesc();

  uint64_t output_id = where->output_operand_id;
  const auto output_tensor_desc =
      CreateOutputTensorDesc(id_to_operand_map, output_id);
  const auto output_tensor_dims = output_tensor_desc.GetDimensions();

  // Broadcast each of the inputs to the output.
  if (condition_tensor_desc.GetDimensions() != output_tensor_dims) {
    condition_tensor_desc.BroadcastTo(output_tensor_dims);
  }
  if (true_value_tensor_desc.GetDimensions() != output_tensor_dims) {
    true_value_tensor_desc.BroadcastTo(output_tensor_dims);
  }
  if (false_value_tensor_desc.GetDimensions() != output_tensor_dims) {
    false_value_tensor_desc.BroadcastTo(output_tensor_dims);
  }

  DML_ELEMENT_WISE_IF_OPERATOR_DESC where_operator_desc{
      .ConditionTensor = &condition_tensor_desc.GetDMLTensorDesc(),
      .ATensor = &true_value_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &false_value_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};

  std::array<const NodeOutput*, 3> inputs{condition, true_value, false_value};
  const OperatorNode* where_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_IF, &where_operator_desc, inputs);
  if (!where_node) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Failed to create where operator."));
  }

  const NodeOutput* output = graph_builder.CreateNodeOutput(
      where_node, std::move(output_tensor_desc), 0);
  // The output id must be unique in the map.
  CHECK(id_to_node_output_map.try_emplace(output_id, output).second);

  return base::ok();
}

}  // namespace

GraphImpl::GraphBufferBindingInfo::GraphBufferBindingInfo() = default;
GraphImpl::GraphBufferBindingInfo::~GraphBufferBindingInfo() = default;

GraphImpl::GraphBufferBindingInfo::GraphBufferBindingInfo(
    GraphBufferBindingInfo&&) = default;
GraphImpl::GraphBufferBindingInfo& GraphImpl::GraphBufferBindingInfo::operator=(
    GraphBufferBindingInfo&&) = default;

GraphImpl::ComputeResources::ComputeResources(
    ComPtr<ID3D12DescriptorHeap> descriptor_heap,
    AlignedByteLength<std::string> input_aligned_byte_length,
    ComPtr<ID3D12Resource> upload_buffer,
    ComPtr<ID3D12Resource> input_buffer,
    AlignedByteLength<std::string> output_aligned_byte_length,
    ComPtr<ID3D12Resource> output_buffer,
    ComPtr<ID3D12Resource> readback_buffer,
    uint64_t temporary_buffer_byte_length,
    ComPtr<ID3D12Resource> temporary_resource)
    : descriptor_heap(std::move(descriptor_heap)),
      input_aligned_byte_length(std::move(input_aligned_byte_length)),
      upload_buffer(std::move(upload_buffer)),
      input_buffer(std::move(input_buffer)),
      output_aligned_byte_length(std::move(output_aligned_byte_length)),
      output_buffer(std::move(output_buffer)),
      readback_buffer(std::move(readback_buffer)),
      temporary_buffer(std::move(temporary_resource)) {
  if (temporary_buffer_byte_length > 0) {
    CHECK_NE(temporary_buffer.Get(), nullptr);
    temporary_buffer_binding =
        DML_BUFFER_BINDING{.Buffer = temporary_buffer.Get(),
                           .Offset = 0,
                           .SizeInBytes = temporary_buffer_byte_length};
    temporary_buffer_binding_desc =
        DML_BINDING_DESC{.Type = DML_BINDING_TYPE_BUFFER,
                         .Desc = &temporary_buffer_binding.value()};
  }
}

GraphImpl::ComputeResources::~ComputeResources() = default;

// static
std::unique_ptr<GraphImpl::ComputeResources>
GraphImpl::AllocateComputeResources(
    CommandRecorder* command_recorder,
    IDMLCompiledOperator* compiled_operator,
    const ComputeResourceInfo& compute_resource_info) {
  TRACE_EVENT0("gpu", "GraphImpl::AllocateComputeResources");

  // Create the descriptor heap.
  DML_BINDING_PROPERTIES execution_binding_properties =
      compiled_operator->GetBindingProperties();
  ComPtr<ID3D12DescriptorHeap> descriptor_heap;
  RETURN_NULL_IF_FAILED(command_recorder->CreateDescriptorHeap(
      execution_binding_properties.RequiredDescriptorCount,
      L"WebNN_Descriptor_Heap_For_Execution", descriptor_heap));

  // Calculate the total byte length of input array buffers to create
  // GPU input buffer and upload buffer, also records the aligned D3D12_RANGE
  // for each input.
  absl::optional<AlignedByteLength<std::string>> aligned_byte_length_of_inputs =
      CalculateAlignedByteLength(
          compute_resource_info.input_name_to_byte_length_map);
  if (!aligned_byte_length_of_inputs) {
    DLOG(ERROR) << "Failed to calculate the aligned byte length of inputs.";
    return nullptr;
  }

  // Create the upload heap that can be written by CPU and read from GPU,
  // and create a resource to map the heap.
  size_t total_byte_length_of_inputs =
      aligned_byte_length_of_inputs.value().total_byte_length;
  ComPtr<ID3D12Resource> upload_buffer;
  RETURN_NULL_IF_FAILED(command_recorder->CreateUploadBuffer(
      total_byte_length_of_inputs, L"WebNN_Upload_Buffer_Inputs",
      upload_buffer));

  // Create the default heap that only can be accessed by GPU not provide CPU
  // access, and create a resource to map the heap.
  ComPtr<ID3D12Resource> input_buffer;
  RETURN_NULL_IF_FAILED(command_recorder->CreateDefaultBuffer(
      total_byte_length_of_inputs, L"WebNN_Default_Buffer_Inputs",
      input_buffer));

  // Calculate the total byte length of outputs array buffer to create
  // an output buffer and readback buffer, also records the aligned D3D12_RANGE
  // for each output.
  absl::optional<AlignedByteLength<std::string>>
      aligned_byte_length_of_outputs = CalculateAlignedByteLength(
          compute_resource_info.output_name_to_byte_length_map);
  if (!aligned_byte_length_of_outputs) {
    DLOG(ERROR) << "Failed to calculate the aligned byte length of outputs.";
    return nullptr;
  }

  // Create the output buffer which will be bound for the graph execution.
  size_t total_byte_length_of_outputs =
      aligned_byte_length_of_outputs.value().total_byte_length;
  ComPtr<ID3D12Resource> output_buffer;
  RETURN_NULL_IF_FAILED(command_recorder->CreateDefaultBuffer(
      total_byte_length_of_outputs, L"WebNN_Default_Buffer_Outputs",
      output_buffer));

  // Create the readback buffer which will be read by CPU.
  ComPtr<ID3D12Resource> readback_buffer;
  RETURN_NULL_IF_FAILED(command_recorder->CreateReadbackBuffer(
      total_byte_length_of_outputs, L"WebNN_ReadBack_Buffer_Outputs",
      readback_buffer));

  // Create and bind the temporary resource if the operator execution requires.
  ComPtr<ID3D12Resource> temporary_buffer;
  uint64_t temporary_buffer_byte_length =
      execution_binding_properties.TemporaryResourceSize;
  if (temporary_buffer_byte_length > 0) {
    RETURN_NULL_IF_FAILED(command_recorder->CreateDefaultBuffer(
        temporary_buffer_byte_length, L"WebNN_Temporary_Buffer_For_Execution",
        temporary_buffer));
  }

  return base::WrapUnique(new ComputeResources(
      std::move(descriptor_heap),
      std::move(aligned_byte_length_of_inputs.value()),
      std::move(upload_buffer), std::move(input_buffer),
      std::move(aligned_byte_length_of_outputs.value()),
      std::move(output_buffer), std::move(readback_buffer),
      temporary_buffer_byte_length, std::move(temporary_buffer)));
}

GraphImpl::GraphImpl(std::unique_ptr<CommandRecorder> command_recorder,
                     ComPtr<ID3D12Resource> persistent_buffer,
                     ComPtr<IDMLCompiledOperator> compiled_operator,
                     ComputeResourceInfo compute_resource_info,
                     GraphBufferBindingInfo graph_buffer_binding_info,
                     std::unique_ptr<ComputeResources> compute_resources)
    : WebNNGraphImpl(std::move(compute_resource_info)),
      persistent_buffer_(std::move(persistent_buffer)),
      command_recorder_(std::move(command_recorder)),
      compiled_operator_(std::move(compiled_operator)),
      graph_buffer_binding_info_(std::move(graph_buffer_binding_info)),
      compute_resources_(std::move(compute_resources)) {
  command_queue_ = command_recorder_->GetCommandQueue();
  dml_device_ = command_recorder_->GetDMLDevice();

  // Create the persistent buffer binding for the graph execution.
  uint64_t persistent_buffer_size =
      compiled_operator_->GetBindingProperties().PersistentResourceSize;
  if (persistent_buffer_size) {
    CHECK_NE(persistent_buffer_.Get(), nullptr);
    persistent_buffer_binding_ =
        DML_BUFFER_BINDING{.Buffer = persistent_buffer_.Get(),
                           .Offset = 0,
                           .SizeInBytes = persistent_buffer_size};
    persistent_buffer_binding_desc_ =
        DML_BINDING_DESC{.Type = DML_BINDING_TYPE_BUFFER,
                         .Desc = &persistent_buffer_binding_.value()};
  }
}

//  Notice that it's the CommandQueue's responsibility to wait for all of the
//  queued work to complete before destructing itself.
GraphImpl::~GraphImpl() = default;

ComPtr<IDMLCompiledOperator> GraphImpl::CompileOnBackgroundThread(
    GraphBuilder graph_builder) {
  TRACE_EVENT0("gpu", "dml::GraphImpl::CompileOnBackgroundThread");
  return graph_builder.Compile(DML_EXECUTION_FLAG_NONE);
}

// static
void GraphImpl::OnCompilationComplete(
    mojom::WebNNContext::CreateGraphCallback callback,
    std::unique_ptr<CommandRecorder> command_recorder,
    base::flat_map<uint64_t, mojo_base::BigBuffer> constant_id_to_buffer_map,
    std::unordered_map<uint64_t, uint32_t> constant_id_to_input_index_map,
    GraphBufferBindingInfo graph_buffer_binding_info,
    ComputeResourceInfo compute_resource_info,
    ComPtr<IDMLCompiledOperator> compiled_operator) {
  TRACE_EVENT0("gpu", "dml::GraphImpl::OnCompilationComplete");
  if (!compiled_operator) {
    DLOG(ERROR) << "Failed to compile the graph.";
    std::move(callback).Run(mojom::CreateGraphResult::NewError(CreateError(
        mojom::Error::Code::kUnknownError, "Failed to compile the graph.")));
    return;
  }

  HRESULT hr = command_recorder->Open();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open the command recorder: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(mojom::CreateGraphResult::NewError(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to open the command recorder.")));
    return;
  }

  // Create the input resource binding for graph initialization. The number of
  // bindings must exactly match the number of inputs (including constants) of
  // the graph, only the constant resource needs to be bound, the inputs for
  // computation supply nullptr for `Buffer` member to indicate 'no binding'.
  //
  // The constant tensor specifying DML_TENSOR_FLAG_OWNED_BY_DML need to bind
  // the resource in the buffer binding (DML_BUFFER_BINDING) array, the index
  // of constant in the array is DML_INPUT_GRAPH_EDGE_DESC.GraphInputIndex which
  // is got from `constant_id_to_input_index_map`.
  //
  // The inputs tensors without the DML_TENSOR_FLAG_OWNED_BY_DML flag is
  // expected to be bound during execution, and not during initialization.
  std::vector<DML_BUFFER_BINDING> input_buffer_binding(
      graph_buffer_binding_info.input_buffer_binding_count,
      DML_BUFFER_BINDING{.Buffer = nullptr, .Offset = 0, .SizeInBytes = 0});
  if (!constant_id_to_buffer_map.empty()) {
    std::map<uint64_t, size_t> constant_id_to_byte_length_map;
    for (auto& [key, buffer] : constant_id_to_buffer_map) {
      constant_id_to_byte_length_map[key] = buffer.size();
    }

    absl::optional<AlignedByteLength<uint64_t>>
        aligned_byte_length_of_constants =
            CalculateAlignedByteLength(constant_id_to_byte_length_map);
    if (!aligned_byte_length_of_constants) {
      DLOG(ERROR)
          << "Failed to calculate the aligned byte length of constants.";
      std::move(callback).Run(mojom::CreateGraphResult::NewError(CreateError(
          mojom::Error::Code::kUnknownError,
          "Failed to calculate the aligned byte length of constants.")));
      return;
    }

    // Create the upload heap that can be written by CPU and read from GPU,
    // and create a resource to map the heap.
    size_t total_byte_length_of_constants =
        aligned_byte_length_of_constants.value().total_byte_length;
    ComPtr<ID3D12Resource> upload_buffer;
    hr = command_recorder->CreateUploadBuffer(total_byte_length_of_constants,
                                              L"WebNN_Upload_Buffer_Constants",
                                              upload_buffer);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create upload buffer for constants: "
                  << logging::SystemErrorCodeToString(hr);
      std::move(callback).Run(mojom::CreateGraphResult::NewError(
          CreateError(mojom::Error::Code::kUnknownError,
                      "Failed to create upload buffer for constants.")));
      return;
    }
    // Create the default heap that only can be accessed by GPU not provide CPU
    // access, and create a resource to map the heap.
    ComPtr<ID3D12Resource> default_buffer;
    hr = command_recorder->CreateDefaultBuffer(
        total_byte_length_of_constants, L"WebNN_Default_Buffer_Constants",
        default_buffer);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create default input buffer for constants: "
                  << logging::SystemErrorCodeToString(hr);
      std::move(callback).Run(mojom::CreateGraphResult::NewError(
          CreateError(mojom::Error::Code::kUnknownError,
                      "Failed to create default input buffer for constants.")));
      return;
    }
    auto constant_buffer_binding = UploadAndCreateBufferBinding<uint64_t>(
        command_recorder.get(), constant_id_to_buffer_map,
        aligned_byte_length_of_constants.value(), std::move(upload_buffer),
        std::move(default_buffer));
    if (!constant_buffer_binding) {
      DLOG(ERROR) << "Failed to upload constant weight data.";
      std::move(callback).Run(mojom::CreateGraphResult::NewError(
          CreateError(mojom::Error::Code::kUnknownError,
                      "Failed to upload constant weight data.")));
      return;
    }
    // The constant tensor must be bound to the binding table during operator
    // initialization, and not during execution.
    for (auto& [constant_id, buffer_binding] :
         constant_buffer_binding.value()) {
      // Get the graph input index with the constant id.
      const auto graph_input_index_iterator =
          constant_id_to_input_index_map.find(constant_id);
      CHECK(graph_input_index_iterator != constant_id_to_input_index_map.end());
      input_buffer_binding[graph_input_index_iterator->second] =
          std::move(buffer_binding);
    }
  }
  DML_BUFFER_ARRAY_BINDING input_buffer_array_binding{
      .BindingCount = base::checked_cast<uint32_t>(input_buffer_binding.size()),
      .Bindings = input_buffer_binding.data()};
  DML_BINDING_DESC input_buffer_binding_desc = {DML_BINDING_TYPE_BUFFER_ARRAY,
                                                &input_buffer_array_binding};

  // Create the persistent resource which is bound as output of operator
  // initializer.
  absl::optional<DML_BINDING_DESC> persistent_buffer_binding_desc;
  absl::optional<DML_BUFFER_BINDING> persistent_buffer_binding;
  DML_BINDING_PROPERTIES execution_binding_properties =
      compiled_operator->GetBindingProperties();
  uint64_t persistent_buffer_size =
      execution_binding_properties.PersistentResourceSize;
  ComPtr<ID3D12Resource> persistent_buffer;
  if (persistent_buffer_size) {
    hr = command_recorder->CreateDefaultBuffer(
        persistent_buffer_size, L"WebNN_Default_Persistent_Buffer",
        persistent_buffer);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create the default buffer: "
                  << logging::SystemErrorCodeToString(hr);
      std::move(callback).Run(mojom::CreateGraphResult::NewError(
          CreateError(mojom::Error::Code::kUnknownError,
                      "Failed to create the default buffer.")));
      return;
    }

    persistent_buffer_binding =
        DML_BUFFER_BINDING{.Buffer = persistent_buffer.Get(),
                           .Offset = 0,
                           .SizeInBytes = persistent_buffer_size};

    persistent_buffer_binding_desc =
        DML_BINDING_DESC{.Type = DML_BINDING_TYPE_BUFFER,
                         .Desc = &persistent_buffer_binding.value()};
  }

  hr = command_recorder->InitializeOperator(compiled_operator.Get(),
                                            input_buffer_binding_desc,
                                            persistent_buffer_binding_desc);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to initialize the operator: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(mojom::CreateGraphResult::NewError(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to initialize the operator.")));
    return;
  }

  hr = command_recorder->CloseAndExecute();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to close and execute the command list: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(mojom::CreateGraphResult::NewError(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to close and execute the command list.")));
    return;
  }

  scoped_refptr<CommandQueue> command_queue(
      command_recorder->GetCommandQueue());

  command_queue->WaitAsync(base::BindOnce(
      &GraphImpl::OnInitializationComplete, std::move(command_recorder),
      std::move(persistent_buffer), std::move(compiled_operator),
      std::move(compute_resource_info), std::move(graph_buffer_binding_info),
      std::move(callback)));
}

// static
void GraphImpl::OnInitializationComplete(
    std::unique_ptr<CommandRecorder> command_recorder,
    ComPtr<ID3D12Resource> persistent_buffer,
    ComPtr<IDMLCompiledOperator> compiled_operator,
    ComputeResourceInfo compute_resource_info,
    GraphBufferBindingInfo graph_buffer_binding_info,
    mojom::WebNNContext::CreateGraphCallback callback,
    HRESULT hr) {
  TRACE_EVENT0("gpu", "dml::GraphImpl::OnInitializationComplete");
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to wait for the initialization to complete: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(mojom::CreateGraphResult::NewError(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to wait for the initialization to complete.")));
    return;
  }

  std::unique_ptr<ComputeResources> compute_resources =
      AllocateComputeResources(command_recorder.get(), compiled_operator.Get(),
                               compute_resource_info);
  if (!compute_resources) {
    std::move(callback).Run(mojom::CreateGraphResult::NewError(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to allocate compute resource.")));
    return;
  }

  scoped_refptr<CommandQueue> command_queue(
      command_recorder->GetCommandQueue());
  // The remote sent to the renderer.
  mojo::PendingRemote<mojom::WebNNGraph> blink_remote;
  // The receiver bound to GraphImpl.
  mojo::MakeSelfOwnedReceiver<mojom::WebNNGraph>(
      base::WrapUnique(new GraphImpl(
          std::move(command_recorder), std::move(persistent_buffer),
          std::move(compiled_operator), std::move(compute_resource_info),
          std::move(graph_buffer_binding_info), std::move(compute_resources))),
      blink_remote.InitWithNewPipeAndPassReceiver());
  command_queue->ReleaseCompletedResources();
  std::move(callback).Run(
      mojom::CreateGraphResult::NewGraphRemote(std::move(blink_remote)));
}

// static
void GraphImpl::CreateAndBuild(
    scoped_refptr<CommandQueue> command_queue,
    ComPtr<IDMLDevice> dml_device,
    const mojom::GraphInfoPtr& graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  TRACE_EVENT0("gpu", "dml::GraphImpl::CreateAndBuild");
  // `CommandRecorder` would keep reference of command queue and DML device.
  std::unique_ptr<CommandRecorder> command_recorder =
      CommandRecorder::Create(command_queue, dml_device);
  if (!command_recorder) {
    DLOG(ERROR) << "Failed to open the command recorder.";
    std::move(callback).Run(mojom::CreateGraphResult::NewError(
        CreateError(mojom::Error::Code::kUnknownError,
                    "Failed to open the command recorder.")));
    return;
  }

  GraphBuilder graph_builder(dml_device);
  IdToNodeOutputMap id_to_node_output_map;
  const IdToOperandMap& id_to_operand_map = graph_info->id_to_operand_map;
  std::unordered_map<uint64_t, uint32_t> constant_id_to_input_index_map;
  GraphBufferBindingInfo graph_buffer_binding_info;
  // Add inputs.
  for (auto& input_id : graph_info->input_operands) {
    auto graph_input_index = CreateInputNode(
        id_to_operand_map, input_id, graph_builder, id_to_node_output_map);
    const OperandPtr& operand = id_to_operand_map.at(input_id);
    CHECK(operand);
    graph_buffer_binding_info
        .graph_input_name_to_index_map[operand->name.value()] =
        graph_input_index;
  }

  // The constant operand in WebNNGraph also is treated as input node in graph
  // desc, the tensor is identified by DML_TENSOR_FLAG_OWNED_BY_DML which must
  // be bound to the binding table during the graph initialization, and not
  // during execution.
  for (auto& [constant_id, _] : graph_info->constant_id_to_buffer_map) {
    auto graph_input_index =
        CreateInputNode(id_to_operand_map, constant_id, graph_builder,
                        id_to_node_output_map, DML_TENSOR_FLAG_OWNED_BY_DML);
    constant_id_to_input_index_map[constant_id] = graph_input_index;
  }

  // Add operations.
  for (auto& operation : graph_info->operations) {
    // For operators that deal with DML API, there is a chance that operator
    // creation will fail. Use `mojom::ErrorPtr` to hold the given error
    // message.
    base::expected<void, mojom::ErrorPtr> create_operator_result;
    switch (operation->which()) {
      case Operation::Tag::kClamp: {
        create_operator_result = CreateOperatorNodeForClamp(
            id_to_operand_map, operation->get_clamp(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kConcat: {
        create_operator_result = CreateOperatorNodeForConcat(
            id_to_operand_map, operation->get_concat(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kConv2d: {
        create_operator_result = CreateOperatorNodeForConv2d(
            id_to_operand_map, operation->get_conv2d(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kElementWiseBinary: {
        create_operator_result = CreateOperatorNodeForBinary(
            id_to_operand_map, operation->get_element_wise_binary(),
            graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kElu: {
        create_operator_result =
            CreateOperatorNodeForElu(id_to_operand_map, operation->get_elu(),
                                     graph_builder, id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kElementWiseUnary: {
        create_operator_result = CreateOperatorNodeForElementWiseUnary(
            id_to_operand_map, operation->get_element_wise_unary(),
            graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kExpand: {
        create_operator_result = CreateOperatorNodeForExpand(
            id_to_operand_map, operation->get_expand(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kGather: {
        create_operator_result = CreateOperatorNodeForGather(
            id_to_operand_map, operation->get_gather(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kGemm: {
        create_operator_result =
            CreateOperatorNodeForGemm(id_to_operand_map, operation->get_gemm(),
                                      graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kLeakyRelu: {
        create_operator_result = CreateOperatorNodeForLeakyRelu(
            id_to_operand_map, operation->get_leaky_relu(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kMatmul: {
        create_operator_result = CreateOperatorNodeForMatmul(
            id_to_operand_map, operation->get_matmul(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kPad: {
        create_operator_result =
            CreateOperatorNodeForPad(id_to_operand_map, operation->get_pad(),
                                     graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kPool2d: {
        create_operator_result = CreateOperatorNodeForPool2d(
            id_to_operand_map, operation->get_pool2d(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kPrelu: {
        create_operator_result = CreateOperatorNodeForPrelu(
            id_to_operand_map, operation->get_prelu(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kReduce: {
        create_operator_result = CreateOperatorNodeForReduce(
            id_to_operand_map, operation->get_reduce(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kRelu: {
        create_operator_result =
            CreateOperatorNodeForUnary<DML_ACTIVATION_RELU_OPERATOR_DESC,
                                       DML_OPERATOR_ACTIVATION_RELU>(
                id_to_operand_map, operation->get_relu(), graph_builder,
                id_to_node_output_map);
        break;
      }
      case Operation::Tag::kResample2d: {
        create_operator_result = CreateOperatorNodeForResample2d(
            id_to_operand_map, operation->get_resample2d(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kReshape: {
        CreateNodeOutputForReshape(id_to_operand_map, operation->get_reshape(),
                                   graph_builder, id_to_node_output_map);
        break;
      }
      case Operation::Tag::kSigmoid: {
        create_operator_result =
            CreateOperatorNodeForUnary<DML_ACTIVATION_SIGMOID_OPERATOR_DESC,
                                       DML_OPERATOR_ACTIVATION_SIGMOID>(
                id_to_operand_map, operation->get_sigmoid(), graph_builder,
                id_to_node_output_map);

        break;
      }
      case Operation::Tag::kSlice: {
        create_operator_result = CreateOperatorNodeForSlice(
            id_to_operand_map, operation->get_slice(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kSoftmax: {
        create_operator_result =
            CreateOperatorNodeForUnary<DML_ACTIVATION_SOFTMAX_OPERATOR_DESC,
                                       DML_OPERATOR_ACTIVATION_SOFTMAX>(
                id_to_operand_map, operation->get_softmax(), graph_builder,
                id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kSplit: {
        create_operator_result = CreateOperatorNodeForSplit(
            id_to_operand_map, operation->get_split(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kTanh: {
        create_operator_result =
            CreateOperatorNodeForUnary<DML_ACTIVATION_TANH_OPERATOR_DESC,
                                       DML_OPERATOR_ACTIVATION_TANH>(
                id_to_operand_map, operation->get_tanh(), graph_builder,
                id_to_node_output_map);
        break;
      }
      case Operation::Tag::kTranspose: {
        create_operator_result = CreateOperatorNodeForTranspose(
            id_to_operand_map, operation->get_transpose(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case Operation::Tag::kWhere: {
        create_operator_result = CreateOperatorNodeForWhere(
            id_to_operand_map, operation->get_where(), graph_builder,
            id_to_node_output_map);
        break;
      }
      default: {
        std::string error_message = "This operator (" +
                                    OpTagToString(operation->which()) +
                                    ") is not supported.";
        DLOG(ERROR) << error_message;
        create_operator_result = base::unexpected(CreateError(
            mojom::Error::Code::kNotSupportedError, std::move(error_message)));
      }
    }
    if (!create_operator_result.has_value()) {
      std::move(callback).Run(mojom::CreateGraphResult::NewError(
          std::move(create_operator_result.error())));
      return;
    }
  }

  for (auto& output_id : graph_info->output_operands) {
    const auto output_iterator = id_to_node_output_map.find(output_id);
    CHECK(output_iterator != id_to_node_output_map.end());
    const NodeOutput* output = output_iterator->second;
    CHECK(output);
    // TODO: A DML graph's output tensor may have adjusted strides rather than
    // default strides which are calculated by its' dimensions. For example,
    // dimensions [1,2,3,4] should have default strides [24,12,4,1] according to
    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/dml-helper-functions#calculatestrides,
    // but the strides may be adjusted for supporting some ops such as
    // transpose. Append an identity operator to consume the adjusted strides to
    // ensure a correct output result.

    // Appending an identity operator DML_OPERATOR_ELEMENT_WISE_IDENTITY which
    // effectively copies input tensor to the output tensor to avoid directly
    // using graph input as output.
    const TensorDesc& output_tensor_desc = output->GetTensorDesc();
    auto output_type = output->GetNode().GetType();
    if (output_type == Node::Type::kInput) {
      TensorDesc identity_tensor_desc(output_tensor_desc.GetDataType(),
                                      DML_TENSOR_FLAG_NONE,
                                      output_tensor_desc.GetDimensions());
      const OperatorNode* identity_node =
          CreateUnaryOperator<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC,
                              DML_OPERATOR_ELEMENT_WISE_IDENTITY>(
              output_tensor_desc, identity_tensor_desc, output, graph_builder);
      if (!identity_node) {
        std::move(callback).Run(mojom::CreateGraphResult::NewError(
            CreateError(mojom::Error::Code::kUnknownError,
                        "Failed to create identity operator.")));
        return;
      }

      output = graph_builder.CreateNodeOutput(identity_node,
                                              std::move(identity_tensor_desc));
    }

    std::string name = id_to_operand_map.at(output_id)->name.value();
    graph_buffer_binding_info.graph_output_name_to_index_map[std::move(name)] =
        graph_builder.CreateOutputEdge(output);
  }

  graph_buffer_binding_info.input_buffer_binding_count =
      constant_id_to_input_index_map.size() +
      graph_buffer_binding_info.graph_input_name_to_index_map.size();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GraphImpl::CompileOnBackgroundThread,
                     std::move(graph_builder)),
      base::BindOnce(&GraphImpl::OnCompilationComplete, std::move(callback),
                     std::move(command_recorder),
                     std::move(graph_info->constant_id_to_buffer_map),
                     std::move(constant_id_to_input_index_map),
                     std::move(graph_buffer_binding_info),
                     ComputeResourceInfo(graph_info)));
}

void GraphImpl::HandleComputationFailure(
    mojom::WebNNGraph::ComputeCallback callback) {
  command_recorder_.reset();
  std::move(callback).Run(ComputeResult::kUnknownError, absl::nullopt);
}

void GraphImpl::HandleComputationFailure(
    const char* error,
    mojom::WebNNGraph::ComputeCallback callback) {
  DLOG(ERROR) << error;
  HandleComputationFailure(std::move(callback));
}

void GraphImpl::HandleComputationFailure(
    const char* error,
    HRESULT hr,
    mojom::WebNNGraph::ComputeCallback callback) {
  DLOG(ERROR) << error << " " << logging::SystemErrorCodeToString(hr);
  HandleComputationFailure(std::move(callback));
}

void GraphImpl::ComputeImpl(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  TRACE_EVENT0("gpu", "dml::GraphImpl::ComputeImpl");
  // Recreate the command recorder if it has been released by last failed
  // computation.
  if (!command_recorder_) {
    command_recorder_ = CommandRecorder::Create(command_queue_, dml_device_);
    if (!command_recorder_) {
      HandleComputationFailure("Failed to create the command recorder.",
                               std::move(callback));
      return;
    }
  }
  // Re-open the command recorder for recording the graph execution commands.
  HRESULT hr = command_recorder_->Open();
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to open the command recorder.", hr,
                             std::move(callback));
    return;
  }

  // Use the existing compute resource if it is available, otherwise allocate a
  // new one.
  std::unique_ptr<ComputeResources> compute_resources =
      std::move(compute_resources_);
  if (!compute_resources) {
    compute_resources = AllocateComputeResources(command_recorder_.get(),
                                                 compiled_operator_.Get(),
                                                 compute_resource_info());
    if (!compute_resources) {
      HandleComputationFailure("Failed to allocate compute resource.",
                               std::move(callback));
      return;
    }
  }
  CHECK(compute_resources);

  // Create the input resource binding for graph execution.
  auto input_buffer_binding = UploadAndCreateBufferBinding<std::string>(
      command_recorder_.get(), named_inputs,
      compute_resources->input_aligned_byte_length,
      compute_resources->upload_buffer, compute_resources->input_buffer);
  if (!input_buffer_binding) {
    HandleComputationFailure(
        "Failed to upload and create the input buffer binding.",
        std::move(callback));
    return;
  }

  std::vector<DML_BINDING_DESC> input_buffer_binding_desc(
      graph_buffer_binding_info_.input_buffer_binding_count,
      DML_BINDING_DESC{.Type = DML_BINDING_TYPE_NONE, .Desc = nullptr});

  // The graph input tensors must be bound to the binding table during the graph
  // execution.
  for (auto& [name, buffer_binding] : input_buffer_binding.value()) {
    // Get the graph input index with the name.
    const auto graph_input_index_iterator =
        graph_buffer_binding_info_.graph_input_name_to_index_map.find(name);
    CHECK(graph_input_index_iterator !=
          graph_buffer_binding_info_.graph_input_name_to_index_map.end());
    uint32_t graph_input_index = graph_input_index_iterator->second;
    input_buffer_binding_desc[graph_input_index] = {DML_BINDING_TYPE_BUFFER,
                                                    &buffer_binding};
  }

  // Create the output buffer bindings for the graph execution.
  size_t output_buffer_binding_count =
      graph_buffer_binding_info_.graph_output_name_to_index_map.size();
  std::vector<DML_BINDING_DESC> output_buffer_binding_desc(
      output_buffer_binding_count,
      DML_BINDING_DESC{.Type = DML_BINDING_TYPE_NONE, .Desc = nullptr});
  std::vector<DML_BUFFER_BINDING> output_buffer_binding;
  output_buffer_binding.reserve(output_buffer_binding_count);

  for (auto& [name, graph_output_index] :
       graph_buffer_binding_info_.graph_output_name_to_index_map) {
    auto& d3d12_range = compute_resources->output_aligned_byte_length
                            .key_to_d3d12_range_map[name];
    output_buffer_binding.push_back(
        DML_BUFFER_BINDING{.Buffer = compute_resources->output_buffer.Get(),
                           .Offset = d3d12_range.Begin,
                           .SizeInBytes = d3d12_range.End - d3d12_range.Begin});
    output_buffer_binding_desc[graph_output_index] = {
        DML_BINDING_TYPE_BUFFER, &output_buffer_binding.back()};
  }

  // Execute the graph with input, output and persistent buffer bindings.
  hr = command_recorder_->ExecuteOperator(
      compiled_operator_.Get(), compute_resources->descriptor_heap,
      input_buffer_binding_desc, output_buffer_binding_desc,
      persistent_buffer_binding_desc_,
      compute_resources->temporary_buffer_binding_desc);
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to execute the operator.", hr,
                             std::move(callback));
    return;
  }

  // Copy the output data from output buffer to readback buffer.
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(compute_resources->output_buffer.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder_->ResourceBarrier(barriers);
  command_recorder_->CopyBufferRegion(
      compute_resources->readback_buffer.Get(), 0,
      compute_resources->output_buffer.Get(), 0,
      compute_resources->output_aligned_byte_length.total_byte_length);
  barriers[0] = CreateTransitionBarrier(compute_resources->output_buffer.Get(),
                                        D3D12_RESOURCE_STATE_COPY_SOURCE,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_recorder_->ResourceBarrier(barriers);

  hr = command_recorder_->CloseAndExecute();
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to close and execute the command list.",
                             hr, std::move(callback));
    return;
  }

  command_queue_->WaitAsync(base::BindOnce(
      &GraphImpl::OnComputationComplete, weak_factory_.GetWeakPtr(),
      std::move(callback), std::move(compute_resources)));
}

void GraphImpl::OnComputationComplete(
    mojom::WebNNGraph::ComputeCallback callback,
    std::unique_ptr<ComputeResources> compute_resources,
    HRESULT hr) {
  TRACE_EVENT0("gpu", "dml::GraphImpl::OnComputationComplete");
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to wait for the computation to complete.",
                             hr, std::move(callback));
    return;
  }

  // Map entire buffer to readback the output data one by one with byte
  // offset.
  void* mapped_readback_output_buffer = nullptr;
  hr = compute_resources->readback_buffer->Map(0, nullptr,
                                               &mapped_readback_output_buffer);
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to map the readback output buffer.", hr,
                             std::move(callback));
    return;
  }

  const std::map<std::string, D3D12_RANGE>&
      graph_output_name_to_d3d12_range_map =
          compute_resources->output_aligned_byte_length.key_to_d3d12_range_map;
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  named_outputs.reserve(graph_output_name_to_d3d12_range_map.size());
  for (auto& [name, d3d12_range] : graph_output_name_to_d3d12_range_map) {
    named_outputs[name] = mojo_base::BigBuffer(base::make_span(
        static_cast<const uint8_t*>(mapped_readback_output_buffer) +
            d3d12_range.Begin,
        compute_resource_info().output_name_to_byte_length_map.at(name)));
  }

  compute_resources->readback_buffer->Unmap(0, nullptr);

  // If there is an existing free compute resource, release this compute
  // resource. Otherwise, recycle this compute resource for the next call.
  if (!compute_resources_) {
    compute_resources_ = std::move(compute_resources);
  }

  command_queue_->ReleaseCompletedResources();
  std::move(callback).Run(ComputeResult::kOk, std::move(named_outputs));
}

}  // namespace webnn::dml
