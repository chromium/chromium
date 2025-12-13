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
  static std::unique_ptr<ModelEditor::ModelInfo> CreateAndBuild(
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

  // Generate a label for emulated operations by combining kInserted, op_type,
  // optional additional_tag, kToEmulate, and the original operation label.
  std::string GenerateEmulatedOpLabel(base::cstring_view op_type,
                                      std::string_view original_label,
                                      std::string_view additional_tag = "");

  // Create a new initializer for the graph with the given shape and data,
  // returning the name of the initializer.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  std::string CreateInitializer(base::span<const int64_t> shape,
                                base::span<const DataType> data);

  // A helper method wrapping the `CreateInitializer` method above. It adds a
  // scalar initializer with the given scalar value (tensor of empty shape) to
  // the graph, returning the name of the initializer.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  std::string CreateScalarInitializer(const DataType& value);

  // A helper method wrapping the `CreateInitializer` method above. It adds a
  // 1D initializer with the given data (tensor of shape [data.size()]) to the
  // graph, returning the name of the initializer.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  std::string Create1DInitializer(base::span<const DataType> data);

  // A helper method wrapping the `CreateInitializer` method above. It adds a 1D
  // int64 initializer with the given uint32 array (shape is [array.size()]) to
  // the graph, returning the name of the initializer.
  std::string CreateInt64InitializerForUint32Array(
      base::span<const uint32_t> array);

  // A helper method wrapping the `CreateInitializer` method above. It creates
  // an initializer of `shape` with all elements set to `value`. The data type
  // of the initializer is determined by the `data_type` parameter.
  std::string CreateInitializerForFloat(OperandDataType data_type,
                                        base::span<const uint32_t> shape,
                                        float value);

  // A helper method wrapping the `CreateScalarInitializer` method above. It
  // adds a scalar initializer with the given MLNumber value to the graph,
  // returning the name of the initializer. The data type of the initializer is
  // determined by the `data_type` parameter.
  std::string CreateScalarInitializer(OperandDataType data_type,
                                      const MLNumber& value);

  // A helper method creating an initializer with all elements set to 1.
  std::string CreateOneInitializer(OperandDataType data_type,
                                   base::span<const uint32_t> shape);

  // A helper method creating an initializer with all elements set to 0.
  std::string CreateZeroInitializer(OperandDataType data_type,
                                    base::span<const uint32_t> shape);

  // A helper function used to transpose the weight or bias layout for the RNN
  // operations (GRU, LSTM, etc.).
  //
  // Example:
  //   To transpose gru weight or bias from "rzn" layout to "zrn" layout, pass
  //   permutation as {1, 0, 2}.
  //   To transpose lstm weight or bias from "ifgo" layout to "iofg" layout,
  //   pass permutation as {0, 3, 1, 2}
  std::string TransposeRnnWeightOrBiasLayout(
      base::cstring_view weight_or_bias,
      base::span<const uint32_t> permutation);

  void AddCastNode(base::cstring_view node_name,
                   base::cstring_view input,
                   base::cstring_view output,
                   ONNXTensorElementDataType to_data_type);
  std::string CreateCastNode(base::cstring_view input,
                             ONNXTensorElementDataType to_data_type);
  void InsertCastNode(base::cstring_view input,
                      base::cstring_view output,
                      ONNXTensorElementDataType to_data_type);

  void AddExpandNode(base::cstring_view node_name,
                     base::cstring_view input,
                     base::cstring_view output,
                     base::span<const uint32_t> shape);
  std::string CreateExpandNode(base::cstring_view input,
                               base::span<const uint32_t> shape);

  void AddResizeNode(base::cstring_view node_name,
                     base::cstring_view input,
                     base::cstring_view scales,
                     base::cstring_view sizes,
                     base::cstring_view mode,
                     base::cstring_view output);
  // A helper function used to blockwise expand the dimension of `input`
  // according to `shape` by using `resize` operator with `nearest` mode.
  std::string BlockwiseExpand(base::cstring_view input,
                              base::span<const uint32_t> shape);

  void AddReshapeNode(base::cstring_view node_name,
                      base::cstring_view input,
                      base::cstring_view output,
                      base::span<const uint32_t> shape);
  std::string CreateReshapeNode(base::cstring_view input,
                                base::span<const uint32_t> shape);
  void InsertReshapeNode(base::cstring_view input,
                         base::cstring_view output,
                         base::span<const uint32_t> shape);

  void AddSliceNode(base::cstring_view node_name,
                    base::cstring_view input,
                    base::cstring_view output,
                    base::span<const int64_t> axes_value,
                    base::span<const int64_t> starts_value,
                    base::span<const int64_t> ends_value,
                    base::span<const int64_t> steps_value);

  void AddTransposeNode(base::cstring_view node_name,
                        base::cstring_view input,
                        base::cstring_view output,
                        base::span<const uint32_t> perm_value);
  std::string CreateTransposeNode(base::cstring_view input,
                                  base::span<const uint32_t> perm_value);

  // Clamp the indices to the range [-dim_size, dim_size), the given data type
  // should be indices's data type.
  std::string ClampIndices(base::cstring_view indices,
                           OperandDataType data_type,
                           uint32_t dim_size);

  // Clamp the indices to ensure that all values in indices are within bounds
  // [-s, s) along axis of size s, i.e. -input_shape[i] <= indices[..., i] <=
  // input_shape[i] - 1. The data type of indices is assumed to be int64.
  std::string ClampGatherNDIndices(base::cstring_view indices,
                                   base::span<const uint32_t> input_shape,
                                   base::span<const uint32_t> indices_shape);

  template <typename T>
  void AddBinaryOperation(const T& operation, base::cstring_view op_type);
  template <typename T>
  void AddUnaryOperation(const T& operation, base::cstring_view op_type);
  template <typename T>
  void AddGatherOperation(const T& operation, base::cstring_view op_type);

  void AddArgMinMaxOperation(const mojom::ArgMinMax& arg_min_max);
  void AddBatchNormalizationOperation(
      const mojom::BatchNormalization& batch_normalization);
  void AddCastOperation(const mojom::ElementWiseUnary& cast);
  void AddClampOperation(const mojom::Clamp& clamp);
  void AddConcatOperation(const mojom::Concat& concat);
  void AddConv2dOperation(const mojom::Conv2d& conv2d);
  void AddCumulativeSumOperation(const mojom::CumulativeSum& cumulative_sum);
  template <typename T>
    requires(std::is_same_v<T, mojom::DequantizeLinear> ||
             std::is_same_v<T, mojom::QuantizeLinear>)
  void AddDequantizeOrQuantizeLinearOperation(const T& operation,
                                              base::cstring_view op_type);
  void AddEluOperation(const mojom::Elu& elu);
  void AddLogicalBinaryOperation(const mojom::ElementWiseBinary& logical_binary,
                                 base::cstring_view op_type);
  void AddLogicalUnaryOperation(const mojom::ElementWiseUnary& logical_unary,
                                base::cstring_view op_type);
  void AddLogicalNotEqualOperation(const mojom::ElementWiseBinary& not_equal);
  void AddElementWiseBinaryOperation(
      const mojom::ElementWiseBinary& element_wise_binary);
  void AddElementWiseUnaryOperation(
      const mojom::ElementWiseUnary& element_wise_unary);
  void AddExpandOperation(const mojom::Expand& expand);
  void AddGatherNDOperation(const mojom::GatherND& gather_nd);
  void AddGemmOperation(const mojom::Gemm& gemm);
  template <typename GruType>
    requires(std::is_same_v<GruType, mojom::Gru> ||
             std::is_same_v<GruType, mojom::GruCell>)
  void AddGruOperation(const GruType& gru);
  void AddHardSigmoidOperation(const mojom::HardSigmoid& hard_sigmoid);
  void AddInstanceNormalizationOperation(
      const mojom::InstanceNormalization& instance_normalization);
  void AddLayerNormalizationOperation(
      const mojom::LayerNormalization& layer_normalization);
  void AddLeakyReluOperation(const mojom::LeakyRelu& leaky_relu);
  void AddLinearOperation(const mojom::Linear& linear);
  template <typename LstmType>
    requires(std::is_same_v<LstmType, mojom::Lstm> ||
             std::is_same_v<LstmType, mojom::LstmCell>)
  void AddLstmOperation(const LstmType& lstm);
  void AddMatMulOperation(const mojom::Matmul& matmul);
  void AddPadOperation(const mojom::Pad& pad);
  void AddPool2dOperation(const mojom::Pool2d& pool2d);
  void AddPreluOperation(const mojom::Prelu& prelu);
  void AddReduceOperation(const mojom::Reduce& reduce);
  void AddResample2dOperation(const mojom::Resample2d& resample2d);
  void AddReshapeOperation(const mojom::Reshape& reshape);
  void AddReverseOperation(const mojom::Reverse& reverse);
  void AddScatterElementsOperation(
      const mojom::ScatterElements& scatter_elements);
  void AddScatterNDOperation(const mojom::ScatterND& scatter_nd);
  void AddSliceOperation(const mojom::Slice& slice);
  void AddSoftmaxOperation(const mojom::Softmax& softmax);
  void AddSplitOperation(const mojom::Split& split);
  void AddTileOperation(const mojom::Tile& tile);
  void AddTransposeOperation(const mojom::Transpose& transpose);
  void AddTriangularOperation(const mojom::Triangular& triangular);
  void AddWhereOperation(const mojom::Where& where);

  std::unique_ptr<ModelEditor::ModelInfo> BuildModel();

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
