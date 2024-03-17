// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_GRAPH_BUILDER_H_
#define SERVICES_WEBNN_TFLITE_GRAPH_BUILDER_H_

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
#include "services/webnn/public/mojom/webnn_graph.mojom-forward.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace webnn::tflite {

// Methods which take a generic numerical type as input (e.g. uint32_t) and
// expect to serialize the data as a TFLite tensor (e.g.
// ::tflite::TensorType_UINT32) may use the `IsSupportedTensorType` concept to
// enforce that the data type maps to a supported TFLite tensor type.
// The list of supported data types may be expanded as needed.
template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);
template <typename T>
concept IsSupportedTensorType = IsAnyOf<T, float, int32_t, uint32_t>;

// This class converts WebNN graph to tflite model and persist into FlatBuffer.
// The schema_generated.h file defines the format for each data structure to
// serialize.
//
// The instances of the class may not be allocated on the heap, but as a member
// variable of a non-stack-allocated and be single-use per conversion.
class GraphBuilder final {
  STACK_ALLOCATED();

 public:
  GraphBuilder(const GraphBuilder&) = delete;
  GraphBuilder& operator=(const GraphBuilder&) = delete;

  // Factory method that creates a GraphBuilder and builds a TFLite Flatbuffer
  // Returns unexpected if it fails.
  [[nodiscard]] static base::expected<flatbuffers::DetachedBuffer, std::string>
  CreateAndBuild(const mojom::GraphInfo& graph_info);

 private:
  using IdToOperandMap = base::flat_map<uint64_t, mojom::OperandPtr>;
  using OperatorCodeOffset = flatbuffers::Offset<::tflite::OperatorCode>;
  using OperatorOffset = flatbuffers::Offset<::tflite::Operator>;
  using BufferOffset = flatbuffers::Offset<::tflite::Buffer>;
  using TensorOffset = flatbuffers::Offset<::tflite::Tensor>;
  using StringOffset = flatbuffers::Offset<flatbuffers::String>;

  explicit GraphBuilder(const mojom::GraphInfo& graph_info);
  ~GraphBuilder();

  // Serialize tensor for input, constant and output operand. It's output
  // operand of graph if the `graph_output_name` is specified, returns the index
  // in the `tflite::Tensor` array if it's successful.
  base::expected<void, std::string> SerializeOperand(
      uint64_t operand_id,
      const mojom::Operand& operand);

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
  uint32_t SerializeBuffer(const mojo_base::BigBuffer& constant);

  // Serializes `buffer` as a tensor with the given `dimensions` and `type `to
  // the flat buffer and returns the index in `tensors_` if it's successful.
  template <typename DataType>
    requires IsSupportedTensorType<DataType>
  int32_t SerializeTensorWithBuffer(base::span<const DataType> buffer,
                                    base::span<const int32_t> dimensions);

  uint32_t GetOperatorCodeIndex(::tflite::BuiltinOperator code);

  // Returns the Operand corresponding to an `operand_id` from `graph_info_`.
  // Will craash if `graph_info_` does not contain `operand_id`.
  const mojom::Operand& GetOperand(uint64_t operand_id) const;

  // Operation serialization helpers for operations not directly declared in the
  // mojom::Operation union.
  //
  // Serialize an operation with a single input and a single output operand.
  // The caller must either provide both `builtin_options_type` and
  // `builtin_options`, or neither.
  OperatorOffset SerializeUnaryOperation(
      ::tflite::BuiltinOperator code,
      uint64_t input_operand_id,
      uint64_t output_operand_id,
      ::tflite::BuiltinOptions builtin_options_type =
          ::tflite::BuiltinOptions_NONE,
      flatbuffers::Offset<void> builtin_options = 0);
  OperatorOffset SerializeCastOperation(uint64_t input_operand_id,
                                        uint64_t output_operand_id);

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
      const mojom::Operand& input_operand,
      int32_t input_tensor_index,
      base::span<const uint32_t> paddings);

  // Insert a tempary transpose operation for input operand with calling
  // `SerializeTransposeOperation`.
  int32_t InsertTransposeOperation(const mojom::Operand& input_operand,
                                   int32_t input_tensor_index,
                                   base::span<const uint32_t> permutation);

  // Serialize functions for members of the mojom::Operation union. Keep these
  // functions in the same order as in webnn_graph.mojom.
  base::expected<OperatorOffset, std::string> SerializeClamp(
      const mojom::Clamp& clamp);
  base::expected<OperatorOffset, std::string> SerializeConv2d(
      const mojom::Conv2d& conv2d);
  OperatorOffset SerializeConcat(const mojom::Concat& concat);
  base::expected<OperatorOffset, std::string> SerializeElementWiseBinary(
      const mojom::ElementWiseBinary& op);
  base::expected<OperatorOffset, std::string> SerializeElementWiseUnary(
      const mojom::ElementWiseUnary& op);
  base::expected<OperatorOffset, std::string> SerializeElu(
      const mojom::Elu& elu);
  base::expected<OperatorOffset, std::string> SerializeGemm(
      const mojom::Gemm& gemm);
  OperatorOffset SerializeHardSwish(const mojom::HardSwish& hard_swish);
  OperatorOffset SerializeLeakyRelu(const mojom::LeakyRelu& leaky_relu);
  base::expected<OperatorOffset, std::string> SerializePad(
      const mojom::Pad& pad);
  base::expected<OperatorOffset, std::string> SerializePool2d(
      const mojom::Pool2d& pool2d);
  OperatorOffset SerializeRelu(const mojom::Relu& relu);
  base::expected<OperatorOffset, std::string> SerializeResample2d(
      const mojom::Resample2d& resample2d);
  base::expected<OperatorOffset, std::string> SerializeReshape(
      const mojom::Reshape& reshape);
  OperatorOffset SerializeSigmoid(const mojom::Sigmoid& sigmoid);
  base::expected<OperatorOffset, std::string> SerializeSlice(
      const mojom::Slice& slice);
  OperatorOffset SerializeSoftmax(const mojom::Softmax& softmax);
  OperatorOffset SerializeTranspose(const mojom::Transpose& transpose);

  // There are no further methods should be called on this class after this
  // function because the buffer of `buffer_` is now owned by the detached
  // buffer.
  flatbuffers::DetachedBuffer FinishAndTakeFlatBuffer(
      base::span<const uint64_t> input_operands,
      base::span<const uint64_t> output_operands);

  // A reference to the WebNN compute graph that `this` instance is converting
  // to TFLite. The creator of `this` must ensure the GraphInfo reference passed
  // into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  flatbuffers::FlatBufferBuilder builder_;
  // `is_created_model_` indicates whether the tflite model is created and the
  // detached buffer owns the buffer and its allocator of the `builder_`.
  bool is_created_model_ = false;

  // Map from operand IDs in the GraphInfo structure to tensor indices in the
  // flat buffer.
  std::map<uint64_t, int32_t> operand_to_index_map_;

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
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_GRAPH_BUILDER_H_
