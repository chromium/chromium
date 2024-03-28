// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_
#define SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_

#include <cstdint>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/coremltools/mlmodel/format/MIL.pb.h"
#include "third_party/coremltools/mlmodel/format/Model.pb.h"

namespace webnn::coreml {

const char kPlaceholderInputName[] = "placeholder";

// Get name identifiers used in CoreML model files for input/output operands.
std::string GetCoreMLNameFromInput(std::string_view input_name);
std::string GetCoreMLNameFromOutput(std::string_view output_name);

// Reads the WebNN graph from the mojom::GraphInfo to
// produce CoreML model and serializes to provided `working_directory`.
// There is nothing macOS-specific in this class.
class GraphBuilder {
 public:
  // Factory method that creates a GraphBuilder, builds and serializes the
  // CoreML model to the `working_directory`. This expects the
  // `working_directory` to be an empty directory.
  //
  // Returns unexpected if it fails.
  [[nodiscard]] static base::expected<std::unique_ptr<GraphBuilder>,
                                      mojom::ErrorPtr>
  CreateAndBuild(const mojom::GraphInfo& graph_info,
                 const base::FilePath& working_directory);

  ~GraphBuilder();

  // Tracks Operand information during graph building, so that
  // future operations can look them up based on operand id.
  //
  // For the inputs of the model, this information is exposed
  // publicly via FindInputOperandInfo.
  struct OperandInfo {
    OperandInfo();
    ~OperandInfo();
    OperandInfo(std::string name,
                std::vector<uint32_t> dimensions,
                webnn::mojom::Operand_DataType data_type,
                CoreML::Specification::MILSpec::DataType mil_data_type);
    OperandInfo(OperandInfo&);
    OperandInfo(OperandInfo&&);

    // Identifier for this operand in coreml model file.
    std::string coreml_name;
    std::vector<uint32_t> dimensions;
    webnn::mojom::Operand::DataType data_type;
    CoreML::Specification::MILSpec::DataType mil_data_type;
  };

  const OperandInfo* FindInputOperandInfo(const std::string& input_name) const;
  const base::FilePath& GetModelFilePath();

 private:
  GraphBuilder(const mojom::GraphInfo& graph_info,
               base::FilePath ml_package_dir,
               base::FilePath model_file_path,
               base::FilePath weights_file_path);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr> BuildCoreMLModel(
      const mojom::GraphInfo& graph_info,
      const base::FilePath& working_directory);

  [[nodiscard]] bool SerializeModel();
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> WriteWeightsToFile(
      CoreML::Specification::MILSpec::Block& block);

  // Returns the Operand corresponding to an `operand_id` from `graph_info_`.
  // Will crash if `graph_info_` does not contain `operand_id`.
  const mojom::Operand& GetOperand(uint64_t operand_id) const;

  // Add input in Model.description and in Program's main function inputs.
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddInput(
      uint64_t input_id,
      CoreML::Specification::MILSpec::Function& main_function);
  void AddPlaceholderInput(
      CoreML::Specification::MILSpec::Function& main_function,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOutput(
      uint64_t output_id);
  [[nodiscard]] base::expected<void, mojom::ErrorPtr> AddOperationForBinary(
      const mojom::ElementWiseBinary& operation,
      CoreML::Specification::MILSpec::Block& block);
  // Add constants as immediate values in the model file.
  void AddConstantImmediateValue(uint32_t constant_id,
                                 CoreML::Specification::MILSpec::Block& block);
  // Add constants to weight file.
  void AddConstantFileValue(uint32_t constant_id,
                            uint64_t offset,
                            const webnn::mojom::Operand& operand,
                            CoreML::Specification::MILSpec::Block& block);

  // Helpers
  [[nodiscard]] const OperandInfo* GetOperandInfo(uint64_t operand_id) const;
  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  PopulateFeatureDescription(
      uint64_t operand_id,
      const webnn::mojom::Operand& operand,
      ::CoreML::Specification::FeatureDescription& feature_description);

  // MILSpec::Program's Function, Block, Operation's inputs/outputs could be
  // defined as NamedValueType.
  void PopulateNamedValueType(
      uint64_t operand_id,
      const webnn::mojom::Operand& operand,
      CoreML::Specification::MILSpec::NamedValueType& named_value_type);
  void PopulateValueType(const webnn::mojom::Operand& operand,
                         CoreML::Specification::MILSpec::ValueType& value_type);

  [[nodiscard]] base::expected<void, mojom::ErrorPtr>
  SetupMlPackageDirStructure(const base::FilePath& working_directory);

  // A reference to the WebNN compute graph that `this` instance is converting
  // to CoreML model. The creator of `this` must ensure the GraphInfo reference
  // passed into `CreateAndBuild()` is valid for as long as `this` exists.
  base::raw_ref<const mojom::GraphInfo> graph_info_;

  CoreML::Specification::Model ml_model_;
  raw_ptr<CoreML::Specification::MILSpec::Program> program_;
  // Used to get operand info to specify input for a MILSpec::Operation.
  std::map<uint64_t, OperandInfo> id_to_op_input_info_map_;
  std::map<std::string, uint64_t> input_name_to_id_map_;
  const base::FilePath ml_package_dir_;
  const base::FilePath model_file_path_;
  const base::FilePath weights_file_path_;
};

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_
