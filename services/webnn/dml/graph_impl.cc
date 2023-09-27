// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/graph_impl.h"

#include <array>

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
#include "services/webnn/dml/graph_builder.h"
#include "services/webnn/dml/tensor_desc.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/error.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {
namespace {

using Microsoft::WRL::ComPtr;
using mojom::ComputeResult;
using mojom::Operand;
using mojom::OperandPtr;
using mojom::Operator;
using mojom::OperatorPtr;

// A map of all mojom operands in `mojom::GraphInfo` using the mojom operand id
// as key.
using IdToOperandMap = base::flat_map<uint64_t, OperandPtr>;
// A map of all NodeOutputInfos using the mojom operand id as key.
using IdToNodeOutputMap = std::map<uint64_t, NodeOutputInfo>;

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
    case Operand::DataType::kInt32:
      return DML_TENSOR_DATA_TYPE_INT32;
    case Operand::DataType::kUint32:
      return DML_TENSOR_DATA_TYPE_UINT32;
    default:
      DLOG(ERROR) << "This data type is not supported.";
      NOTREACHED_NORETURN();
  }
}

std::string OpKindToString(Operator::Kind kind) {
  switch (kind) {
    case Operator::Kind::kClamp:
      return "clamp";
    case Operator::Kind::kConv2d:
      return "conv2d";
    case Operator::Kind::kAdd:
      return "add";
    case Operator::Kind::kSub:
      return "sub";
    case Operator::Kind::kMul:
      return "mul";
    case Operator::Kind::kDiv:
      return "div";
    case Operator::Kind::kMax:
      return "max";
    case Operator::Kind::kMin:
      return "min";
    case Operator::Kind::kPow:
      return "pow";
    case Operator::Kind::kGemm:
      return "gemm";
    case Operator::Kind::kRelu:
      return "relu";
    case Operator::Kind::kReshape:
      return "reshape";
    case Operator::Kind::kSoftmax:
      return "softmax";
  }
}

// Record the total byte length of buffers and the D3D12_RANGE for each
// buffer, all with the required alignment.
template <typename Key>
struct AlignedByteLength {
  size_t total_byte_length = 0;
  std::map<Key, D3D12_RANGE> key_to_d3d12_range_map;
};

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
    const base::flat_map<Key, mojo_base::BigBuffer>& key_to_buffer_map) {
  // Copy all array buffers of constants/inputs to an upload heap and create a
  // committed resource which is mapped to the heap.
  //
  // Calculate the total byte length of constants/inputs array buffer to create
  // an upload buffer which can be read by GPU.
  std::map<Key, size_t> key_to_byte_length_map;
  for (auto& [key, buffer] : key_to_buffer_map) {
    key_to_byte_length_map[key] = buffer.size();
  }

  absl::optional<AlignedByteLength<Key>> aligned_byte_length =
      CalculateAlignedByteLength(key_to_byte_length_map);
  if (!aligned_byte_length) {
    DLOG(ERROR) << "Failed to calculate the aligned byte length.";
    return absl::nullopt;
  }

  // Create the upload heap that can be written by CPU and read from GPU,
  // and create a resource to map the heap.
  size_t total_byte_length = aligned_byte_length.value().total_byte_length;
  ComPtr<ID3D12Resource> upload_buffer;
  HRESULT hr = command_recorder->CreateUploadBuffer(
      total_byte_length, L"WebNN_Upload_Buffer", upload_buffer);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create upload buffer for inputs: "
                << logging::SystemErrorCodeToString(hr);
    return absl::nullopt;
  }
  // Create the default heap that only can be accessed by GPU not provide CPU
  // access, and create a resource to map the heap.
  ComPtr<ID3D12Resource> default_buffer;
  hr = command_recorder->CreateDefaultBuffer(
      total_byte_length, L"WebNN_Default_Input_Buffer", default_buffer);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create default buffer for inputs: "
                << logging::SystemErrorCodeToString(hr);
    return absl::nullopt;
  }

  // Map entire resource to copy the array buffer of constant/input one by one
  // with byte offset.
  void* mapped_upload_buffer = nullptr;
  hr = upload_buffer->Map(0, nullptr, &mapped_upload_buffer);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to map upload buffer for inputs: "
                << logging::SystemErrorCodeToString(hr);
    return absl::nullopt;
  }

  std::map<Key, DML_BUFFER_BINDING> key_to_buffer_binding_map;
  for (auto& [key, buffer] : key_to_buffer_map) {
    // Copy the input data to the upload heap with byte offset
    auto& d3d12_range = aligned_byte_length.value().key_to_d3d12_range_map[key];
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
                          std::move(upload_buffer), total_byte_length);

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
  NodeInfo input_node = graph_builder.CreateInputNode();
  NodeOutputInfo input_node_output =
      graph_builder.CreateNodeOutput(input_node, std::move(input_tensor_desc));
  id_to_node_output_map[input_id] = std::move(input_node_output);
  return input_node.index;
}

const NodeOutputInfo& GetInputNodeOutputInfo(
    const OperatorPtr& operation,
    const IdToNodeOutputMap& id_to_node_output_map,
    uint32_t index = 0) {
  CHECK_LT(index, operation->input_operands.size());
  auto input_id = operation->input_operands[index];
  const auto input_iterator = id_to_node_output_map.find(input_id);
  CHECK(input_iterator != id_to_node_output_map.end());
  return input_iterator->second;
}

const NodeOutputInfo& GetNodeOutputInfo(
    const IdToNodeOutputMap& id_to_node_output_map,
    uint64_t operand_id) {
  const auto input_iterator = id_to_node_output_map.find(operand_id);
  CHECK(input_iterator != id_to_node_output_map.end());
  return input_iterator->second;
}

const TensorDesc GetOutputTensorDesc(const OperatorPtr& operation,
                                     const IdToOperandMap& id_to_operand_map,
                                     uint32_t index = 0) {
  CHECK_LT(index, operation->output_operands.size());
  const auto output_id = operation->output_operands[index];
  const auto output_iterator = id_to_operand_map.find(output_id);
  CHECK(output_iterator != id_to_operand_map.end());
  auto& output_operand = output_iterator->second;

  return TensorDesc(GetTensorDataType(output_operand->data_type),
                    output_operand->dimensions);
}

const TensorDesc CreateOutputTensorDesc(const IdToOperandMap& id_to_operand_map,
                                        uint64_t output_id) {
  const OperandPtr& output_operand = id_to_operand_map.at(output_id);
  return TensorDesc(GetTensorDataType(output_operand->data_type),
                    output_operand->dimensions);
}

void CreateNodeOutput(const OperatorPtr& operation,
                      GraphBuilder& graph_builder,
                      const NodeInfo& operator_node,
                      TensorDesc output_tensor_desc,
                      IdToNodeOutputMap& id_to_node_output_map,
                      uint32_t index = 0) {
  CHECK_LT(index, operation->output_operands.size());
  auto output_id = operation->output_operands[index];
  NodeOutputInfo node_output = graph_builder.CreateNodeOutput(
      operator_node, std::move(output_tensor_desc), index);
  CHECK(id_to_node_output_map.find(output_id) == id_to_node_output_map.end());
  id_to_node_output_map[output_id] = std::move(node_output);
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForClamp(
    const IdToOperandMap& id_to_operand_map,
    const OperatorPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map);
  auto input_tensor_desc =
      graph_builder.GetNodeOutput(input_node_output_info).tensor_desc;

  auto output_tensor_desc = GetOutputTensorDesc(operation, id_to_operand_map);

  CHECK(operation->attributes);
  auto& clamp_attributes = operation->attributes->get_clamp();
  CHECK(clamp_attributes);

  DML_ELEMENT_WISE_CLIP_OPERATOR_DESC clamp_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      // No scale or bias applies to the input.
      .ScaleBias = nullptr,
      .Min = clamp_attributes->min_value,
      .Max = clamp_attributes->max_value};
  NodeInfo clamp_node_info = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ELEMENT_WISE_CLIP, &clamp_operator_desc,
      {input_node_output_info});
  if (clamp_node_info.type == NodeInfo::Type::kInvalid) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Failed to create clamp operator."));
  }

  CreateNodeOutput(operation, graph_builder, clamp_node_info,
                   output_tensor_desc, id_to_node_output_map);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForConv2d(
    const IdToOperandMap& id_to_operand_map,
    const OperatorPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map, 0);
  auto input_tensor_desc =
      graph_builder.GetNodeOutput(input_node_output_info).tensor_desc;
  CHECK_EQ(input_tensor_desc.GetDimensions().size(), 4u);

  const auto& filter_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map, 1);
  auto filter_tensor_desc =
      graph_builder.GetNodeOutput(filter_node_output_info).tensor_desc;

  auto output_tensor_desc = GetOutputTensorDesc(operation, id_to_operand_map);

  CHECK(operation->attributes);
  auto& conv2d_attributes = operation->attributes->get_conv2d();
  CHECK(conv2d_attributes);

  std::vector<NodeOutputInfo> input_node_output_infos = {
      input_node_output_info, filter_node_output_info};
  absl::optional<TensorDesc> reshaped_bias_tensor_desc;
  auto& bias_operand_id = conv2d_attributes->bias_operand_id;
  if (bias_operand_id) {
    const auto bias_node_output_iterator =
        id_to_node_output_map.find(bias_operand_id.value());
    CHECK(bias_node_output_iterator != id_to_node_output_map.end());

    auto bias_node_output =
        graph_builder.GetNodeOutput(bias_node_output_iterator->second);
    const auto& bias_tensor_desc = bias_node_output.tensor_desc;
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

    auto reshaped_bias_node_output_info = graph_builder.CreateNodeOutput(
        bias_node_output.node_info, reshaped_bias_tensor_desc.value());
    input_node_output_infos.push_back(reshaped_bias_node_output_info);
  }

  switch (conv2d_attributes->input_layout) {
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

  std::array<uint32_t, 2> strides = {conv2d_attributes->strides->height,
                                     conv2d_attributes->strides->width};
  std::array<uint32_t, 2> dilations = {conv2d_attributes->dilations->height,
                                       conv2d_attributes->dilations->width};
  std::array<uint32_t, 2> start_padding = {
      conv2d_attributes->padding->beginning->height,
      conv2d_attributes->padding->beginning->width};
  std::array<uint32_t, 2> end_padding = {
      conv2d_attributes->padding->ending->height,
      conv2d_attributes->padding->ending->width};
  // The outputPadding parameter is used in the ConTranspose2d operator, and is
  // only used to disambiguate output shape when needed.
  std::array<uint32_t, 2> default_out_padding = {0, 0};

  // Currently only DML_OPERATOR_ACTIVATION_RELU is supported as the fused
  // activation. DML_OPERATOR_ELEMENT_WISE_CLIP will be supported after the
  // DirectML version upper than DML_FEATURE_LEVEL_6_0.
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-feature-level-history#dml_feature_level_6_0
  //
  // TODO: Use a union of all activation operator structures to support and
  // simplify the creation of fused activation operators.
  absl::optional<DML_ACTIVATION_RELU_OPERATOR_DESC> dml_relu_desc;
  absl::optional<DML_OPERATOR_DESC> dml_activation_desc;
  if (conv2d_attributes->activation) {
    switch (conv2d_attributes->activation->kind) {
      case Operator::Kind::kRelu: {
        dml_relu_desc = DML_ACTIVATION_RELU_OPERATOR_DESC{
            .InputTensor = nullptr, .OutputTensor = nullptr};
        dml_activation_desc =
            DML_OPERATOR_DESC{.Type = DML_OPERATOR_ACTIVATION_RELU,
                              .Desc = &dml_relu_desc.value()};
        break;
      }
      default: {
        DLOG(ERROR) << "This fusion type is not supported.";
        return base::unexpected(
            mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                              "This fusion type is not supported."));
      }
    }
  }

  DML_CONVOLUTION_OPERATOR_DESC conv2d_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .FilterTensor = &filter_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = (reshaped_bias_tensor_desc.has_value())
                        ? &reshaped_bias_tensor_desc->GetDMLTensorDesc()
                        : nullptr,
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION,
      .Direction = DML_CONVOLUTION_DIRECTION_FORWARD,
      .DimensionCount =
          2u, /*Determines the size of the Strides, Dilations, StartPadding,
                 EndPadding, and OutputPadding arrays.*/
      .Strides = strides.data(),
      .Dilations = dilations.data(),
      .StartPadding = start_padding.data(),
      .EndPadding = end_padding.data(),
      .OutputPadding = default_out_padding.data(),
      .GroupCount = conv2d_attributes->groups,
      .FusedActivation = (dml_activation_desc.has_value())
                             ? &dml_activation_desc.value()
                             : nullptr};

  auto conv2d_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_CONVOLUTION, &conv2d_operator_desc, input_node_output_infos);
  if (conv2d_node.type == NodeInfo::Type::kInvalid) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Failed to create conv2d operator."));
  }

  if (conv2d_attributes->input_layout ==
      mojom::InputOperandLayout::kChannelsLast) {
    // Transpose the output tensor from nchw to nhwc layout.
    output_tensor_desc.Transpose(kNchwToNhwcPermutation);
  }

  CreateNodeOutput(operation, graph_builder, conv2d_node, output_tensor_desc,
                   id_to_node_output_map);

  return base::ok();
}

template <typename DML_OPERATOR_DESC>
NodeInfo CreateBinaryOperator(
    const TensorDesc& a_tensor,
    const TensorDesc& b_tensor,
    const TensorDesc& output_tensor,
    GraphBuilder& graph_builder,
    DML_OPERATOR_TYPE operator_type,
    const std::vector<NodeOutputInfo>& node_output_infos) {
  DML_OPERATOR_DESC binary_operator_desc{
      .ATensor = &a_tensor.GetDMLTensorDesc(),
      .BTensor = &b_tensor.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor.GetDMLTensorDesc()};
  return graph_builder.CreateOperatorNode(operator_type, &binary_operator_desc,
                                          node_output_infos);
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForBinary(
    const IdToOperandMap& id_to_operand_map,
    const OperatorPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_a_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map, 0);
  auto input_a_tensor_desc =
      graph_builder.GetNodeOutput(input_a_node_output_info).tensor_desc;
  const auto& input_b_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map, 1);
  auto input_b_tensor_desc =
      graph_builder.GetNodeOutput(input_b_node_output_info).tensor_desc;

  const auto output_tensor_desc =
      GetOutputTensorDesc(operation, id_to_operand_map);

  auto output_dimensions = output_tensor_desc.GetDimensions();
  if (input_a_tensor_desc.GetDimensions() != output_dimensions) {
    input_a_tensor_desc.BroadcastTo(output_dimensions);
  }
  if (input_b_tensor_desc.GetDimensions() != output_dimensions) {
    input_b_tensor_desc.BroadcastTo(output_dimensions);
  }

  NodeInfo binary_node;
  std::vector<NodeOutputInfo> input_node_output_infos = {
      input_a_node_output_info, input_b_node_output_info};
  switch (operation->kind) {
    case mojom::Operator::Kind::kAdd: {
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_ADD_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_ADD,
          input_node_output_infos);
      break;
    }
    case mojom::Operator::Kind::kDiv: {
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_DIVIDE_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_DIVIDE,
          input_node_output_infos);
      break;
    }
    case mojom::Operator::Kind::kMax: {
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_MAX_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_MAX,
          input_node_output_infos);
      break;
    }
    case mojom::Operator::Kind::kMin: {
      binary_node = CreateBinaryOperator<DML_ELEMENT_WISE_MIN_OPERATOR_DESC>(
          input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
          graph_builder, DML_OPERATOR_ELEMENT_WISE_MIN,
          input_node_output_infos);
      break;
    }
    case mojom::Operator::Kind::kMul: {
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_MULTIPLY,
              input_node_output_infos);
      break;
    }
    case mojom::Operator::Kind::kSub: {
      binary_node =
          CreateBinaryOperator<DML_ELEMENT_WISE_SUBTRACT_OPERATOR_DESC>(
              input_a_tensor_desc, input_b_tensor_desc, output_tensor_desc,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_SUBTRACT,
              input_node_output_infos);
      break;
    }
    case mojom::Operator::Kind::kPow: {
      DML_ELEMENT_WISE_POW_OPERATOR_DESC element_wise_operator_desc{
          .InputTensor = &input_a_tensor_desc.GetDMLTensorDesc(),
          .ExponentTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};
      binary_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_ELEMENT_WISE_POW, &element_wise_operator_desc,
          input_node_output_infos);
      break;
    }
    default:
      LOG(ERROR) << "This operator type is not supported";
      NOTREACHED_NORETURN();
  }
  if (binary_node.type == NodeInfo::Type::kInvalid) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError,
        "Failed to create " + OpKindToString(operation->kind) + " operator."));
  }

  CreateNodeOutput(operation, graph_builder, binary_node, output_tensor_desc,
                   id_to_node_output_map);

  return base::ok();
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForPool2d(
    const IdToOperandMap& id_to_operand_map,
    const mojom::Pool2dPtr& pool2d,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_node_output_info =
      GetNodeOutputInfo(id_to_node_output_map, pool2d->input_operand_id);
  auto input_tensor_desc =
      graph_builder.GetNodeOutput(input_node_output_info).tensor_desc;

  uint64_t output_id = pool2d->output_operand_id;
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
  NodeInfo pool2d_node_info;
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
        return base::unexpected(mojom::Error::New(
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
      pool2d_node_info = graph_builder.CreateOperatorNode(
          DML_OPERATOR_AVERAGE_POOLING, &average_pooling_desc,
          {input_node_output_info});
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
      pool2d_node_info = graph_builder.CreateOperatorNode(
          DML_OPERATOR_MAX_POOLING2, &max_pooling_desc,
          {input_node_output_info});
      break;
    }
    default:
      DLOG(ERROR) << "Invalid Pool2d operator type";
      NOTREACHED_NORETURN();
  }

  if (pool2d_node_info.type == NodeInfo::Type::kInvalid) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Failed to create pooling operator."));
  }
  if (pool2d->layout == mojom::InputOperandLayout::kChannelsLast) {
    // Transpose the output tensor from nchw to nhwc layout.
    output_tensor_desc.Transpose(kNchwToNhwcPermutation);
  }

  CHECK(id_to_node_output_map.find(output_id) == id_to_node_output_map.end());
  id_to_node_output_map[output_id] = graph_builder.CreateNodeOutput(
      pool2d_node_info, std::move(output_tensor_desc), 0);

  return base::ok();
}

template <typename DML_OPERATOR_DESC>
NodeInfo CreateUnaryOperator(const TensorDesc& input_tensor,
                             const TensorDesc& output_tensor,
                             const NodeOutputInfo& node_output_info,
                             GraphBuilder& graph_builder,
                             DML_OPERATOR_TYPE operator_type) {
  DML_OPERATOR_DESC unary_operator_desc{
      .InputTensor = &input_tensor.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor.GetDMLTensorDesc()};
  return graph_builder.CreateOperatorNode(operator_type, &unary_operator_desc,
                                          {node_output_info});
}

base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForUnary(
    const IdToOperandMap& id_to_operand_map,
    const OperatorPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map);
  auto input_tensor_desc =
      graph_builder.GetNodeOutput(input_node_output_info).tensor_desc;

  const auto output_tensor_desc =
      GetOutputTensorDesc(operation, id_to_operand_map);

  NodeInfo unary_node;
  switch (operation->kind) {
    case mojom::Operator::Kind::kRelu: {
      unary_node = CreateUnaryOperator<DML_ACTIVATION_RELU_OPERATOR_DESC>(
          input_tensor_desc, output_tensor_desc, input_node_output_info,
          graph_builder, DML_OPERATOR_ACTIVATION_RELU);
      break;
    }
    case mojom::Operator::Kind::kSoftmax: {
      unary_node = CreateUnaryOperator<DML_ACTIVATION_SOFTMAX_OPERATOR_DESC>(
          input_tensor_desc, output_tensor_desc, input_node_output_info,
          graph_builder, DML_OPERATOR_ACTIVATION_SOFTMAX);
      break;
    }
    default:
      DLOG(ERROR) << "This operator type is not supported";
      NOTREACHED_NORETURN();
  }
  if (unary_node.type == NodeInfo::Type::kInvalid) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError,
        "Failed to create " + OpKindToString(operation->kind) + " operator."));
  }

  CreateNodeOutput(operation, graph_builder, unary_node, output_tensor_desc,
                   id_to_node_output_map);

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
                                const OperatorPtr& operation,
                                GraphBuilder& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map);
  auto input_node_output = graph_builder.GetNodeOutput(input_node_output_info);
  TensorDesc input_tensor_desc = input_node_output.tensor_desc;

  const auto output_tensor_desc =
      GetOutputTensorDesc(operation, id_to_operand_map);

  NodeInfo input_node = input_node_output.node_info;
  CHECK_NE(input_node.type, NodeInfo::Type::kInvalid);

  CreateNodeOutput(operation, graph_builder, input_node, output_tensor_desc,
                   id_to_node_output_map);
}

// Creates a DirectML operator for the WebNN general matrix multiplication
// (GEMM) of the expression alpha * A * B + beta * C.
base::expected<void, mojom::ErrorPtr> CreateOperatorNodeForGemm(
    const IdToOperandMap& id_to_operand_map,
    const OperatorPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  const auto& input_a_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map, 0);
  auto input_a_tensor_desc =
      graph_builder.GetNodeOutput(input_a_node_output_info).tensor_desc;

  const auto& input_b_node_output_info =
      GetInputNodeOutputInfo(operation, id_to_node_output_map, 1);
  auto input_b_tensor_desc =
      graph_builder.GetNodeOutput(input_b_node_output_info).tensor_desc;

  std::vector<NodeOutputInfo> input_node_output_infos{input_a_node_output_info,
                                                      input_b_node_output_info};

  const auto output_tensor_desc =
      GetOutputTensorDesc(operation, id_to_operand_map);

  absl::optional<TensorDesc> input_c_tensor_desc;
  CHECK(operation->attributes);
  auto& gemm_attributes = operation->attributes->get_gemm();
  CHECK(gemm_attributes);

  auto& c_operand_id = gemm_attributes->c_operand_id;
  if (c_operand_id) {
    uint64_t input_c_id = c_operand_id.value();

    const auto input_c_node_output_iterator =
        id_to_node_output_map.find(input_c_id);
    CHECK(input_c_node_output_iterator != id_to_node_output_map.end());

    NodeOutputInfo input_c_node_output_info =
        input_c_node_output_iterator->second;
    input_c_tensor_desc =
        graph_builder.GetNodeOutput(input_c_node_output_info).tensor_desc;

    // Ensure the graph edge for c operand will be created.
    input_node_output_infos.push_back(input_c_node_output_info);

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
      .TransA = (gemm_attributes->a_transpose) ? DML_MATRIX_TRANSFORM_TRANSPOSE
                                               : DML_MATRIX_TRANSFORM_NONE,
      .TransB = (gemm_attributes->b_transpose) ? DML_MATRIX_TRANSFORM_TRANSPOSE
                                               : DML_MATRIX_TRANSFORM_NONE,
      .Alpha = gemm_attributes->alpha,
      .Beta = gemm_attributes->beta,
      .FusedActivation = nullptr,  // Not supported
  };

  NodeInfo gemm_node_info = graph_builder.CreateOperatorNode(
      DML_OPERATOR_GEMM, &gemm_operator_desc, input_node_output_infos);
  if (gemm_node_info.type == NodeInfo::Type::kInvalid) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Failed to create gemm operator."));
  }

  CreateNodeOutput(operation, graph_builder, gemm_node_info, output_tensor_desc,
                   id_to_node_output_map);

  return base::ok();
}

// TODO(crbug.com/1273291): Removes this function when all operators are
// implemented in the `union Operation`.
base::expected<void, mojom::ErrorPtr> CreateGenericOperator(
    const IdToOperandMap& id_to_operand_map,
    const OperatorPtr& operation,
    GraphBuilder& graph_builder,
    IdToNodeOutputMap& id_to_node_output_map) {
  // For operators that deal with DML API, there is a chance that operator
  // creation will fail. Use `mojom::ErrorPtr` to hold the given error
  // message.
  base::expected<void, mojom::ErrorPtr> create_operator_result;
  switch (operation->kind) {
    case Operator::Kind::kClamp: {
      create_operator_result = CreateOperatorNodeForClamp(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
      break;
    }
    case Operator::Kind::kConv2d: {
      create_operator_result = CreateOperatorNodeForConv2d(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
      break;
    }
    case Operator::Kind::kAdd:
    case Operator::Kind::kDiv:
    case Operator::Kind::kMax:
    case Operator::Kind::kMin:
    case Operator::Kind::kMul:
    case Operator::Kind::kPow:
    case Operator::Kind::kSub: {
      create_operator_result = CreateOperatorNodeForBinary(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
      break;
    }
    case Operator::Kind::kRelu:
    case Operator::Kind::kSoftmax: {
      create_operator_result = CreateOperatorNodeForUnary(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
      break;
    }
    case Operator::Kind::kReshape: {
      CreateNodeOutputForReshape(id_to_operand_map, operation, graph_builder,
                                 id_to_node_output_map);
      break;
    }
    case Operator::Kind::kGemm: {
      create_operator_result = CreateOperatorNodeForGemm(
          id_to_operand_map, operation, graph_builder, id_to_node_output_map);
      break;
    }
    default:
      DLOG(ERROR) << "This operator kind (" + OpKindToString(operation->kind) +
                         ") is not supported.";
      create_operator_result = base::unexpected(mojom::Error::New(
          mojom::Error::Code::kNotSupportedError,
          "This operator (" + OpKindToString(operation->kind) +
              ") is not supported."));
  }
  return create_operator_result;
}

}  // namespace

GraphImpl::GraphBufferBindingInfo::GraphBufferBindingInfo() = default;
GraphImpl::GraphBufferBindingInfo::~GraphBufferBindingInfo() = default;

GraphImpl::GraphBufferBindingInfo::GraphBufferBindingInfo(
    GraphBufferBindingInfo&&) = default;
GraphImpl::GraphBufferBindingInfo& GraphImpl::GraphBufferBindingInfo::operator=(
    GraphBufferBindingInfo&&) = default;

GraphImpl::GraphImpl(std::unique_ptr<CommandRecorder> command_recorder,
                     ComPtr<ID3D12Resource> persistent_buffer,
                     ComPtr<IDMLCompiledOperator> compiled_operator,
                     ComputeResourceInfo compute_resource_info,
                     GraphBufferBindingInfo graph_buffer_binding_info)
    : WebNNGraphImpl(std::move(compute_resource_info)),
      persistent_buffer_(std::move(persistent_buffer)),
      command_recorder_(std::move(command_recorder)),
      compiled_operator_(std::move(compiled_operator)),
      graph_buffer_binding_info_(std::move(graph_buffer_binding_info)) {
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
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError, "Failed to compile the graph."));
    return;
  }

  HRESULT hr = command_recorder->Open();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open the command recorder: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError,
        "Failed to open the command recorder."));
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
    auto constant_buffer_binding = UploadAndCreateBufferBinding<uint64_t>(
        command_recorder.get(), constant_id_to_buffer_map);
    if (!constant_buffer_binding) {
      DLOG(ERROR) << "Failed to upload constant weight data.";
      std::move(callback).Run(ToError<mojom::CreateGraphResult>(
          mojom::Error::Code::kUnknownError,
          "Failed to upload constant weight data."));
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
      std::move(callback).Run(ToError<mojom::CreateGraphResult>(
          mojom::Error::Code::kUnknownError,
          "Failed to create the default buffer."));
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
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError,
        "Failed to initialize the operator."));
    return;
  }

  hr = command_recorder->CloseAndExecute();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to close and execute the command list: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError,
        "Failed to close and execute the command list."));
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
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError,
        "Failed to wait for the initialization to complete."));
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
          std::move(graph_buffer_binding_info))),
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
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError,
        "Failed to open the command recorder."));
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
      case mojom::Operation::Tag::kPool2d: {
        create_operator_result = CreateOperatorNodeForPool2d(
            id_to_operand_map, operation->get_pool2d(), graph_builder,
            id_to_node_output_map);
        break;
      }
      case mojom::Operation::Tag::kGenericOperator: {
        create_operator_result = CreateGenericOperator(
            id_to_operand_map, operation->get_generic_operator(), graph_builder,
            id_to_node_output_map);
        break;
      }
    }
    if (!create_operator_result.has_value()) {
      std::move(callback).Run(mojom::CreateGraphResult::NewError(
          std::move(create_operator_result.error())));
      return;
    }
  }

  std::vector<NodeOutputInfo> graph_outputs;
  graph_outputs.reserve(graph_info->output_operands.size());

  for (auto& output_id : graph_info->output_operands) {
    const auto output_iterator = id_to_node_output_map.find(output_id);
    CHECK(output_iterator != id_to_node_output_map.end());
    NodeOutputInfo node_output_info = output_iterator->second;
    NodeOutput node_output = graph_builder.GetNodeOutput(node_output_info);
    TensorDesc output_tensor_desc = node_output.tensor_desc;

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
    auto output_type = node_output.node_info.type;
    if (output_type == NodeInfo::Type::kInput) {
      TensorDesc identity_tensor_desc(output_tensor_desc.GetDataType(),
                                      DML_TENSOR_FLAG_NONE,
                                      output_tensor_desc.GetDimensions());
      NodeInfo identity_node =
          CreateUnaryOperator<DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC>(
              output_tensor_desc, identity_tensor_desc, node_output_info,
              graph_builder, DML_OPERATOR_ELEMENT_WISE_IDENTITY);

      node_output_info = graph_builder.CreateNodeOutput(
          identity_node, std::move(identity_tensor_desc));
    }

    std::string name = id_to_operand_map.at(output_id)->name.value();
    graph_buffer_binding_info.graph_output_name_to_index_map[std::move(name)] =
        graph_builder.CreateOutputEdge(node_output_info);
  }

  graph_buffer_binding_info.input_buffer_binding_count =
      constant_id_to_input_index_map.size() +
      graph_buffer_binding_info.graph_input_name_to_index_map.size();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
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

  // Create the input resource binding for graph execution.
  auto input_buffer_binding = UploadAndCreateBufferBinding<std::string>(
      command_recorder_.get(), named_inputs);
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

  // Calculate the total byte length of outputs array buffer to create
  // an output buffer and readback buffer, also records the aligned D3D12_RANGE
  // for each output.
  // TODO(crbug.com/1480227): Allows to compute for some selected outputs
  // instead of all outputs of the DML graph.
  absl::optional<AlignedByteLength<std::string>>
      aligned_byte_length_of_outputs = CalculateAlignedByteLength(
          compute_resource_info().output_name_to_byte_length_map);
  if (!aligned_byte_length_of_outputs) {
    HandleComputationFailure(
        "Failed to calculate the aligned byte length of outputs.",
        std::move(callback));
    return;
  }

  // Create the output buffer which will be bound for the graph execution.
  size_t total_byte_length_of_outputs =
      aligned_byte_length_of_outputs.value().total_byte_length;
  ComPtr<ID3D12Resource> default_output_buffer;
  hr = command_recorder_->CreateDefaultBuffer(total_byte_length_of_outputs,
                                              L"WebNN_Default_Output_Buffer",
                                              default_output_buffer);
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to create the default output buffer.", hr,
                             std::move(callback));
    return;
  }

  // Create the readback buffer which will be read by CPU.
  ComPtr<ID3D12Resource> readback_output_buffer;
  hr = command_recorder_->CreateReadbackBuffer(total_byte_length_of_outputs,
                                               L"WebNN_Readback_Output_Buffer",
                                               readback_output_buffer);
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to create the readback output buffer.", hr,
                             std::move(callback));
    return;
  }

  // Create the output buffer bindings for the graph execution.
  size_t output_buffer_binding_count =
      graph_buffer_binding_info_.graph_output_name_to_index_map.size();
  std::vector<DML_BINDING_DESC> output_buffer_binding_desc(
      output_buffer_binding_count,
      DML_BINDING_DESC{.Type = DML_BINDING_TYPE_NONE, .Desc = nullptr});
  std::vector<DML_BUFFER_BINDING> output_buffer_binding;
  output_buffer_binding.reserve(output_buffer_binding_count);

  std::map<std::string, D3D12_RANGE> graph_output_name_to_d3d12_range_map =
      std::move(aligned_byte_length_of_outputs.value().key_to_d3d12_range_map);
  for (auto& [name, graph_output_index] :
       graph_buffer_binding_info_.graph_output_name_to_index_map) {
    auto& d3d12_range = graph_output_name_to_d3d12_range_map[name];
    output_buffer_binding.push_back(
        DML_BUFFER_BINDING{.Buffer = default_output_buffer.Get(),
                           .Offset = d3d12_range.Begin,
                           .SizeInBytes = d3d12_range.End - d3d12_range.Begin});
    output_buffer_binding_desc[graph_output_index] = {
        DML_BINDING_TYPE_BUFFER, &output_buffer_binding.back()};
  }

  // Execute the graph with input, output and persistent buffer bindings.
  hr = command_recorder_->ExecuteOperator(
      compiled_operator_.Get(), input_buffer_binding_desc,
      output_buffer_binding_desc, persistent_buffer_binding_desc_);
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to execute the operator.", hr,
                             std::move(callback));
    return;
  }

  // Copy the output data from output buffer to readback buffer.
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(default_output_buffer.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_SOURCE);
  command_recorder_->ResourceBarrier(barriers);
  command_recorder_->CopyBufferRegion(readback_output_buffer.Get(), 0,
                                      default_output_buffer.Get(), 0,
                                      total_byte_length_of_outputs);
  barriers[0] = CreateTransitionBarrier(default_output_buffer.Get(),
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
      std::move(callback), std::move(readback_output_buffer),
      std::move(graph_output_name_to_d3d12_range_map)));
}

void GraphImpl::OnComputationComplete(
    mojom::WebNNGraph::ComputeCallback callback,
    ComPtr<ID3D12Resource> readback_output_buffer,
    std::map<std::string, D3D12_RANGE> graph_output_name_to_d3d12_range_map,
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
  hr = readback_output_buffer->Map(0, nullptr, &mapped_readback_output_buffer);
  if (FAILED(hr)) {
    HandleComputationFailure("Failed to map the readback output buffer.", hr,
                             std::move(callback));
    return;
  }

  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  named_outputs.reserve(graph_output_name_to_d3d12_range_map.size());
  for (auto& [name, d3d12_range] : graph_output_name_to_d3d12_range_map) {
    named_outputs[name] = mojo_base::BigBuffer(base::make_span(
        static_cast<const uint8_t*>(mapped_readback_output_buffer) +
            d3d12_range.Begin,
        compute_resource_info().output_name_to_byte_length_map.at(name)));
  }

  readback_output_buffer->Unmap(0, nullptr);
  command_queue_->ReleaseCompletedResources();
  std::move(callback).Run(ComputeResult::kOk, std::move(named_outputs));
}

}  // namespace webnn::dml
