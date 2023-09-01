// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
#define SERVICES_WEBNN_WEBNN_TEST_UTILS_H_

#include <string>
#include <vector>

#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"

namespace webnn {

// GraphInfoBuilder is a helper class for test cases that builds a GraphInfoPtr
// defined by mojom which describes an entire WebNN graph information. It
// provides methods to create all of the operands and operators for the
// GraphInfoPtr.
class GraphInfoBuilder final {
 public:
  GraphInfoBuilder();
  GraphInfoBuilder(const GraphInfoBuilder&) = delete;
  GraphInfoBuilder& operator=(const GraphInfoBuilder&) = delete;
  ~GraphInfoBuilder();

  uint64_t BuildIntermediateOperand(const std::vector<uint32_t>& dimensions,
                                    mojom::Operand::DataType type);

  uint64_t BuildInput(const std::string& name,
                      const std::vector<uint32_t>& dimensions,
                      mojom::Operand::DataType type);

  uint64_t BuildConstant(const std::vector<uint32_t>& dimensions,
                         mojom::Operand::DataType type,
                         base::span<const uint8_t> values);

  uint64_t BuildOutput(const std::string& name,
                       const std::vector<uint32_t>& dimensions,
                       mojom::Operand::DataType type);

  void BuildOperator(
      mojom::Operator::Kind kind,
      const std::vector<uint64_t>& inputs,
      const std::vector<uint64_t>& outputs,
      mojom::OperatorAttributesPtr operator_attributes = nullptr);

  const mojom::GraphInfoPtr& GetGraphInfo() const { return graph_info_; }

  // Get a clone of internal graph info. This is used by
  // `WebNNContextDMLImplTest` because mojom::WebNNContext::CreateGraph()` needs
  // to take the ownership of graph info.
  //
  // Notice cloning of graph info could be expensive and should only be used in
  // tests.
  mojom::GraphInfoPtr CloneGraphInfo() const;

 private:
  uint64_t BuildOperand(
      const std::vector<uint32_t>& dimensions,
      mojom::Operand::DataType type,
      mojom::Operand::Kind kind = mojom::Operand::Kind::kOutput);

  mojom::GraphInfoPtr graph_info_;
  uint64_t operand_id_ = 0;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
