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
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<Adapter> adapter_;
};

void WebNNGraphDMLImplTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  ASSERT_TRUE(InitializeGLDisplay());
  Adapter::EnableDebugLayerForTesting();
  adapter_ = Adapter::GetInstanceForTesting();
  ASSERT_NE(adapter_.get(), nullptr);
  // Graph compilation relies on IDMLDevice1::CompileGraph introduced in
  // DirectML version 1.2 or DML_FEATURE_LEVEL_2_1, so skip the tests if the
  // DirectML version doesn't support this feature.
  SKIP_TEST_IF(!adapter_->IsDMLDeviceCompileGraphSupportedForTesting());
}

bool WebNNGraphDMLImplTest::CreateAndBuildGraph(
    const mojom::GraphInfoPtr& graph_info) {
  base::RunLoop build_graph_run_loop;
  bool result = false;
  GraphImpl::CreateAndBuild(
      adapter_->command_queue(), adapter_->dml_device(), graph_info,
      base::BindLambdaForTesting(
          [&](mojom::CreateGraphResultPtr create_graph_result) {
            result = create_graph_result->is_graph_remote();
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

template <typename T>
struct OperandInfo {
  mojom::Operand::DataType type;
  std::vector<uint32_t> dimensions;
  std::vector<T> values;
};

struct ActivationOperator {
  mojom::Operator::Kind kind;
  mojom::OperatorAttributesPtr attributes;
};

template <typename T>
struct Conv2dTester {
  OperandInfo<T> input;
  OperandInfo<T> filter;
  struct Conv2dAttributes {
    std::vector<uint32_t> padding = {0, 0, 0, 0};
    std::vector<uint32_t> strides = {1, 1};
    std::vector<uint32_t> dilations = {1, 1};
    uint32_t groups = 1;
    mojom::InputOperandLayout input_layout =
        mojom::InputOperandLayout::kChannelsFirst;
    absl::optional<OperandInfo<T>> bias;
    absl::optional<ActivationOperator> activation;
  };
  Conv2dAttributes attributes;
  OperandInfo<T> output;

  void Test(WebNNGraphDMLImplTest& helper) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t filter_operand_id =
        builder.BuildConstant(filter.dimensions, filter.type,
                              base::as_bytes(base::make_span(filter.values)));
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    mojom::Conv2dAttributesPtr mojo_attributes = mojom::Conv2dAttributes::New();
    mojo_attributes->padding = mojom::Padding2d::New(
        mojom::Size2d::New(attributes.padding[0],
                           attributes.padding[2]) /*beginning padding*/,
        mojom::Size2d::New(attributes.padding[1],
                           attributes.padding[3]) /*ending padding*/);
    mojo_attributes->strides =
        mojom::Size2d::New(attributes.strides[0], attributes.strides[1]);
    mojo_attributes->dilations =
        mojom::Size2d::New(attributes.dilations[0], attributes.dilations[1]);
    mojo_attributes->groups = attributes.groups;
    mojo_attributes->input_layout = attributes.input_layout;

    if (attributes.bias.has_value()) {
      mojo_attributes->bias_operand_id = builder.BuildConstant(
          attributes.bias->dimensions, attributes.bias->type,
          base::as_bytes(base::make_span(attributes.bias->values)));
    }

    if (attributes.activation.has_value()) {
      mojo_attributes->activation = mojom::Operator::New();
      mojo_attributes->activation->kind =
          std::move(attributes.activation.value().kind);
      mojo_attributes->activation->attributes =
          std::move(attributes.activation.value().attributes);
    }

    builder.BuildOperator(
        mojom::Operator::Kind::kConv2d, {input_operand_id, filter_operand_id},
        {output_operand_id},
        mojom::OperatorAttributes::NewConv2d(std::move(mojo_attributes)));
    EXPECT_TRUE(helper.CreateAndBuildGraph(builder.GetGraphInfo()));
  }
};

// Test building a DML graph with single operator conv2d.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorConv2d) {
  // Test conv2d with NCHW layout, padding = {1, 1, 1, 1}, fusing with bias.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0}},
        .attributes = {.padding = {1, 1, 1, 1},
                       .bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {1.0}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 5, 5}}}
        .Test(*this);
  }
  // Test conv2d with NCHW layout, padding = {1, 1, 1, 1}, without bias.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0}},
        .attributes = {.padding = {1, 1, 1, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 5, 5}}}
        .Test(*this);
  }
  // Test conv2d with NHWC layout, padding = {1, 1, 1, 1}.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 5, 5, 1}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0}},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout = mojom::InputOperandLayout::kChannelsLast,
                       .bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {1.0}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 5, 1}}}
        .Test(*this);
  }
  // Test conv2d with NHWC layout, fusing with relu activation.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 5, 5, 1}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0}},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout = mojom::InputOperandLayout::kChannelsLast,
                       .bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {1.0}},
                       .activation =
                           ActivationOperator{mojom::Operator::Kind::kRelu,
                                              nullptr}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 5, 1}}}
        .Test(*this);
  }
  // Test depthwise conv2d by setting groups to input channels.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 4, 2, 2}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {4, 1, 2, 2},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 1.0, 4.0, 7.0, 2.0,
                              5.0, 8.0}},
        .attributes = {.padding = {0, 0, 0, 0},
                       .groups = 4,
                       .input_layout =
                           mojom::InputOperandLayout::kChannelsFirst,
                       .bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {4},
                               .values = {1.0, 2.0, 3.0, 4.0}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 4, 1, 1}}}
        .Test(*this);
  }
}

template <typename T>
struct ElementWiseBinaryTester {
  OperandInfo<T> lhs;
  OperandInfo<T> rhs;
  mojom::Operator::Kind kind;
  OperandInfo<T> output;
  void Test(WebNNGraphDMLImplTest& helper) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t lhs_operand_id =
        builder.BuildInput("lhs", lhs.dimensions, lhs.type);
    uint64_t rhs_operand_id =
        builder.BuildInput("rhs", rhs.dimensions, rhs.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildOperator(kind, {lhs_operand_id, rhs_operand_id},
                          {output_operand_id});
    EXPECT_TRUE(WebNNGraphImpl::ValidateGraph(builder.GetGraphInfo()));
    EXPECT_TRUE(helper.CreateAndBuildGraph(builder.GetGraphInfo()));
  }
};

// Test building a DML graph with single operator element-wise binary.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorElementWiseBinary) {
  // Test building a DML graph with single operator add.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4}},
        .kind = mojom::Operator::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator add using broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 4}},
        .kind = mojom::Operator::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator div.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 3, 3, 1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 3, 3, 1}},
        .kind = mojom::Operator::Kind::kDiv,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 3, 3, 1}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator div using broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 1, 1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 3, 4}},
        .kind = mojom::Operator::Kind::kDiv,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator max.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 5, 3, 3}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 5, 3, 3}},
        .kind = mojom::Operator::Kind::kMax,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 3, 3}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator max using broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 5, 3, 3}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 5, 1, 1}},
        .kind = mojom::Operator::Kind::kMax,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 3, 3}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator min.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 3, 3, 1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 3, 3, 1}},
        .kind = mojom::Operator::Kind::kMin,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 3, 3, 1}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator min using broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 3, 1, 1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 3, 1}},
        .kind = mojom::Operator::Kind::kMin,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 3, 3, 1}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator mul.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 5, 3, 3}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 5, 3, 3}},
        .kind = mojom::Operator::Kind::kMul,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 3, 3}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator mul using broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 5, 3, 3}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1}},
        .kind = mojom::Operator::Kind::kMul,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 3, 3}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator pow.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4}},
        .kind = mojom::Operator::Kind::kPow,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator pow using broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 4}},
        .kind = mojom::Operator::Kind::kPow,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator sub.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4}},
        .kind = mojom::Operator::Kind::kSub,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4}}}
        .Test(*this);
  }
  // Test building a DML graph with single operator sub using broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 4}},
        .kind = mojom::Operator::Kind::kSub,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4}}}
        .Test(*this);
  }
}

template <typename T>
struct Pool2dTester {
  OperandInfo<T> input;
  struct Pool2dAttributes {
    std::vector<uint32_t> window_dimensions;
    std::vector<uint32_t> padding;
    std::vector<uint32_t> strides;
    std::vector<uint32_t> dilations;
    mojom::InputOperandLayout layout;
  };
  Pool2dAttributes attributes;
  mojom::Pool2d::Kind kind;
  OperandInfo<T> output;

  void Test(WebNNGraphDMLImplTest& helper) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildPool2d(kind, input_operand_id, output_operand_id,
                        std::move(attributes));
    EXPECT_TRUE(helper.CreateAndBuildGraph(builder.GetGraphInfo()));
  }
};

// Test building a DML graph with single operator average pool2d.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorAveragePool2d) {
  {
    // Test average pool2d with nchw layout, strides=2, padding=1, and floor
    // rounding.
    // Since there is no float16 data type in C++, we use uint16 to store the
    // binary data of float16.
    Pool2dTester<uint16_t>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 3, 7, 7}},
        .attributes = {.window_dimensions = {4, 4},
                       .padding = {1, 1, 1, 1},
                       .strides = {2, 2},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Pool2d::Kind::kAveragePool2d,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 3, 3, 3}}}
        .Test(*this);
  }
  {
    // Test average pool2d with nhwc layout, strides=2, padding=1 and ceil
    // rounding.
    Pool2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 7, 7, 3}},
        .attributes = {.window_dimensions = {4, 4},
                       .padding = {1, 1, 1, 1},
                       .strides = {2, 2},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsLast},
        .kind = mojom::Pool2d::Kind::kAveragePool2d,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 4, 4, 3}}}
        .Test(*this);
  }
}

// Test building a DML graph with single operator max pool2d with nchw layout.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorMaxPool2d) {
  // DML_MAX_POOLING2_OPERATOR_DESC was introduced in DML_FEATURE_LEVEL_2_1.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_2_1));
  {
    // Test max pool2d with nchw layout, strides=2, padding=1, and floor
    // rounding.
    Pool2dTester<uint16_t>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 3, 7, 7}},
        .attributes = {.window_dimensions = {4, 4},
                       .padding = {1, 1, 1, 1},
                       .strides = {2, 2},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Pool2d::Kind::kMaxPool2d,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 3, 3, 3}}}
        .Test(*this);
  }
}

template <typename T>
struct UnaryOperatorTester {
  mojom::Operator::Kind kind;
  OperandInfo<T> input;
  OperandInfo<T> output;
  void Test(WebNNGraphDMLImplTest& helper) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildOperator(kind, {input_operand_id}, {output_operand_id});
    EXPECT_TRUE(WebNNGraphImpl::ValidateGraph(builder.GetGraphInfo()));
    EXPECT_TRUE(helper.CreateAndBuildGraph(builder.GetGraphInfo()));
  }
};

// Test building a DML graph with single operator relu.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorRelu) {
  {
    UnaryOperatorTester<float>{
        .kind = mojom::Operator::Kind::kRelu,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4}}}
        .Test(*this);
  }
}

// Test building a DML graph with single operator softmax.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorSoftmax) {
  // DML_ACTIVATION_SOFTMAX_OPERATOR_DESC support for 2 dimensions was
  // introduced in DML_FEATURE_LEVEL_3_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_3_0));
  {
    UnaryOperatorTester<float>{
        .kind = mojom::Operator::Kind::kSoftmax,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 3}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3}}}
        .Test(*this);
  }
}

// Test building a DML graph with two relu operators.
//    [input]
//       |
//      relu1
//       |
//      relu2
TEST_F(WebNNGraphDMLImplTest, BuildGraphWithTwoRelu) {
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

template <typename T>
struct GemmTester {
  mojom::Operator::Kind kind;
  OperandInfo<T> input_a;
  OperandInfo<T> input_b;
  struct GemmAttributes {
    absl::optional<OperandInfo<T>> input_c;
    float alpha = 1.0;
    float beta = 1.0;
    bool a_transpose = false;
    bool b_transpose = false;
  };
  GemmAttributes attributes;
  OperandInfo<T> output;

  void Test(WebNNGraphDMLImplTest& helper) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_a_operand_id =
        builder.BuildInput("input_a", input_a.dimensions, input_a.type);
    uint64_t input_b_operand_id =
        builder.BuildInput("input_b", input_b.dimensions, input_b.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    mojom::GemmAttributesPtr mojom_attributes = mojom::GemmAttributes::New();
    if (attributes.input_c.has_value()) {
      mojom_attributes->c_operand_id = builder.BuildInput(
          "input_c", attributes.input_c->dimensions, attributes.input_c->type);
    }
    mojom_attributes->alpha = attributes.alpha;
    mojom_attributes->beta = attributes.beta;
    mojom_attributes->a_transpose = attributes.a_transpose;
    mojom_attributes->b_transpose = attributes.b_transpose;

    builder.BuildOperator(
        mojom::Operator::Kind::kGemm, {input_a_operand_id, input_b_operand_id},
        {output_operand_id},
        mojom::OperatorAttributes::NewGemm(std::move(mojom_attributes)));
    EXPECT_TRUE(WebNNGraphImpl::ValidateGraph(builder.GetGraphInfo()));
    EXPECT_TRUE(helper.CreateAndBuildGraph(builder.GetGraphInfo()));
  }
};

// Test building a DML graph with single operator gemm.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorGemm) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));

  // Test building a DML graph with single operator gemm.
  {
    GemmTester<float>{.input_a = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 2}},
                      .input_b = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 2}},
                      .output = {.type = mojom::Operand::DataType::kFloat32,
                                 .dimensions = {2, 2}}}
        .Test(*this);
  }

  // Test gemm with a third input.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2}},
        .attributes = {.input_c =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 2}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2}}}
        .Test(*this);
  }

  // Test broadcasting the third input's dimensions from  {1,2} to {2,2}.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2}},
        .attributes = {.input_c =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1, 2}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2}}}
        .Test(*this);
  }

  // Test broadcasting the third input's dimensions from  {2,1} to {2,2}.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2}},
        .attributes = {.input_c =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 1}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2}}}
        .Test(*this);
  }

  // Test gemm with a third input which is a scalar.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2}},
        .attributes = {.input_c =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2}}}
        .Test(*this);
  }
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
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  std::vector<float> constant_data = {5.0, 6.0, 7.0, 8.0};
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
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
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, mojom::Operand::DataType::kFloat32);
  std::vector<float> constant_data = {5.0, 6.0, 7.0, 8.0};
  uint64_t constant_a_operand_id =
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));

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
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, mojom::Operand::DataType::kFloat32);
  std::vector<float> constant_data = {5.0, 6.0, 7.0, 8.0};
  uint64_t constant_a_operand_id =
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));

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
