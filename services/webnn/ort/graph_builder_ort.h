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
#include "base/numerics/checked_math.h"
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

namespace internal {

// Supported tensor types for immediate values. The list can be expanded as
// needed.
template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template <typename T>
concept IsSupportedTensorType = IsAnyOf<T,
                                        float,
                                        uint16_t,
                                        int32_t,
                                        uint32_t,
                                        int64_t,
                                        uint64_t,
                                        int8_t,
                                        uint8_t>;

}  // namespace internal

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
  [[nodiscard]] static base::expected<std::unique_ptr<ModelEditor::ModelInfo>,
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

  // Generate the unique name of a newly created operand by combining a prefix
  // "inserted" and `next_operand_id_`, and then increase `next_operand_id_`.
  std::string GenerateOperandName();

  // Generate a unique name for a newly created node by combining `label` and
  // `next_operation_id_`. ORT model doesn't allow duplicate names.
  std::string GenerateNodeName(std::string_view label);

  // Create a new initializer for the graph with the given shape and data,
  // returning the name of the initializer.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  std::string CreateInitializer(base::span<const int64_t> shape,
                                base::span<const DataType> data);

  // A helper method wrapping the `CreateInitializer` above. It creates a
  // scalar initializer with the given scalar value (tensor of empty shape) to
  // the graph, returning the name of the initializer.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  std::string CreateScalarInitializer(const DataType& value);

  // A helper method creating an int64 tensor with the given shape value.
  // It can be used by `reshape` and `expand` to create an initializer that
  // specifies the output's shape.
  std::string CreateInitializerForShape(base::span<const uint32_t> shape);

  void AddCastNode(base::cstring_view name,
                   base::cstring_view input,
                   base::cstring_view output,
                   OperandDataType to_data_type);
  void InsertCastNode(base::cstring_view input,
                      base::cstring_view output,
                      OperandDataType to_data_type);

  void AddExpandNode(base::cstring_view node_name,
                     base::cstring_view input,
                     base::cstring_view output,
                     base::span<const uint32_t> shape);
  std::string CreateExpandNode(base::cstring_view input,
                               base::span<const uint32_t> shape);

  template <typename T>
  void AddBinaryOperation(const T& operation, base::cstring_view op_type);
  template <typename T>
  void AddUnaryOperation(const T& operation, base::cstring_view op_type);

  void AddArgMinMaxOperation(const mojom::ArgMinMax& arg_min_max);
  void AddCastOperation(const mojom::ElementWiseUnary& cast);
  void AddClampOperation(const mojom::Clamp& clamp);
  void AddConcatOperation(const mojom::Concat& concat);
  void AddConv2dOperation(const mojom::Conv2d& conv2d);
  void AddElementWiseBinaryOperation(
      const mojom::ElementWiseBinary& element_wise_binary);
  void AddElementWiseUnaryOperation(
      const mojom::ElementWiseUnary& element_wise_unary);
  void AddExpandOperation(const mojom::Expand& expand);
  void AddGemmOperation(const mojom::Gemm& gemm);
  void AddLeakyReluOperation(const mojom::LeakyRelu& leaky_relu);
  void AddPool2dOperation(const mojom::Pool2d& pool2d);
  void AddPreluOperation(const mojom::Prelu& prelu);
  void AddReshapeOperation(const mojom::Reshape& reshape);
  void AddSoftmaxOperation(const mojom::Softmax& softmax);
  void AddSplitOperation(const mojom::Split& split);
  void AddTransposeOperation(const mojom::Transpose& transpose);

  [[nodiscard]] base::expected<std::unique_ptr<ModelEditor::ModelInfo>,
                               mojom::ErrorPtr>
  BuildModel();

  // An increasing id starting from 0, used for generating unique names for each
  // operand.
  base::CheckedNumeric<uint32_t> next_operand_id_ = 0;

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
