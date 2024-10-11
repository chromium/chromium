// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_GRAPH_BUILDER_DML_H_
#define SERVICES_WEBNN_DML_GRAPH_BUILDER_DML_H_

#include <list>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "services/webnn/dml/tensor_desc.h"
#include "services/webnn/webnn_constant_operand.h"
#include "third_party/microsoft_dxheaders/include/directml.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::dml {

class InputNode;
class GraphNode;

// Represents a node, which is either an input node or graph node, within
// a graph.
class COMPONENT_EXPORT(WEBNN_SERVICE) Node {
 public:
  enum class Type {
    kInput,
    kGraph,
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
  const GraphNode* AsGraphNode() const;

 protected:
  Type type_;
};

// Represents an input node. Created by `GraphBuilderDml::CreateInputNode()`.
// Holds the graph input index which is used to set
// `DML_INPUT_GRAPH_EDGE_DESC::GraphInputIndex`.
class InputNode final : public Node {
 public:
  explicit InputNode(uint32_t graph_input_index);
  ~InputNode() override;

  uint32_t GetGraphInputIndex() const;

 private:
  const uint32_t graph_input_index_;
};

// Represents a graph node. Created by `CreateOperatorNode()` or
// `CreateConstantNode()`.
// The node index is increased from 0 when a new graph node is created. It's
// used to identify a graph node when creating DirectML graph edge structures,
// e.g. `FromNodeIndex` or `ToNodeIndex` of `DML_INTERMEDIATE_GRAPH_EDGE_DESC`.
// The graph nodes should be kept in the same order when creating
// `DML_GRAPH_DESC::Nodes`.
class GraphNode : public Node {
 public:
  explicit GraphNode(uint32_t node_index);
  ~GraphNode() override;

  uint32_t GetNodeIndex() const;
  virtual DML_GRAPH_NODE_DESC GetDMLGraphNodeDesc() const = 0;

 private:
  const uint32_t node_index_;
};

// Represents an output (edge) of a node. Created by
// `GraphBuilderDml::CreateNodeOutput`. Holds the index and tensor description
// of this node output.
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

// GraphBuilderDml is a helper class to build a DML graph. It provides methods
// to create the input nodes, operator nodes and connect these nodes. The input
// edges and intermediate edges are created when connecting nodes, and the
// output edges are created at last to indicate which node's output is graph's
// output.
class COMPONENT_EXPORT(WEBNN_SERVICE) GraphBuilderDml final {
 public:
  explicit GraphBuilderDml(Microsoft::WRL::ComPtr<IDMLDevice1> device);

  GraphBuilderDml(const GraphBuilderDml& other) = delete;
  GraphBuilderDml& operator=(const GraphBuilderDml& other) = delete;

  GraphBuilderDml(GraphBuilderDml&& other);
  GraphBuilderDml& operator=(GraphBuilderDml&& other);

  ~GraphBuilderDml();

  // Create a constant or non-constant input node stored in
  // `GraphBuilderDml::input_nodes_` and returns its pointer.
  const InputNode* CreateInputNode();

  // Create the IDMLOperator for the DML graph, meanwhile, connect multiple node
  // outputs to one node, thus the corresponding input edges and intermediate
  // edges are created.
  // It's expected to pass an operator desc pointer to parameter 'void*
  // operator_desc' which depends on the DML_OPERATOR_TYPE.
  // The input node output can be a nullptr when no edge needs to be
  // created for this input. For example, given an operator with three optional
  // inputs, `inputs = [input1, nullptr, input3]` means that the second input
  // doesn't have an edge and should be skipped.
  // TODO(crbug.com/330051532): change `inputs` to a map indexed explicitly by
  // input index.
  //
  // When creation of IDMLOperator succeeds, it creates an operator node
  // stored in `GraphBuilderDml::graph_nodes_` and returns its pointer.
  const GraphNode* CreateOperatorNode(DML_OPERATOR_TYPE type,
                                      const void* operator_desc,
                                      base::span<const NodeOutput*> inputs,
                                      std::string_view label);

  // Create a constant node stored in `GraphBuilderDml::graph_nodes_` and
  // return its pointer.
  const GraphNode* CreateConstantNode(
      std::unique_ptr<WebNNConstantOperand> constant_operand);

  // Create a node output stored in `GraphBuilderDml::node_outputs_` and return
  // its pointer.
  const NodeOutput* CreateNodeOutput(const Node* node,
                                     TensorDesc tensor_desc,
                                     uint32_t output_index = 0);

  // Create an output edge for a node output, return the graph's output index.
  uint32_t CreateOutputEdge(const NodeOutput* node_output);

  // Notice that IDMLDevice1::CompileGraph may take a long time to compile
  // shaders (if not cached before), so this method should be called on a
  // background thread to avoid blocking the current thread.
  base::expected<Microsoft::WRL::ComPtr<IDMLCompiledOperator>, HRESULT> Compile(
      DML_EXECUTION_FLAGS flags) const;

 private:
  Microsoft::WRL::ComPtr<IDMLDevice1> dml_device_;

  std::vector<DML_INPUT_GRAPH_EDGE_DESC> dml_input_edges_;
  std::vector<DML_INTERMEDIATE_GRAPH_EDGE_DESC> dml_intermediate_edges_;
  std::vector<DML_OUTPUT_GRAPH_EDGE_DESC> dml_output_edges_;

  // `std::list` never invalidates the pointers to its elements.
  std::list<InputNode> input_nodes_;
  std::vector<std::unique_ptr<GraphNode>> graph_nodes_;
  std::list<NodeOutput> node_outputs_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_GRAPH_BUILDER_DML_H_
