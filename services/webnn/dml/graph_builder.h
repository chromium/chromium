// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_GRAPH_BUILDER_H_
#define SERVICES_WEBNN_DML_GRAPH_BUILDER_H_

#include <DirectML.h>
#include <list>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "services/webnn/dml/tensor_desc.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class InputNode;
class OperatorNode;

// Represents a node, which is either an input node or operator node, within a
// graph.
class Node {
 public:
  enum class Type {
    kInput,
    kOperator,
  };

  explicit Node(Type type);
  virtual ~Node();

  // Not copyable or movable.
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  Node(Node&&) = delete;
  Node& operator=(Node&&) = delete;

  Type GetType() const;

  const InputNode* AsInputNode() const;
  const OperatorNode* AsOperatorNode() const;

 protected:
  Type type_;
};

// Represents a graph input node. Created by `GraphBuilder::CreateInputNode()`.
// Holds the graph input index which is used to set
// `DML_INPUT_GRAPH_EDGE_DESC::GraphInputIndex`.
class InputNode final : public Node {
 public:
  explicit InputNode(uint32_t graph_input_index);
  ~InputNode() override;

  uint32_t GetGraphInputIndex() const;

 private:
  uint32_t graph_input_index_ = 0;
};

// Represents a graph operator node. Created by
// `GraphBuilder::CreateOperatorNode()`. Holds the node index and DirectML
// operator.
//
// The node index is increased from 0 when a new operator node is created. The
// node index is used to identify an operator node when creating DirectML graph
// edge structures, e.g. `FromNodeIndex` or `ToNodeIndex` of
// `DML_INTERMEDIATE_GRAPH_EDGE_DESC`. The operator nodes should be kept in the
// same order when creating `DML_GRAPH_DESC::Nodes`.
class OperatorNode final : public Node {
 public:
  OperatorNode(uint32_t operator_index, ComPtr<IDMLOperator> dml_operator);
  ~OperatorNode() override;

  uint32_t GetNodeIndex() const;
  const DML_OPERATOR_GRAPH_NODE_DESC& GetDMLOperatorNodeDesc() const;

 private:
  uint32_t node_index_ = 0;
  ComPtr<IDMLOperator> dml_operator_;
  DML_OPERATOR_GRAPH_NODE_DESC dml_operator_node_desc_;
};

// Represents an output (edge) of a node. Created by
// `GraphBuilder::CreateNodeOutput`. Holds the index and tensor description of
// this node output.
//
// The output index is used to identity the node output when creating DirectML
// graph edge structures, e.g., `FromNodeOutputIndex` of
// `DML_INTERMEDIATE_GRAPH_EDGE_DESC`.
class NodeOutput {
 public:
  NodeOutput(const Node& node, uint32_t output_index, TensorDesc tensor_desc);
  ~NodeOutput();

  // Not copyable or movable.
  NodeOutput(const NodeOutput&) = delete;
  NodeOutput& operator=(const NodeOutput&) = delete;
  NodeOutput(NodeOutput&&) = delete;
  NodeOutput& operator=(NodeOutput&&) = delete;

  const Node& GetNode() const;
  uint32_t GetOutputIndex() const;
  const TensorDesc& GetTensorDesc() const;

 private:
  // The node that provides the node output.
  const raw_ref<const Node> node_;
  // An operator node may have multiple outputs. This output index identifies
  // which one of the operator node's outputs this NodeOutput represents. It
  // ranges from 0 to node output count - 1. It would be used by DirectML
  // internally. For example, as the split operator described by
  // DML_SPLIT_OPERATOR_DESC:
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_split_operator_desc,
  // if the output count is 3, the output index is in range [0, 2].
  uint32_t output_index_ = 0;
  TensorDesc tensor_desc_;
};

// GraphBuilder is a helper class to build a DML graph. It provides methods to
// create the input nodes, operator nodes and connect these nodes. The input
// edges and intermediate edges are created when connecting nodes, and the
// output edges are created at last to indicate which node's output is graph's
// output.
class GraphBuilder final {
 public:
  explicit GraphBuilder(ComPtr<IDMLDevice> device);

  GraphBuilder(const GraphBuilder& other) = delete;
  GraphBuilder& operator=(const GraphBuilder& other) = delete;

  GraphBuilder(GraphBuilder&& other);
  GraphBuilder& operator=(GraphBuilder&& other);

  ~GraphBuilder();

  // Create a constant or non-constant input node stored in
  // `GraphBuilder::input_nodes_` and returns its pointer.
  const InputNode* CreateInputNode();

  // Create the IDMLOperator for the DML graph, meanwhile, connect multiple node
  // outputs to one node, thus the corresponding input edges and intermediate
  // edges are created.
  // It's expected to pass an operator desc pointer to parameter 'void*
  // operator_desc' which depends on the DML_OPERATOR_TYPE.
  //
  // When creation of IDMLOperator succeeds, it creates an operator node
  // stored in `GraphBuilder::operator_nodes_` and returns its pointer. When it
  // fails to create IDMLOperator, a nullptr is returned.
  const OperatorNode* CreateOperatorNode(DML_OPERATOR_TYPE type,
                                         const void* operator_desc,
                                         base::span<const NodeOutput*> inputs);

  // Create a node output stored in `GraphBuilder::node_outputs_` and return its
  // pointer.
  const NodeOutput* CreateNodeOutput(const Node* node,
                                     TensorDesc tensor_desc,
                                     uint32_t output_index = 0);

  // Create an output edge for a node output, return the graph's output index.
  uint32_t CreateOutputEdge(const NodeOutput* node_output);

  // Notice that IDMLDevice1::CompileGraph may take a long time to compile
  // shaders (if not cached before), so this method may block the current
  // thread. Consider posting this method to the thread pool to avoid blocking.
  ComPtr<IDMLCompiledOperator> Compile(DML_EXECUTION_FLAGS flags) const;

 private:
  ComPtr<IDMLDevice> dml_device_;

  std::vector<DML_INPUT_GRAPH_EDGE_DESC> dml_input_edges_;
  std::vector<DML_INTERMEDIATE_GRAPH_EDGE_DESC> dml_intermediate_edges_;
  std::vector<DML_OUTPUT_GRAPH_EDGE_DESC> dml_output_edges_;

  // `std::list` never invalidates the pointers to its elements.
  std::list<InputNode> input_nodes_;
  std::list<OperatorNode> operator_nodes_;
  std::list<NodeOutput> node_outputs_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_GRAPH_BUILDER_H_
