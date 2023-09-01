// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <DirectML.h>
#include <wrl.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/graph_impl.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

class WebNNGraphDMLImplTest : public TestBase {
 public:
  void SetUp() override;

  bool CreateAndBuildGraph(const mojom::GraphInfoPtr& graph_info);

 protected:
  bool is_compile_graph_supported_ = true;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<Adapter> adapter_;
};

void WebNNGraphDMLImplTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  ASSERT_TRUE(InitializeGLDisplay());
  Adapter::EnableDebugLayerForTesting();
  adapter_ = Adapter::GetInstanceForTesting();
  ASSERT_NE(adapter_.get(), nullptr);
  is_compile_graph_supported_ =
      adapter_->IsDMLDeviceCompileGraphSupportedForTesting();
}

bool WebNNGraphDMLImplTest::CreateAndBuildGraph(
    const mojom::GraphInfoPtr& graph_info) {
  base::RunLoop build_graph_run_loop;
  bool result = false;
  GraphImpl::CreateAndBuild(
      adapter_->command_queue(), adapter_->dml_device(), graph_info,
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::WebNNGraph> remote) {
            result = remote.is_valid();
            build_graph_run_loop.Quit();
          }));
  build_graph_run_loop.Run();
  return result;
}

// Test building a DML graph with single operator clamp.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorClamp) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  mojom::ClampAttributesPtr clamp_attributes = mojom::ClampAttributes::New();
  clamp_attributes->max_value = 3;
  clamp_attributes->min_value = 0;
  builder.BuildOperator(
      mojom::Operator::Kind::kClamp, {input_operand_id}, {output_operand_id},
      mojom::OperatorAttributes::NewClamp(std::move(clamp_attributes)));
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

struct OperandInfo {
  mojom::Operand::DataType type;
  std::vector<uint32_t> dimensions;
};

struct Pool2dTester {
  OperandInfo input;
  struct Pool2dAttributes {
    std::vector<uint32_t> window_dimensions;
    std::vector<uint32_t> padding;
    std::vector<uint32_t> strides;
    std::vector<uint32_t> dilations;
    mojom::InputOperandLayout layout;
  };
  Pool2dAttributes attributes;
  mojom::Operator::Kind kind;
  OperandInfo output;

  void Test(WebNNGraphDMLImplTest& helper) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    mojom::Pool2dAttributesPtr mojo_attributes = mojom::Pool2dAttributes::New();
    mojo_attributes->window_dimensions = mojom::Size2d::New(
        attributes.window_dimensions[0], attributes.window_dimensions[1]);
    mojo_attributes->padding = mojom::Padding2d::New(
        mojom::Size2d::New(attributes.padding[0],
                           attributes.padding[2]) /*beginning padding*/,
        mojom::Size2d::New(attributes.padding[1],
                           attributes.padding[3]) /*ending padding*/);
    mojo_attributes->strides =
        mojom::Size2d::New(attributes.strides[0], attributes.strides[1]);
    mojo_attributes->dilations =
        mojom::Size2d::New(attributes.dilations[0], attributes.dilations[1]);
    mojo_attributes->layout = attributes.layout;
    mojom::OperatorAttributesPtr pool2d_attributes =
        mojom::OperatorAttributes::NewPool2d(std::move(mojo_attributes));

    builder.BuildOperator(kind, {input_operand_id}, {output_operand_id},
                          std::move(pool2d_attributes));
    EXPECT_TRUE(helper.CreateAndBuildGraph(builder.GetGraphInfo()));
  }
};

// Test building a DML graph with single operator average pool2d.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorAveragePool2d) {
  {
    // Test average pool2d with nchw layout, strides=2, padding=1, and floor
    // rounding.
    Pool2dTester{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 3, 7, 7}},
        .attributes = {.window_dimensions = {4, 4},
                       .padding = {1, 1, 1, 1},
                       .strides = {2, 2},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Operator::Kind::kAveragePool2d,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 3, 3, 3}}}
        .Test(*this);
  }
  {
    // Test average pool2d with nhwc layout, strides=2, padding=1 and ceil
    // rounding.
    Pool2dTester{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 7, 7, 3}},
        .attributes = {.window_dimensions = {4, 4},
                       .padding = {1, 1, 1, 1},
                       .strides = {2, 2},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsLast},
        .kind = mojom::Operator::Kind::kAveragePool2d,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 4, 4, 3}}}
        .Test(*this);
  }
}

// Test building a DML graph with single operator max pool2d with nchw layout.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorMaxPool2d) {
  // DML_MAX_POOLING2_OPERATOR_DESC was introduced in DML_FEATURE_LEVEL_2_1.
  SKIP_TEST_IF(GetMaxSupportedDMLFeatureLevel(adapter_->dml_device()) <
               DML_FEATURE_LEVEL_2_1);
  {
    // Test max pool2d with nchw layout, strides=2, padding=1, and floor
    // rounding.
    Pool2dTester{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 3, 7, 7}},
        .attributes = {.window_dimensions = {4, 4},
                       .padding = {1, 1, 1, 1},
                       .strides = {2, 2},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Operator::Kind::kMaxPool2d,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 3, 3, 3}}}
        .Test(*this);
  }
}

// Test building a DML graph with single operator relu.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorRelu) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {input_operand_id},
                        {output_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with two relu operators.
//    [input]
//       |
//      relu1
//       |
//      relu2
TEST_F(WebNNGraphDMLImplTest, BuildGraphWithTwoRelu) {
  SKIP_TEST_IF(!is_compile_graph_supported_);
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t relu1_output_id = builder.BuildIntermediateOperand(
      {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {input_operand_id},
                        {relu1_output_id});
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {relu1_output_id},
                        {output_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with single operator reshape.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorReshape) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kReshape, {input_operand_id},
                        {output_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with two operators (reshape as the last node).
//    [input]
//       |
//      relu
//       |
//     reshape
TEST_F(WebNNGraphDMLImplTest, BuildGraphWithReshapeAsLastNode) {
  SKIP_TEST_IF(!is_compile_graph_supported_);
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t relu_output_id = builder.BuildIntermediateOperand(
      {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {input_operand_id},
                        {relu_output_id});
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kReshape, {relu_output_id},
                        {output_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with two operators (reshape as an intermediate
// node).
//    [input]
//       |
//    reshape
//       |
//      relu
TEST_F(WebNNGraphDMLImplTest, BuildGraphWithReshapeAsIntermediateNode) {
  SKIP_TEST_IF(!is_compile_graph_supported_);
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t reshape_output_id = builder.BuildIntermediateOperand(
      {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kReshape, {input_operand_id},
                        {reshape_output_id});
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {reshape_output_id},
                        {output_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with two reshape operators
//    [input]
//       |
//    reshape1
//       |
//    reshape2
TEST_F(WebNNGraphDMLImplTest, BuildGraphWithTwoReshape) {
  SKIP_TEST_IF(!is_compile_graph_supported_);
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t reshape_output_id = builder.BuildIntermediateOperand(
      {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kReshape, {input_operand_id},
                        {reshape_output_id});
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kReshape, {reshape_output_id},
                        {output_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with two operators and two outputs
//      [input]
//       /   \
//  reshape   relu
//     |        |
// [output1] [output2]
TEST_F(WebNNGraphDMLImplTest, BuildGraphWithTwoOutputs) {
  SKIP_TEST_IF(!is_compile_graph_supported_);
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output1_operand_id = builder.BuildOutput(
      "output1", {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kReshape, {input_operand_id},
                        {output1_operand_id});
  uint64_t output2_operand_id = builder.BuildOutput(
      "output2", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {input_operand_id},
                        {output2_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with single operator gemm.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorGemm) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  mojom::GemmAttributesPtr attributes = mojom::GemmAttributes::New();
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {input_a_operand_id, input_b_operand_id},
      {output_operand_id},
      mojom::OperatorAttributes::NewGemm(std::move(attributes)));
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with single operator gemm but with a third input.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorGemmWithThirdInput) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat16);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, mojom::Operand::DataType::kFloat16);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat16);
  mojom::GemmAttributesPtr attributes = mojom::GemmAttributes::New();
  attributes->c_operand_id =
      builder.BuildInput("c", {2, 2}, mojom::Operand::DataType::kFloat16);
  attributes->alpha = 1.0f;
  attributes->beta = 0.0f;
  attributes->a_transpose = true;
  attributes->b_transpose = true;
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {input_a_operand_id, input_b_operand_id},
      {output_operand_id},
      mojom::OperatorAttributes::NewGemm(std::move(attributes)));
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with three gemm operations.
//    [input] [input] [input] [input]
//           \    /     \    /
//            gemm       gemm
//                \      /
//                  gemm
TEST_F(WebNNGraphDMLImplTest, BuildMultipleOperatorGemm) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t intermediate_1_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {input_a_operand_id, input_b_operand_id},
      {intermediate_1_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {input_a_operand_id, input_b_operand_id},
      {intermediate_2_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm,
      {intermediate_1_operand_id, intermediate_2_operand_id},
      {output_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with one input and one constant.
TEST_F(WebNNGraphDMLImplTest, BuildOneInputAndOneConstantOperand) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(GetMaxSupportedDMLFeatureLevel(adapter_->dml_device()) <
               DML_FEATURE_LEVEL_4_0);
  // Build the mojom graph info.
  std::vector<float> constant_data = {5.0, 6.0, 7.0, 8.0};
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id = builder.BuildConstant(
      {2, 2}, mojom::Operand::DataType::kFloat32,
      base::make_span(reinterpret_cast<const uint8_t*>(constant_data.data()),
                      constant_data.size() * sizeof(float)));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  mojom::GemmAttributesPtr attributes = mojom::GemmAttributes::New();
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {input_a_operand_id, input_b_operand_id},
      {output_operand_id},
      mojom::OperatorAttributes::NewGemm(std::move(attributes)));
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with two inputs and two constant in the following
// topology.
//    [input_a] [constant_a] [input_b] [constant_b]
//           \    /                \    /
//            gemm                  gemm
//                \                /
//                       gemm
TEST_F(WebNNGraphDMLImplTest, BuildMultipleInputsAppendingConstants) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(GetMaxSupportedDMLFeatureLevel(adapter_->dml_device()) <
               DML_FEATURE_LEVEL_4_0);
  SKIP_TEST_IF(!is_compile_graph_supported_);
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, mojom::Operand::DataType::kFloat32);
  std::vector<float> constant_data = {5.0, 6.0, 7.0, 8.0};
  uint64_t constant_a_operand_id = builder.BuildConstant(
      {2, 2}, mojom::Operand::DataType::kFloat32,
      base::make_span(reinterpret_cast<const uint8_t*>(constant_data.data()),
                      constant_data.size() * sizeof(float)));
  uint64_t constant_b_operand_id = builder.BuildConstant(
      {2, 2}, mojom::Operand::DataType::kFloat32,
      base::make_span(reinterpret_cast<const uint8_t*>(constant_data.data()),
                      constant_data.size() * sizeof(float)));

  // The order of inputs are [input_a, constant_a, input_b, constant_b].
  uint64_t intermediate_1_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {input_a_operand_id, constant_a_operand_id},
      {intermediate_1_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {input_b_operand_id, constant_b_operand_id},
      {intermediate_2_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm,
      {intermediate_1_operand_id, intermediate_2_operand_id},
      {output_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with two inputs and two constant in the following
// topology.
//    [constant_a] [input_a] [constant_b] [input_b]
//           \    /                \    /
//            gemm                  gemm
//                \                /
//                       gemm
TEST_F(WebNNGraphDMLImplTest, BuildMultipleConstantsAppendingInputs) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(GetMaxSupportedDMLFeatureLevel(adapter_->dml_device()) <
               DML_FEATURE_LEVEL_4_0);
  SKIP_TEST_IF(!is_compile_graph_supported_);
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, mojom::Operand::DataType::kFloat32);
  std::vector<float> constant_data = {5.0, 6.0, 7.0, 8.0};
  uint64_t constant_a_operand_id = builder.BuildConstant(
      {2, 2}, mojom::Operand::DataType::kFloat32,
      base::make_span(reinterpret_cast<const uint8_t*>(constant_data.data()),
                      constant_data.size() * sizeof(float)));
  uint64_t constant_b_operand_id = builder.BuildConstant(
      {2, 2}, mojom::Operand::DataType::kFloat32,
      base::make_span(reinterpret_cast<const uint8_t*>(constant_data.data()),
                      constant_data.size() * sizeof(float)));

  // The order of inputs are [constant_a, input_a, constant_b, input_b].
  uint64_t intermediate_1_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {constant_a_operand_id, input_a_operand_id},
      {intermediate_1_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm, {constant_b_operand_id, input_b_operand_id},
      {intermediate_2_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(
      mojom::Operator::Kind::kGemm,
      {intermediate_1_operand_id, intermediate_2_operand_id},
      {output_operand_id},
      mojom::OperatorAttributes::NewGemm(mojom::GemmAttributes::New()));
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

}  // namespace webnn::dml
