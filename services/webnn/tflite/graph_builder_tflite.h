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
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
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
concept IsSupportedTensorType = IsAnyOf<T, float, int32_t, uint32_t, int64_t>;

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
  GraphBuilderTflite(const GraphBuilderTflite&) = delete;
  GraphBuilderTflite& operator=(const GraphBuilderTflite&) = delete;

  // Factory method that creates a GraphBuilderTflite and builds a TFLite
  // Flatbuffer Returns unexpected if it fails.
  [[nodiscard]] static base::expected<flatbuffers::DetachedBuffer, std::string>
  CreateAndBuild(
      ContextProperties context_properties,
      const mojom::GraphInfo& graph_info,
      const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands);

  static ContextProperties GetContextProperties();

 private:
  using IdToOperandMap = base::flat_map<uint64_t, mojom::OperandPtr>;
  using OperatorCodeOffset = flatbuffers::Offset<::tflite::OperatorCode>;
  using OperatorOffset = flatbuffers::Offset<::tflite::Operator>;
  using BufferOffset = flatbuffers::Offset<::tflite::Buffer>;
  using TensorOffset = flatbuffers::Offset<::tflite::Tensor>;
  using StringOffset = flatbuffers::Offset<flatbuffers::String>;

  GraphBuilderTflite(
      ContextProperties context_properties,
      const mojom::GraphInfo& graph_info,
      const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands);
  ~GraphBuilderTflite();

  // Maps to WebNN operand information.
  struct TensorInfo {
    TensorInfo();
    TensorInfo(int32_t index,
               ::tflite::TensorType data_type,
               base::span<const int32_t> dimensions);
    ~TensorInfo();

    // Copyable and movable.
    TensorInfo(const TensorInfo&);
    TensorInfo& operator=(const TensorInfo&);
    TensorInfo(TensorInfo&& other);
    TensorInfo& operator=(TensorInfo&& other);

    int32_t index;
    ::tflite::TensorType data_type;
    std::vector<int32_t> dimensions;
  };

  // Serialize tensor for input, constant and output operand and return the
  // tensor information if it's successful. The `override_tensor_type` is used
  // to override the tensor type, such as when dequantising a float16 operator
  // to float32 before serializing an operator which does not support float32.
  base::expected<TensorInfo, std::string> SerializeOperand(
      uint64_t operand_id,
      std::optional<::tflite::TensorType> override_tensor_type = std::nullopt);

  // Call `SerializeOperand` to serialize the input operand if it's not
  // serialized, and insert a TFLite dequantize operator to convert float16 to
  // float32 for graph input, constant and intermediate operands if the current
  // operation doesn't support float16 inference (`operation_supports_float16`
  // is false).
  base::expected<TensorInfo, std::string> SerializeInputTensorInfo(
      uint64_t operand_id,
      bool operation_supports_float16 = false);

  // Call `SerializeOperand` to serialize the output operand and insert a TFLite
  // cast operator to convert float32 to float16 if the operand is graph output
  // and the current operation doesn't support float16 inference. The
  // `override_tensor_type` is used to override the output tensor type the same
  // as input, for example the input data type has been overridden to float32 of
  // intermediate operands (Reshape), so the output tensor type should be
  // float32 with the argument.
  base::expected<TensorInfo, std::string> SerializeOutputTensorInfo(
      uint64_t operand_id,
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
      const mojom::Operation& op);

  // Serializes the constant data (e.g. weights) to the flat buffer and returns
  // the index in the `tflite::Buffer` array if it's successful.
  //
  // The `Buffer` in TFLite schema is the table of raw data buffers, it is used
  // for WebNN constant operations. Referenced by tensors with the index of
  // buffer.
  uint32_t SerializeBuffer(base::span<const uint8_t> buffer);

  // Serializes `buffer` as a tensor with the given `dimensions` and `type `to
  // the flat buffer and returns the index in `tensors_` if it's successful.
  template <typename DataType>
    requires internal::IsSupportedTensorType<DataType>
  int32_t SerializeTensorWithBuffer(base::span<const DataType> buffer,
                                    base::span<const int32_t> dimensions);

  // Serializes temporary tensor for casting the data type of input operand or
  // decomposing WebNN operation that isn't supported in TFLite schema.
  int32_t SerializeTemporaryTensor(base::span<const int32_t> dimensions,
                                   ::tflite::TensorType tensor_type);

  uint32_t GetOperatorCodeIndex(::tflite::BuiltinOperator code,
                                int32_t version = 1);

  // Returns the Operand corresponding to an `operand_id` from `graph_info_`.
  // Will crash if `graph_info_` does not contain `operand_id`.
  const mojom::Operand& GetOperand(uint64_t operand_id) const;

  // Operation serialization helpers for operations not directly declared in the
  // mojom::Operation union.
  //
  // Serialize an operation with a single input and a single output operand.
  // The caller must either provide both `builtin_options_type` and
  // `builtin_options`, or neither.
  OperatorOffset SerializeUnaryOperation(
      ::tflite::BuiltinOperator code,
      int32_t input_tensor_index,
      int32_t output_tensor_index,
      ::tflite::BuiltinOptions builtin_options_type =
          ::tflite::BuiltinOptions_NONE,
      flatbuffers::Offset<void> builtin_options = 0);
  OperatorOffset SerializeCastOperation(
      int32_t input_tensor_index,
      ::tflite::TensorType input_tensor_type,
      int32_t output_tensor_index,
      ::tflite::TensorType output_tensor_type);

  // A helper function is used by WebNN binary operation direct or emulated
  // implementation.
  OperatorOffset SerializeBinaryOperation(::tflite::BuiltinOperator code,
                                          int32_t lhs_tensor_index,
                                          int32_t rhs_tensor_index,
                                          int32_t output_tensor_index);

  // This function is called by `SerializeConcat` to serialize WebNN
  // concat operator or used to emulate WebNN operations.
  OperatorOffset SerializeConcatOperation(
      base::span<const int32_t> input_tensor_indices,
      int32_t output_tensor_index,
      uint32_t axis);

  // This function serializes a TFLite dequantize operator to convert float16
  // data type to float32.
  int32_t SerializeDequantizeOperation(
      int32_t input_tensor_index,
      base::span<const int32_t> input_dimensions);

  // This function is called by `SerializeMatmul` to serialize WebNN
  // matmul operator or used to emulate WebNN operations.
  OperatorOffset SerializeMatmulOperation(int32_t a_tensor_index,
                                          int32_t b_tensor_index,
                                          int32_t output_tensor_index);

  // A helper function is used to emulate batch, layer or instance
  // normalization.
  OperatorOffset SerializeNormalizationOperation(
      base::span<const int32_t> input_dimensions,
      ::tflite::TensorType input_tensor_type,
      int32_t input_tensor_index,
      int32_t output_tensor_index,
      int32_t mean_tensor_index,
      int32_t variance_tensor_index,
      float epsilon,
      std::optional<int32_t> scale_tensor_index,
      std::optional<int32_t> bias_tensor_index);

  // Compute the means and variance values for the instance and layer
  // normalization.
  std::tuple<int32_t, int32_t> ComputeMeanAndVarianceForNormalization(
      base::span<const int32_t> input_dimensions,
      ::tflite::TensorType input_tensor_type,
      int32_t input_tensor_index,
      base::span<const int32_t> axes);
  int32_t TransposeAndReshapeLayerNormalizationScaleBias(
      base::span<const int32_t> input_dimensions,
      const TensorInfo& scale_or_bias_tensor_info,
      base::span<const uint32_t> axes);

  // This function is called by `SerializeReduce` to serialize WebNN
  // reduce operators or used to emulate WebNN operations.
  OperatorOffset SerializeReduceOperation(
      ::tflite::BuiltinOperator operator_code,
      int32_t input_tensor_index,
      int32_t output_tensor_index,
      base::span<const int32_t> axes,
      bool keep_dimensions);

  // This function is called by `SerializeReshape` to serialize WebNN
  // reshape operator or used to emulate WebNN operations.
  OperatorOffset SerializeReshapeOperation(int32_t input_tensor_index,
                                           int32_t output_tensor_index,
                                           base::span<const int32_t> new_shape);

  // This function is called by `SerializeSlice` to serialize WebNN
  // slice operator or used to emulate WebNN operations.
  base::expected<OperatorOffset, std::string> SerializeSliceOperation(
      int32_t input_tensor_index,
      int32_t output_tensor_index,
      base::span<const int32_t> slice_starts,
      base::span<const int32_t> slice_sizes);

  // This function is called by `SerializeLinear` to serialize WebNN linear or
  // used to emulate WebNN operation that isn't supported in TFLite schema.
  OperatorOffset SerializeLinearOperation(
      base::span<const int32_t> input_dimensions,
      ::tflite::TensorType input_tensor_type,
      int32_t input_tensor_index,
      int32_t output_tensor_index,
      float alpha,
      float beta);

  // This function is called by `SerializeTranspose` to serialize WebNN
  // transpose operator or used to insert a tempary operator to transpose
  // different layout.
  OperatorOffset SerializeTransposeOperation(
      int32_t input_tensor_index,
      int32_t output_tensor_index,
      base::span<const uint32_t> permutation);

  // Insert a tempary pad operation if the `paddings` can't be converted to
  // tflite padding mode.
  base::expected<int32_t, std::string> InsertPadOperation(
      const TensorInfo& input_tensor_info,
      base::span<const uint32_t> paddings);

  // Insert a tempary transpose operation for input operand with calling
  // `SerializeTransposeOperation`.
  int32_t InsertTransposeOperation(base::span<const int32_t> input_dimensions,
                                   ::tflite::TensorType input_tensor_type,
                                   int32_t input_tensor_index,
                                   base::span<const uint32_t> permutation);

  // Serialize a sub graph (pow appending mul operation) for erf operation.
  int32_t SerializeSubGraphPowMul(base::span<const int32_t> input_dimensions,
                                  ::tflite::TensorType input_tensor_type,
                                  int32_t input_tensor_index,
                                  float pow_exponent,
                                  float mul_alpha);

  // Serialize a sub graph (input * weight + bias) for gru cell.
  int32_t SerializeSubGraphMatmulAdd(base::span<const int32_t> input_dimensions,
                                     ::tflite::TensorType input_tensor_type,
                                     int32_t input_tensor_index,
                                     int32_t weight_tensor_index,
                                     std::optional<int32_t> bias_tensor_index);

  // Serialize a sub graph (slice appending transpose operation) for gru cell.
  base::expected<int32_t, std::string> SerializeSubGraphSliceTranspose(
      ::tflite::TensorType input_tensor_type,
      int32_t input_tensor_index,
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
        int32_t input_tensor_index,
        int32_t weight_tensor_index,
        int32_t recurrent_weight_tensor_index,
        std::optional<int32_t> bias_tensor_index,
        std::optional<int32_t> recurrent_bias_tensor_index,
        int32_t hidden_state_tensor_index,
        int32_t hidden_size,
        base::span<const mojom::RecurrentNetworkActivation> activations);
    ~RecurrentNetworkBase();

    RecurrentNetworkBase(const RecurrentNetworkBase&) = delete;
    RecurrentNetworkBase& operator=(const RecurrentNetworkBase&) = delete;

    RecurrentNetworkBase(RecurrentNetworkBase&&) = delete;
    RecurrentNetworkBase& operator=(RecurrentNetworkBase&&) = delete;

    base::span<const int32_t> input_dimensions;
    const ::tflite::TensorType input_tensor_type;
    const int32_t input_tensor_index;
    const int32_t weight_tensor_index;
    const int32_t recurrent_weight_tensor_index;
    const std::optional<int32_t> bias_tensor_index;
    const std::optional<int32_t> recurrent_bias_tensor_index;
    const int32_t hidden_state_tensor_index;
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
        int32_t input_tensor_index,
        int32_t output_tensor_index,
        int32_t weight_tensor_index,
        int32_t recurrent_weight_tensor_index,
        std::optional<int32_t> bias_tensor_index,
        std::optional<int32_t> recurrent_bias_tensor_index,
        int32_t hidden_state_tensor_index,
        int32_t hidden_size,
        bool reset_after,
        mojom::GruWeightLayout layout,
        base::span<const mojom::RecurrentNetworkActivation> activations);
    ~GruCellOperation();

    const int32_t output_tensor_index;
    const bool reset_after;
    const mojom::GruWeightLayout layout;
  };

  // A helper function for serializing update, reset and new gate, the argument
  // `reset_gate_tensor_index` only be used for new gate.
  base::expected<int32_t, std::string> SerializeGruGate(
      const GruCellOperation& gru_cell,
      GruGateType type,
      std::optional<int32_t> reset_gate_tensor_index = std::nullopt);

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
        int32_t input_tensor_index,
        base::span<const int32_t> output_tensor_indices,
        int32_t weight_tensor_index,
        int32_t recurrent_weight_tensor_index,
        std::optional<int32_t> bias_tensor_index,
        std::optional<int32_t> recurrent_bias_tensor_index,
        int32_t hidden_state_tensor_index,
        int32_t hidden_size,
        int32_t cell_state_tensor_index,
        std::optional<int32_t> peephole_weight_tensor_index,
        mojom::LstmWeightLayout layout,
        base::span<const mojom::RecurrentNetworkActivation> activations);
    ~LstmCellOperation();

    base::span<const int32_t> output_tensor_indices;
    const int32_t cell_state_tensor_index;
    std::optional<int32_t> peephole_weight_tensor_index;
    const mojom::LstmWeightLayout layout;
  };

  // A helper function for serializing input, forget, cell and output gate.
  base::expected<int32_t, std::string> SerializeLstmGate(
      const LstmCellOperation& lstm_cell,
      LstmGateType type);

  // This function is called by `SerializeLstmCell` to serialize WebNN lstmCell
  // or used to emulate lstm operation.
  base::expected<OperatorOffset, std::string> SerializeLstmCellOperation(
      const LstmCellOperation& lstm_cell);

  // Get initial hidden and cell state tensor index if existed or serialize an
  // empty tensor.
  base::expected<int32_t, std::string> GetInitialHiddenAndCellState(
      std::optional<uint64_t> state_operand_id,
      base::span<const int32_t> state_dimensions);

  // Reshape hidden and cell state, concat the reshaped tensor if the input
  // tensor of concat is provided.
  int32_t ReshapeHiddenAndCellState(
      ::tflite::TensorType input_tensor_type,
      int32_t input_tensor_index,
      base::span<const int32_t> new_shape,
      std::optional<int32_t> concat_input_tensor_index,
      base::span<const int32_t> concat_output_shape);

  // Serialize a sub graph (slice appending squeeze operation) for gru.
  base::expected<int32_t, std::string> SerializeSubGraphSliceSqueeze(
      ::tflite::TensorType input_tensor_type,
      int32_t input_tensor_index,
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
  base::expected<OperatorOffset, std::string> SerializeReciprocal(
      const TensorInfo& input_tensor_info,
      const TensorInfo& output_tensor_info);
  base::expected<OperatorOffset, std::string> SerializeReduce(
      const mojom::Reduce& reduce);
  base::expected<OperatorOffset, std::string> SerializeReduceSumSquare(
      const TensorInfo& input_tensor_info,
      base::span<const int32_t> axes,
      bool keep_dimensions,
      int32_t output_tensor_index);
  base::expected<OperatorOffset, std::string> SerializeRelu(
      const mojom::Relu& relu);
  base::expected<OperatorOffset, std::string> SerializeResample2d(
      const mojom::Resample2d& resample2d);
  base::expected<OperatorOffset, std::string> SerializeReshape(
      uint64_t input_operand_id,
      uint64_t output_operand_id);
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

  // No further methods may be called on this class after calling this method
  // because the buffer of `buffer_` is now owned by the detached buffer.
  flatbuffers::DetachedBuffer FinishAndTakeFlatBuffer(
      base::span<const uint64_t> input_operands,
      base::span<const uint64_t> output_operands);

  const ContextProperties context_properties_;

  // A reference to the WebNN compute graph that `this` instance is converting
  // to TFLite. The creator of `this` must ensure the GraphInfo reference passed
  // into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  // A reference to the constant operands used by this graph. The creator of
  // `this` must ensure this reference is valid for as long as `this` exists.
  base::raw_ref<
      const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>>
      constant_operands_;

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
  std::map<uint64_t, TensorInfo> operand_to_tensor_info_map_;

  // The following std::vector<Offset<tflite:XXX>>> stores the weights of model
  // and the tensor information (shape, data type).
  //
  // The first entry of this `tflite::Buffer` array must be an empty buffer.
  // The tensor index in the `tflite::Tensor` array is used to create `Operator`
  // and `SubGraph`.
  std::vector<BufferOffset> buffers_;
  std::vector<TensorOffset> tensors_;

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
};

}  // namespace tflite
}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_BUILDER_TFLITE_H_
