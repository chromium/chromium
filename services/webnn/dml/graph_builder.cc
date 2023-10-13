// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/graph_builder.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "services/webnn/dml/error.h"

namespace webnn::dml {

Node::Node(Type type) : type_(type) {}
Node::~Node() = default;

Node::Type Node::GetType() const {
  return type_;
}

const InputNode* Node::AsInputNode() const {
  CHECK_EQ(GetType(), Node::Type::kInput);
  return static_cast<const InputNode*>(this);
}

const OperatorNode* Node::AsOperatorNode() const {
  CHECK_EQ(GetType(), Node::Type::kOperator);
  return static_cast<const OperatorNode*>(this);
}

InputNode::InputNode(uint32_t graph_input_index)
    : Node(Node::Type::kInput), graph_input_index_(graph_input_index) {}

InputNode::~InputNode() = default;

uint32_t InputNode::GetGraphInputIndex() const {
  CHECK_EQ(type_, Node::Type::kInput);
  return graph_input_index_;
}

OperatorNode::OperatorNode(uint32_t node_index,
                           ComPtr<IDMLOperator> dml_operator)
    : Node(Node::Type::kOperator),
      node_index_(node_index),
      dml_operator_(std::move(dml_operator)) {
  dml_operator_node_desc_ =
      DML_OPERATOR_GRAPH_NODE_DESC{.Operator = dml_operator_.Get()};
}

OperatorNode::~OperatorNode() = default;

uint32_t OperatorNode::GetNodeIndex() const {
  CHECK_EQ(type_, Node::Type::kOperator);
  return node_index_;
}

const DML_OPERATOR_GRAPH_NODE_DESC& OperatorNode::GetDMLOperatorNodeDesc()
    const {
  CHECK_EQ(type_, Node::Type::kOperator);
  return dml_operator_node_desc_;
}

NodeOutput::NodeOutput(const Node& node,
                       uint32_t output_index,
                       TensorDesc tensor_desc)
    : node_(node),
      output_index_(output_index),
      tensor_desc_(std::move(tensor_desc)) {}

NodeOutput::~NodeOutput() = default;

const Node& NodeOutput::GetNode() const {
  return node_.get();
}

uint32_t NodeOutput::GetOutputIndex() const {
  return output_index_;
}

const TensorDesc& NodeOutput::GetTensorDesc() const {
  return tensor_desc_;
}

GraphBuilder::GraphBuilder(ComPtr<IDMLDevice> dml_device)
    : dml_device_(std::move(dml_device)) {}

GraphBuilder::GraphBuilder(GraphBuilder&& other) = default;
GraphBuilder& GraphBuilder::operator=(GraphBuilder&& other) = default;

GraphBuilder::~GraphBuilder() = default;

const InputNode* GraphBuilder::CreateInputNode() {
  const uint32_t graph_input_index =
      base::checked_cast<uint32_t>(input_nodes_.size());
  input_nodes_.emplace_back(graph_input_index);
  return &input_nodes_.back();
}

const OperatorNode* GraphBuilder::CreateOperatorNode(
    DML_OPERATOR_TYPE type,
    const void* operator_desc,
    base::span<const NodeOutput*> inputs) {
  DML_OPERATOR_DESC op_desc{.Type = type, .Desc = operator_desc};
  ComPtr<IDMLOperator> dml_operator;
  RETURN_NULL_IF_FAILED(
      dml_device_->CreateOperator(&op_desc, IID_PPV_ARGS(&dml_operator)));

  uint32_t operator_node_index =
      base::checked_cast<uint32_t>(operator_nodes_.size());
  operator_nodes_.emplace_back(operator_node_index, std::move(dml_operator));
  const OperatorNode* operator_node = &operator_nodes_.back();

  // Connect input node outputs to this operator node that creates the input
  // edges and intermediate edges.
  for (uint32_t node_input_index = 0;
       node_input_index < base::checked_cast<uint32_t>(inputs.size());
       ++node_input_index) {
    const NodeOutput* operator_input = inputs[node_input_index];
    CHECK(operator_input);
    const Node& from_node = operator_input->GetNode();
    switch (from_node.GetType()) {
      case Node::Type::kInput: {
        const InputNode* from_input_node = from_node.AsInputNode();
        DML_INPUT_GRAPH_EDGE_DESC input_edge{
            .GraphInputIndex = from_input_node->GetGraphInputIndex(),
            .ToNodeIndex = operator_node->GetNodeIndex(),
            .ToNodeInputIndex = node_input_index};
        dml_input_edges_.push_back(std::move(input_edge));
        break;
      }
      case Node::Type::kOperator: {
        const OperatorNode* from_operator_node = from_node.AsOperatorNode();
        DML_INTERMEDIATE_GRAPH_EDGE_DESC intermediate_edge{
            .FromNodeIndex = from_operator_node->GetNodeIndex(),
            .FromNodeOutputIndex = operator_input->GetOutputIndex(),
            .ToNodeIndex = operator_node->GetNodeIndex(),
            .ToNodeInputIndex = node_input_index};
        dml_intermediate_edges_.push_back(std::move(intermediate_edge));
        break;
      }
    }
  }

  return operator_node;
}

const NodeOutput* GraphBuilder::CreateNodeOutput(const Node* node,
                                                 TensorDesc tensor_desc,
                                                 uint32_t output_index) {
  CHECK(node);
  node_outputs_.emplace_back(*node, output_index, std::move(tensor_desc));
  return &node_outputs_.back();
}

uint32_t GraphBuilder::CreateOutputEdge(const NodeOutput* node_output) {
  CHECK(node_output);
  const OperatorNode* from_operator_node =
      node_output->GetNode().AsOperatorNode();
  uint32_t graph_output_index =
      base::checked_cast<uint32_t>(dml_output_edges_.size());
  DML_OUTPUT_GRAPH_EDGE_DESC output_edge = {
      .FromNodeIndex = from_operator_node->GetNodeIndex(),
      .FromNodeOutputIndex = node_output->GetOutputIndex(),
      .GraphOutputIndex = graph_output_index};
  dml_output_edges_.push_back(std::move(output_edge));
  return graph_output_index;
}

ComPtr<IDMLCompiledOperator> GraphBuilder::Compile(
    DML_EXECUTION_FLAGS flags) const {
  TRACE_EVENT0("gpu", "dml::GraphBuilder::Compile");
  // Ensure `dml_nodes` vector is ordered by node index of operator node.
  std::vector<DML_GRAPH_NODE_DESC> dml_nodes(operator_nodes_.size());
  for (const auto& operator_node : operator_nodes_) {
    uint32_t node_index = operator_node.GetNodeIndex();
    CHECK_LT(node_index, dml_nodes.size());
    dml_nodes[node_index] =
        DML_GRAPH_NODE_DESC{.Type = DML_GRAPH_NODE_TYPE_OPERATOR,
                            .Desc = &operator_node.GetDMLOperatorNodeDesc()};
  }

  std::vector<DML_GRAPH_EDGE_DESC> dml_input_edges(dml_input_edges_.size());
  for (size_t i = 0; i < dml_input_edges.size(); ++i) {
    dml_input_edges[i] = DML_GRAPH_EDGE_DESC{.Type = DML_GRAPH_EDGE_TYPE_INPUT,
                                             .Desc = &dml_input_edges_[i]};
  }

  std::vector<DML_GRAPH_EDGE_DESC> dml_intermediate_edges(
      dml_intermediate_edges_.size());
  for (size_t i = 0; i < dml_intermediate_edges.size(); ++i) {
    dml_intermediate_edges[i] =
        DML_GRAPH_EDGE_DESC{.Type = DML_GRAPH_EDGE_TYPE_INTERMEDIATE,
                            .Desc = &dml_intermediate_edges_[i]};
  }

  std::vector<DML_GRAPH_EDGE_DESC> dml_output_edges(dml_output_edges_.size());
  for (size_t i = 0; i < dml_output_edges.size(); ++i) {
    dml_output_edges[i] = DML_GRAPH_EDGE_DESC{
        .Type = DML_GRAPH_EDGE_TYPE_OUTPUT, .Desc = &dml_output_edges_[i]};
  }

  DML_GRAPH_DESC dml_graph_desc = {
      .InputCount = base::checked_cast<uint32_t>(input_nodes_.size()),
      .OutputCount = base::checked_cast<uint32_t>(dml_output_edges_.size()),
      .NodeCount = base::checked_cast<uint32_t>(dml_nodes.size()),
      .Nodes = dml_nodes.data(),
      .InputEdgeCount = base::checked_cast<uint32_t>(dml_input_edges.size()),
      .InputEdges = dml_input_edges.data(),
      .OutputEdgeCount = base::checked_cast<uint32_t>(dml_output_edges.size()),
      .OutputEdges = dml_output_edges.data(),
      .IntermediateEdgeCount =
          base::checked_cast<uint32_t>(dml_intermediate_edges.size()),
      .IntermediateEdges = dml_intermediate_edges.data()};

  ComPtr<IDMLDevice1> dml_device1;
  RETURN_NULL_IF_FAILED(
      dml_device_->QueryInterface(IID_PPV_ARGS(&dml_device1)));

  ComPtr<IDMLCompiledOperator> compiled_operator;
  RETURN_NULL_IF_FAILED(dml_device1->CompileGraph(
      &dml_graph_desc, flags, IID_PPV_ARGS(&compiled_operator)));
  return compiled_operator;
}

}  // namespace webnn::dml
