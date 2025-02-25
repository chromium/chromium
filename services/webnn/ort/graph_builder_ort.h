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
#include "services/webnn/ort/ort_model_builder.h"
#include "services/webnn/ort/scoped_ort_types.h"
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
concept IsSupportedTensorType = IsAnyOf<T, float, uint16_t, int64_t>;

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
      std::unique_ptr<OrtModelBuilder::ModelInfo>,
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

  const mojom::Operand& GetOperand(uint64_t operand_id);

  // Get the unique name of an existing operand by its id.
  std::string GetOperandNameById(uint64_t operand_id);

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

  // Insert a cast operation before an operation to convert its input to the
  // target `to_data_type`, return the output name of the cast operation. The
  // `input_name` specifies the input to be casted.
  std::string PrependCast(std::string_view input_name,
                          ONNXTensorElementDataType to_data_type);

  [[nodiscard]] base::expected<std::string, mojom::ErrorPtr> PrependReshape(
      std::string_view input_name,
      base::span<const int64_t> new_shape);

  std::string PrependTranspose(std::string_view input_name,
                               base::span<const uint32_t> permutation);

  // Insert a cast operation after an operation to convert its output to the
  // target `to_data_type`. The `input_name` specifies the cast operation's
  // input (the output of the operation to be casted), and the `output_name`
  // specifies the cast operation's output.
  void AppendCast(std::string_view input_name,
                  std::string_view output_name,
                  ONNXTensorElementDataType to_data_type);

  void AppendTranspose(std::string_view input_name,
                       std::string_view output_name,
                       base::span<const uint32_t> permutation);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddSliceNode(
      std::string_view node_name,
      std::string_view input_name,
      std::string_view output_name,
      std::string_view axes_name,
      base::span<const int64_t> starts,
      base::span<const int64_t> ends,
      base::span<const int64_t> steps);

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
  void AddEluOperation(const mojom::Elu& elu);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddExpandOperation(
      const mojom::Expand& expand);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddDequantizeLinearOperation(
      const mojom::DequantizeLinear& dequantize_linear);
  void AddGatherOperation(const mojom::Gather& gather);
  void AddGatherElementsOperation(const mojom::GatherElements& gather_elements);
  void AddGatherNDOperation(const mojom::GatherND& gather_nd);
  void AddGemmOperation(const mojom::Gemm& gemm);
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
  void AddMatMulOperation(const mojom::Matmul& matmul);
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

  [[nodiscard]] base::expected<std::unique_ptr<OrtModelBuilder::ModelInfo>,
                               mojom::ErrorPtr>
  BuildModel();

  // Used for inserting new operands into graph.
  uint64_t next_operand_id_ = 0;

  // Used for inserting new operation into graph.
  uint64_t next_operation_id_ = 0;

  // A reference to the WebNN compute graph that `this` instance is converting
  // to ONNX model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
      constant_operands_;

  const ContextProperties context_properties_;

  OrtModelBuilder model_builder_;
};

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
