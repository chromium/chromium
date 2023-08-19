// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/graph_builder.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "services/webnn/dml/error.h"

namespace webnn::dml {

GraphBuilder::GraphBuilder(ComPtr<IDMLDevice> dml_device)
    : dml_device_(std::move(dml_device)) {}

GraphBuilder::GraphBuilder(GraphBuilder&& other) = default;
GraphBuilder& GraphBuilder::operator=(GraphBuilder&& other) = default;

GraphBuilder::~GraphBuilder() = default;

NodeInfo GraphBuilder::CreateInputNode() {
  // The input index should increase from 0 as the input is added.
  return {NodeInfo::Type::kInput, input_count_++};
}

const NodeOutput& GraphBuilder::GetNodeOutput(
    const NodeOutputInfo& node_output_info) const {
  CHECK_LT(node_output_info.index,
           base::checked_cast<uint32_t>(node_outputs_.size()));
  return node_outputs_[node_output_info.index];
}

NodeInfo GraphBuilder::CreateOperatorNode(
    DML_OPERATOR_TYPE type,
    const void* operator_desc,
    const std::vector<NodeOutputInfo>& node_output_infos) {
  DML_OPERATOR_DESC op_desc = {type, operator_desc};
  Microsoft::WRL::ComPtr<IDMLOperator> dml_operator;
  HRESULT hr =
      dml_device_->CreateOperator(&op_desc, IID_PPV_ARGS(&dml_operator));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create dml operator : "
                << logging::SystemErrorCodeToString(hr);
    return {NodeInfo::Type::kInvalid, 0};
  }

  // Create the operator node. The node index is increased as the operator node
  // is added.
  uint32_t index = base::checked_cast<uint32_t>(dml_operators_.size());
  NodeInfo node_info = {NodeInfo::Type::kOperator, index};

  dml_operators_.push_back(std::move(dml_operator));
  DML_OPERATOR_GRAPH_NODE_DESC dml_node_desc = {
      .Operator = dml_operators_.back().Get()};
  dml_nodes_.push_back(std::move(dml_node_desc));

  // Connect multiple node outputs to one node to create the input edges and
  // intermediate edges.
  for (uint32_t input_index = 0;
       input_index < base::checked_cast<uint32_t>(node_output_infos.size());
       ++input_index) {
    NodeOutput node_output = GetNodeOutput(node_output_infos[input_index]);
    NodeInfo from_node_info = node_output.node_info;
    if (from_node_info.type == NodeInfo::Type::kInput) {
      DML_INPUT_GRAPH_EDGE_DESC input_edge{
          .GraphInputIndex = from_node_info.index,
          .ToNodeIndex = node_info.index,
          .ToNodeInputIndex = input_index};

      dml_input_edges_.push_back(std::move(input_edge));
    } else if (from_node_info.type == NodeInfo::Type::kOperator) {
      DML_INTERMEDIATE_GRAPH_EDGE_DESC intermediate_edge{
          .FromNodeIndex = from_node_info.index,
          .FromNodeOutputIndex = node_output.output_index,
          .ToNodeIndex = node_info.index,
          .ToNodeInputIndex = input_index};

      dml_intermediate_edges_.push_back(std::move(intermediate_edge));
    } else {
      NOTREACHED_NORETURN();
    }
  }

  return node_info;
}

NodeOutputInfo GraphBuilder::CreateNodeOutput(const NodeInfo& node_info,
                                              TensorDesc tensor,
                                              uint32_t output_index) {
  CHECK_NE(node_info.type, NodeInfo::Type::kInvalid);
  node_outputs_.push_back(
      NodeOutput{node_info, output_index, std::move(tensor)});
  // The node output index is increased as the node output is added.
  return {base::checked_cast<uint32_t>(node_outputs_.size() - 1)};
}

ComPtr<IDMLCompiledOperator> GraphBuilder::Compile(
    const std::vector<NodeOutputInfo>& node_output_infos,
    DML_EXECUTION_FLAGS flags) const {
  ComPtr<IDMLCompiledOperator> compiled_operator;
  // If there is only one operator node in the graph, just compile the operator
  // and return the compiled operator.
  if (dml_operators_.size() == 1) {
    RETURN_NULL_IF_FAILED(dml_device_->CompileOperator(
        dml_operators_[0].Get(), flags, IID_PPV_ARGS(&compiled_operator)));
    return compiled_operator;
  }

  // Create output edges with node outputs.
  size_t outputs_count = node_output_infos.size();
  std::vector<DML_OUTPUT_GRAPH_EDGE_DESC> output_edges(outputs_count);
  for (size_t index = 0; index < outputs_count; ++index) {
    NodeOutput node_output = GetNodeOutput(node_output_infos[index]);
    DML_OUTPUT_GRAPH_EDGE_DESC output_edge = {
        .FromNodeIndex = node_output.node_info.index,
        .FromNodeOutputIndex = node_output.output_index,
        .GraphOutputIndex = base::checked_cast<uint32_t>(index)};
    output_edges[index] = std::move(output_edge);
  }

  std::vector<DML_GRAPH_NODE_DESC> dml_nodes(dml_nodes_.size());
  for (size_t i = 0; i < dml_nodes.size(); ++i) {
    dml_nodes[i] = {DML_GRAPH_NODE_TYPE_OPERATOR, &dml_nodes_[i]};
  }

  std::vector<DML_GRAPH_EDGE_DESC> dml_input_edges(dml_input_edges_.size());
  for (size_t i = 0; i < dml_input_edges.size(); ++i) {
    dml_input_edges[i] = {DML_GRAPH_EDGE_TYPE_INPUT, &dml_input_edges_[i]};
  }

  std::vector<DML_GRAPH_EDGE_DESC> dml_intermediate_edges(
      dml_intermediate_edges_.size());
  for (size_t i = 0; i < dml_intermediate_edges.size(); ++i) {
    dml_intermediate_edges[i] = {DML_GRAPH_EDGE_TYPE_INTERMEDIATE,
                                 &dml_intermediate_edges_[i]};
  }

  std::vector<DML_GRAPH_EDGE_DESC> dml_output_edges(output_edges.size());
  for (size_t i = 0; i < dml_output_edges.size(); ++i) {
    dml_output_edges[i] = {DML_GRAPH_EDGE_TYPE_OUTPUT, &output_edges[i]};
  }

  DML_GRAPH_DESC dml_graph_desc = {
      .InputCount = input_count_,
      .OutputCount = base::checked_cast<uint32_t>(outputs_count),
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

  RETURN_NULL_IF_FAILED(dml_device1->CompileGraph(
      &dml_graph_desc, flags, IID_PPV_ARGS(&compiled_operator)));
  return compiled_operator;
}

}  // namespace webnn::dml
