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
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/types/expected.h"
#include "services/webnn/ort/allocator_ort.h"
#include "services/webnn/ort/ort_model_builder.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

class WebNNConstantOperand;

namespace ort {

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

    const OperandInfo& GetOperandInfo(uint64_t operand_id) const;

    std::map<uint64_t, OperandInfo> id_to_operand_info;

    std::unique_ptr<OrtModelBuilder::ModelInfo> model_info;
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
                 scoped_refptr<AllocatorOrt> allocator);

  GraphBuilderOrt(const GraphBuilderOrt&) = delete;
  GraphBuilderOrt& operator=(const GraphBuilderOrt&) = delete;

  ~GraphBuilderOrt();

 private:
  GraphBuilderOrt(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      scoped_refptr<AllocatorOrt> allocator);

  const mojom::Operand& GetOperand(uint64_t operand_id);
  std::string GetOperandName(uint64_t operand_id);

  // Create a new initializer for the graph with the given shape, data and data
  // type, return the name of the initializer.
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
  template <typename T>
  std::string CreateInitializer(base::span<const uint32_t> shape,
                                base::span<const T> data,
                                OperandDataType data_type);

  void AddInput(uint64_t input_id);
  void AddOutput(uint64_t output_id);

  // Similar to the `CreateInitializer` above, add an initializer to the graph
  // with the given constant from WebNN.
  void AddInitializer(uint64_t constant_id);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddBatchNormalizationOperation(
      const mojom::BatchNormalization& batch_normalization);

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
  void AddConv2dOperation(const mojom::Conv2d& conv2d);
  void AddExpandOperation(const mojom::Expand& expand);
  void AddGatherOperation(const mojom::Gather& gather);
  void AddGemmOperation(const mojom::Gemm& gemm);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddInstanceNormalizationOperation(
      const mojom::InstanceNormalization& instance_normalization);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddLayerNormalizationOperation(
      const mojom::LayerNormalization& layer_normalization);
  void AddLogicalNotOperation(const mojom::ElementWiseUnary& logical_not);
  void AddMatMulOperation(const mojom::Matmul& matmul);
  void AddPool2dOperation(const mojom::Pool2d& pool2d);
  void AddReduceOperation(const mojom::Reduce& reduce);
  void AddReshapeOperation(const mojom::Reshape& reshape);
  void AddSliceOperation(const mojom::Slice& slice);
  void AddSoftmaxOperation(const mojom::Softmax& softmax);
  void AddTransposeOperation(const mojom::Transpose& transpose);
  void AddWhereOperation(const mojom::Where& where);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> BuildModel();

  // Used for inserting new operands into graph.
  uint64_t next_operand_id_ = 0;

  // A reference to the WebNN compute graph that `this` instance is converting
  // to ONNX model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
      constant_operands_;

  const ContextProperties context_properties_;

  OrtModelBuilder model_builder_;

  std::unique_ptr<Result> result_;
};

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
