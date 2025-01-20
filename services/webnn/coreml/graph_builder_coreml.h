// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_GRAPH_BUILDER_COREML_H_
#define SERVICES_WEBNN_COREML_GRAPH_BUILDER_COREML_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
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
    IsAnyOf<T, Float16, float, int32_t, int8_t, uint8_t, char, bool>;
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
  class WeightsFileHandle;
  class ScopedWeightItem {
   public:
    ScopedWeightItem(WeightsFileHandle& weights_file_handle,
                     size_t byte_size,
                     uint64_t offset);
    ~ScopedWeightItem();
    ScopedWeightItem(const ScopedWeightItem&) = delete;
    ScopedWeightItem& operator=(const ScopedWeightItem&) = delete;

    base::expected<void, mojom::ErrorPtr> WriteBytes(
        base::span<const uint8_t> bytes);

    base::expected<void, mojom::ErrorPtr> Finalize();
    uint64_t offset() { return offset_; }

   private:
    base::raw_ref<WeightsFileHandle> weights_file_handle_;
    bool has_error_ = false;
    bool finalized_ = false;
    size_t byte_size_;
    size_t size_written_ = 0;
    const uint64_t offset_;
  };

  class WeightsFileHandle {
   public:
    static std::optional<std::unique_ptr<GraphBuilderCoreml::WeightsFileHandle>>
    CreateWeightsHandle(const base::FilePath& weights_file_path);

    WeightsFileHandle(base::File weights_file, uint64_t current_offset);
    WeightsFileHandle(const WeightsFileHandle&) = delete;
    WeightsFileHandle(WeightsFileHandle&&) = delete;
    ~WeightsFileHandle();

    // Write a single weight item.
    base::expected<uint64_t, mojom::ErrorPtr> Write(
        base::span<const uint8_t> bytes,
        OperandDataType data_type);

    base::expected<std::unique_ptr<ScopedWeightItem>, mojom::ErrorPtr>
    CreateScopedWeightItem(OperandDataType data_type, size_t byte_size);

    // Need to be called to update weight count after all weights are written.
    base::expected<void, mojom::ErrorPtr> Finalize();

    size_t GetByteSize(OperandDataType data_type);
    friend class ScopedWeightItem;

   private:
    // `WeightItemInitialize` `WriteBytes`, `WriteItemFinalize` allows callers
    // to make multiple partial writes then close of the weight item.
    base::expected<uint64_t, mojom::ErrorPtr> WeightItemInitialize(
        OperandDataType data_type,
        size_t byte_size);
    base::expected<void, mojom::ErrorPtr> WriteBytes(
        base::span<const uint8_t> bytes);
    base::expected<void, mojom::ErrorPtr> WeightItemFinalize(size_t byte_size);

    base::File weights_file_;
    uint64_t current_offset_ = 0;
    uint32_t num_of_weights_ = 0;
    base::TimeDelta weights_write_time_;
    bool has_error_ = false;
    bool finalized_ = false;
  };

  GraphBuilderCoreml(
      const mojom::GraphInfo& graph_info,
      ContextProperties context_properties,
      const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
          constant_operands,
      base::FilePath ml_package_dir,
      std::unique_ptr<WeightsFileHandle> weights_file_handle);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> BuildCoreMLModel();

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> SerializeModel();

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> WriteImmediateWeights(
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
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddUnaryOperation(
      std::string_view op_name,
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
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddUnaryOperation(
      std::string_view op_name,
      const T& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddUnaryFloatsOperationWithEpsilon(
      std::string_view op_name,
      uint64_t input_operand_id,
      uint64_t output_operand_id,
      float epsilon,
      CoreML::Specification::MILSpec::Block& block);
  template <typename T>
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddUnaryFloatsOperationWithEpsilon(
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
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForCast(
      uint64_t input_operand_id,
      uint64_t output_operand_id,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForClamp(
      const mojom::Clamp& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForConcat(
      base::span<const uint64_t> input_operand_ids,
      uint64_t output_operand_id,
      uint32_t axis,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForConcat(
      const mojom::Concat& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForConv2d(
      const mojom::Conv2d& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForCumulativeSum(const mojom::CumulativeSum& operation,
                               CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForElementwiseBinary(
      std::variant<uint64_t, CoreML::Specification::MILSpec::Value> lhs_operand,
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
  void AddOperationForFill(CoreML::Specification::MILSpec::Value value,
                           uint64_t output_operand_id,
                           CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGather(
      const mojom::Gather& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForGatherElements(const mojom::GatherElements& operation,
                                CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGatherND(
      const mojom::GatherND& operation,
      CoreML::Specification::MILSpec::Block& block);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGelu(
      const mojom::Gelu& operation,
      CoreML::Specification::MILSpec::Block& block);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGemm(
      uint64_t a_operand_id,
      uint64_t b_operand_id,
      std::optional<uint64_t> c_operand_id,
      uint64_t output_operand_id,
      CoreML::Specification::MILSpec::Block& block,
      bool a_transpose = false,
      bool b_transpose = false,
      float alpha = 1.0f,
      float beta = 1.0f);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGemm(
      const mojom::Gemm& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGru(
      const mojom::Gru& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForGruCell(
      const mojom::GruCell& operation,
      CoreML::Specification::MILSpec::Block& block);
  base::expected<void, mojom::ErrorPtr> AddOperationForGruSingleStep(
      uint64_t input_operand_id,
      uint64_t hidden_state_operand_id,
      uint64_t output_operand_id,
      base::span<const uint64_t> weights,
      base::span<const uint64_t> recurrent_weights,
      std::optional<base::span<const uint64_t>> biases,
      std::optional<base::span<const uint64_t>> recurrent_biases,
      uint32_t hidden_size,
      mojom::GruWeightLayout layout,
      mojom::RecurrentNetworkActivation activation,
      mojom::RecurrentNetworkActivation output_activation,
      bool reset_after,
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
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForLstm(
      const mojom::Lstm& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForLstm(
      uint64_t input_operand_id,
      uint64_t weight_operand_id,
      uint64_t recurrent_weight_operand_id,
      uint32_t hidden_size,
      std::optional<uint64_t> bias_operand_id,
      std::optional<uint64_t> recurrent_bias_operand_id,
      std::optional<uint64_t> peephole_weight_operand_id,
      std::optional<uint64_t> initial_hidden_state_operand_id,
      std::optional<uint64_t> initial_cell_state_operand_id,
      bool return_sequence,
      mojom::RecurrentNetworkDirection direction,
      mojom::LstmWeightLayout layout,
      base::span<const mojom::RecurrentNetworkActivation> activations,
      base::span<const uint64_t> output_operand_ids,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForLstmCell(
      const mojom::LstmCell& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForMatmul(
      uint64_t input_x_operand_id,
      uint64_t input_y_operand_id,
      bool transpose_x,
      bool transpose_y,
      uint64_t output_operand_id,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForMatmul(
      const mojom::Matmul& operation,
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
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  AddOperationForScatterElements(const mojom::ScatterElements& operation,
                                 CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForScatterND(
      uint64_t input_operand_id,
      uint64_t indices_operand_id,
      uint64_t updates_operand_id,
      uint64_t output_operand_id,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForScatterND(
      const mojom::ScatterND& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSlice(
      uint64_t input_operand_id,
      uint64_t output_operand_id,
      base::span<const int32_t> beginnings,
      base::span<const int32_t> endings,
      base::span<const int32_t> strides,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSlice(
      const mojom::Slice& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSoftmax(
      const mojom::Softmax& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSplit(
      uint64_t input_operand_id,
      base::span<const uint64_t> output_operand_ids,
      uint32_t axis,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForSplit(
      const mojom::Split& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForTile(
      const mojom::Tile& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForTranspose(
      uint64_t input_operand_id,
      uint64_t output_operand_id,
      base::span<const uint32_t> permutation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForTranspose(
      const mojom::Transpose& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForTriangular(
      const mojom::Triangular& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForWhere(
      const mojom::Where& operation,
      CoreML::Specification::MILSpec::Block& block);

  // Add constants as immediate values in the model file.
  void AddConstantImmediateValue(uint64_t constant_id,
                                 CoreML::Specification::MILSpec::Block& block);
  // Create a Value that points to an offset in weight file.
  CoreML::Specification::MILSpec::Value CreateConstantFileValue(
      uint64_t constant_id,
      uint64_t offset);

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

  std::string GetCoreMLNameFromOperand(uint64_t operand_id);
  [[nodiscard]] base::expected<uint64_t, mojom::ErrorPtr>
  GenerateInternalOperandInfo(
      CoreML::Specification::MILSpec::DataType mil_data_type,
      base::span<const uint32_t> dimensions);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> SetInputFromOperand(
      google::protobuf::Map<std::string,
                            CoreML::Specification::MILSpec::Argument>& inputs,
      std::string_view key,
      uint64_t operand_id);

  // Helper function to return input[index] using squeeze(slice(input)).
  base::expected<uint64_t, mojom::ErrorPtr> SliceFirstDimension(
      uint64_t input_operand_id,
      int32_t index,
      CoreML::Specification::MILSpec::Block& block);

  // Split to output operands and squeeze it.
  base::expected<void, mojom::ErrorPtr> SplitAndSqueeze(
      uint64_t input_operand_id,
      base::span<uint64_t> output_operand_ids,
      int32_t axis,
      CoreML::Specification::MILSpec::Block& block);
  // Set input from a constant operand with an alternative order. The reordered
  // constant won't be re-used across operations.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  SetInputFromConstantReordered(
      google::protobuf::Map<std::string,
                            CoreML::Specification::MILSpec::Argument>& inputs,
      std::string_view key,
      base::span<const uint8_t> bytes,
      OperandDataType data_type,
      base::span<const uint32_t> dimensions,
      base::span<const std::pair<size_t, size_t>> new_order);
  // Set input from two constants added up with an alternative order. The
  // reordered constant won't be re-used across operations.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  SetInputFromTwoConstantsReordered(
      google::protobuf::Map<std::string,
                            CoreML::Specification::MILSpec::Argument>& inputs,
      std::string_view key,
      base::span<const uint8_t> a_bytes,
      base::span<const uint8_t> b_bytes,
      OperandDataType data_type,
      base::span<const uint32_t> dimensions,
      base::span<const std::pair<size_t, size_t>> new_order);
  // A reference to the WebNN compute graph that `this` instance is
  // converting to CoreML model. The creator of `this` must ensure the
  // GraphInfo reference passed into `CreateAndBuild()` is valid for as long
  // as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  base::raw_ref<
      const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>>
      constant_operands_;

  const ContextProperties context_properties_;

  // Points to offset in weight file or identifier in coreml model file.
  base::flat_map<uint64_t, std::variant<uint64_t, std::string_view>>
      constant_pointers_;

  // Used to generate unique names for internal operands generated for WebNN
  // operations that need to be decomposed into multiple CoreML operations.
  base::CheckedNumeric<uint64_t> internal_operand_id_;

  CoreML::Specification::Model ml_model_;
  raw_ptr<CoreML::Specification::MILSpec::Program> program_;

  std::unique_ptr<WeightsFileHandle> weights_file_handle_;

  std::unique_ptr<Result> result_;
};

}  // namespace coreml
}  // namespace webnn

#endif  // SERVICES_WEBNN_COREML_GRAPH_BUILDER_COREML_H_
