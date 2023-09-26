// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <DirectML.h>
#include <wrl.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/context_impl.h"
#include "services/webnn/dml/graph_impl.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

namespace {

void BuildAndCompute(
    mojom::GraphInfoPtr graph_info,
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    base::flat_map<std::string, mojo_base::BigBuffer>& named_outputs) {
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  mojo::Remote<mojom::WebNNGraph> webnn_graph_remote;

  WebNNContextProviderImpl::Create(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  // Create the dml::ContextImpl through context provider.
  bool was_callback_called = false;
  base::RunLoop run_loop_create_context;
  auto options = mojom::CreateContextOptions::New();
  webnn_provider_remote->CreateWebNNContext(
      std::move(options),
      base::BindLambdaForTesting(
          [&](mojom::CreateContextResultPtr create_context_result) {
            if (create_context_result->is_context_remote()) {
              webnn_context_remote.Bind(
                  std::move(create_context_result->get_context_remote()));
            }
            was_callback_called = true;
            run_loop_create_context.Quit();
          }));
  run_loop_create_context.Run();
  EXPECT_TRUE(was_callback_called);
  EXPECT_TRUE(webnn_context_remote.is_bound());

  // The dml::GraphImpl should be built successfully.
  base::RunLoop run_loop_create_graph;
  was_callback_called = false;
  webnn_context_remote->CreateGraph(
      std::move(graph_info),
      base::BindLambdaForTesting(
          [&](mojom::CreateGraphResultPtr create_graph_result) {
            webnn_graph_remote.Bind(
                std::move(create_graph_result->get_graph_remote()));
            was_callback_called = true;
            run_loop_create_graph.Quit();
          }));
  run_loop_create_graph.Run();
  EXPECT_TRUE(was_callback_called);
  EXPECT_TRUE(webnn_graph_remote.is_bound());

  // The dml::GraphImpl should compute successfully.
  base::RunLoop run_loop_graph_compute;
  was_callback_called = false;
  webnn_graph_remote->Compute(
      std::move(named_inputs),
      base::BindLambdaForTesting(
          [&](mojom::ComputeResult result,
              absl::optional<base::flat_map<std::string, mojo_base::BigBuffer>>
                  named_results) {
            EXPECT_EQ(result, mojom::ComputeResult::kOk);
            EXPECT_TRUE(named_results.has_value() &&
                        !named_results.value().empty());
            was_callback_called = true;
            named_outputs = std::move(named_results.value());
            run_loop_graph_compute.Quit();
          }));
  run_loop_graph_compute.Run();
  EXPECT_TRUE(was_callback_called);

  webnn_graph_remote.reset();
  webnn_context_remote.reset();
  webnn_provider_remote.reset();
  base::RunLoop().RunUntilIdle();
}

template <typename T>
mojo_base::BigBuffer VectorToBigBuffer(const std::vector<T>& data) {
  return mojo_base::BigBuffer(base::as_bytes(base::make_span(data)));
}

template <typename T>
std::vector<T> BigBufferToVector(mojo_base::BigBuffer big_buffer) {
  std::vector<T> data(big_buffer.size() / sizeof(T));
  memcpy(data.data(), big_buffer.data(), big_buffer.size());
  return data;
}

// This method is especially for checking the floating-point output data of some
// ops like the element wise binary pow, unary operator softmax, etc. The output
// data needs to be compared with the expected output data per element by macros
// EXPECT_FLOAT_EQ.
void VerifyFloatDataIsEqual(const std::vector<float>& data,
                            const std::vector<float>& expected_data) {
  EXPECT_EQ(data.size(), expected_data.size());
  for (size_t i = 0; i < data.size(); ++i) {
    EXPECT_FLOAT_EQ(data[i], expected_data[i]);
  }
}

}  // namespace

class WebNNGraphDMLImplTest : public TestBase {
 public:
  void SetUp() override;

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

// Test building and computing a DML graph with single operator clamp.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorClamp) {
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}));
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

  void Test() {
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

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;

    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        BigBufferToVector<float>(std::move(named_outputs["output"])),
        output.values);
  }
};

// Test building and computing a DML graph with single operator conv2d.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorConv2d) {
  // Test conv2d with NCHW layout, padding = {1, 1, 1, 1}, fusing with bias.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 5, 5},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {1}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 5, 5},
                   .values = {13,  22,  28,  34,  25,  34,  55, 64,  73,
                              52,  64,  100, 109, 118, 82,  94, 145, 154,
                              163, 112, 73,  112, 118, 124, 85}}}
        .Test();
  }
  // Test conv2d with NCHW layout, padding = {1, 1, 1, 1}, without bias.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 5, 5},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.padding = {1, 1, 1, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 5, 5},
                   .values = {12,  21,  27, 33,  24,  33,  54, 63,  72,
                              51,  63,  99, 108, 117, 81,  93, 144, 153,
                              162, 111, 72, 111, 117, 123, 84}}}
        .Test();
  }
  // Test conv2d with NHWC layout, padding = {1, 1, 1, 1}.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 5, 5, 1},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout =
                           mojom::InputOperandLayout::kChannelsLast},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 5, 1},
                   .values = {12,  21,  27, 33,  24,  33,  54, 63,  72,
                              51,  63,  99, 108, 117, 81,  93, 144, 153,
                              162, 111, 72, 111, 117, 123, 84}}}
        .Test();
  }
  // Test conv2d with NHWC layout, fusing with relu activation.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 5, 5, 1},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout = mojom::InputOperandLayout::kChannelsLast,
                       .bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {-100}},
                       .activation =
                           ActivationOperator{mojom::Operator::Kind::kRelu,
                                              nullptr}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 5, 1},
                   .values = {0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0}}}
        .Test();
  }
}

template <typename T>
struct ElementWiseBinaryTester {
  OperandInfo<T> lhs;
  OperandInfo<T> rhs;
  mojom::Operator::Kind kind;
  OperandInfo<T> output;
  void Test() {
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

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"lhs", VectorToBigBuffer(lhs.values)});
    named_inputs.insert({"rhs", VectorToBigBuffer(rhs.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        BigBufferToVector<float>(std::move(named_outputs["output"])),
        output.values);
  }
};

// Test building and computing a DML graph with single operator element-wise
// binary.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorElementWiseBinary) {
  // Test building and computing a DML graph with single operator add.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 4, 3, 2, 1}},
        .kind = mojom::Operator::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {7, 7, 7, 7, 7, 7}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator add using
  // broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 2},
                .values = {1, 11}},
        .kind = mojom::Operator::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 2},
                   .values = {2, 12, 3, 13, 4, 14, 5, 15, 6, 16, 7, 17}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator div.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {2, 2, 2, 2, 2, 2}},
        .kind = mojom::Operator::Kind::kDiv,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0.5, 1, 1.5, 2, 2.5, 3}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator div using
  // broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 1, 1},
                .values = {1, 2}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 3, 2},
                .values = {2, 2, 2, 2, 2, 2}},
        .kind = mojom::Operator::Kind::kDiv,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 2},
                   .values = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1, 1, 1, 1, 1, 1}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator max.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 4, 3, 2, 1}},
        .kind = mojom::Operator::Kind::kMax,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {6, 5, 4, 4, 5, 6}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator max using
  // broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 1, 1},
                .values = {6, 3}},
        .kind = mojom::Operator::Kind::kMax,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {6, 6, 6, 4, 5, 6}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator min.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 4, 3, 2, 1}},
        .kind = mojom::Operator::Kind::kMin,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 2, 3, 3, 2, 1}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator min using
  // broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 3, 1, 1},
                .values = {1, 2, 3}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 2, 1},
                .values = {2, 1}},
        .kind = mojom::Operator::Kind::kMin,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 3, 2, 1},
                   .values = {1, 1, 2, 1, 2, 1}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator mul.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 4, 3, 2, 1}},
        .kind = mojom::Operator::Kind::kMul,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {6, 10, 12, 12, 10, 6}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator mul using
  // broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 1, 1},
                .values = {6, 5}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .kind = mojom::Operator::Kind::kMul,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {6, 12, 18, 20, 25, 30}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator pow.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 1, 1, 2, 1}},
        .kind = mojom::Operator::Kind::kPow,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 4, 3, 4, 25, 6}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator pow using
  // broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 3, 1},
                .values = {1, 2, 1}},
        .kind = mojom::Operator::Kind::kPow,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 4, 3, 4, 25, 6}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator sub.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 1, 2, 1, 2}},
        .kind = mojom::Operator::Kind::kSub,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 0, 2, 2, 4, 4}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator sub using
  // broadcasting.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::Operator::Kind::kSub,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {-1, 0, 1, 2, 3, 4}}}
        .Test();
  }
}

struct Pool2dAttributes {
  std::vector<uint32_t> window_dimensions;
  std::vector<uint32_t> padding;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> dilations;
  mojom::InputOperandLayout layout;
};

template <typename T>
struct Pool2dTester {
  OperandInfo<T> input;
  Pool2dAttributes attributes;
  mojom::Pool2d::Kind kind;
  OperandInfo<T> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildPool2d(kind, input_operand_id, output_operand_id,
                        std::move(attributes));

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
              output.values);
  }
};

// Test building and computing a DML graph with single operator average pool2d.
//
// TODO(crbug.com/1484475): Verify the support for
// mojom::Operand::DataType::kFloat16 data type.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorAveragePool2d) {
  {
    // Test average pool2d with nchw layout
    Pool2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                             16, 17, 18}},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Pool2d::Kind::kAveragePool2d,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 2, 2},
                   .values = {3, 4, 6, 7, 12, 13, 15, 16}}}
        .Test();
  }
  {
    // Test average pool2d with nhwc layout
    Pool2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 3, 3, 2},
                  .values = {1, 10, 2, 11, 3, 12, 4, 13, 5, 14, 6, 15, 7, 16, 8,
                             17, 9, 18}},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsLast},
        .kind = mojom::Pool2d::Kind::kAveragePool2d,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 2, 2},
                   .values = {3, 12, 4, 13, 6, 15, 7, 16}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator max pool2d with
// nchw layout.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorMaxPool2d) {
  // Test max pool2d with nchw layout, strides=1, padding=0, and floor
  // rounding.
  Pool2dTester<float>{
      .input = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 3},
                .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                           16, 17, 18}},
      .attributes = {.window_dimensions = {2, 2},
                     .padding = {0, 0, 0, 0},
                     .strides = {1, 1},
                     .dilations = {1, 1},
                     .layout = mojom::InputOperandLayout::kChannelsFirst},
      .kind = mojom::Pool2d::Kind::kMaxPool2d,
      .output = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {1, 2, 2, 2},
                 .values = {5, 6, 8, 9, 14, 15, 17, 18}}}
      .Test();
}

template <typename T>
struct UnaryOperatorTester {
  mojom::Operator::Kind kind;
  OperandInfo<T> input;
  OperandInfo<T> output;
  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildOperator(kind, {input_operand_id}, {output_operand_id});

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        BigBufferToVector<float>(std::move(named_outputs["output"])),
        output.values);
  }
};

// Test building and computing a DML graph with single operator softmax.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorSoftmax) {
  // DML_ACTIVATION_SOFTMAX_OPERATOR_DESC support for 2 dimensions was
  // introduced in DML_FEATURE_LEVEL_3_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_3_0));
  {
    UnaryOperatorTester<float>{
        .kind = mojom::Operator::Kind::kSoftmax,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {3, 4},
                  .values = {0.4301911, 0.54719144, -1.1637765, 0.18390046,
                             0.58390397, 0.1735679, 0.539724, -0.953514,
                             -0.59202826, -0.17344485, 0.14395015,
                             -0.37920907}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3, 4},
                   .values = {0.32165375, 0.36157736, 0.0653337, 0.25143513,
                              0.35271573, 0.23400122, 0.33747196, 0.07581109,
                              0.17110129, 0.26004094, 0.35717794, 0.21167983}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator relu.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorRelu) {
  {
    UnaryOperatorTester<float>{
        .kind = mojom::Operator::Kind::kRelu,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 4},
                  .values = {-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22,  23,  24}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4},
                   .values = {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                              13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}}}
        .Test();
  }

  // Test with 8-byte-length input/output.
  {
    UnaryOperatorTester<float>{
        .kind = mojom::Operator::Kind::kRelu,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 1},
                  .values = {-1, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 1},
                   .values = {0, 2}}}
        .Test();
  }
}

// Test building and computing a DML graph with two relu operators.
//    [input]
//       |
//      relu1
//       |
//      relu2
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithTwoRelu) {
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(
      BigBufferToVector<float>(std::move(named_outputs["output"])),
      std::vector<float>({0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                          13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}));
}

// Test building and computing a DML graph with single operator reshape.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorReshape) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kReshape, {input_operand_id},
                        {output_operand_id});

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1,  2,  3,  4,  5,  6,  7,  8,
                                   9,  10, 11, 12, 13, 14, 15, 16,
                                   17, 18, 19, 20, 21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            input_data);
}

// Test building and computing a DML graph with two operators (reshape as the
// last node).
//    [input]
//       |
//      relu
//       |
//     reshape
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithReshapeAsLastNode) {
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1,  2,  3,  4,  5,  6,  7,  8,
                                   9,  10, 11, 12, 13, 14, 15, 16,
                                   17, 18, 19, 20, 21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            input_data);
}

// Test building and computing a DML graph with two operators (reshape as an
// intermediate node).
//    [input]
//       |
//    reshape
//       |
//      relu
TEST_F(WebNNGraphDMLImplTest,
       BuildAndComputeGraphWithReshapeAsIntermediateNode) {
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1,  2,  3,  4,  5,  6,  7,  8,
                                   9,  10, 11, 12, 13, 14, 15, 16,
                                   17, 18, 19, 20, 21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            input_data);
}

// Test building and computing a DML graph with two reshape operators
//    [input]
//       |
//    reshape1
//       |
//    reshape2
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithTwoReshape) {
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1,  2,  3,  4,  5,  6,  7,  8,
                                   9,  10, 11, 12, 13, 14, 15, 16,
                                   17, 18, 19, 20, 21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            input_data);
}

// Test building and computing a DML graph with two operators and two outputs
//      [input]
//       /   \
//  reshape   relu
//     |        |
// [output1] [output2]
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithTwoOutputs) {
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(
      BigBufferToVector<float>(std::move(named_outputs["output1"])),
      std::vector<float>({-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12,
                          13, 14, 15, 16, 17, 18, 19, 20, 21, 22,  23,  24}));
  EXPECT_EQ(
      BigBufferToVector<float>(std::move(named_outputs["output2"])),
      std::vector<float>({0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                          13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}));
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

  void Test() {
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

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input_a", VectorToBigBuffer(input_a.values)});
    named_inputs.insert({"input_b", VectorToBigBuffer(input_b.values)});
    if (attributes.input_c.has_value()) {
      named_inputs.insert(
          {"input_c", VectorToBigBuffer(attributes.input_c->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
              output.values);
  }
};

// Test building and computing a DML graph with single operator gemm.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorGemm) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));

  // Test gemm without a third input.
  {
    GemmTester<float>{.input_a = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 2},
                                  .values = {1, 2, 3, 4}},
                      .input_b = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 2},
                                  .values = {1, 2, 3, 4}},
                      .output = {.type = mojom::Operand::DataType::kFloat32,
                                 .dimensions = {2, 2},
                                 .values = {7, 10, 15, 22}}}
        .Test();
  }

  // Test gemm with a third input.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .attributes = {.input_c =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 2},
                               .values = {1, 1, 1, 1}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {8, 11, 16, 23}}}
        .Test();
  }

  // Test broadcasting the third input's dimensions from  {1,2} to {2,2}.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .attributes = {.input_c =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1, 2},
                               .values = {1, 2}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {8, 12, 16, 24}}}
        .Test();
  }

  // Test broadcasting the third input's dimensions from  {2,1} to {2,2}.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .attributes = {.input_c =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 1},
                               .values = {1, 2}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {8, 11, 17, 24}}}
        .Test();
  }

  // Test gemm with a third input which is a scalar.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .attributes = {.input_c =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {1}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {8, 11, 16, 23}}}
        .Test();
  }
}

// Test building and computing a DML graph with three gemm operations.
//    [input_a] [input_b] [input_a] [input_b]
//           \    /                \    /
//            gemm                  gemm
//                \                /
//                       gemm
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeMultipleOperatorGemm) {
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_a_data = {1, 2, 3, 4};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_a_data)});
  std::vector<float> input_b_data = {1, 1, 1, 1};
  named_inputs.insert({"input_b", VectorToBigBuffer(input_b_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({30, 30, 70, 70}));
}

// Test building and computing a DML graph with one input and one constant.
TEST_F(WebNNGraphDMLImplTest, BuildOneInputAndOneConstantOperand) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  std::vector<float> constant_data = {5, 6, 7, 8};
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_a_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_a_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({12, 14, 12, 14}));
}

// Test building and computing a DML graph with two inputs and two constant in
// the following topology.
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
  std::vector<float> constant_data = {1, 1, 1, 1};
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1, 2, 3, 4};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_data)});
  named_inputs.insert({"input_b", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({30, 30, 70, 70}));
}

// Test building and computing a DML graph with two inputs and two constant in
// the following topology.
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
  std::vector<float> constant_data = {1, 2, 3, 4};
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_data)});
  named_inputs.insert({"input_b", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({30, 30, 70, 70}));
}

// Test building a DML graph in the following topology.
//    [input_a] [input_b]
//           \    /
//            add
//             |
//            relu
//             |
//          max pooling
TEST_F(WebNNGraphDMLImplTest, BuildMaxPooingAsThirdOperator) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id = builder.BuildInput(
      "input_a", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id = builder.BuildInput(
      "input_b", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t intermediate_1_operand_id = builder.BuildIntermediateOperand(
      {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kAdd,
                        {input_a_operand_id, input_b_operand_id},
                        {intermediate_1_operand_id});

  // Relu.
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu,
                        {intermediate_1_operand_id},
                        {intermediate_2_operand_id});

  // Max pooling.
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildPool2d(
      mojom::Pool2d::Kind::kMaxPool2d, intermediate_2_operand_id,
      output_operand_id,
      Pool2dAttributes{.window_dimensions = {1, 1},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst});

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_data)});
  named_inputs.insert({"input_b", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({2, 2, 2, 2}));
}

// Test building a DML graph in the following topology.
//    [input_a] [input_b]
//           \    /
//            add
//             |
//          max pooling
//             |
//            relu
TEST_F(WebNNGraphDMLImplTest, BuildMaxPooingAsSecondOperator) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id = builder.BuildInput(
      "input_a", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id = builder.BuildInput(
      "input_b", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t intermediate_1_operand_id = builder.BuildIntermediateOperand(
      {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kAdd,
                        {input_a_operand_id, input_b_operand_id},
                        {intermediate_1_operand_id});

  // Max pooling.
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildPool2d(
      mojom::Pool2d::Kind::kMaxPool2d, intermediate_1_operand_id,
      intermediate_2_operand_id,
      Pool2dAttributes{.window_dimensions = {1, 1},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst});

  // Relu.
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu,
                        {intermediate_2_operand_id}, {output_operand_id});

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_data)});
  named_inputs.insert({"input_b", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({2, 2, 2, 2}));
}

// Test building a DML graph in the following topology.
//      [input_a]
//          |
//      max pooling
//                  [input_b]
//           \        /
//               add
//                |
//               relu
TEST_F(WebNNGraphDMLImplTest, BuildMaxPooingAsFirstOperator) {
  // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id = builder.BuildInput(
      "input_a", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t intermediate_1_operand_id = builder.BuildIntermediateOperand(
      {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildPool2d(
      mojom::Pool2d::Kind::kMaxPool2d, input_a_operand_id,
      intermediate_1_operand_id,
      Pool2dAttributes{.window_dimensions = {1, 1},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst});

  // Add operation.
  uint64_t input_b_operand_id = builder.BuildInput(
      "input_b", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kAdd,
                        {intermediate_1_operand_id, input_b_operand_id},
                        {intermediate_2_operand_id});

  // Relu.
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu,
                        {intermediate_2_operand_id}, {output_operand_id});

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_data)});
  named_inputs.insert({"input_b", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({2, 2, 2, 2}));
}

}  // namespace webnn::dml
