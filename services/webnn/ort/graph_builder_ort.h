// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
#define SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/types/expected.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

namespace webnn {

class WebNNContextImpl;
class WebNNConstantOperand;

namespace ort {

class ContextImplOrt;

class GraphBuilderOrt {
  STACK_ALLOCATED();

 public:
  // Tracks Operand information during graph building, so that
  // future operations can look them up based on operand id.
  // When an operation is decomposed, additional `OperandInfo` entities are
  // created to represent intermediate layers.
  struct OperandInfo {
    OperandInfo();
    OperandInfo(std::string name,
                OperandDataType data_type,
                base::span<const uint32_t> uint32_shape);
    OperandInfo(OperandInfo&);
    OperandInfo(OperandInfo&&);
    ~OperandInfo();

    std::string name;
    ONNXTensorElementDataType onnx_data_type;
    std::vector<int64_t> int64_shape;
  };

  struct Result {
    explicit Result();
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    ~Result();

    const ScopedOrtModel& GetModel();

    const OperandInfo& GetOperandInfo(uint64_t operand_id) const;

    const std::map<uint64_t, OperandInfo>& id_to_operand_info_map() const;

    ScopedOrtModel model;
    std::map<uint64_t, OperandInfo> operand_infos;

    // TODO: Consider reusing constant operands instead of copying them to
    // `weights`.
    //
    // Store the weights which should be alive for inference session.
    std::vector<base::HeapArray<uint8_t>> weights;
  };

  // Factory method that creates a GraphBuilderOrt, builds and serializes the
  // ONNX model to the `working_directory`. This expects the
  // `working_directory` to be an empty directory.
  //
  // Returns unexpected if it fails.
  [[nodiscard]] static base::expected<std::unique_ptr<Result>, mojom::ErrorPtr>
  CreateAndBuild(const mojom::GraphInfo& graph_info,
                 ContextProperties context_properties,
                 base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
                     constant_operands,
                 ContextImplOrt* context);

  GraphBuilderOrt(const GraphBuilderOrt&) = delete;
  GraphBuilderOrt& operator=(const GraphBuilderOrt&) = delete;

  ~GraphBuilderOrt();

 private:
  GraphBuilderOrt(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      ContextImplOrt* context);

  const mojom::Operand& GetOperand(uint64_t operand_id);
  std::string GetOperandName(uint64_t operand_id);

  // Some initializers must be uploaded to raw data, e.g. Reshape op needs
  // parameter *shape* as raw data to do shape inference.
  //
  // IssueA: Are there other ops requiring initializer as raw data? Currently we
  // upload all constants into external data (for potential zero-copy) but what
  // if some ops like Reshape can't take external data as initializers?
  //
  // Create a new initializer copied into graph.
  uint64_t NewInitializerAsRawData(base::span<const uint32_t> shape,
                                   base::span<const uint8_t> data,
                                   OperandDataType data_type);

  void AddInput(uint64_t input_id);
  void AddOutput(uint64_t output_id);

  // TODO: Figure out whether to upload constants to external data or raw data
  // in graph. See IssueA.
  // Add initializer to external data.
  void AddInitializer(uint64_t constant_id);

  template <typename T>
  void AddBinaryOperation(const T& operation, std::string op_type);

  template <typename T>
  void AddUnaryOperation(const T& operation, std::string_view op_type);

  void AddElementWiseBinaryOperation(
      const mojom::ElementWiseBinary& element_wise_binary);
  void AddElementWiseUnaryOperation(
      const mojom::ElementWiseUnary& element_wise_unary);
  void AddCastOperation(const mojom::ElementWiseUnary& cast);
  void AddClampOperation(const mojom::Clamp& clamp);
  void AddGemmOperation(const mojom::Gemm& gemm);
  void AddLogicalNotOperation(const mojom::ElementWiseUnary& logical_not);
  void AddReshapeOperation(const mojom::Reshape& reshape);
  void AddSoftmaxOperation(const mojom::Softmax& softmax);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> BuildModel();

  // WebNNContextImpl owns this object.
  const raw_ptr<WebNNContextImpl> context_;

  // Used for inserting new operands into graph.
  uint64_t next_operand_id_ = 0;

  // A reference to the WebNN compute graph that `this` instance is converting
  // to ONNX model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
      constant_operands_;

  const ContextProperties context_properties_;

  ScopedOrtGraph graph_;

  std::unique_ptr<Result> result_;
};

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
