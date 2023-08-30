// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_GRAPH_BUILDER_H_
#define SERVICES_WEBNN_DML_GRAPH_BUILDER_H_

#include <DirectML.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "services/webnn/dml/tensor_desc.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

// It represents the info of a node.
struct NodeInfo {
  enum class Type {
    kInvalid,
    kInput,
    kOperator,
  };

  Type type = Type::kInvalid;
  // For NodeType::kInput, it indicates the graph's input index within
  // GraphBuilder::input_count_ and is counted from 0;
  // For NodeType::kOperator, it indicates the dml operator location in
  // GraphBuilder::dml_operators_ and is counted from 0.
  uint32_t index = 0;
};

// It represents the info of a node output.
struct NodeOutputInfo {
  // It indicates the NodeOutput location in GraphBuilder::node_outputs_ and is
  // counted from 0.
  uint32_t index = 0;
};

// NodeOutput is created from a node, it represent an output of this node. It
// mainly consists of the output index and the output tensor of the node.
struct NodeOutput final {
  // The node info that provides the node output.
  NodeInfo node_info = {};
  // An operator node may have multiple outputs. This output index identifies
  // which one of the operator node's outputs this NodeOutput represents. It
  // ranges from 0 to node output count - 1. It would be used by DirectML
  // internally. For example, as the split operator described by
  // DML_SPLIT_OPERATOR_DESC:
  // https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_split_operator_desc,
  // if the output count is 3, the output index is in range [0, 2].
  uint32_t output_index = 0;
  TensorDesc tensor_desc;
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

  // Create constant and non-constant input nodes for the DML graph.
  NodeInfo CreateInputNode();

  // Create the IDMLOperator for the DML graph, meanwhile, connect multiple node
  // outputs to one node, thus the corresponding input edges and intermediate
  // edges are created.
  // It's expected to pass an operator desc pointer to parameter 'void*
  // operator_desc' which depends on the DML_OPERATOR_TYPE.
  //
  // Attention: No guarantee that the operator node will be created
  // successfully, so the returned NodeInfo must be checked for validity. It
  // returns an invalid NodeInfo whose type equals to NodeInfo::Type::kInvalid
  // when it fails to build an operator node.
  NodeInfo CreateOperatorNode(
      DML_OPERATOR_TYPE type,
      const void* operator_desc,
      const std::vector<NodeOutputInfo>& node_output_infos);

  // Create a node output stored in GraphBuilder::node_outputs_ and return its'
  // location index in NodeOutputInfo.
  NodeOutputInfo CreateNodeOutput(const NodeInfo& node_info,
                                  TensorDesc tensor,
                                  uint32_t output_index = 0);

  // For single operator graph, it just calls IDMLDevice::CompileOperator() with
  // most widely Windows versions support.
  // For multiple operators graph, it firstly queries whether IDMLDevice1 is
  // available, if it is, it calls IDMLDevice1::CompileGraph().
  // Also notice that IDMLDevice1::CompileGraph takes long time to compile
  // shaders (if not cached before), so this method may block current thread.
  // Consider posting this method to thread pool to avoid blocking.
  ComPtr<IDMLCompiledOperator> Compile(
      const std::vector<NodeOutputInfo>& node_output_infos,
      DML_EXECUTION_FLAGS flags) const;

  const NodeOutput& GetNodeOutput(const NodeOutputInfo& node_output_info) const;

 private:
  std::vector<DML_OPERATOR_GRAPH_NODE_DESC> dml_nodes_;
  std::vector<DML_INPUT_GRAPH_EDGE_DESC> dml_input_edges_;
  std::vector<DML_INTERMEDIATE_GRAPH_EDGE_DESC> dml_intermediate_edges_;
  // IDMLOperator is referenced by DML_OPERATOR_GRAPH_NODE_DESC. It should
  // outlive the DML_OPERATOR_GRAPH_NODE_DESC.
  std::vector<ComPtr<IDMLOperator>> dml_operators_;
  ComPtr<IDMLDevice> dml_device_;

  uint32_t input_count_ = 0;
  std::vector<NodeOutput> node_outputs_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_GRAPH_BUILDER_H_
