// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_
#define SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_

#include <cstdint>
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/coremltools/mlmodel/format/MIL.pb.h"
#include "third_party/coremltools/mlmodel/format/Model.pb.h"

namespace webnn::coreml {

// Get name identifiers used in CoreML model files for input/output operands.
std::string GetCoreMLNameFromInput(const std::string& input_name);
std::string GetCoreMLNameFromOutput(const std::string& output_name);

// Reads the WebNN graph from the mojom::GraphInfo to
// produce a protobuf message that corresponds to the
// contents of an equivalent .mlmodel file.
// There is nothing macOS-specific in this class.
class GraphBuilder {
  using IdToOperandMap = base::flat_map<uint64_t, mojom::OperandPtr>;

 public:
  // Factory method that creates a GraphBuilder and builds the
  // the serialized protobuf contents of a .mlmodel file.
  // Returns unexpected if it fails.
  [[nodiscard]] static base::expected<std::unique_ptr<GraphBuilder>,
                                      std::string>
  CreateAndBuild(const mojom::GraphInfo& graph_info);

  ~GraphBuilder();

  std::string GetSerializedCoreMLModel();

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
    webnn::mojom::Operand_DataType data_type;
    CoreML::Specification::MILSpec::DataType mil_data_type;
  };

  const OperandInfo* FindInputOperandInfo(const std::string& input_name) const;

 private:
  GraphBuilder();

  [[nodiscard]] base::expected<void, std::string> BuildCoreMLModel(
      const mojom::GraphInfo& graph_info);

  // Add input in Model.description and in Program's main function inputs.
  [[nodiscard]] base::expected<void, std::string> AddInput(
      const IdToOperandMap& id_to_operand_map,
      uint64_t input_id,
      CoreML::Specification::MILSpec::Function& main_function);
  [[nodiscard]] base::expected<void, std::string> AddOperationForBinary(
      const IdToOperandMap& id_to_operand_map,
      const mojom::ElementWiseBinary& operation,
      CoreML::Specification::MILSpec::Block& block);
  [[nodiscard]] base::expected<void, std::string> AddOutput(
      const IdToOperandMap& id_to_operand_map,
      uint64_t output_id);

  // Helpers
  [[nodiscard]] const OperandInfo* GetOperandInfo(uint64_t operand_id) const;
  [[nodiscard]] base::expected<void, std::string> PopulateFeatureDescription(
      uint64_t operand_id,
      const webnn::mojom::Operand& operand,
      ::CoreML::Specification::FeatureDescription* feature_description);

  // MILSpec::Program's Function, Block, Operation's inputs/outputs could be
  // defined as NamedValueType.
  void PopulateNamedValueType(
      uint64_t operand_id,
      const webnn::mojom::Operand& operand,
      CoreML::Specification::MILSpec::NamedValueType* value_type);

 private:
  CoreML::Specification::Model ml_model_;
  raw_ptr<CoreML::Specification::MILSpec::Program> program_;
  // Used to get operand info to specify input for a MILSpec::Operation.
  std::map<uint64_t, OperandInfo> id_to_op_input_info_map_;
  std::map<std::string, uint64_t> input_name_to_id_map_;
};

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_GRAPH_BUILDER_H_
