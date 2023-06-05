// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn {

namespace {

mojom::OperandPtr CreateOperand(const std::string& name,
                                const std::vector<uint32_t>& dimensions,
                                mojom::Operand::DataType type) {
  auto operand = mojom::Operand::New();
  operand->data_type = type;
  operand->dimensions = dimensions;
  operand->name = name;
  return operand;
}

mojom::OperatorPtr CreateOperator(mojom::Operator::Kind kind,
                                  const std::vector<uint64_t>& inputs,
                                  const std::vector<uint64_t>& outputs) {
  auto operation = mojom::Operator::New();
  operation->kind = kind;
  operation->input_operands = inputs;
  operation->output_operands = outputs;

  return operation;
}

}  // namespace

class WebNNGraphImplTest : public testing::Test {
 public:
  WebNNGraphImplTest(const WebNNGraphImplTest&) = delete;
  WebNNGraphImplTest& operator=(const WebNNGraphImplTest&) = delete;

  void TearDown() override { operand_id_ = 0; }

  uint64_t BuildInput(mojom::GraphInfoPtr& graph_info,
                      const std::string& name,
                      const std::vector<uint32_t>& dimensions,
                      mojom::Operand::DataType type) {
    auto operand = CreateOperand(name, dimensions, type);
    operand->kind = mojom::Operand::Kind::kInput;
    operand_id_++;
    CHECK(graph_info->id_to_operand_map.find(operand_id_) ==
          graph_info->id_to_operand_map.end());
    graph_info->id_to_operand_map[operand_id_] = std::move(operand);
    graph_info->input_operands.push_back(operand_id_);
    return operand_id_;
  }

  uint64_t BuildOutput(mojom::GraphInfoPtr& graph_info,
                       const std::string& name,
                       const std::vector<uint32_t>& dimensions,
                       mojom::Operand::DataType type) {
    auto operand = CreateOperand(name, dimensions, type);
    operand->kind = mojom::Operand::Kind::kOutput;
    operand_id_++;
    CHECK(graph_info->id_to_operand_map.find(operand_id_) ==
          graph_info->id_to_operand_map.end());
    graph_info->id_to_operand_map[operand_id_] = std::move(operand);
    graph_info->output_operands.push_back(operand_id_);
    return operand_id_;
  }

  bool ValidateGraph(mojom::GraphInfoPtr graph_info) {
    return WebNNGraphImpl::ValidateAndBuildGraph(
        base::BindLambdaForTesting(
            [&](mojo::PendingRemote<mojom::WebNNGraph> remote) {}),
        std::move(graph_info));
  }

 protected:
  WebNNGraphImplTest() = default;
  ~WebNNGraphImplTest() override = default;

 private:
  uint64_t operand_id_ = 0;
  base::test::TaskEnvironment task_environment_;
};

struct OperandInfo {
  mojom::Operand::DataType type;
  std::vector<uint32_t> dimensions;
};

struct ElementWiseBinaryTester {
  mojom::Operator::Kind kind;
  OperandInfo lhs;
  OperandInfo rhs;
  OperandInfo output;
  bool expected;

  void Test(WebNNGraphImplTest& helper) {
    // Build the graph with mojo type.
    auto graph_info = mojom::GraphInfo::New();
    uint64_t lhs_operand_id =
        helper.BuildInput(graph_info, "lhs", lhs.dimensions, lhs.type);
    uint64_t rhs_operand_id =
        helper.BuildInput(graph_info, "rhs", rhs.dimensions, rhs.type);
    uint64_t output_operand_id = helper.BuildOutput(
        graph_info, "output", output.dimensions, output.type);
    auto operation = CreateOperator(kind, {lhs_operand_id, rhs_operand_id},
                                    {output_operand_id});
    graph_info->operators.emplace_back(std::move(operation));
    auto result = helper.ValidateGraph(std::move(graph_info));
    EXPECT_EQ(result, expected);
  }
};

TEST_F(WebNNGraphImplTest, ElementWiseBinaryTest) {
  {
    // Testing building add with two input dimensions - {8, 1, 6, 1} and {7, 1,
    // 5}. Both the a and b dimensions have axes with length one that are
    // expanded to a larger size during the broadcast operation.
    // a_dimensions     (4d) 8 * 1 * 6 * 1
    // b_dimensions     (3d)     7 * 1 * 5
    // output_dimenions (4d) 8 * 7 * 6 * 5
    ElementWiseBinaryTester{
        .kind = mojom::Operator::Kind::kAdd,
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {8, 1, 6, 1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {7, 1, 5}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {8, 7, 6, 5}},
        .expected = true}
        .Test(*this);
  }
  {
    // Testing building add with two input dimensions - {4, 2, 1} and {4}.
    // a_dimensions     (3d) 4 * 2 * 1
    // b_dimensions     (1d)         4
    // output_dimenions (3d) 4 * 2 * 4
    ElementWiseBinaryTester{
        .kind = mojom::Operator::Kind::kSub,
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {4, 2, 1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32, .dimensions = {4}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {4, 2, 4}},
        .expected = true}
        .Test(*this);
  }
  {
    // Test the invalid graph for the input shapes are not broadcastable.
    ElementWiseBinaryTester{
        .kind = mojom::Operator::Kind::kMul,
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {4, 2}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32, .dimensions = {4}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {4, 2}},
        .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    ElementWiseBinaryTester{
        .kind = mojom::Operator::Kind::kDiv,
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {4, 2}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {4, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2}},
        .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph for input types don't match.
    ElementWiseBinaryTester{
        .kind = mojom::Operator::Kind::kMax,
        .lhs = {.type = mojom::Operand::DataType::kFloat32, .dimensions = {2}},
        .rhs = {.type = mojom::Operand::DataType::kInt32, .dimensions = {2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2}},
        .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph for output types don't match.
    ElementWiseBinaryTester{
        .kind = mojom::Operator::Kind::kMin,
        .lhs = {.type = mojom::Operand::DataType::kFloat32, .dimensions = {2}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32, .dimensions = {2}},
        .output = {.type = mojom::Operand::DataType::kInt32, .dimensions = {2}},
        .expected = false}
        .Test(*this);
  }
}

struct ReluTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test(WebNNGraphImplTest& helper) {
    // Build the graph with mojo type.
    auto graph_info = mojom::GraphInfo::New();
    uint64_t input_operand_id =
        helper.BuildInput(graph_info, "input", input.dimensions, input.type);
    uint64_t output_operand_id = helper.BuildOutput(
        graph_info, "output", output.dimensions, output.type);
    auto operation = CreateOperator(mojom::Operator::Kind::kRelu,
                                    {input_operand_id}, {output_operand_id});
    graph_info->operators.emplace_back(std::move(operation));
    auto result = helper.ValidateGraph(std::move(graph_info));
    EXPECT_EQ(result, expected);
  }
};

TEST_F(WebNNGraphImplTest, ReluTest) {
  {
    // Test relu operator for 3-D tensor with float32 input.
    ReluTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                         .dimensions = {2, 6, 4}},
               .output = {.type = mojom::Operand::DataType::kFloat32,
                          .dimensions = {2, 6, 4}},
               .expected = true}
        .Test(*this);
  }
  {
    // Test relu operator for 4-D tensor with int32 input.
    ReluTester{.input = {.type = mojom::Operand::DataType::kInt32,
                         .dimensions = {1, 5, 3, 7}},
               .output = {.type = mojom::Operand::DataType::kInt32,
                          .dimensions = {1, 5, 3, 7}},
               .expected = true}
        .Test(*this);
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    ReluTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                         .dimensions = {4, 2}},
               .output = {.type = mojom::Operand::DataType::kFloat32,
                          .dimensions = {2}},
               .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph for output types don't match.
    ReluTester{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2}},
        .output = {.type = mojom::Operand::DataType::kInt32, .dimensions = {2}},
        .expected = false}
        .Test(*this);
  }
}

struct ReshapeTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test(WebNNGraphImplTest& helper) {
    // Build the graph with mojo type.
    auto graph_info = mojom::GraphInfo::New();
    uint64_t input_operand_id =
        helper.BuildInput(graph_info, "input", input.dimensions, input.type);
    uint64_t output_operand_id = helper.BuildOutput(
        graph_info, "output", output.dimensions, output.type);
    auto operation = CreateOperator(mojom::Operator::Kind::kReshape,
                                    {input_operand_id}, {output_operand_id});
    graph_info->operators.emplace_back(std::move(operation));
    auto result = helper.ValidateGraph(std::move(graph_info));
    EXPECT_EQ(result, expected);
  }
};

TEST_F(WebNNGraphImplTest, ReshapeTest) {
  {
    // Test reshape operator from 2-D tensor to 1-D tensor.
    ReshapeTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                            .dimensions = {2, 4}},
                  .output = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {8}},
                  .expected = true}
        .Test(*this);
  }
  {
    // Test reshape operator from 4-D tensor to 2-D tensor.
    ReshapeTester{.input = {.type = mojom::Operand::DataType::kInt32,
                            .dimensions = {1, 3, 2, 1}},
                  .output = {.type = mojom::Operand::DataType::kInt32,
                             .dimensions = {1, 6}},
                  .expected = true}
        .Test(*this);
  }
  {
    // Test the invalid graph when one value of new shape is 0.
    ReshapeTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                            .dimensions = {4, 2}},
                  .output = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 0}},
                  .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph when the number of input elements are not equal to
    // the number of output elements.
    ReshapeTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                            .dimensions = {2, 3, 4}},
                  .output = {.type = mojom::Operand::DataType::kInt32,
                             .dimensions = {3, 5}},
                  .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph for output types don't match.
    ReshapeTester{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2}},
        .output = {.type = mojom::Operand::DataType::kInt32, .dimensions = {2}},
        .expected = false}
        .Test(*this);
  }
}

struct SoftmaxTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test(WebNNGraphImplTest& helper) {
    // Build the graph with mojo type.
    auto graph_info = mojom::GraphInfo::New();
    uint64_t input_operand_id =
        helper.BuildInput(graph_info, "input", input.dimensions, input.type);
    uint64_t output_operand_id = helper.BuildOutput(
        graph_info, "output", output.dimensions, output.type);
    auto operation = CreateOperator(mojom::Operator::Kind::kSoftmax,
                                    {input_operand_id}, {output_operand_id});
    graph_info->operators.emplace_back(std::move(operation));
    auto result = helper.ValidateGraph(std::move(graph_info));
    EXPECT_EQ(result, expected);
  }
};

TEST_F(WebNNGraphImplTest, SoftmaxTest) {
  {
    // Test softmax operator for input operand with [2, 2] dimensions.
    SoftmaxTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                            .dimensions = {2, 2}},
                  .output = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}},
                  .expected = true}
        .Test(*this);
  }
  {
    // Test softmax operator for input operand with [1, 4] dimensions.
    SoftmaxTester{.input = {.type = mojom::Operand::DataType::kFloat16,
                            .dimensions = {1, 4}},
                  .output = {.type = mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 4}},
                  .expected = true}
        .Test(*this);
  }
  {
    // Test the invalid graph when building softmax with 4-D input.
    SoftmaxTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                            .dimensions = {1, 1, 4, 2}},
                  .output = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 4, 2}},
                  .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph when building softmax with int32 input.
    SoftmaxTester{.input = {.type = mojom::Operand::DataType::kInt32,
                            .dimensions = {2, 3}},
                  .output = {.type = mojom::Operand::DataType::kInt32,
                             .dimensions = {2, 3}},
                  .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    SoftmaxTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                            .dimensions = {4, 2}},
                  .output = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {2}},
                  .expected = false}
        .Test(*this);
  }
  {
    // Test the invalid graph for output types don't match.
    SoftmaxTester{.input = {.type = mojom::Operand::DataType::kFloat32,
                            .dimensions = {2, 5}},
                  .output = {.type = mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 5}},
                  .expected = false}
        .Test(*this);
  }
}

}  // namespace webnn
