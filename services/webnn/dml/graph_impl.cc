// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/graph_impl.h"

#include <array>

#include "base/bits.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/graph_builder.h"
#include "services/webnn/dml/tensor_desc.h"
#include "services/webnn/dml/utils.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {
namespace {

using Microsoft::WRL::ComPtr;
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
    case Operator::Kind::kRelu:
      return "relu";
    case Operator::Kind::kReshape:
      return "reshape";
    case Operator::Kind::kSoftmax:
      return "softmax";
    default:
      return base::NumberToString(base::checked_cast<uint32_t>(kind));
  }
}

// Upload constants/inputs buffers in one Direct3D 12 committed resource, the
// DML_BUFFER_BINDING specifies a resource binding described by a range of bytes
// in the single buffer.
template <typename T>
absl::optional<base::flat_map<T, DML_BUFFER_BINDING>>
UploadAndCreateBufferBinding(
    CommandRecorder* command_recorder,
    const base::flat_map<T, mojo_base::BigBuffer>& input_to_buffer_map) {
  // Copy all array buffers of constants/inputs to an upload heap and create a
  // committed resource which is mapped to the heap.
  //
  // Calculate the total byte length of constants/inputs array buffer to create
  // an upload buffer which can be read by GPU.
  base::CheckedNumeric<size_t> total_byte_length(0);
  base::flat_map<T, D3D12_RANGE> input_to_range_map;
  for (auto& [input_id, input_buffer] : input_to_buffer_map) {
    auto& subresource_range = input_to_range_map[input_id];
    // There is only one upload heap for all constants/inputs, the byte offset
    // in the `Begin` attribute is used to get the copied address for each
    // constant/input tensor.
    subresource_range.Begin = total_byte_length.ValueOrDie();

    // The buffer has a minimum base address alignment requirement of 16 bytes
    // in the macro `DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT`:
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-directml-constants
    total_byte_length += base::bits::AlignUp<size_t>(
        input_buffer.size(), DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT);
    if (!total_byte_length.IsValid()) {
      DLOG(ERROR) << "Failed to calculate the total byte length of the input.";
      return absl::nullopt;
    }
    // The aligned byte length calculated with `End` sub `Begin` attribute is
    // used to set the `SizeInBytes` field of `DML_BUFFER_BINDING`.
    subresource_range.End = total_byte_length.ValueOrDie();
  }

  // Create the upload heap that can be written by CPU and read from GPU, and
  // create a resource to map the heap.
  ComPtr<ID3D12Resource> upload_buffer;
  HRESULT hr = command_recorder->CreateUploadBuffer(
      total_byte_length.ValueOrDie(), upload_buffer);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create upload buffer for the input: "
                << logging::SystemErrorCodeToString(hr);
    return absl::nullopt;
  }
  // Create the default heap that only can be accessed by GPU not provide CPU
  // access, and create a resource to map the heap.
  ComPtr<ID3D12Resource> default_buffer;
  hr = command_recorder->CreateDefaultBuffer(total_byte_length.ValueOrDie(),
                                             default_buffer);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create default buffer: "
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
  base::flat_map<T, DML_BUFFER_BINDING> buffer_binding;
  for (auto& [input_id, input_buffer] : input_to_buffer_map) {
    // Copy the input data to the upload heap with byte offset
    auto& subresource_range = input_to_range_map.at(input_id);
    memcpy(
        static_cast<uint8_t*>(mapped_upload_buffer) + subresource_range.Begin,
        input_buffer.data(), input_buffer.size());
    // Create the buffer binding for each constant/input and push back into the
    // DML_BUFFER_BINDING array.
    auto size_in_bytes = subresource_range.End - subresource_range.Begin;
    buffer_binding[input_id] =
        DML_BUFFER_BINDING{.Buffer = default_buffer.Get(),
                           .Offset = subresource_range.Begin,
                           .SizeInBytes = size_in_bytes};
  }
  upload_buffer->Unmap(0, nullptr);

  UploadBufferWithBarrier(command_recorder, default_buffer.Get(),
                          upload_buffer.Get(), total_byte_length.ValueOrDie());
  // Keep the default_buffer and upload_buffer alive until the GPU work is done.
  command_recorder->GetCommandQueue()->ReferenceUntilCompleted(
      std::move(default_buffer));
  command_recorder->GetCommandQueue()->ReferenceUntilCompleted(
      std::move(upload_buffer));

  return buffer_binding;
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

bool CreateOperatorNodeForClamp(const IdToOperandMap& id_to_operand_map,
                                const OperatorPtr& operation,
                                GraphBuilder& graph_builder,
                                IdToNodeOutputMap& id_to_node_output_map) {
  uint64_t input_id = operation->input_operands[0];
  const auto input_iterator = id_to_node_output_map.find(input_id);
  CHECK(input_iterator != id_to_node_output_map.end());
  NodeOutputInfo input_node_output_info = input_iterator->second;
  TensorDesc input_tensor_desc =
      graph_builder.GetNodeOutput(input_node_output_info).tensor_desc;

  uint64_t output_id = operation->output_operands[0];
  const OperandPtr& output_operand = id_to_operand_map.at(output_id);
  TensorDesc output_tensor_desc(GetTensorDataType(output_operand->data_type),
                                output_operand->dimensions);

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
    return false;
  }

  NodeOutputInfo clamp_output_info = graph_builder.CreateNodeOutput(
      clamp_node_info, std::move(output_tensor_desc));
  id_to_node_output_map[output_id] = std::move(clamp_output_info);
  return true;
}

bool CreateOperatorNodeForPool2d(const IdToOperandMap& id_to_operand_map,
                                 const OperatorPtr& operation,
                                 GraphBuilder& graph_builder,
                                 IdToNodeOutputMap& id_to_node_output_map) {
  uint64_t input_id = operation->input_operands[0];
  const auto input_iterator = id_to_node_output_map.find(input_id);
  CHECK(input_iterator != id_to_node_output_map.end());
  NodeOutputInfo input_node_output_info = input_iterator->second;
  TensorDesc input_tensor_desc =
      graph_builder.GetNodeOutput(input_node_output_info).tensor_desc;

  uint64_t output_id = operation->output_operands[0];
  const OperandPtr& output_operand = id_to_operand_map.at(output_id);
  TensorDesc output_tensor_desc(GetTensorDataType(output_operand->data_type),
                                output_operand->dimensions);

  CHECK(operation->attributes);
  auto& pool2d_attributes = operation->attributes->get_pool2d();
  CHECK(pool2d_attributes);

  switch (pool2d_attributes->layout) {
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
    default:
      DLOG(ERROR) << "Invalid Pool2d layout";
      NOTREACHED_NORETURN();
  }

  std::array<uint32_t, 2> strides = {pool2d_attributes->strides->height,
                                     pool2d_attributes->strides->width};
  std::array<uint32_t, 2> dilations = {pool2d_attributes->dilations->height,
                                       pool2d_attributes->dilations->width};
  std::array<uint32_t, 2> window_dimensions = {
      pool2d_attributes->window_dimensions->height,
      pool2d_attributes->window_dimensions->width};
  std::array<uint32_t, 2> start_padding = {
      pool2d_attributes->padding->beginning->height,
      pool2d_attributes->padding->beginning->width};
  std::array<uint32_t, 2> end_padding = {
      pool2d_attributes->padding->ending->height,
      pool2d_attributes->padding->ending->width};
  NodeInfo pool2d_node_info;
  switch (operation->kind) {
      // TODO(crbug.com/1273291): Add L2Pool2d operator.

    case mojom::Operator::Kind::kAveragePool2d: {
      // TODO(crbug.com/1273291): Work around dilation support for L2 and
      // average pooling. According to WebNN spec:
      // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pool2d, dilations are
      // supported by pooling operations, while for DirectML AVERAGE_POOLING and
      // LP_POOLING don't support dilations.
      // Spec issue tracked on
      // https://github.com/webmachinelearning/webnn/issues/180.
      if (dilations[0] != 1 || dilations[1] != 1) {
        DLOG(ERROR)
            << "Dilations are unsupported for DML average pooling operator";
        return false;
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
    case mojom::Operator::Kind::kMaxPool2d: {
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
    return false;
  }
  if (pool2d_attributes->layout == mojom::InputOperandLayout::kChannelsLast) {
    // Transpose the output tensor from nchw to nhwc layout.
    output_tensor_desc.Transpose(kNchwToNhwcPermutation);
  }

  NodeOutputInfo pool2d_output_info = graph_builder.CreateNodeOutput(
      pool2d_node_info, std::move(output_tensor_desc));
  id_to_node_output_map[output_id] = std::move(pool2d_output_info);
  return true;
}

bool CreateOperatorNodeForRelu(const IdToOperandMap& id_to_operand_map,
                               const OperatorPtr& operation,
                               GraphBuilder& graph_builder,
                               IdToNodeOutputMap& id_to_node_output_map) {
  uint64_t input_id = operation->input_operands[0];
  const auto input_iterator = id_to_node_output_map.find(input_id);
  CHECK(input_iterator != id_to_node_output_map.end());
  NodeOutputInfo input_node_output = input_iterator->second;
  TensorDesc input_tensor_desc =
      graph_builder.GetNodeOutput(input_node_output).tensor_desc;

  uint64_t output_id = operation->output_operands[0];
  const OperandPtr& output_operand = id_to_operand_map.at(output_id);
  TensorDesc output_tensor_desc(GetTensorDataType(output_operand->data_type),
                                output_operand->dimensions);

  DML_ACTIVATION_RELU_OPERATOR_DESC relu_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};
  NodeInfo relu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_RELU, &relu_operator_desc, {input_node_output});
  if (relu_node.type == NodeInfo::Type::kInvalid) {
    return false;
  }
  NodeOutputInfo relu_output =
      graph_builder.CreateNodeOutput(relu_node, std::move(output_tensor_desc));
  id_to_node_output_map[output_id] = std::move(relu_output);
  return true;
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
  uint64_t input_id = operation->input_operands[0];
  const auto input_iterator = id_to_node_output_map.find(input_id);
  CHECK(input_iterator != id_to_node_output_map.end());
  NodeOutputInfo input_node_output_info = input_iterator->second;
  NodeOutput input_node_output =
      graph_builder.GetNodeOutput(input_node_output_info);
  TensorDesc input_tensor_desc = input_node_output.tensor_desc;
  NodeInfo input_node = input_node_output.node_info;
  uint64_t output_id = operation->output_operands[0];
  const OperandPtr& output_operand = id_to_operand_map.at(output_id);
  TensorDesc output_tensor_desc(input_tensor_desc.GetDataType(),
                                DML_TENSOR_FLAG_NONE,
                                output_operand->dimensions);
  NodeOutputInfo reshaped_input_node_output =
      graph_builder.CreateNodeOutput(input_node, std::move(output_tensor_desc));
  id_to_node_output_map[output_id] = std::move(reshaped_input_node_output);
}

// Creates a DirectML operator for the WebNN general matrix multiplication
// (GEMM) of the expression alpha * A * B + beta * C.
bool CreateOperatorNodeForGemm(const IdToOperandMap& id_to_operand_map,
                               const OperatorPtr& operation,
                               GraphBuilder& graph_builder,
                               IdToNodeOutputMap& id_to_node_output_map) {
  uint64_t input_a_id = operation->input_operands[0];
  uint64_t input_b_id = operation->input_operands[1];

  const auto input_a_node_output_iterator =
      id_to_node_output_map.find(input_a_id);
  CHECK(input_a_node_output_iterator != id_to_node_output_map.end());

  const auto input_b_node_output_iterator =
      id_to_node_output_map.find(input_b_id);
  CHECK(input_b_node_output_iterator != id_to_node_output_map.end());

  NodeOutputInfo input_a_node_output = input_a_node_output_iterator->second;
  TensorDesc input_a_tensor_desc =
      graph_builder.GetNodeOutput(input_a_node_output).tensor_desc;

  NodeOutputInfo input_b_node_output = input_b_node_output_iterator->second;
  TensorDesc input_b_tensor_desc =
      graph_builder.GetNodeOutput(input_b_node_output).tensor_desc;

  uint64_t output_id = operation->output_operands[0];
  const OperandPtr& output_operand = id_to_operand_map.at(output_id);
  TensorDesc output_tensor_desc(GetTensorDataType(output_operand->data_type),
                                output_operand->dimensions);

  absl::optional<TensorDesc> input_c_tensor_desc = absl::nullopt;
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

    // TODO(crbug.com/1471201): Support broadcasting for C.
    auto input_c_shape = input_c_tensor_desc->GetDimensions();
    if (input_c_shape.size() < 2) {
      return false;
    }

    auto output_shape = output_tensor_desc.GetDimensions();
    CHECK_EQ(output_shape.size(), input_c_shape.size());

    if (output_shape[0] != input_c_shape[0] ||
        output_shape[1] != input_c_shape[1]) {
      return false;
    }
  }

  DML_GEMM_OPERATOR_DESC gemm_operator_desc{
      .ATensor = &input_a_tensor_desc.GetDMLTensorDesc(),
      .BTensor = &input_b_tensor_desc.GetDMLTensorDesc(),
      .CTensor = (input_c_tensor_desc.has_value())
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
      DML_OPERATOR_GEMM, &gemm_operator_desc,
      {input_a_node_output, input_b_node_output});
  if (gemm_node_info.type == NodeInfo::Type::kInvalid) {
    return false;
  }

  NodeOutputInfo gemm_output = graph_builder.CreateNodeOutput(
      gemm_node_info, std::move(output_tensor_desc));
  id_to_node_output_map[output_id] = std::move(gemm_output);

  return true;
}

}  // namespace

GraphImpl::InputBufferBindingInfo::InputBufferBindingInfo() = default;
GraphImpl::InputBufferBindingInfo::~InputBufferBindingInfo() = default;

GraphImpl::GraphImpl(std::unique_ptr<CommandRecorder> command_recorder,
                     ComPtr<ID3D12Resource> persistent_buffer,
                     ComPtr<IDMLCompiledOperator> compiled_operator,
                     std::unique_ptr<ComputeResourceInfo> compute_resource_info)
    : WebNNGraphImpl(std::move(compute_resource_info)),
      persistent_buffer_(std::move(persistent_buffer)),
      command_recorder_(std::move(command_recorder)),
      compiled_operator_(std::move(compiled_operator)) {}

//  Notice that it's the CommandQueue's responsibility to wait for all of the
//  queued work to complete before destructing itself.
GraphImpl::~GraphImpl() = default;

ComPtr<IDMLCompiledOperator> GraphImpl::CompileOnBackgroundThread(
    std::vector<NodeOutputInfo> graph_outputs,
    GraphBuilder graph_builder) {
  return graph_builder.Compile(graph_outputs, DML_EXECUTION_FLAG_NONE);
}

// static
void GraphImpl::OnCompilationComplete(
    mojom::WebNNContext::CreateGraphCallback callback,
    std::unique_ptr<CommandRecorder> command_recorder,
    base::flat_map<uint64_t, mojo_base::BigBuffer> constant_id_to_buffer_map,
    std::unique_ptr<InputBufferBindingInfo> input_buffer_binding_info,
    std::unique_ptr<ComputeResourceInfo> compute_resource_info,
    ComPtr<IDMLCompiledOperator> compiled_operator) {
  if (!compiled_operator) {
    DLOG(ERROR) << "Failed to compile the graph.";
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  HRESULT hr = command_recorder->Open();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open the command recorder: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(mojo::NullRemote());
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
  // is got from `constant_id_to_graph_input_index_map`.
  //
  // TODO(crbug.com/1273291): Support single operator input buffer binding.
  auto num_inputs =
      compute_resource_info->input_name_to_byte_length_map.size() +
      constant_id_to_buffer_map.size();
  // The inputs tensors without the DML_TENSOR_FLAG_OWNED_BY_DML flag is
  // expected to be bound during execution, and not during initialization.
  std::vector<DML_BUFFER_BINDING> input_buffer_binding(
      num_inputs,
      DML_BUFFER_BINDING{.Buffer = nullptr, .Offset = 0, .SizeInBytes = 0});
  if (!constant_id_to_buffer_map.empty()) {
    auto constant_buffer_binding = UploadAndCreateBufferBinding<uint64_t>(
        command_recorder.get(), constant_id_to_buffer_map);
    if (!constant_buffer_binding) {
      DLOG(ERROR) << "Failed to upload constant weight data.";
      std::move(callback).Run(mojo::NullRemote());
      return;
    }
    // The constant tensor must be bound to the binding table during operator
    // initialization, and not during execution.
    for (auto& [constant_id, buffer_binding] :
         constant_buffer_binding.value()) {
      // Get the graph input index with the constant id.
      auto& constant_id_to_graph_input_index_map =
          input_buffer_binding_info->constant_id_to_graph_input_index_map;
      const auto graph_input_index_iterator =
          constant_id_to_graph_input_index_map.find(constant_id);
      CHECK(graph_input_index_iterator !=
            constant_id_to_graph_input_index_map.end());
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
  absl::optional<DML_BINDING_DESC> persistent_buffer_binding_desc =
      absl::nullopt;
  DML_BINDING_PROPERTIES execution_binding_properties =
      compiled_operator->GetBindingProperties();
  uint64_t persistent_buffer_size =
      execution_binding_properties.PersistentResourceSize;
  ComPtr<ID3D12Resource> persistent_buffer;
  if (persistent_buffer_size) {
    hr = command_recorder->CreateDefaultBuffer(persistent_buffer_size,
                                               persistent_buffer);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create the default buffer: "
                  << logging::SystemErrorCodeToString(hr);
      std::move(callback).Run(mojo::NullRemote());
      return;
    }

    DML_BUFFER_BINDING persistent_buffer_binding{
        .Buffer = persistent_buffer.Get(),
        .Offset = 0,
        .SizeInBytes = persistent_buffer_size};

    persistent_buffer_binding_desc = DML_BINDING_DESC{
        .Type = DML_BINDING_TYPE_BUFFER, .Desc = &persistent_buffer_binding};
  }

  hr = command_recorder->InitializeOperator(compiled_operator.Get(),
                                            input_buffer_binding_desc,
                                            persistent_buffer_binding_desc);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to initialize the operator: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  hr = command_recorder->CloseAndExecute();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to close and execute the command list: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  scoped_refptr<CommandQueue> command_queue(
      command_recorder->GetCommandQueue());

  // Ensure the GPU resources needed by the initialization work on the
  // CommandQueue not to be released before the work completes.
  if (persistent_buffer) {
    command_queue->ReferenceUntilCompleted(persistent_buffer);
  }
  //  The IDMLCompiledOperator should also be referenced before the work
  //  completes.
  command_queue->ReferenceUntilCompleted(compiled_operator);

  command_queue->WaitAsync(base::BindOnce(
      &GraphImpl::OnInitializationComplete, std::move(command_recorder),
      std::move(persistent_buffer), std::move(compiled_operator),
      std::move(compute_resource_info), std::move(callback)));
}

// static
void GraphImpl::OnInitializationComplete(
    std::unique_ptr<CommandRecorder> command_recorder,
    ComPtr<ID3D12Resource> persistent_buffer,
    ComPtr<IDMLCompiledOperator> compiled_operator,
    std::unique_ptr<ComputeResourceInfo> compute_resource_info,
    mojom::WebNNContext::CreateGraphCallback callback,
    HRESULT hr) {
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to wait for the initialization to complete: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(std::move(mojo::NullRemote()));
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
          std::move(compiled_operator), std::move(compute_resource_info))),
      blink_remote.InitWithNewPipeAndPassReceiver());
  command_queue->ReleaseCompletedResources();
  std::move(callback).Run(std::move(blink_remote));
}

// static
void GraphImpl::CreateAndBuild(
    scoped_refptr<CommandQueue> command_queue,
    ComPtr<IDMLDevice> dml_device,
    const mojom::GraphInfoPtr& graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  // `CommandRecorder` would keep reference of command queue and DML device.
  std::unique_ptr<CommandRecorder> command_recorder =
      CommandRecorder::Create(command_queue, dml_device);
  if (!command_recorder) {
    DLOG(ERROR) << "Failed to open the command recorder.";
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  GraphBuilder graph_builder(dml_device);
  IdToNodeOutputMap id_to_node_output_map;
  const IdToOperandMap& id_to_operand_map = graph_info->id_to_operand_map;
  auto input_buffer_binding_info = std::make_unique<InputBufferBindingInfo>();
  // Add inputs.
  for (auto& input_id : graph_info->input_operands) {
    auto graph_input_index = CreateInputNode(
        id_to_operand_map, input_id, graph_builder, id_to_node_output_map);
    const OperandPtr& operand = id_to_operand_map.at(input_id);
    CHECK(operand);
    input_buffer_binding_info
        ->graph_input_name_to_index_map[operand->name.value()] =
        graph_input_index;
  }

  // The constant operand in WebNNGraph also is treated as input node in graph
  // desc, the tensor is identified by DML_TENSOR_FLAG_OWNED_BY_DML which must
  // be bound to the binding table during operator initialization, and not
  // during execution.
  for (auto& [constant_id, _] : graph_info->constant_id_to_buffer_map) {
    auto graph_input_index =
        CreateInputNode(id_to_operand_map, constant_id, graph_builder,
                        id_to_node_output_map, DML_TENSOR_FLAG_OWNED_BY_DML);
    input_buffer_binding_info
        ->constant_id_to_graph_input_index_map[constant_id] = graph_input_index;
  }

  // Add operations.
  for (auto& operation : graph_info->operators) {
    // For operators that deal with DML API, there is a chance that operator
    // creation will fail.
    bool was_creation_successful = true;
    switch (operation->kind) {
      case Operator::Kind::kClamp: {
        was_creation_successful = CreateOperatorNodeForClamp(
            id_to_operand_map, operation, graph_builder, id_to_node_output_map);
        break;
      }
      case Operator::Kind::kAveragePool2d:
      case Operator::Kind::kMaxPool2d: {
        was_creation_successful = CreateOperatorNodeForPool2d(
            id_to_operand_map, operation, graph_builder, id_to_node_output_map);
        break;
      }
      case Operator::Kind::kRelu: {
        was_creation_successful = CreateOperatorNodeForRelu(
            id_to_operand_map, operation, graph_builder, id_to_node_output_map);
        break;
      }
      case Operator::Kind::kReshape: {
        CreateNodeOutputForReshape(id_to_operand_map, operation, graph_builder,
                                   id_to_node_output_map);
        break;
      }
      case Operator::Kind::kGemm: {
        was_creation_successful = CreateOperatorNodeForGemm(
            id_to_operand_map, operation, graph_builder, id_to_node_output_map);
        break;
      }
      default:
        DLOG(ERROR) << "This operator kind (" +
                           OpKindToString(operation->kind) +
                           ") is not supported.";
        was_creation_successful = false;
    }
    if (!was_creation_successful) {
      std::move(callback).Run(mojo::NullRemote());
      // TODO(crbug.com/1471367): Report an error message to JS code when it
      // fails to create an operator.
      return;
    }
  }

  std::vector<NodeOutputInfo> graph_outputs;
  graph_outputs.reserve(graph_info->output_operands.size());
  for (auto& output_id : graph_info->output_operands) {
    const auto output_iterator = id_to_node_output_map.find(output_id);
    CHECK(output_iterator != id_to_node_output_map.end());
    NodeOutputInfo node_output_info = output_iterator->second;

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
    NodeOutput output_node_output =
        graph_builder.GetNodeOutput(node_output_info);
    TensorDesc output_tensor_desc = output_node_output.tensor_desc;
    auto output_type = output_node_output.node_info.type;
    if (output_type == NodeInfo::Type::kInput) {
      TensorDesc identity_tensor_desc(output_tensor_desc.GetDataType(),
                                      DML_TENSOR_FLAG_NONE,
                                      output_tensor_desc.GetDimensions());
      DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC identity_operator_desc{
          .InputTensor = &output_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &identity_tensor_desc.GetDMLTensorDesc()};
      NodeInfo identity_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_ELEMENT_WISE_IDENTITY, &identity_operator_desc,
          {node_output_info});
      NodeOutputInfo identity_node_output_info = graph_builder.CreateNodeOutput(
          identity_node, std::move(identity_tensor_desc));
      graph_outputs.push_back(std::move(identity_node_output_info));
    } else {
      graph_outputs.push_back(std::move(node_output_info));
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GraphImpl::CompileOnBackgroundThread,
                     std::move(graph_outputs), std::move(graph_builder)),
      base::BindOnce(&GraphImpl::OnCompilationComplete, std::move(callback),
                     std::move(command_recorder),
                     std::move(graph_info->constant_id_to_buffer_map),
                     std::move(input_buffer_binding_info),
                     std::make_unique<ComputeResourceInfo>(graph_info)));
}

void GraphImpl::ComputeImpl(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  // Create the input resource binding for graph execution. Only the input
  // tensors of graph need to be bound.
  absl::optional<base::flat_map<std::string, DML_BUFFER_BINDING>>
      input_buffer_binding = UploadAndCreateBufferBinding<std::string>(
          command_recorder_.get(), named_inputs);
  if (!input_buffer_binding) {
    DLOG(ERROR) << "Failed to upload input buffers";
    std::move(callback).Run(mojom::ComputeResult::kUnknownError, absl::nullopt);
    return;
  }

  // TODO(crbug.com/1273291): Execute the compiled operator with inputs/outputs.
  std::move(callback).Run(mojom::ComputeResult::kUnknownError, absl::nullopt);
}

}  // namespace webnn::dml
