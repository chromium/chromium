// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/graph_impl.h"

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/graph_builder.h"
#include "services/webnn/dml/tensor_desc.h"
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

// Define some methods like CreateInputNode and CreateOperatorNodeForRelu here
// to focus on converting the mojo graph struct to corresponding DML graph node
// by using dml::GraphBuilder as a helper. dml::GraphBuilder should be decoupled
// from mojo graph structs and focus on manipulating DML graph structs.
void CreateInputNode(const IdToOperandMap& id_to_operand_map,
                     uint64_t input_id,
                     GraphBuilder& graph_builder,
                     IdToNodeOutputMap& id_to_node_output_map) {
  const OperandPtr& operand = id_to_operand_map.at(input_id);
  TensorDesc input_tensor_desc(GetTensorDataType(operand->data_type),
                               operand->dimensions);
  NodeInfo input_node = graph_builder.CreateInputNode();
  NodeOutputInfo input_node_output = graph_builder.CreateNodeOutput(
      std::move(input_node), std::move(input_tensor_desc));
  id_to_node_output_map[input_id] = std::move(input_node_output);
}

void CreateOperatorNodeForRelu(const IdToOperandMap& id_to_operand_map,
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
  NodeOutputInfo relu_output =
      graph_builder.CreateNodeOutput(relu_node, std::move(output_tensor_desc));
  id_to_node_output_map[output_id] = std::move(relu_output);
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

}  // namespace

GraphImpl::GraphImpl(std::unique_ptr<CommandRecorder> command_recorder,
                     ComPtr<ID3D12Resource> persistent_buffer,
                     ComPtr<IDMLCompiledOperator> compiled_operator)
    : persistent_buffer_(std::move(persistent_buffer)),
      command_recorder_(std::move(command_recorder)),
      compiled_operator_(std::move(compiled_operator)) {}

//  Notice that it's the CommandQueue's responsibility to wait for all of the
//  queued work to complete before destructing itself.
GraphImpl::~GraphImpl() = default;

// Static
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
  // Add inputs.
  for (auto& input_id : graph_info->input_operands) {
    CreateInputNode(id_to_operand_map, input_id, graph_builder,
                    id_to_node_output_map);
  }

  // TODO(crbug.com/1455278): Add constants. It depends on the mojo constant
  // definition is ready.

  // Add operations.
  for (auto& operation : graph_info->operators) {
    switch (operation->kind) {
      case Operator::Kind::kRelu: {
        CreateOperatorNodeForRelu(id_to_operand_map, operation, graph_builder,
                                  id_to_node_output_map);
        break;
      }
      case Operator::Kind::kReshape: {
        CreateNodeOutputForReshape(id_to_operand_map, operation, graph_builder,
                                   id_to_node_output_map);
        break;
      }
      default:
        DLOG(ERROR) << "This operator kind (" +
                           OpKindToString(operation->kind) +
                           ") is not supported.";
        std::move(callback).Run(mojo::NullRemote());
        return;
    }
  }

  std::vector<NodeOutputInfo> graph_outputs;
  graph_outputs.reserve(graph_info->output_operands.size());
  for (auto& output_id : graph_info->output_operands) {
    const auto output_iterator = id_to_node_output_map.find(output_id);
    CHECK(output_iterator != id_to_node_output_map.end());
    NodeOutputInfo node_output = output_iterator->second;

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
    NodeOutput output_node = graph_builder.GetNodeOutput(node_output);
    TensorDesc output_tensor_desc = output_node.tensor_desc;
    auto output_type = output_node.node_info.type;
    if (output_type == NodeInfo::Type::kInput) {
      TensorDesc identity_tensor_desc(output_tensor_desc.GetDataType(),
                                      DML_TENSOR_FLAG_NONE,
                                      output_tensor_desc.GetDimensions());
      DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC identity_operator_desc{
          .InputTensor = &output_tensor_desc.GetDMLTensorDesc(),
          .OutputTensor = &identity_tensor_desc.GetDMLTensorDesc()};
      NodeInfo identity_node = graph_builder.CreateOperatorNode(
          DML_OPERATOR_ELEMENT_WISE_IDENTITY, &identity_operator_desc,
          {node_output});
      NodeOutputInfo identity_node_output = graph_builder.CreateNodeOutput(
          identity_node, std::move(identity_tensor_desc));
      graph_outputs.push_back(std::move(identity_node_output));
    } else {
      graph_outputs.push_back(std::move(node_output));
    }
  }

  // TODO(crbug.com/1273291): This method compiles all DML operators into an
  // IDMLCompiledOperator which can be dispatched to GPU. It's a time-consuming
  // method, so consider posting it to other threads rather than calling it in
  // GPU main thread to avoid blocking.
  ComPtr<IDMLCompiledOperator> compiled_operator =
      graph_builder.Compile(graph_outputs, DML_EXECUTION_FLAG_NONE);
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

  // TODO(crbug.com/1273291): Create the input resource binding for
  // operator initialization. Only the constant resource needs to be bound.

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

  hr = command_recorder->InitializeOperator(
      compiled_operator.Get(), absl::nullopt, persistent_buffer_binding_desc);
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

  // Ensure the GPU resources needed by the initialization work on the
  // CommandQueue not to be released before the work completes.
  if (!persistent_buffer) {
    command_queue->ReferenceUntilCompleted(persistent_buffer);
  }
  //  The IDMLCompiledOperator should also be referenced before the work
  //  completes.
  command_queue->ReferenceUntilCompleted(compiled_operator);

  hr = command_queue->WaitAsync(
      base::BindOnce(&GraphImpl::OnWaitForBuildSignal,
                     std::move(command_recorder), std::move(persistent_buffer),
                     std::move(compiled_operator), std::move(callback)));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to wait the initialization completed: "
                << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(mojo::NullRemote());
    return;
  }
}

// static
void GraphImpl::OnWaitForBuildSignal(
    std::unique_ptr<CommandRecorder> command_recorder,
    ComPtr<ID3D12Resource> persistent_buffer,
    ComPtr<IDMLCompiledOperator> compiled_operator,
    mojom::WebNNContext::CreateGraphCallback callback) {
  scoped_refptr<CommandQueue> command_queue(
      command_recorder->GetCommandQueue());
  // The remote sent to the renderer.
  mojo::PendingRemote<mojom::WebNNGraph> blink_remote;
  // The receiver bound to GraphImpl.
  mojo::MakeSelfOwnedReceiver<mojom::WebNNGraph>(
      base::WrapUnique(new GraphImpl(std::move(command_recorder),
                                     std::move(persistent_buffer),
                                     std::move(compiled_operator))),
      blink_remote.InitWithNewPipeAndPassReceiver());
  command_queue->ReleaseCompletedResources();
  std::move(callback).Run(std::move(blink_remote));
}

}  // namespace webnn::dml
