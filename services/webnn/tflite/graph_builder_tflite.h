// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_GRAPH_BUILDER_TFLITE_H_
#define SERVICES_WEBNN_TFLITE_GRAPH_BUILDER_TFLITE_H_

#include <concepts>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/types/expected.h"
#include "base/types/fixed_array.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/compiler/mlir/lite/schema/schema_generated.h"

namespace webnn {

class WebNNConstantOperand;

namespace tflite {

namespace internal {

// Methods which take a generic numerical type as input (e.g. uint32_t) and
// expect to serialize the data as a TFLite tensor (e.g.
// ::tflite::TensorType_UINT32) may use the `IsSupportedTensorType` concept to
// enforce that the data type maps to a supported TFLite tensor type.
// The list of supported data types may be expanded as needed.
template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);
template <typename T>
concept IsSupportedTensorType =
    IsAnyOf<T, float, int32_t, uint32_t, int64_t, int8_t, uint8_t, bool>;

}  // namespace internal

// This class converts WebNN graph to tflite model and persist into FlatBuffer.
// The schema_generated.h file defines the format for each data structure to
// serialize.
//
// The instances of the class may not be allocated on the heap, but as a member
// variable of a non-stack-allocated class and be single-use per conversion.
class GraphBuilderTflite final {
  STACK_ALLOCATED();

 public:
  struct Result {
    Result(flatbuffers::DetachedBuffer buffer,
           base::flat_map<std::string, int> input_name_to_index,
           base::flat_map<std::string, int> output_name_to_index,
           std::vector<uint8_t> buffer_data,
           bool graph_requires_fp32_precision);
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&&);
    Result& operator=(Result&&);
    ~Result();

    flatbuffers::DetachedBuffer buffer;
    base::flat_map<std::string, int> input_name_to_index;
    base::flat_map<std::string, int> output_name_to_index;
    std::vector<uint8_t> buffer_data;
    bool graph_requires_fp32_precision;
  };

  GraphBuilderTflite(const GraphBuilderTflite&) = delete;
  GraphBuilderTflite& operator=(const GraphBuilderTflite&) = delete;

  // Factory method that creates a GraphBuilderTflite and builds a TFLite
  // Flatbuffer Returns unexpected if it fails.
  [[nodiscard]] static base::expected<Result, std::string> CreateAndBuild(
      ContextProperties context_properties,
      const mojom::GraphInfo& graph_info,
      const base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands,
      const base::flat_map<OperandId, base::flat_set<OperationId>>&
          operand_to_dependent_operations,
      const base::flat_map<OperandId, OperationId>&
          operand_to_producing_operation);

  static ContextProperties GetContextProperties();

 private:
  using IdToOperandMap = base::flat_map<OperandId, mojom::OperandPtr>;
  using OperatorCodeOffset = flatbuffers::Offset<::tflite::OperatorCode>;
  using OperatorOffset = flatbuffers::Offset<::tflite::Operator>;
  using BufferOffset = flatbuffers::Offset<::tflite::Buffer>;
  using TensorOffset = flatbuffers::Offset<::tflite::Tensor>;
  using StringOffset = flatbuffers::Offset<flatbuffers::String>;
  using QuantizateParametersOffset =
      flatbuffers::Offset<::tflite::QuantizationParameters>;
  using BufferIndex = uint32_t;
  using OperatorCodeIndex = uint32_t;
  using TensorIndex = int32_t;

  GraphBuilderTflite(
      ContextProperties context_properties,
      const mojom::GraphInfo& graph_info,
      const base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands,
      const base::flat_map<OperandId, base::flat_set<OperationId>>&
          operand_to_dependent_operations,
      const base::flat_map<OperandId, OperationId>&
          operand_to_producing_operation);
  ~GraphBuilderTflite();

  // Maps to WebNN operand information.
  struct TensorInfo {
    TensorInfo();
    TensorInfo(TensorIndex index,
               ::tflite::TensorType data_type,
               base::span<const int32_t> dimensions,
               std::optional<std::string> name = std::nullopt,
               QuantizateParametersOffset = 0);
    ~TensorInfo();

    // Copyable and movable.
    TensorInfo(const TensorInfo&);
    TensorInfo& operator=(const TensorInfo&);
    TensorInfo(TensorInfo&& other);
    TensorInfo& operator=(TensorInfo&& other);

    TensorIndex index;
    ::tflite::TensorType data_type;
    std::vector<int32_t> dimensions;
    std::optional<std::string> name;
    QuantizateParametersOffset quantize_params;
  };

  // Serialize tensor for input, constant and output operand and return the
  // tensor information. The `override_tensor_type` is used to override the
  // tensor type, such as when dequantising a float16 operator to float32 before
  // serializing an operator which does not support float32.
  TensorInfo SerializeOperand(
      OperandId operand_id,
      QuantizateParametersOffset quantize_params,
      std::optional<::tflite::TensorType> override_tensor_type = std::nullopt);

  // Call `SerializeOperand` to serialize the input operand if it's not
  // serialized, and insert a TFLite dequantize operator to convert float16 to
  // float32 for graph input, constant and intermediate operands if the current
  // operation doesn't support float16 inference (`operation_supports_float16`
  // is false).
  base::expected<TensorInfo, std::string> SerializeInputTensorInfo(
      OperandId operand_id,
      QuantizateParametersOffset quantize_params = 0,
      bool operation_supports_float16 = false,
      bool fuse_dequantize_quantize = false);

  // Call `SerializeOperand` to serialize the output operand and insert a TFLite
  // cast operator to convert float32 to float16 if the operand is graph output
  // and the current operation doesn't support float16 inference. The
  // `override_tensor_type` is used to override the output tensor type the same
  // as input, for example the input data type has been overridden to float32 of
  // intermediate operands (Reshape), so the output tensor type should be
  // float32 with the argument.
  TensorInfo SerializeOutputTensorInfo(
      OperandId operand_id,
      QuantizateParametersOffset quantize_params = 0,
      bool operation_supports_float16 = false,
      std::optional<::tflite::TensorType> override_tensor_type = std::nullopt);

  // The following steps implement the `SerializeOperation` function:
  // 1. Create `tflite::OperatorCode` with the kind of operator.
  // 2. Create `tflite::Operator` with the tensor index of inputs and outputs
  //    operand.
  //
  // Returns error messages if it could not be serialized because of unsupported
  // options or it is otherwise invalid.
  base::expected<void, std::string> SerializeOperation(
      const mojom::Operation& op,
      OperationId operation_index);

  // Serializes the constant data (e.g. weights) to the flat buffer and returns
  // the index in the `tflite::Buffer` array if it's successful.
  //
  // The `Buffer` in TFLite schema is the table of raw data buffers, it is used
  // for WebNN constant operations. Referenced by tensors with the index of
  // buffer.
  BufferIndex SerializeBuffer(base::span<const uint8_t> buffer);

  // Serializes `buffer` as a tensor with the given `dimensions` and `type `to
  // the flat buffer and returns the index in `tensors_` if it's successful.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  TensorIndex SerializeTensorWithBuffer(base::span<const DataType> buffer,
                                        base::span<const int32_t> dimensions);

  // Serializes temporary tensor for casting the data type of input operand or
  // decomposing WebNN operation that isn't supported in TFLite schema.
  TensorIndex SerializeTemporaryTensor(
      base::span<const int32_t> dimensions,
      ::tflite::TensorType tensor_type,
      QuantizateParametersOffset quantize_params = 0);

  OperatorCodeIndex GetOperatorCodeIndex(::tflite::BuiltinOperator code,
                                         int32_t version = 1);

  // Returns the Operand corresponding to an `operand_id` from `graph_info_`.
  // Will crash if `graph_info_` does not contain `operand_id`.
  const mojom::Operand& GetOperand(OperandId operand_id) const;

  // Get the value if the operand is constant.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  base::span<const DataType> GetConstantValue(OperandId operand_id);

  // Get the value from constant operand and cast it to int64 data type.
  base::FixedArray<int64_t> GetConstantInt64Value(OperandId operand_id);

  // Operation serialization helpers for operations not directly declared in
  // the mojom::Operation union.
  //
  // Serialize an operation with a single input and a single output operand.
  // The caller must either provide both `builtin_options_type` and
  // `builtin_options`, or neither.
  OperatorOffset SerializeUnaryOperation(
      ::tflite::BuiltinOperator code,
      TensorIndex input_tensor_index,
      TensorIndex output_tensor_index,
      ::tflite::BuiltinOptions builtin_options_type =
          ::tflite::BuiltinOptions_NONE,
      flatbuffers::Offset<void> builtin_options = 0);
  OperatorOffset SerializeCastOperation(
      TensorIndex input_tensor_index,
      ::tflite::TensorType input_tensor_type,
      TensorIndex output_tensor_index,
      ::tflite::TensorType output_tensor_type);

  // Serializes specializations of the pow operator for the square and square
  // root operations.
  OperatorOffset SerializeSquareOperation(
      TensorIndex input_tensor_index,
      ::tflite::TensorType input_tensor_type,
      TensorIndex output_tensor_index);
  base::expected<OperatorOffset, std::string> SerializeSquareRootOperation(
      TensorIndex input_tensor_index,
      ::tflite::TensorType input_tensor_type,
      TensorIndex output_tensor_index);

  // A helper function is used by WebNN binary operation direct or emulated
  // implementation.
  OperatorOffset SerializeBinaryOperation(::tflite::BuiltinOperator code,
                                          TensorIndex lhs_tensor_index,
                                          TensorIndex rhs_tensor_index,
                                          TensorIndex output_tensor_index);

  // Serialize a sub graph (min appending max operation) for clamp.
  template <typename DataType>
  OperatorOffset SerializeSubGraphMaxMin(const TensorInfo& input_tensor_info,
                                         TensorIndex output_tensor_index,
                                         base::span<const DataType> min_values,
                                         base::span<const DataType> max_values);

  // Serialize gather_nd indices tensor.
  template <typename DataType>
  base::expected<TensorIndex, std::string> SerializeGatherNDIndices(
      const TensorInfo& indices_tensor_info,
      const TensorInfo& input_tensor_info);
  TensorIndex CastGatherIndices(const TensorInfo& indices_tensor_info);

  // This function is called by `SerializeGatherND` to serialize WebNN
  // gatherND or gatherElements.
  OperatorOffset SerializeGatherNDOperation(TensorIndex input_tensor_index,
                                            TensorIndex indices_tensor_index,
                                            TensorIndex output_tensor_index);

  // Serialize coordinates for gather and scatter elements.
  template <typename DataType>
    requires(std::is_same_v<DataType, int32_t> ||
             std::is_same_v<DataType, int64_t>)
  base::expected<TensorIndex, std::string> SerializeElementsCoordinates(
      base::span<const uint32_t> indices_dimensions,
      base::span<const DataType> indices_value,
      base::span<const int32_t> input_dimensions,
      int32_t axis);

  // This function is called by `SerializeConcat` to serialize WebNN
  // concat operator or used to emulate WebNN operations.
  OperatorOffset SerializeConcatOperation(
      base::span<const TensorIndex> input_tensor_indices,
      TensorIndex output_tensor_index,
      uint32_t axis);

  // This function serializes a TFLite dequantize operator to convert float16
  // data type to float32.
  TensorIndex SerializeDequantizeOperation(
      TensorIndex input_tensor_index,
      base::span<const int32_t> input_dimensions);

  // Get int64 zero point from int4 constant operand.
  base::FixedArray<int64_t> GetInt64ZeroPointFromInt4(
      OperandId zero_point_operand_id);
  base::FixedArray<int64_t> GetInt64ZeroPoint(OperandId zero_point_operand_id);
  // Serialize quantize params for quantizeLinear and dequantizeLinear.
  std::optional<QuantizateParametersOffset> SerializeQuantizeParams(
      OperandId zero_point_operand_id,
      OperandId scale_operand_id,
      size_t input_rank);

  // This function is called by `SerializeMatmul` to serialize WebNN
  // matmul operator or used to emulate WebNN operations.
  OperatorOffset SerializeMatmulOperation(TensorIndex a_tensor_index,
                                          TensorIndex b_tensor_index,
                                          TensorIndex output_tensor_index);

  // A helper function is used to emulate batch, layer or instance
  // normalization.
  OperatorOffset SerializeNormalizationOperation(
      base::span<const int32_t> input_dimensions,
      ::tflite::TensorType input_tensor_type,
      TensorIndex input_tensor_index,
      TensorIndex output_tensor_index,
      TensorIndex mean_tensor_index,
      TensorIndex variance_tensor_index,
      float epsilon,
      std::optional<TensorIndex> scale_tensor_index,
      std::optional<TensorIndex> bias_tensor_index);

  // Compute the means and variance values for the instance and layer
  // normalization.
  std::tuple<TensorIndex, TensorIndex> ComputeMeanAndVarianceForNormalization(
      base::span<const int32_t> input_dimensions,
      ::tflite::TensorType input_tensor_type,
      TensorIndex input_tensor_index,
      base::span<const int32_t> axes);
  TensorIndex TransposeAndReshapeLayerNormalizationScaleBias(
      base::span<const int32_t> input_dimensions,
      const TensorInfo& scale_or_bias_tensor_info,
      base::span<const uint32_t> axes);

  // This function is called by `SerializeReduce` to serialize WebNN
  // reduce operators or used to emulate WebNN operations.
  OperatorOffset SerializeReduceOperation(
      ::tflite::BuiltinOperator operator_code,
      TensorIndex input_tensor_index,
      TensorIndex output_tensor_index,
      base::span<const int32_t> axes,
      bool keep_dimensions);

  // This function is called by `SerializeReshape` to serialize WebNN
  // reshape operator or used to emulate WebNN operations.
  OperatorOffset SerializeReshapeOperation(TensorIndex input_tensor_index,
                                           TensorIndex output_tensor_index,
                                           base::span<const int32_t> new_shape);

  // This function is called by `SerializeSlice` to serialize WebNN
  // slice operator or used to emulate WebNN operations.
  base::expected<OperatorOffset, std::string> SerializeSliceOperation(
      TensorIndex input_tensor_index,
      TensorIndex output_tensor_index,
      base::span<const int32_t> slice_starts,
      base::span<const int32_t> slice_sizes);

  // This function is called by `SerializeLinear` to serialize WebNN linear or
  // used to emulate WebNN operation that isn't supported in TFLite schema.
  OperatorOffset SerializeLinearOperation(
      base::span<const int32_t> input_dimensions,
      ::tflite::TensorType input_tensor_type,
      TensorIndex input_tensor_index,
      TensorIndex output_tensor_index,
      float alpha,
      float beta);

  // This function is called by `SerializeTranspose` to serialize WebNN
  // transpose operator or used to insert a tempary operator to transpose
  // different layout.
  OperatorOffset SerializeTransposeOperation(
      TensorIndex input_tensor_index,
      TensorIndex output_tensor_index,
      base::span<const int32_t> input_shape,
      base::span<const uint32_t> permutation);

  // This function is called by SerializeScatterND or SerializeScatterElements
  // to serialize WebNN scatterND or scatterElements operation.
  OperatorOffset SerializeWebNNScatterND(const TensorInfo& input_tensor_info,
                                         const TensorInfo& updates_tensor_info,
                                         TensorIndex indices_tensor_index,
                                         TensorIndex output_tensor_index);
  // This function is called by `SerializeWebNNScatterND` to implement WebNN
  // scatterND operation.
  OperatorOffset SerializeTFLiteScatterND(
      base::span<const int32_t> input_shapes,
      TensorIndex indices_tensor_index,
      TensorIndex updates_tensor_index,
      TensorIndex output_tensor_index);

  // This function is called by `SerializeReverse` to serialize WebNN
  // reverse operation.
  OperatorOffset SerializeReverseOperation(TensorIndex input_tensor_index,
                                           base::span<const int32_t> axes,
                                           TensorIndex output_tensor_index);

  // This function is called by `SerializeWhere` to serialize WebNN where
  // operation or used to emulate scatterND operation.
  OperatorOffset SerializeWhereOperation(TensorIndex condition_tensor_index,
                                         TensorIndex true_tensor_index,
                                         TensorIndex false_tensor_index,
                                         TensorIndex output_tensor_index);

  // Insert a tempary pad operation if the `paddings` can't be converted to
  // tflite padding mode.
  base::expected<TensorIndex, std::string> InsertPadOperation(
      const TensorInfo& input_tensor_info,
      base::span<const uint32_t> paddings);

  // Insert a tempary transpose operation for input operand with calling
  // `SerializeTransposeOperation`.
  TensorIndex InsertTransposeOperation(const TensorInfo& input_tensor_info,
                                       base::span<const uint32_t> permutation);

  // Serialize a sub graph (pow appending mul operation) for erf operation.
  TensorIndex SerializeSubGraphPowMul(
      base::span<const int32_t> input_dimensions,
      ::tflite::TensorType input_tensor_type,
      TensorIndex input_tensor_index,
      int pow_exponent,
      float mul_alpha);

  // Serialize a sub graph (input * weight + bias) for gru cell.
  TensorIndex SerializeSubGraphMatmulAdd(
      base::span<const int32_t> input_dimensions,
      ::tflite::TensorType input_tensor_type,
      TensorIndex input_tensor_index,
      TensorIndex weight_tensor_index,
      std::optional<TensorIndex> bias_tensor_index);

  // Serialize a sub graph (slice appending transpose operation) for gru cell.
  base::expected<TensorIndex, std::string> SerializeSubGraphSliceTranspose(
      ::tflite::TensorType input_tensor_type,
      TensorIndex input_tensor_index,
      base::span<const int32_t> slice_starts,
      base::span<const int32_t> slice_sizes);

  enum class GruGateType { kUpdate, kReset, kNew };

  // The common attributes between gru and lstm.
  struct RecurrentNetworkBase {
    STACK_ALLOCATED();

   public:
    RecurrentNetworkBase(
        base::span<const int32_t> input_dimensions,
        ::tflite::TensorType input_tensor_type,
        TensorIndex input_tensor_index,
        TensorIndex weight_tensor_index,
        TensorIndex recurrent_weight_tensor_index,
        std::optional<TensorIndex> bias_tensor_index,
        std::optional<TensorIndex> recurrent_bias_tensor_index,
        TensorIndex hidden_state_tensor_index,
        int32_t hidden_size,
        base::span<const mojom::RecurrentNetworkActivation> activations);
    ~RecurrentNetworkBase();

    RecurrentNetworkBase(const RecurrentNetworkBase&) = delete;
    RecurrentNetworkBase& operator=(const RecurrentNetworkBase&) = delete;

    RecurrentNetworkBase(RecurrentNetworkBase&&) = delete;
    RecurrentNetworkBase& operator=(RecurrentNetworkBase&&) = delete;

    base::span<const int32_t> input_dimensions;
    const ::tflite::TensorType input_tensor_type;
    const TensorIndex input_tensor_index;
    const TensorIndex weight_tensor_index;
    const TensorIndex recurrent_weight_tensor_index;
    const std::optional<TensorIndex> bias_tensor_index;
    const std::optional<TensorIndex> recurrent_bias_tensor_index;
    const TensorIndex hidden_state_tensor_index;
    const int32_t hidden_size;
    const base::span<const mojom::RecurrentNetworkActivation> activations;
  };

  // The struct maps to mojom::GruCell to use tflite tensor index instead of
  // the operand id in mojom definition.
  struct GruCellOperation : public RecurrentNetworkBase {
    STACK_ALLOCATED();

   public:
    GruCellOperation(
        base::span<const int32_t> input_dimensions,
        ::tflite::TensorType input_tensor_type,
        TensorIndex input_tensor_index,
        TensorIndex output_tensor_index,
        TensorIndex weight_tensor_index,
        TensorIndex recurrent_weight_tensor_index,
        std::optional<TensorIndex> bias_tensor_index,
        std::optional<TensorIndex> recurrent_bias_tensor_index,
        TensorIndex hidden_state_tensor_index,
        int32_t hidden_size,
        bool reset_after,
        mojom::GruWeightLayout layout,
        base::span<const mojom::RecurrentNetworkActivation> activations);
    ~GruCellOperation();

    const TensorIndex output_tensor_index;
    const bool reset_after;
    const mojom::GruWeightLayout layout;
  };

  // A helper function for serializing update, reset and new gate, the argument
  // `reset_gate_tensor_index` only be used for new gate.
  base::expected<TensorIndex, std::string> SerializeGruGate(
      const GruCellOperation& gru_cell,
      GruGateType type,
      std::optional<TensorIndex> reset_gate_tensor_index = std::nullopt);

  // This function is called by `SerializeGruCell` to serialize WebNN gruCell or
  // used to emulate gru operation.
  base::expected<OperatorOffset, std::string> SerializeGruCellOperation(
      const GruCellOperation& gru_cell);

  // A helper function for serializing WebNN gru and lstm operations.
  template <typename RecurrentNetworkType>
    requires(std::is_same_v<RecurrentNetworkType, mojom::Gru> ||
             std::is_same_v<RecurrentNetworkType, mojom::Lstm>)
  base::expected<OperatorOffset, std::string> SerializeRecurrentNetwork(
      const RecurrentNetworkType& recurrent_network);

  enum class LstmGateType { kInput, kForget, kCell, kOutput };

  // The struct maps to mojom::LstmCell to use tflite tensor index instead of
  // the operand id in mojom definition.
  struct LstmCellOperation : public RecurrentNetworkBase {
    STACK_ALLOCATED();

   public:
    LstmCellOperation(
        base::span<const int32_t> input_dimensions,
        ::tflite::TensorType input_tensor_type,
        TensorIndex input_tensor_index,
        base::span<const TensorIndex> output_tensor_indices,
        TensorIndex weight_tensor_index,
        TensorIndex recurrent_weight_tensor_index,
        std::optional<TensorIndex> bias_tensor_index,
        std::optional<TensorIndex> recurrent_bias_tensor_index,
        TensorIndex hidden_state_tensor_index,
        int32_t hidden_size,
        TensorIndex cell_state_tensor_index,
        std::optional<TensorIndex> peephole_weight_tensor_index,
        mojom::LstmWeightLayout layout,
        base::span<const mojom::RecurrentNetworkActivation> activations);
    ~LstmCellOperation();

    base::span<const TensorIndex> output_tensor_indices;
    const TensorIndex cell_state_tensor_index;
    std::optional<TensorIndex> peephole_weight_tensor_index;
    const mojom::LstmWeightLayout layout;
  };

  // A helper function for serializing input, forget, cell and output gate.
  base::expected<TensorIndex, std::string> SerializeLstmGate(
      const LstmCellOperation& lstm_cell,
      LstmGateType type);

  // This function is called by `SerializeLstmCell` to serialize WebNN lstmCell
  // or used to emulate lstm operation.
  base::expected<OperatorOffset, std::string> SerializeLstmCellOperation(
      const LstmCellOperation& lstm_cell);

  // Get initial hidden and cell state tensor index if existed or serialize an
  // empty tensor.
  base::expected<TensorIndex, std::string> GetInitialHiddenAndCellState(
      std::optional<OperandId> state_operand_id,
      base::span<const int32_t> state_dimensions);

  // Reshape hidden and cell state, concat the reshaped tensor if the input
  // tensor of concat is provided.
  TensorIndex ReshapeHiddenAndCellState(
      ::tflite::TensorType input_tensor_type,
      TensorIndex input_tensor_index,
      base::span<const int32_t> new_shape,
      std::optional<TensorIndex> concat_input_tensor_index,
      base::span<const int32_t> concat_output_shape);

  // Serialize a sub graph (slice appending squeeze operation) for gru.
  base::expected<TensorIndex, std::string> SerializeSubGraphSliceSqueeze(
      ::tflite::TensorType input_tensor_type,
      TensorIndex input_tensor_index,
      base::span<const int32_t> slice_starts,
      base::span<const int32_t> slice_sizes,
      int32_t squeeze_axis);

  // Serialize functions for members of the mojom::Operation union. Keep these
  // functions in the same order as in webnn_graph.mojom.
  base::expected<OperatorOffset, std::string> SerializeArgMinMax(
      const mojom::ArgMinMax& arg_min_max);
  base::expected<OperatorOffset, std::string> SerializeBatchNormalization(
      const mojom::BatchNormalization& batch_normalization);
  base::expected<OperatorOffset, std::string> SerializeClamp(
      const mojom::Clamp& clamp);
  base::expected<OperatorOffset, std::string> SerializeConv2d(
      const mojom::Conv2d& conv2d);
  base::expected<OperatorOffset, std::string> SerializeConcat(
      const mojom::Concat& concat);
  base::expected<OperatorOffset, std::string> SerializeCumulativeSum(
      const mojom::CumulativeSum& cumulative_sum);
  base::expected<OperatorOffset, std::string> SerializeElementWiseBinary(
      const mojom::ElementWiseBinary& op);
  base::expected<OperatorOffset, std::string> SerializeElementWiseUnary(
      const mojom::ElementWiseUnary& op);
  base::expected<OperatorOffset, std::string> SerializeElu(
      const mojom::Elu& elu);
  base::expected<OperatorOffset, std::string> SerializeErf(
      const TensorInfo& input_tensor_info,
      const TensorInfo& output_tensor_info);
  base::expected<OperatorOffset, std::string> SerializeExpand(
      const mojom::Expand& expand);
  base::expected<OperatorOffset, std::string> SerializeGather(
      const mojom::Gather& gather);
  base::expected<OperatorOffset, std::string> SerializeGatherElements(
      const mojom::GatherElements& gather_elements);
  base::expected<OperatorOffset, std::string> SerializeGatherND(
      const mojom::GatherND& gather_nd);
  base::expected<OperatorOffset, std::string> SerializeGelu(
      const mojom::Gelu& gelu);
  base::expected<OperatorOffset, std::string> SerializeGemm(
      const mojom::Gemm& gemm);
  base::expected<OperatorOffset, std::string> SerializeGruCell(
      const mojom::GruCell& gru_cell);
  base::expected<OperatorOffset, std::string> SerializeHardSigmoid(
      const mojom::HardSigmoid& hard_sigmoid);
  base::expected<OperatorOffset, std::string> SerializeHardSwish(
      const mojom::HardSwish& hard_swish);
  OperatorOffset SerializeIdentityOperation(TensorIndex input_tensor_index,
                                            TensorIndex output_tensor_index,
                                            base::span<const int32_t> shape);
  base::expected<OperatorOffset, std::string> SerializeInstanceNormalization(
      const mojom::InstanceNormalization& instance_normalization);
  base::expected<OperatorOffset, std::string> SerializeLayerNormalization(
      const mojom::LayerNormalization& layer_normalization);
  base::expected<OperatorOffset, std::string> SerializeLeakyRelu(
      const mojom::LeakyRelu& leaky_relu);
  base::expected<OperatorOffset, std::string> SerializeLinear(
      const mojom::Linear& linear);
  OperatorOffset SerializeLogicalNot(const TensorInfo& input_tensor_info,
                                     const TensorInfo& output_tensor_info);
  base::expected<OperatorOffset, std::string> SerializeLstmCell(
      const mojom::LstmCell& lstm_cell);
  base::expected<OperatorOffset, std::string> SerializeMatmul(
      const mojom::Matmul& matmul);
  base::expected<OperatorOffset, std::string> SerializePad(
      const mojom::Pad& pad);
  base::expected<OperatorOffset, std::string> SerializePool2d(
      const mojom::Pool2d& pool2d);
  base::expected<OperatorOffset, std::string> SerializePrelu(
      const mojom::Prelu& prelu);
  base::expected<OperatorOffset, std::string> SerializeQuantizeLinear(
      const mojom::QuantizeLinear& quantize_linear);
  base::expected<OperatorOffset, std::string> SerializeDequantizeLinear(
      const mojom::DequantizeLinear& dequantize_linear);
  base::expected<OperatorOffset, std::string> SerializeReciprocal(
      const TensorInfo& input_tensor_info,
      const TensorInfo& output_tensor_info);
  base::expected<OperatorOffset, std::string> SerializeReduce(
      const mojom::Reduce& reduce);
  base::expected<OperatorOffset, std::string> SerializeReduceSumSquare(
      const TensorInfo& input_tensor_info,
      base::span<const int32_t> axes,
      bool keep_dimensions,
      TensorIndex output_tensor_index);
  base::expected<OperatorOffset, std::string> SerializeRelu(
      const mojom::Relu& relu);
  base::expected<OperatorOffset, std::string> SerializeResample2d(
      const mojom::Resample2d& resample2d);
  base::expected<OperatorOffset, std::string> SerializeReshape(
      const mojom::Reshape& reshape);
  base::expected<OperatorOffset, std::string> SerializeReverse(
      const mojom::Reverse& reverse);
  base::expected<OperatorOffset, std::string> SerializeScatterElements(
      const mojom::ScatterElements& scatter_elements);
  base::expected<OperatorOffset, std::string> SerializeScatterND(
      const mojom::ScatterND& scatter_nd);
  base::expected<OperatorOffset, std::string> SerializeSigmoid(
      const mojom::Sigmoid& sigmoid);
  base::expected<OperatorOffset, std::string> SerializeSlice(
      const mojom::Slice& slice);
  base::expected<OperatorOffset, std::string> SerializeSoftmax(
      const mojom::Softmax& softmax);
  base::expected<OperatorOffset, std::string> SerializeSoftplus(
      const mojom::Softplus& softplus);
  base::expected<OperatorOffset, std::string> SerializeSoftsign(
      const mojom::Softsign& softsign);
  base::expected<OperatorOffset, std::string> SerializeSplit(
      const mojom::Split& split);
  OperatorOffset SerializeTan(const TensorInfo& input_tensor_info,
                              const TensorInfo& output_tensor_info);
  base::expected<OperatorOffset, std::string> SerializeTanh(
      const mojom::Tanh& tanh);
  base::expected<OperatorOffset, std::string> SerializeTile(
      const mojom::Tile& tile);
  base::expected<OperatorOffset, std::string> SerializeTriangular(
      const mojom::Triangular& triangular);
  base::expected<OperatorOffset, std::string> SerializeTranspose(
      const mojom::Transpose& transpose);
  base::expected<OperatorOffset, std::string> SerializeWhere(
      const mojom::Where& where);

  bool RequiresFloat32Precision(const mojom::Operation& op);

  // Check if inputs and outputs are quantized tensors and matches
  // op specific fusion criteria required by TFLite, if so we can remove the
  // preceding `dequantizeLinear` and subsequent `quantizeLinear`.
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
    const mojom::Conv2d& conv2d);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Concat& concat);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::ElementWiseBinary& binary);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(const mojom::Elu& elu);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Gather& gather);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Pool2d& pool2d);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Reduce& reduce);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Reshape& reshape);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Slice& slice);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Softmax& softmax);
  std::optional<base::FixedArray<TensorInfo>> CanFuseQuantizeAndGetOutput(
      const mojom::Split& split);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Transpose& transpose);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Tanh& tanh);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::Sigmoid& sigmoid);
  std::optional<TensorInfo> CanFuseQuantizeAndGetOutput(
      const mojom::LeakyRelu& leaky_relu);
  // Helper for activation operations to check if specific fusion criteria
  // required by TFLite are met and return next quantizeLinear operation
  // information if so.
  // This is shared by `tanh`, `sigmoid` and `leakyRelu`.
  template <typename OpType>
  std::optional<std::pair<OperationId, QuantizateParametersOffset>>
  CanFuseQuantizeForActivationOperation(const OpType& op);
  bool IsDequantizeOutput(OperandId operand_id);
  // Get the dequantize op by its output operand id.
  const mojom::DequantizeLinear& GetDequantizeOp(OperandId operand_id);
  const mojom::QuantizeLinear& GetQuantizeOp(OperationId operation_index);

  // Called before graph serialization to attach quantization params to
  // dequantizeLinear input and upstream nodes if they are quantization agnostic
  // operations.
  base::expected<void, std::string> TryTraverseToSerializeQuantizedInput(
      const mojom::DequantizeLinear& dequantize_linear);

  // Try to serialize `dequantize_linear`'s input with quantization params and
  // return if it's successful.
  bool TrySerializeQuantizedInput(
      const mojom::DequantizeLinear& dequantize_linear,
      OperationId operation_index);
  // Serialize `quantize_linear`'s output with quantization params and
  // mark the `quantize_linear` to be skipped.
  TensorInfo SerializeQuantizedOutput(
      std::pair<OperationId, QuantizateParametersOffset> quantize_op_info);
  // Check if next op is quantize and its parameters can be serialized, if so
  // mark it to-be skipped and return the quantized output.
  std::optional<std::pair<OperationId, QuantizateParametersOffset>>
  IsNextOpQuantize(OperandId output_operand_id,
                   SupportedDataTypes supported_quantized_types);
  // Check if the input of DequantizeLinear is (u)int8, the output of
  // QuantizeLinear has been validated (u)int8 in `IsNextOpQuantize`, and its
  // scale and zero point are scalar values.
  //
  // Used by DQ->op->Q fusion to satisfy XNNPACK delegate's validation in
  // `CheckTensorFloat32OrQUInt8Type`.
  template <typename OpType>
    requires(std::is_same_v<OpType, mojom::DequantizeLinear> ||
             std::is_same_v<OpType, mojom::QuantizeLinear>)
  bool IsInts8AndScalarScale(const OpType& op);

  bool IsSerializedWithMismatchQuantizeParameters(
      OperandId operand_id,
      QuantizateParametersOffset quantize_params);

  bool AreConstantOperandsEqual(OperandId lhs_operand_id,
                                OperandId rhs_operand_id);

  // No further methods may be called on this class after calling this method
  // because the buffer of `buffer_` is now owned by the detached buffer.
  Result FinishAndTakeResult(base::span<const OperandId> input_operands,
                             base::span<const OperandId> output_operands,
                             bool has_fp32_operation);

  const ContextProperties context_properties_;

  // A reference to the WebNN compute graph that `this` instance is converting
  // to TFLite. The creator of `this` must ensure the GraphInfo reference passed
  // into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  // A reference to the constant operands used by this graph. The creator of
  // `this` must ensure this reference is valid for as long as `this` exists.
  base::raw_ref<
      const base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>>
      constant_operands_;

  // A reference to output operand dependency map. The creator of `this` must
  // ensure this reference is valid for as long as `this` exists.
  base::raw_ref<const base::flat_map<OperandId, base::flat_set<OperationId>>>
      operand_to_dependent_operations_;

  // A reference to input operand dependency map. The creator of `this` must
  // ensure this reference is valid for as long as `this` exists.
  base::raw_ref<const base::flat_map<OperandId, OperationId>>
      operand_to_producing_operation_;

  flatbuffers::FlatBufferBuilder builder_;
  // `is_created_model_` indicates whether the tflite model is created and the
  // detached buffer owns the buffer and its allocator of the `builder_`.
  bool is_created_model_ = false;

  // Map from operand IDs in the GraphInfo structure to tensor index and data
  // type, the tensor type will be tflite::TensorType_FLOAT32 instead of
  // tflite::TensorType_FLOAT16 if the TFLite builtin operators don't support
  // float16, so the next operation (for example the relu in below subgraph)
  // doesn't need to insert dequantize:
  //
  //                                         [input]         [weight]
  //     [input]   [weight]                     |                |
  //         \        /                     dequantize       dequantize
  //           Matmul              =>               \          /
  //              |                                    Matmul
  //             Relu                                     |
  //              |                                      Relu
  //           [output]                                   |
  //                                                   [output]
  absl::flat_hash_map<OperandId, TensorInfo> operand_to_tensor_info_map_;

  // The following std::vector<Offset<tflite:XXX>>> stores the weights of model
  // and the tensor information (shape, data type).
  //
  // The first entry of this `tflite::Buffer` array must be an empty buffer.
  // The tensor index in the `tflite::Tensor` array is used to create `Operator`
  // and `SubGraph`.
  std::vector<BufferOffset> buffers_;
  std::vector<TensorOffset> tensors_;

  // Rather than serializing buffer contents into the Flatbuffer we store an
  // offset into this vector, which avoids the 2GB size limit.
  // TODO(https://crbug.com/383999372): Write this to a file instead of holding
  // it in memory.
  std::vector<uint8_t> buffer_data_;

  // The following std::vector<Offset<tflite:XXX>>> stores all operator
  // information including operator type, the index of input output tensor to
  // get tensor's information.
  //
  // The list of all operator codes used in this model. This is kept in order
  // because operators carry an index into this std::vector.
  std::vector<OperatorCodeOffset> operator_codes_;
  std::vector<OperatorOffset> operators_;
  // The cast operators convert float32 to float16 for the graph output operand,
  // hold the cast operator to insert after the unsupported float16 inference
  // operation.
  std::vector<OperatorOffset> graph_output_cast_operators_;

  // output_operand_id -> [dequantize_operation_index, serialized].
  // Tracks dequantizeLinear operations to be lazily serialized.
  base::flat_map<OperandId, std::pair<OperationId, bool>>
      lazy_serialized_dequantize_operations_;

  base::flat_set<OperationId> quantize_ops_to_skip_;

  // Mapping of the offset to scale_operand_id and zero_point_operand_id.
  // Because there is no way to retrieve the underlying data from the flatbuffer
  // offset, we store mapping to constant operands to check when encountering
  // Q->DQ, whether their quantization params match.
  base::flat_map<QuantizateParametersOffset::offset_type,
                 std::pair<OperandId, OperandId>>
      quantize_param_data_;
};

}  // namespace tflite
}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_BUILDER_TFLITE_H_
