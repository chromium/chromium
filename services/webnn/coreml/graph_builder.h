// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_
#define SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/types/expected.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/coremltools/mlmodel/format/MIL.pb.h"
#include "third_party/coremltools/mlmodel/format/Model.pb.h"

namespace webnn::coreml {

struct Float16 {
  uint16_t data;
};

namespace internal {
// Supported tensor types for immediate values. The list can be expanded as
// needed.
template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);
template <typename T>
concept IsSupportedTensorType = IsAnyOf<T,
                                        Float16,
                                        float,
                                        int32_t,
                                        char,
                                        bool>;
}  // namespace internal

inline constexpr char kPlaceholderInputName[] = "placeholder";

// Get name identifiers used in CoreML model files for input/output operands.
std::string GetCoreMLNameFromInput(std::string_view input_name);
std::string GetCoreMLNameFromOutput(std::string_view output_name);

// Reads the WebNN graph from the mojom::GraphInfo to
// produce CoreML model and serializes to provided `working_directory`.
// There is nothing macOS-specific in this class.
//
// The instances of the class may not be allocated on the heap, but as a member
// variable of a non-stack-allocated class and be single-use per conversion.
class GraphBuilder {
  STACK_ALLOCATED();

 public:
  // Tracks Operand information during graph building, so that
  // future operations can look them up based on operand id.
  //
  // For the inputs of the model, this information is exposed
  // publicly via FindInputOperandInfo.
  struct OperandInfo {
    OperandInfo();
    OperandInfo(std::string name, std::vector<uint32_t> dimensions,
                mojom::Operand::DataType data_type,
                CoreML::Specification::MILSpec::DataType mil_data_type);
    OperandInfo(OperandInfo&);
    OperandInfo(OperandInfo&&);
    ~OperandInfo();

    // Identifier for this operand in coreml model file.
    std::string coreml_name;
    std::vector<uint32_t> dimensions;
    mojom::Operand::DataType data_type;
    CoreML::Specification::MILSpec::DataType mil_data_type;
  };

  struct Result {
    explicit Result(base::FilePath ml_package_dir);
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    ~Result();

    // This method must be called with an `input_name` which corresponds to some
    // input, or else it will crash.
    const OperandInfo& FindInputOperandInfo(
        const std::string& input_name) const;
    const base::FilePath& GetModelFilePath();

    [[nodiscard]] const OperandInfo& GetOperandInfo(uint64_t operand_id) const;

    const base::FilePath ml_package_dir;
    // Used to get operand info to specify input for a MILSpec::Operation.
    std::map<std::string, uint64_t> input_name_to_id_map;
    std::map<uint64_t, OperandInfo> id_to_operand_info_map;
  };

  // Factory method that creates a GraphBuilder, builds and serializes the
  // CoreML model to the `working_directory`. This expects the
  // `working_directory` to be an empty directory.
  //
  // Returns unexpected if it fails.
  [[nodiscard]] static base::expected<std::unique_ptr<Result>, mojom::ErrorPtr>
  CreateAndBuild(const mojom::GraphInfo& graph_info,
                 const base::FilePath& working_directory);

  GraphBuilder(const GraphBuilder&) = delete;
  GraphBuilder& operator=(const GraphBuilder&) = delete;

  ~GraphBuilder();

 private:
  GraphBuilder(const mojom::GraphInfo& graph_info,
               base::FilePath ml_package_dir);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> BuildCoreMLModel();

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> SerializeModel();
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> WriteWeightsToFile(
      CoreML::Specification::MILSpec::Block& block);

  // No further methods may be called on this class after calling this method.
  [[nodiscard]] std::unique_ptr<Result> FinishAndTakeResult();

  // Add input in Model.description and in Program's main function inputs.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddInput(
      uint64_t input_id,
      CoreML::Specification::MILSpec::Function& main_function);
  void AddPlaceholderInput(
      CoreML::Specification::MILSpec::Function& main_function,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOutput(
      uint64_t output_id);

  // Serialization functions for members of the mojom::Operation union. Keep
  // these functions in the same order as in webnn_graph.mojom.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForCast(
      const std::string& input_name,
      uint64_t output_operand_id,
      webnn::mojom::Operand::DataType input_data_type,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForClamp(
      const mojom::Clamp& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForConcat(
      const mojom::Concat& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForConv2d(
      const mojom::Conv2d& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForElementwiseBinary(
      const mojom::ElementWiseBinary& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForElementwiseUnary(const mojom::ElementWiseUnary& operation,
                                  CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForPool2d(
      const mojom::Pool2d& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForRelu(
      const mojom::Relu& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForResample2d(
      const mojom::Resample2d& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSigmoid(
      const mojom::Sigmoid& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSoftsign(
      const mojom::Softsign& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForTanh(
      const mojom::Tanh& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForTranspose(
      const mojom::Transpose& operation,
      CoreML::Specification::MILSpec::Block& block);

  // Add constants as immediate values in the model file.
  base::expected<void, mojom::ErrorPtr> AddConstantImmediateValue(
      uint64_t constant_id,
      CoreML::Specification::MILSpec::Block& block);
  // Add constants to weight file.
  base::expected<void, mojom::ErrorPtr> AddConstantFileValue(
      uint64_t constant_id,
      uint64_t offset,
      CoreML::Specification::MILSpec::Block& block);
  // Populate generic fields that apply to both `AddConstantImmediateValue`
  // and `AddConstantFileValue`.
  base::expected<void, mojom::ErrorPtr> PopulateConstantOpFromOperand(
      uint64_t constant_id,
      CoreML::Specification::MILSpec::Operation& op);

  // Helpers.
  const mojom::Operand& GetOperand(uint64_t operand_id) const;

  [[nodiscard]] const OperandInfo& GetOperandInfo(uint64_t operand_id) const;
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  PopulateFeatureDescription(
      uint64_t operand_id,
      ::CoreML::Specification::FeatureDescription& feature_description);

  // Accessors for fields declared in `result_`.
  const base::FilePath& ml_package_dir() const {
    return result_->ml_package_dir;
  }
  std::map<std::string, uint64_t>& input_name_to_id_map() const {
    return result_->input_name_to_id_map;
  }
  std::map<uint64_t, OperandInfo>& id_to_operand_info_map() const {
    return result_->id_to_operand_info_map;
  }

  // MILSpec::Program's Function, Block, Operation's inputs/outputs could be
  // defined as NamedValueType.
  void PopulateNamedValueType(
      uint64_t operand_id,
      CoreML::Specification::MILSpec::NamedValueType& named_value_type);
  void PopulateNamedValueType(
      std::string_view name,
      CoreML::Specification::MILSpec::DataType mil_data_type,
      base::span<const uint32_t> dimensions,
      CoreML::Specification::MILSpec::NamedValueType& named_value_type);
  // Update the `id_to_op_input_info_map_` to be used by ops later.
  void UpdateCoreMLInputInfoMap(uint64_t operand_id);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  SetupMlPackageDirStructure();

  std::string GetCoreMLNameFromOperand(uint64_t operand_id);
  [[nodiscard]] std::string GenerateCoreMLNameForInternalOperand();

  // A reference to the WebNN compute graph that `this` instance is converting
  // to CoreML model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  // Used to generate unique names for internal operands generated for WebNN
  // operations that need to be decomposed into multiple CoreML operations.
  uint64_t internal_operand_id_ = 0;

  CoreML::Specification::Model ml_model_;
  raw_ptr<CoreML::Specification::MILSpec::Program> program_;

  std::unique_ptr<Result> result_;
};

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_
