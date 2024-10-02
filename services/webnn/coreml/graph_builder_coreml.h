// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_GRAPH_BUILDER_COREML_H_
#define SERVICES_WEBNN_COREML_GRAPH_BUILDER_COREML_H_

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/coremltools/mlmodel/format/MIL.pb.h"
#include "third_party/coremltools/mlmodel/format/Model.pb.h"

namespace webnn {

class WebNNConstantOperand;

namespace coreml {

struct Float16 {
  uint16_t data;
};

namespace internal {
// Supported tensor types for immediate values. The list can be expanded as
// needed.
template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);
template <typename T>
concept IsSupportedTensorType =
    IsAnyOf<T, Float16, float, int32_t, int8_t, char, bool>;
}  // namespace internal

inline constexpr char kPlaceholderInputName[] = "placeholder";

// Get name identifiers used in CoreML model files for input or output operands.
std::string GetCoreMLNameFromInput(std::string_view input_name,
                                   uint64_t operand_id);
std::string GetCoreMLNameFromOutput(std::string_view output_name,
                                    uint64_t operand_id);

// Reads the WebNN graph from the mojom::GraphInfo to
// produce CoreML model and serializes to provided `working_directory`.
// There is nothing macOS-specific in this class.
//
// The instances of the class may not be allocated on the heap, but as a member
// variable of a non-stack-allocated class and be single-use per conversion.
class GraphBuilderCoreml {
  STACK_ALLOCATED();

 public:
  // Tracks Operand information during graph building, so that
  // future operations can look them up based on operand id.
  // When an operation is decomposed, additional `OperandInfo` entities are
  // created to represent intermediate layers.
  struct OperandInfo {
    OperandInfo();
    OperandInfo(std::string name,
                base::span<const uint32_t> dimensions,
                CoreML::Specification::MILSpec::DataType mil_data_type);
    OperandInfo(OperandInfo&);
    OperandInfo(OperandInfo&&);
    ~OperandInfo();

    // Identifier for this operand in coreml model file.
    std::string coreml_name;
    // Due to the limitations of CoreML not supporting 0D input at model
    // entry point, model 0D inputs are splitted into two nodes, with the
    // external facing node that's casted to 1D array and internal node that
    // preserves the 0D shape.
    std::string external_coreml_name;
    std::vector<uint32_t> dimensions;
    CoreML::Specification::MILSpec::DataType mil_data_type;
  };

  struct Result {
    explicit Result(base::FilePath ml_package_dir);
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    ~Result();

    const base::FilePath& GetModelFilePath();

    [[nodiscard]] const OperandInfo& GetOperandInfo(uint64_t operand_id) const;

    const base::FilePath ml_package_dir;
    std::map<uint64_t, OperandInfo> id_to_operand_info_map;
  };

  // Factory method that creates a GraphBuilderCoreml, builds and serializes the
  // CoreML model to the `working_directory`. This expects the
  // `working_directory` to be an empty directory.
  //
  // Returns unexpected if it fails.
  [[nodiscard]] static base::expected<std::unique_ptr<Result>, mojom::ErrorPtr>
  CreateAndBuild(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands,
      const base::FilePath& working_directory);

  static ContextProperties GetContextProperties();

  GraphBuilderCoreml(const GraphBuilderCoreml&) = delete;
  GraphBuilderCoreml& operator=(const GraphBuilderCoreml&) = delete;

  ~GraphBuilderCoreml();

 private:
  GraphBuilderCoreml(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands,
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
      CoreML::Specification::MILSpec::Function& main_function,
      CoreML::Specification::MILSpec::Block& block);
  void AddPlaceholderInput(
      CoreML::Specification::MILSpec::Function& main_function,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOutput(
      uint64_t output_id);

  // Helper function for simple unary operations.
  enum class SupportedDataType { kFloats, kFloatsAndInt32 };

  [[nodiscard]] base::expected<CoreML::Specification::MILSpec::Operation*,
                               mojom::ErrorPtr>
  CreateUnaryOperation(SupportedDataType supported_data_type,
                       std::string_view op_name,
                       uint64_t input_operand_id,
                       uint64_t output_operand_id,
                       CoreML::Specification::MILSpec::Block& block,
                       std::string_view operand_op_name);
  // TODO: crbug.com/345271830 - remove this after all callers check with
  // `context_properties_.data_type_limits`.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddUnaryOperation(
      SupportedDataType supported_data_type,
      std::string_view op_name,
      uint64_t input_operand_id,
      uint64_t output_operand_id,
      CoreML::Specification::MILSpec::Block& block,
      std::string_view operand_op_name);
  void AddUnaryOperation(std::string_view op_name,
                         uint64_t input_operand_id,
                         uint64_t output_operand_id,
                         CoreML::Specification::MILSpec::Block& block);
  template <typename T>
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddUnaryOperation(
      SupportedDataType supported_data_type,
      std::string_view op_name,
      const T& operation,
      CoreML::Specification::MILSpec::Block& block,
      std::string_view operand_op_name);
  template <typename T>
  void AddUnaryOperation(std::string_view op_name,
                         const T& operation,
                         CoreML::Specification::MILSpec::Block& block);
  void AddUnaryFloatsOperationWithEpsilon(
      std::string_view op_name,
      std::string_view input_name,
      CoreML::Specification::MILSpec::DataType input_mil_data_type,
      uint64_t output_operand_id,
      float epsilon,
      CoreML::Specification::MILSpec::Block& block);
  template <typename T>
  void AddUnaryFloatsOperationWithEpsilon(
      std::string_view op_name,
      const T& operation,
      float epsilon,
      CoreML::Specification::MILSpec::Block& block);

  // Serialization functions for members of the mojom::Operation union. Keep
  // these functions in the same order as in webnn_graph.mojom.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForArgMinMax(
      const mojom::ArgMinMax& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForBatchNormalization(
      const mojom::BatchNormalization& operation,
      CoreML::Specification::MILSpec::Block& block);
  void AddOperationForCast(uint64_t input_operand_id,
                           uint64_t output_operand_id,
                           CoreML::Specification::MILSpec::Block& block);
  void AddOperationForClamp(const mojom::Clamp& operation,
                            CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForConcat(
      const mojom::Concat& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForConv2d(
      const mojom::Conv2d& operation,
      CoreML::Specification::MILSpec::Block& block);
  void AddOperationForCumulativeSum(
      const mojom::CumulativeSum& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForElementwiseBinary(
      uint64_t lhs_operand_id,
      std::variant<uint64_t, CoreML::Specification::MILSpec::Value> rhs_operand,
      uint64_t output_operand_id,
      const mojom::ElementWiseBinary::Kind kind,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForElementwiseUnary(mojom::ElementWiseUnary::Kind kind,
                                  uint64_t input_operand_id,
                                  uint64_t output_operand_id,
                                  CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForElu(
      const mojom::Elu& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForExpand(
      const mojom::Expand& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGather(
      const mojom::Gather& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGemm(
      const mojom::Gemm& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForHardSigmoid(uint64_t input_operand_id,
                             float alpha,
                             float beta,
                             uint64_t output_operand_id,
                             CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForHardSigmoid(const mojom::HardSigmoid& operation,
                             CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForHardSwish(
      const mojom::HardSwish& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForInstanceNormalization(
      const mojom::InstanceNormalization& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForLayerNormalization(
      const mojom::LayerNormalization& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForLeakyRelu(
      const mojom::LeakyRelu& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForLinear(
      const mojom::Linear& operation,
      CoreML::Specification::MILSpec::Block& block);
  void AddOperationForMatmul(uint64_t input_x_operand_id,
                             uint64_t input_y_operand_id,
                             bool transpose_x,
                             bool transpose_y,
                             uint64_t output_operand_id,
                             CoreML::Specification::MILSpec::Block& block);
  void AddOperationForMatmul(const mojom::Matmul& operation,
                             CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForPad(
      const mojom::Pad& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForPool2d(
      const mojom::Pool2d& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForReduce(
      const mojom::Reduce& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForResample2d(
      const mojom::Resample2d& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForReshape(
      uint64_t input_operand_id,
      uint64_t output_operand_id,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForReshape(
      const mojom::Reshape& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSlice(
      const mojom::Slice& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSoftmax(
      const mojom::Softmax& operation,
      CoreML::Specification::MILSpec::Block& block);
  void AddOperationForSplit(const mojom::Split& operation,
                            CoreML::Specification::MILSpec::Block& block);
  void AddOperationForTile(const mojom::Tile& operation,
                           CoreML::Specification::MILSpec::Block& block);
  void AddOperationForTranspose(const mojom::Transpose& operation,
                                CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForWhere(
      const mojom::Where& operation,
      CoreML::Specification::MILSpec::Block& block);

  // Add constants as immediate values in the model file.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddConstantImmediateValue(
      uint64_t constant_id,
      CoreML::Specification::MILSpec::Block& block);
  // Add constants to weight file.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddConstantFileValue(
      uint64_t constant_id,
      uint64_t offset,
      CoreML::Specification::MILSpec::Block& block);

  // Populate generic fields that apply to all `const` operations.
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
  void PopulateNamedValueTypeForInput(
      uint64_t operand_id,
      CoreML::Specification::MILSpec::NamedValueType& named_value_type);
  // Update the `id_to_op_input_info_map_` to be used by ops later.
  void UpdateCoreMLInputInfoMap(uint64_t operand_id);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  SetupMlPackageDirStructure();

  std::string GetCoreMLNameFromOperand(uint64_t operand_id);
  [[nodiscard]] base::expected<uint64_t, mojom::ErrorPtr>
  GenerateInternalOperandInfo(
      CoreML::Specification::MILSpec::DataType mil_data_type,
      base::span<const uint32_t> dimensions);

  // A reference to the WebNN compute graph that `this` instance is converting
  // to CoreML model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  base::raw_ref<
      const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>>
      constant_operands_;

  const ContextProperties context_properties_;

  // Used to generate unique names for internal operands generated for WebNN
  // operations that need to be decomposed into multiple CoreML operations.
  base::CheckedNumeric<uint64_t> internal_operand_id_;

  CoreML::Specification::Model ml_model_;
  raw_ptr<CoreML::Specification::MILSpec::Program> program_;

  std::unique_ptr<Result> result_;
};

}  // namespace coreml
}  // namespace webnn

#endif  // SERVICES_WEBNN_COREML_GRAPH_BUILDER_COREML_H_
