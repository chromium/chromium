// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TFLITE_CONVERTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TFLITE_CONVERTER_H_

#include "base/types/expected.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace blink {

class MLOperand;
class MLOperator;

// This class converts WebNN graph to tflite model and persist into FlatBuffer.
// The schema_generated.h file defines the format for each data structure to
// serialize.
//
// The instances of the class may not be allocated on the heap, but as a member
// variable of a non-stack-allocated and be single-use per conversion.
class MLGraphTfLiteConverter final {
  STACK_ALLOCATED();

 public:
  MLGraphTfLiteConverter();
  ~MLGraphTfLiteConverter();

  MLGraphTfLiteConverter(const MLGraphTfLiteConverter&) = delete;
  MLGraphTfLiteConverter& operator=(const MLGraphTfLiteConverter&) = delete;

  // Serialize tensor for input, constant and output operand. It's output
  // operand of graph if the `graph_output_name` is specified, returns the index
  // in the `tflite::Tensor` array if it's successful.
  int32_t SerializeTensor(
      const MLOperand* operand,
      absl::optional<String> graph_output_name = absl::nullopt);

  // The following steps implement the `SerializeOperation` function:
  // 1. Create `tflite::OperatorCode` with the kind of operator.
  // 2. Create `tflite::Operator` with the tensor index of inputs and outputs
  //    operand.
  //
  // Returns error messages if it could not be serialized because of unsupported
  // options or it is otherwise invalid.
  base::expected<void, String> SerializeOperation(
      const HeapHashMap<Member<const MLOperand>, int32_t>& operand_index_map,
      const MLOperator* ml_operator);

  // There are no further methods should be called on this class after this
  // function because the buffer of `FlatBufferBuilder` have been owned by the
  // detached buffer.
  flatbuffers::DetachedBuffer FinishAndTakeFlatBuffer();

 private:
  // Serializes the constant data (e.g. weights) to the flat buffer and returns
  // the index in the `tflite::Buffer` array if it's successful.
  //
  // The `Buffer` in TF-Lite schema is the table of raw data buffers, it is used
  // for WebNN constant operations. Referenced by tensors with the index of
  // buffer.
  uint32_t SerializeBuffer(const MLOperand* constant);

  flatbuffers::FlatBufferBuilder builder_;
  // `is_created_model_` indicates whether the tflite model is created and the
  // detached buffer owns the buffer and its allocator of the `builder_`.
  bool is_created_model_{false};

  // Keep track of input output tensor ids.
  Vector<int32_t> graph_input_ids_;
  Vector<int32_t> graph_outputs_ids_;

  // The following Vector<Offset<tflite:XXX>>> stores the weights of model and
  // the tensor information (shape, data type).
  //
  // The first entry of this `tflite::Buffer` array must be an empty buffer.
  // The tensor index in the `tflite::Tensor` array is used to create `Operator`
  // and `SubGraph`.
  Vector<flatbuffers::Offset<tflite::Buffer>> buffers_;
  Vector<flatbuffers::Offset<tflite::Tensor>> tensors_;

  // The following Vector<Offset<tflite:XXX>>> stores all operator information
  // including operator type, the index of input output tensor to get tensor's
  // information.
  //
  // The list of all operator codes used in this model. This is kept in order
  // because operators carry an index into this vector.
  Vector<flatbuffers::Offset<tflite::OperatorCode>> operator_codes_;
  Vector<flatbuffers::Offset<tflite::Operator>> operators_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TFLITE_CONVERTER_H_
