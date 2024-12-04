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
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/types/expected.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/onnx/proto/onnx.pb.h"

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
                base::span<const uint32_t> shape,
                onnx::TensorProto::DataType onnx_data_type);
    OperandInfo(OperandInfo&);
    OperandInfo(OperandInfo&&);
    ~OperandInfo();

    std::string name;
    std::vector<uint32_t> shape;
    onnx::TensorProto::DataType onnx_data_type;
  };

  struct Result {
    explicit Result(base::FilePath working_directory);
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    ~Result();

    const base::FilePath& GetModelFilePath();

    [[nodiscard]] const OperandInfo& GetOperandInfo(uint64_t operand_id) const;

    [[nodiscard]] const std::map<uint64_t, OperandInfo>&
    id_to_operand_info_map() const;

    const base::FilePath working_directory;
    std::map<uint64_t, OperandInfo> operand_infos;
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
                 const base::FilePath& working_directory);

  GraphBuilderOrt(const GraphBuilderOrt&) = delete;
  GraphBuilderOrt& operator=(const GraphBuilderOrt&) = delete;

  ~GraphBuilderOrt();

 private:
  GraphBuilderOrt(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::FilePath working_directory);

  const mojom::Operand& GetOperand(uint64_t operand_id);
  std::string GetOperandName(uint64_t operand_id);

  void AddInput(uint64_t input_id);
  void AddOutput(uint64_t output_id);
  void AddInitializer(uint64_t constant_id);

  template <typename T>
  void AddUnaryOperation(const T& operation, std::string op_type);

  void AddElementWiseUnaryOperation(
      const mojom::ElementWiseUnary& element_wise_unary);
  void AddCastOperation(const mojom::ElementWiseUnary& cast);
  void AddLogicalNotOperation(const mojom::ElementWiseUnary& logical_not);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> BuildModel();

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> SerializeModel();

  // A reference to the WebNN compute graph that `this` instance is converting
  // to ONNX model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
      constant_operands_;

  const ContextProperties context_properties_;

  onnx::ModelProto model_;

  std::unique_ptr<Result> result_;
};

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_GRAPH_BUILDER_ORT_H_
