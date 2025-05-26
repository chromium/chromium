// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
#define SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_

#include <memory>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/strings/cstring_view.h"
#include "base/types/expected.h"
#include "services/webnn/ort/model_editor.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"

namespace webnn {

class WebNNConstantOperand;

namespace ort {

// This class converts WebNN graph to ORT model.
//
// The instances of the class may not be allocated on the heap, but as a member
// variable of a non-stack-allocated class and be single-use per conversion.
class GraphBuilderOrt {
  STACK_ALLOCATED();

 public:
  // Factory method that creates a `GraphBuilderOrt`, builds the graph and
  // returns `ModelEditor::ModelInfo` which contains the model itself and the
  // external data (weights).
  //
  // Returns unexpected if it fails.
  [[nodiscard]] base::expected<std::unique_ptr<ModelEditor::ModelInfo>,
                               mojom::ErrorPtr>
  CreateAndBuild(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands);

  GraphBuilderOrt(const GraphBuilderOrt&) = delete;
  GraphBuilderOrt& operator=(const GraphBuilderOrt&) = delete;

  ~GraphBuilderOrt();

 private:
  GraphBuilderOrt(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands);

  const mojom::Operand& GetOperand(OperandId operand_id) const;

  // Get the name of an existing operand by its id.
  std::string GetOperandNameById(OperandId operand_id) const;

  // Generate a unique name for a newly created operation by combining
  // `label` and `next_operation_id_`. ORT model doesn't allow duplicate
  // names.
  std::string GenerateOperationName(std::string_view label);

  template <typename T>
  void AddBinaryOperation(const T& operation, base::cstring_view op_type);
  template <typename T>
  void AddUnaryOperation(const T& operation, base::cstring_view op_type);

  void AddCastOperation(const mojom::ElementWiseUnary& cast);
  void AddElementWiseBinaryOperation(
      const mojom::ElementWiseBinary& element_wise_binary);
  void AddElementWiseUnaryOperation(
      const mojom::ElementWiseUnary& element_wise_unary);

  [[nodiscard]] base::expected<std::unique_ptr<ModelEditor::ModelInfo>,
                               mojom::ErrorPtr>
  BuildModel();

  // An increasing id starting from 0, used for generating unique names for each
  // operation.
  OperationId next_operation_id_ = 0;

  // A reference to the WebNN compute graph that `this` instance is converting
  // to ORT model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
      constant_operands_;

  const ContextProperties context_properties_;

  ModelEditor model_editor_;
};

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
