// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
#define SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/types/expected.h"
#include "services/webnn/ort/ort_model_editor.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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

// The returned operand name has a format of "label_id". Adding operand id at
// the end ensures that the name is unique.
std::string GetOperandName(std::string_view label, uint64_t id);

class GraphBuilderOrt {
  STACK_ALLOCATED();

 public:
  // Factory method that creates a GraphBuilderOrt, builds and serializes the
  // ONNX model to the `working_directory`. This expects the
  // `working_directory` to be an empty directory.
  //
  // Returns unexpected if it fails.
  [[nodiscard]] static base::expected<
      std::unique_ptr<OrtModelEditor::ModelInfo>,
      mojom::ErrorPtr>
  CreateAndBuild(const mojom::GraphInfo& graph_info,
                 ContextProperties context_properties,
                 base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
                     constant_operands);

  GraphBuilderOrt(const GraphBuilderOrt&) = delete;
  GraphBuilderOrt& operator=(const GraphBuilderOrt&) = delete;

  ~GraphBuilderOrt();

 private:
  GraphBuilderOrt(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands);

  const mojom::Operand& GetOperand(uint64_t operand_id) const;

  // Get the unique name of an existing operand by its id.
  std::string GetOperandNameById(uint64_t operand_id) const;

  // Generate the unique name of a newly created operand by combining a prefix
  // "inserted" and `next_operand_id_`, and then increase `next_operand_id_`.
  std::string GenerateNextOperandName();

  // Generate the unique name of a newly created operation by combining the
  // `next_operation_id_` and the `label`. ONNX doesn't allow duplicate node
  // names.
  std::string GenerateNextOperationName(std::string_view label);

  // Create a new initializer for the graph with the given shape and data,
  // return the name of the initializer.
  //
  // The provided data must match `DataType` according to the ONNX operators'
  // data type constraints. For example: Provide float data for ONNX float data
  // type. In particular, we use uint16_t to carry bits of float16.
  //
  // The guidelines recommends using raw data when:
  // 1. The byte size of the data is less than 128.
  // 2. The initializer is used for shape inference.
  // Otherwise, use external data.
  //
  // Actually, 128 byte size would cover all initializers used for shape
  // inference, because it could carry 16 x int64_t values and the existing
  // WebNN maximum rank is 8, so whether to use raw data only depends on the
  // data size.
  //
  // For example, some initializers will use raw data to do shape inference:
  // 1. Reshape: parameter *shape*.
  // 2. Reduce: parameter *axes*.
  // 3. Expand: parameter *shape*.
  // 4. Slice: parameter *starts*, *ends* and *steps*.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr> CreateInitializer(
      base::span<const uint32_t> shape,
      base::span<const DataType> data);

  // A helper method wrapping the `CreateInitializer` above. It creates a
  // scalar initializer with the given scalar value (tensor of empty shape) to
  // the graph, returns the name of the initializer.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
  CreateScalarInitializer(const DataType& value);

  // Iterate over the graph info before adding nodes to find the bool operands.
  // For example:
  // [Less] -> Bool -> [Cast] -> Uint8 -> [Cast] -> Bool -> [Where]
  void FindBoolOperands();

  void AddCastNode(std::string_view node,
                   std::string_view input,
                   std::string_view output,
                   ONNXTensorElementDataType to_data_type);

  // Insert a cast operation before an operation to convert its input to the
  // target `to_data_type`, return the output name of the cast operation. The
  // `input` specifies the input to be casted.
  std::string PrependCast(std::string_view input,
                          ONNXTensorElementDataType to_data_type);

  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr> PrependReshape(
      std::string_view input,
      base::span<const uint32_t> new_shape);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AppendReshape(
      std::string_view input,
      std::string_view output,
      base::span<const uint32_t> new_shape);

  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr> PrependExpand(
      std::string_view input,
      base::span<const uint32_t> shape);

  // A helper function used to handle the bias of recurrent network operators
  // (GRU, LSTM, etc.).
  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
  CreateOrReshapeBias(const std::optional<uint32_t>& bias_id,
                      OperandDataType input_data_type,
                      const std::vector<uint32_t>& bias_dims);

  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr>
  CreateScaleOrBiasForNormalization(OperandDataType data_type,
                                    base::span<const uint32_t> scale_shape,
                                    float value);

  // A helper function used to transpose the weight or bias layout for the RNN
  // operators (GRU, LSTM, etc.).
  //
  // Example:
  //   To transpose gru weight or bias from "rzn" layout to "zrn" layout, pass
  //   permutation as {1, 0, 2}.
  //   To transpose lstm weight or bais from "ifgo" layout to "iofg" layout,
  //   pass permutation as {0, 3, 1, 2}
  std::string TransposeRnnWeightOrBiasLayout(
      std::string_view weight_or_bias,
      base::span<const uint32_t> permutation);

  std::string PrependTranspose(std::string_view input,
                               base::span<const uint32_t> permutation);

  // Insert a cast operation after an operation to convert its output to the
  // target `to_data_type`. The `input` specifies the cast operation's
  // input (the output of the operation to be casted), and the `output`
  // specifies the cast operation's output.
  void AppendCast(std::string_view input,
                  std::string_view output,
                  ONNXTensorElementDataType to_data_type);

  void AppendTranspose(std::string_view input,
                       std::string_view output,
                       base::span<const uint32_t> permutation);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddSliceNode(
      std::string_view node,
      std::string_view input,
      std::string_view output,
      std::string_view axes,
      base::span<const int64_t> starts_value,
      base::span<const int64_t> ends_value,
      base::span<const int64_t> steps_value);

  // This clamp is emulated by min(max(input_value, min_value), max_value).
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr> Clamp(
      std::string_view input,
      base::span<const uint32_t> shape,
      base::span<const DataType> min_value,
      base::span<const DataType> max_value);

  // Clamp the indices within the dimension size to prevent out-of-bound
  // reading.
  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr> ClampIndices(
      std::string_view indices,
      OperandDataType indices_data_type,
      uint32_t dim_size);

  void AddInput(uint64_t input_id);
  void AddOutput(uint64_t output_id);

  // Similar to the `CreateInitializer` above, add an initializer to the graph
  // with the given constant from WebNN.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddInitializer(
      uint64_t constant_id);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddBatchNormalizationOperation(
      const mojom::BatchNormalization& batch_normalization);

  template <typename T>
  void AddBinaryOperation(const T& operation, std::string_view op_type);

  template <typename T>
  void AddUnaryOperation(const T& operation, std::string_view op_type);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddElementWiseBinaryOperation(
      const mojom::ElementWiseBinary& element_wise_binary);
  void AddElementWiseUnaryOperation(
      const mojom::ElementWiseUnary& element_wise_unary);
  void AddElementWiseLogicalOperation(
      absl::variant<const mojom::ElementWiseBinary*,
                    const mojom::ElementWiseUnary*> operation,
      std::string_view op_type);
  void AddElementWiseLogicalNotEqualOperation(
      const mojom::ElementWiseBinary& not_equal);
  void AddArgMinMaxOperation(const mojom::ArgMinMax& arg_min_max);
  void AddCastOperation(const mojom::ElementWiseUnary& cast);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddClampOperation(
      const mojom::Clamp& clamp);
  void AddConcatOperation(const mojom::Concat& concat);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddConv2dOperation(
      const mojom::Conv2d& conv2d);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddCumulativeSumOperation(
      const mojom::CumulativeSum& cumulative_sum);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddEluOperation(
      const mojom::Elu& elu);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddExpandOperation(
      const mojom::Expand& expand);
  template <typename DequantizeOrQuantizeLinear>
    requires(
        std::is_same_v<DequantizeOrQuantizeLinear, mojom::DequantizeLinear> ||
        std::is_same_v<DequantizeOrQuantizeLinear, mojom::QuantizeLinear>)
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddDequantizeOrQuantizeLinearOperation(
      std::string_view op_type,
      const DequantizeOrQuantizeLinear& operation_ptr);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddGatherOperation(
      const mojom::Gather& gather);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddGatherElementsOperation(const mojom::GatherElements& gather_elements);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddGatherNDOperation(
      const mojom::GatherND& gather_nd);
  void AddGemmOperation(const mojom::Gemm& gemm);
  template <typename GruType>
    requires(std::is_same_v<GruType, mojom::Gru> ||
             std::is_same_v<GruType, mojom::GruCell>)
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddGruOperation(
      const GruType& gru);
  void AddHardSigmoidOperation(const mojom::HardSigmoid& hard_sigmoid);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddInstanceNormalizationOperation(
      const mojom::InstanceNormalization& instance_normalization);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddLayerNormalizationOperation(
      const mojom::LayerNormalization& layer_normalization);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddLinearOperation(
      const mojom::Linear& linear);
  void AddLogicalNotOperation(const mojom::ElementWiseUnary& logical_not);
  void AddLeakyReluOperation(const mojom::LeakyRelu& leaky_relu);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddMatMulOperation(
      const mojom::Matmul& matmul,
      const std::map<uint64_t,
                     raw_ptr<const mojom::Operation, CtnExperimental>>&
          output_id_to_fusible_matmulnbits_map);
  template <typename LstmType>
    requires(std::is_same_v<LstmType, mojom::Lstm> ||
             std::is_same_v<LstmType, mojom::LstmCell>)
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddLstmOperation(
      const LstmType& lstm);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddPadOperation(
      const mojom::Pad& pad);
  void AddPool2dOperation(const mojom::Pool2d& pool2d);
  void AddPreluOperation(const mojom::Prelu& prelu);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddReduceOperation(
      const mojom::Reduce& reduce);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddResample2dOperation(
      const mojom::Resample2d& resample2d);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddReshapeOperation(
      const mojom::Reshape& reshape);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddReverseOperation(
      const mojom::Reverse& reverse);
  void AddScatterElementsOperation(
      const mojom::ScatterElements& scatter_elements);
  void AddScatterNDOperation(const mojom::ScatterND& scatter_nd);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddSliceOperation(
      const mojom::Slice& slice);
  void AddSoftmaxOperation(const mojom::Softmax& softmax);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddSplitOperation(
      const mojom::Split& split);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddTileOperation(
      const mojom::Tile& tile);
  void AddTransposeOperation(const mojom::Transpose& transpose);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddTriangularOperation(
      const mojom::Triangular& triangular);
  void AddWhereOperation(const mojom::Where& where);

  [[nodiscard]] base::expected<std::unique_ptr<OrtModelEditor::ModelInfo>,
                               mojom::ErrorPtr>
  BuildModel();

  // Used for inserting new operands into graph.
  uint64_t next_operand_id_ = 0;

  // Used for inserting new operation into graph.
  uint64_t next_operation_id_ = 0;

  // Operands that can be kept in bool data type without inserting cast
  // operators to/from uint8 data type.
  std::unordered_set<uint64_t> bool_operands_;

  // Bool operands that should be casted to uint8.
  std::unordered_set<uint64_t> bool_operands_to_be_casted_to_uint8_;

  // Map the uint8 operands back to the bool operands after inserting the cast
  // operators.
  std::unordered_map<std::string, std::string> uint8_to_bool_operands_map_;

  // A reference to the WebNN compute graph that `this` instance is converting
  // to ONNX model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
      constant_operands_;

  const ContextProperties context_properties_;

  OrtModelEditor model_editor_;
};

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
