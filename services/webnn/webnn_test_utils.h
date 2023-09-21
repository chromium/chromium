// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TEST_UTILS_H_
#define SERVICES_WEBNN_WEBNN_TEST_UTILS_H_

#include <string>
#include <vector>

#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

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

  // The generic type `T` is the pool2d attributes from different unit test.
  template <typename T>
  void BuildPool2d(mojom::Pool2d::Kind kind,
                   uint64_t input_operand_id,
                   uint64_t output_operand_id,
                   const T& attributes) {
    mojom::Pool2dPtr pool2d = mojom::Pool2d::New();
    pool2d->kind = kind;
    pool2d->input_operand_id = input_operand_id;
    pool2d->output_operand_id = output_operand_id;

    auto& window_dimensions = attributes.window_dimensions;
    CHECK_EQ(window_dimensions.size(), 2u);
    pool2d->window_dimensions =
        mojom::Size2d::New(window_dimensions[0], window_dimensions[1]);
    pool2d->padding = mojom::Padding2d::New(
        mojom::Size2d::New(attributes.padding[0],
                           attributes.padding[2]) /* beginning padding*/,
        mojom::Size2d::New(attributes.padding[1],
                           attributes.padding[3]) /* ending padding*/);
    pool2d->strides =
        mojom::Size2d::New(attributes.strides[0], attributes.strides[1]);
    pool2d->dilations =
        mojom::Size2d::New(attributes.dilations[0], attributes.dilations[1]);
    pool2d->layout = attributes.layout;

    graph_info_->operations.push_back(
        mojom::Operation::NewPool2d(std::move(pool2d)));
  }

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
