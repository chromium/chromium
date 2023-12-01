// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <DirectML.h>
#include <stdint.h>
#include <wrl.h>
#include <cmath>
#include <type_traits>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
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
#include "third_party/fp16/src/include/fp16.h"

namespace webnn::dml {

namespace {

// Since there is no float16 data type in C++, use uint16_t to represent the
// binary data.
using float16 = uint16_t;

enum class BuildAndComputeExpectation { kSuccess, kCreateGraphFailure };
void BuildAndCompute(
    mojom::GraphInfoPtr graph_info,
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    base::flat_map<std::string, mojo_base::BigBuffer>& named_outputs,
    BuildAndComputeExpectation expectation =
        BuildAndComputeExpectation::kSuccess) {
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
  webnn_context_remote.set_disconnect_handler(
      base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                     &run_loop_create_graph));
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
  if (expectation == BuildAndComputeExpectation::kCreateGraphFailure) {
    EXPECT_FALSE(was_callback_called);
    EXPECT_FALSE(webnn_graph_remote.is_bound());
    EXPECT_TRUE(webnn_context_remote.is_bound());
    webnn_graph_remote.reset();
    webnn_context_remote.reset();
    webnn_provider_remote.reset();
    base::RunLoop().RunUntilIdle();
    return;
  }
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
  EXPECT_EQ(expectation, BuildAndComputeExpectation::kSuccess);
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
  ASSERT_EQ(data.size(), expected_data.size());
  for (size_t i = 0; i < data.size(); ++i) {
    EXPECT_FLOAT_EQ(data[i], expected_data[i]);
  }
}

// Convert a vector of 32-bit floating-point data to a vector of 16-bit
// floating-point data, both in IEEE precision format.
std::vector<float16> Float16FromFloat32(const std::vector<float>& fp32_data) {
  std::vector<float16> fp16_data;
  fp16_data.reserve(fp32_data.size());

  for (size_t i = 0; i < fp32_data.size(); i++) {
    fp16_data.push_back(fp16_ieee_from_fp32_value(fp32_data[i]));
  }

  return fp16_data;
}

// Convert a vector of 16-bit floating-point data to a vector of 32-bit
// floating-point data, both in IEEE precision format.
std::vector<float> Float16ToFloat32(const std::vector<float16>& fp16_data) {
  std::vector<float> fp32_data;
  fp32_data.reserve(fp16_data.size());

  for (size_t i = 0; i < fp16_data.size(); i++) {
    fp32_data.push_back(fp16_ieee_to_fp32_value(fp16_data[i]));
  }

  return fp32_data;
}

// Get the output data from a `mojo_base::BigBuffer` as 32-bit floating-point
// number.
std::vector<float> GetFloatOutputData(mojo_base::BigBuffer big_buffer,
                                      mojom::Operand::DataType type) {
  std::vector<float> output_data;
  if (type == mojom::Operand::DataType::kFloat16) {
    output_data =
        Float16ToFloat32(BigBufferToVector<float16>(std::move(big_buffer)));
  } else if (type == mojom::Operand::DataType::kFloat32) {
    output_data = BigBufferToVector<float>(std::move(big_buffer));
  } else {
    DLOG(ERROR) << "This data type is not supported.";
    NOTREACHED_NORETURN();
  }

  return output_data;
}

template <typename T>
struct OperandInfo {
  mojom::Operand::DataType type;
  std::vector<uint32_t> dimensions;
  std::vector<T> values;
};

void VerifyIsEqual(mojo_base::BigBuffer actual,
                   const OperandInfo<float>& expected) {
  VerifyFloatDataIsEqual(GetFloatOutputData(std::move(actual), expected.type),
                         expected.values);
}
void VerifyIsEqual(mojo_base::BigBuffer actual,
                   const OperandInfo<float16>& expected) {
  VerifyFloatDataIsEqual(GetFloatOutputData(std::move(actual), expected.type),
                         Float16ToFloat32(expected.values));
}
template <typename T>
void VerifyIsEqual(mojo_base::BigBuffer actual,
                   const OperandInfo<T>& expected) {
  EXPECT_EQ(BigBufferToVector<T>(std::move(actual)), expected.values);
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
  auto adapter_creation_result = Adapter::GetInstanceForTesting();
  ASSERT_TRUE(adapter_creation_result.has_value());
  adapter_ = adapter_creation_result.value();
  // Graph compilation relies on IDMLDevice1::CompileGraph introduced in
  // DirectML version 1.2 or DML_FEATURE_LEVEL_2_1, so skip the tests if the
  // DirectML version doesn't support this feature.
  SKIP_TEST_IF(!adapter_->IsDMLDeviceCompileGraphSupportedForTesting());
}

struct ClampAttributes {
  float min_value;
  float max_value;
};

template <typename T>
struct Conv2dTester {
  mojom::Conv2d_Type type;
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
    absl::optional<mojom::Activation::Tag> activation;
    absl::optional<ClampAttributes> clamp_attributes;
    absl::optional<float> elu_alpha;
    absl::optional<float> leaky_relu_alpha;
  };
  Conv2dAttributes attributes;
  OperandInfo<float> output;

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

    absl::optional<uint64_t> bias_operand_id;
    if (attributes.bias.has_value()) {
      bias_operand_id = builder.BuildConstant(
          attributes.bias->dimensions, attributes.bias->type,
          base::as_bytes(base::make_span(attributes.bias->values)));
    }

    builder.BuildConv2d(type, input_operand_id, filter_operand_id,
                        output_operand_id, std::move(attributes),
                        bias_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;

    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }
};

// Test building and computing a DML graph with single operator conv2d.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorConv2d) {
  // Test conv2d with NCHW layout, padding = {1, 1, 1, 1}, float 32 data type,
  // fusing with bias.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kDirect,
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
  // Test conv2d with NCHW layout, padding = {1, 1, 1, 1}, float 16 data type,
  // fusing with bias.
  {
    Conv2dTester<float16>{
        .type = mojom::Conv2d_Type::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 1, 5, 5},
                  .values = Float16FromFloat32(
                      {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})},
        .filter = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 1, 3, 3},
                   .values = Float16FromFloat32(std::vector<float>(9, 1))},
        .attributes = {.padding = {1, 1, 1, 1},
                       .bias =
                           OperandInfo<float16>{
                               .type = mojom::Operand::DataType::kFloat16,
                               .dimensions = {1},
                               .values = Float16FromFloat32({1})}},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 1, 5, 5},
                   .values = {13,  22,  28,  34,  25,  34,  55, 64,  73,
                              52,  64,  100, 109, 118, 82,  94, 145, 154,
                              163, 112, 73,  112, 118, 124, 85}}}
        .Test();
  }
  // Test conv2d with NCHW layout, padding = {1, 1, 1, 1}, float 32 data type,
  // without bias.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kDirect,
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
  // Test conv2d with NHWC layout, padding = {1, 1, 1, 1}, float 32 data type,
  // without bias.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kDirect,
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
  // Test conv2d with NHWC layout, float 16 data type, padding = {1, 1, 1, 1},
  // without bias.
  {
    Conv2dTester<float16>{
        .type = mojom::Conv2d_Type::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 5, 5, 1},
                  .values = Float16FromFloat32(
                      {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})},
        .filter = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 1, 3, 3},
                   .values = Float16FromFloat32(std::vector<float>(9, 1))},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout =
                           mojom::InputOperandLayout::kChannelsLast},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 5, 5, 1},
                   .values = {12,  21,  27, 33,  24,  33,  54, 63,  72,
                              51,  63,  99, 108, 117, 81,  93, 144, 153,
                              162, 111, 72, 111, 117, 123, 84}}}
        .Test();
  }
  // Test conv2d with NCHW layout, float 32 data type, bias and fusing with elu
  // activation.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 3, 3},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7, 8}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 1, 1},
                   .values = {1}},
        .attributes = {.bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {-5}},
                       .activation = mojom::Activation::Tag::kElu,
                       .elu_alpha = 0.8},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {-0.7946096424007316, -0.7853474888890126,
                              -0.7601703453057089, -0.6917317734107099,
                              -0.5056964470628461, 0, 1, 2, 3}}}
        .Test();
  }
  // Test conv2d with NCHW layout, float 32 data type, bias and fusing with
  // leakyRelu activation.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 4, 4},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                             15}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {-60}},
                       .activation = mojom::Activation::Tag::kLeakyRelu,
                       .leaky_relu_alpha = 0.02},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = {-0.3, -0.12, 21, 30}}}
        .Test();
  }
  // Test conv2d with NHWC layout, float 32 data type, fusing with bias and relu
  // activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kDirect,
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
                       .activation = mojom::Activation::Tag::kRelu},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 5, 1},
                   .values = {0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0}}}
        .Test();
  }
  // Test conv2d with NHWC layout, float 16 data type, fusing with bias and relu
  // activation.
  {
    Conv2dTester<float16>{
        .type = mojom::Conv2d_Type::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 5, 5, 1},
                  .values = Float16FromFloat32(
                      {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})},
        .filter = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 1, 3, 3},
                   .values = Float16FromFloat32(std::vector<float>(9, 1))},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout = mojom::InputOperandLayout::kChannelsLast,
                       .bias =
                           OperandInfo<float16>{
                               .type = mojom::Operand::DataType::kFloat16,
                               .dimensions = {1},
                               .values = Float16FromFloat32({-100})},
                       .activation = mojom::Activation::Tag::kRelu},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 5, 5, 1},
                   .values = {0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0}}}
        .Test();
  }
  // Test conv2d with NCHW layout, fusing with sigmoid activation.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 1, 3, 3},
                  .values = {0.7529087201709872, 0.7520291960017611,
                             0.594952773514815, 0.21631854011984264,
                             0.07589348976741683, 0.15106785419828572,
                             0.12124850358598671, 0.5364335407319905,
                             0.5937089927693522, 0.9910031422560608,
                             0.36309423611370084, 0.9289673923363004,
                             0.22727376737331384, 0.5414123970044269,
                             0.0844534212564596, 0.6765284772046276,
                             0.619325655574763, 0.39292160755260475}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3, 1, 2, 2},
                   .values = {0.14543837927656278, 0.9671129790291346,
                              0.10836050336762582, 0.320230810822804,
                              0.6952692250382182, 0.5070913293589028,
                              0.0813970738017622, 0.5303338853508432,
                              0.30721364807734, 0.4324123448833208,
                              0.9849002194630809, 0.4281076188358701}},
        .attributes = {.input_layout =
                           mojom::InputOperandLayout::kChannelsFirst,
                       .activation = mojom::Activation::Tag::kSigmoid},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3, 2, 2},
                   .values = {0.7077627182006836, 0.6772933602333069,
                              0.5719422101974487, 0.5999819040298462,
                              0.7236577272415161, 0.7131744623184204,
                              0.618513286113739,  0.6196115612983704,
                              0.690409243106842,  0.6519721746444702,
                              0.6102449893951416, 0.704983651638031,
                              0.6666978597640991, 0.7382584810256958,
                              0.6959947943687439, 0.5874307155609131,
                              0.7647256255149841, 0.6926159262657166,
                              0.6934033632278442, 0.6633020043373108,
                              0.7144469618797302, 0.7469926476478577,
                              0.7747598886489868, 0.7273134589195251}}}
        .Test();
  }
  // Test conv2d with NCHW layout, fusing with tanh activation.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 5, 5},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 0.05)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout =
                           mojom::InputOperandLayout::kChannelsFirst,
                       .activation = mojom::Activation::Tag::kTanh},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 5, 5},
                   .values = {0.5370495669980353, 0.7818063576087741,
                              0.874053287886007,  0.9288576214547277,
                              0.8336546070121552, 0.9288576214547277,
                              0.9910074536781176, 0.9963341221150144,
                              0.9985079423323266, 0.9878803970168317,
                              0.9963341221150144, 0.9998996556706324,
                              0.9999592018254402, 0.9999834124992523,
                              0.9993931059399421, 0.9998171682522957,
                              0.9999988852198828, 0.9999995467640772,
                              0.9999998157280003, 0.999969775809118,
                              0.9985079423323266, 0.999969775809118,
                              0.9999834124992523, 0.9999908965525104,
                              0.9995503664595334}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator convTranspose2d.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorConvTranspose2d) {
  // Test convTranspose2d with default attributes.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kTransposed,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 3, 3},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7, 8}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 3},
                   .values = std::vector<float>(18, 1)},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 5, 5},
                   .values = {0., 1.,  3.,  3.,  2.,  3., 8.,  15., 12., 7.,
                              9., 21., 36., 27., 15., 9., 20., 33., 24., 13.,
                              6., 13., 21., 15., 8.,  0., 1.,  3.,  3.,  2.,
                              3., 8.,  15., 12., 7.,  9., 21., 36., 27., 15.,
                              9., 20., 33., 24., 13., 6., 13., 21., 15., 8.}}}
        .Test();
  }
  // Test convTranspose2d with NHWC input layout.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kTransposed,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 3, 3, 1},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7, 8}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 3},
                   .values = std::vector<float>(18, 1)},
        .attributes = {.input_layout =
                           mojom::InputOperandLayout::kChannelsLast},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 5, 2},
                   .values = {0., 0., 1.,  1.,  3.,  3.,  3.,  3.,  2.,  2.,
                              3., 3., 8.,  8.,  15., 15., 12., 12., 7.,  7.,
                              9., 9., 21., 21., 36., 36., 27., 27., 15., 15.,
                              9., 9., 20., 20., 33., 33., 24., 24., 13., 13.,
                              6., 6., 13., 13., 21., 21., 15., 15., 8.,  8.}}}
        .Test();
  }
  // Test convTranspose2d with padding = {1, 1, 1, 1}.
  {
    Conv2dTester<float>{.type = mojom::Conv2d_Type::kTransposed,
                        .input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {1, 1, 2, 2},
                                  .values = {0, 1, 2, 3}},
                        .filter = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 1, 2, 2},
                                   .values = {0, 1, 2, 3}},
                        .attributes = {.padding = {1, 1, 1, 1}},
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 1, 1, 1},
                                   .values = {4.}}}
        .Test();
  }
  // Test convTranspose2d with groups = 2.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kTransposed,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 2, 2},
                  .values = {2, 4, 0, 1, 2, 4, 0, 1}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 1, 2, 2},
                   .values = {3, 1, 1, 5, 3, 1, 1, 5}},
        .attributes = {.groups = 2},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 3},
                   .values = {6., 14., 4., 2., 17., 21., 0., 1., 5., 6., 14.,
                              4., 2., 17., 21., 0., 1., 5.}}}
        .Test();
  }
  // Test convTranspose2d with strides = {3, 2}.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kTransposed,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 3, 3},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7, 8}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 3},
                   .values = std::vector<float>(18, 1)},
        .attributes = {.strides = {3, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 9, 7},
                   .values = {0.,  0.,  1., 1.,  3.,  2., 2.,  0.,  0.,  1.,
                              1.,  3.,  2., 2.,  0.,  0., 1.,  1.,  3.,  2.,
                              2.,  3.,  3., 7.,  4.,  9., 5.,  5.,  3.,  3.,
                              7.,  4.,  9., 5.,  5.,  3., 3.,  7.,  4.,  9.,
                              5.,  5.,  6., 6.,  13., 7., 15., 8.,  8.,  6.,
                              6.,  13., 7., 15., 8.,  8., 6.,  6.,  13., 7.,
                              15., 8.,  8., 0.,  0.,  1., 1.,  3.,  2.,  2.,
                              0.,  0.,  1., 1.,  3.,  2., 2.,  0.,  0.,  1.,
                              1.,  3.,  2., 2.,  3.,  3., 7.,  4.,  9.,  5.,
                              5.,  3.,  3., 7.,  4.,  9., 5.,  5.,  3.,  3.,
                              7.,  4.,  9., 5.,  5.,  6., 6.,  13., 7.,  15.,
                              8.,  8.,  6., 6.,  13., 7., 15., 8.,  8.,  6.,
                              6.,  13., 7., 15., 8.,  8.}}}
        .Test();
  }
  // Test convTranspose2d with strides = {3, 2} and specify the output size to
  // be {10, 8}, which is equivalent to setting outputPadding = {1, 1}.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kTransposed,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 3, 3},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7, 8}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 3},
                   .values = std::vector<float>(18, 1)},
        .attributes = {.strides = {3, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 10, 8},
                   .values = {0.,  0., 1.,  1.,  3., 2.,  2.,  0., 0.,  0.,  1.,
                              1.,  3., 2.,  2.,  0., 0.,  0.,  1., 1.,  3.,  2.,
                              2.,  0., 3.,  3.,  7., 4.,  9.,  5., 5.,  0.,  3.,
                              3.,  7., 4.,  9.,  5., 5.,  0.,  3., 3.,  7.,  4.,
                              9.,  5., 5.,  0.,  6., 6.,  13., 7., 15., 8.,  8.,
                              0.,  6., 6.,  13., 7., 15., 8.,  8., 0.,  6.,  6.,
                              13., 7., 15., 8.,  8., 0.,  0.,  0., 0.,  0.,  0.,
                              0.,  0., 0.,  0.,  0., 1.,  1.,  3., 2.,  2.,  0.,
                              0.,  0., 1.,  1.,  3., 2.,  2.,  0., 0.,  0.,  1.,
                              1.,  3., 2.,  2.,  0., 3.,  3.,  7., 4.,  9.,  5.,
                              5.,  0., 3.,  3.,  7., 4.,  9.,  5., 5.,  0.,  3.,
                              3.,  7., 4.,  9.,  5., 5.,  0.,  6., 6.,  13., 7.,
                              15., 8., 8.,  0.,  6., 6.,  13., 7., 15., 8.,  8.,
                              0.,  6., 6.,  13., 7., 15., 8.,  8., 0.,  0.,  0.,
                              0.,  0., 0.,  0.,  0., 0.}}}
        .Test();
  }
  // Test convTranspose2d fusing with bias.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d_Type::kTransposed,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 2, 2},
                  .values = {0, 1, 2, 3}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = {0, 1, 2, 3}},
        .attributes = {.bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {1}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {1., 1., 2., 1., 5., 7., 5., 13., 10.}}}
        .Test();
  }
  // Test convTranspose2d float 16 data type, fusing with bias and relu
  // activation.
  {
    Conv2dTester<float16>{
        .type = mojom::Conv2d_Type::kTransposed,
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 1, 2, 2},
                  .values = Float16FromFloat32({0, 1, 2, 3})},
        .filter = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 1, 2, 2},
                   .values = Float16FromFloat32({0, 1, 2, 3})},
        .attributes = {.bias =
                           OperandInfo<float16>{
                               .type = mojom::Operand::DataType::kFloat16,
                               .dimensions = {1},
                               .values = Float16FromFloat32({-5})},
                       .activation = mojom::Activation::Tag::kRelu},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 1, 3, 3},
                   .values = {0., 0., 0., 0., 0., 1., 0., 7., 4.}}}
        .Test();
  }
}

// I is the type of the inputs, both of which must be the same.
// O is the type of the output, which by default is the same as the input.
// Logical operators, however, have uint8_t (bool) as outputs.
template <typename I, typename O = I>
struct ElementWiseBinaryTester {
  OperandInfo<I> lhs;
  OperandInfo<I> rhs;
  mojom::ElementWiseBinary::Kind kind;
  OperandInfo<O> output;
  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t lhs_operand_id =
        builder.BuildInput("lhs", lhs.dimensions, lhs.type);
    uint64_t rhs_operand_id =
        builder.BuildInput("rhs", rhs.dimensions, rhs.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildElementWiseBinary(kind, lhs_operand_id, rhs_operand_id,
                                   output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"lhs", VectorToBigBuffer(lhs.values)});
    named_inputs.insert({"rhs", VectorToBigBuffer(rhs.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }
};

// Test building and computing a DML graph with single operator element-wise
// binary.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorElementWiseBinary) {
  // Test building and computing a DML graph with single operator add for 0-D
  // scalars.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {},
                .values = {1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {},
                .values = {6}},
        .kind = mojom::ElementWiseBinary::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {},
                   .values = {7}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator add.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 4, 3, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {7, 7, 7, 7, 7, 7}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator add using
  // broadcasting from 0-D scalar.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {},
                .values = {1}},
        .kind = mojom::ElementWiseBinary::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {2, 3, 4, 5, 6, 7}}}
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
        .kind = mojom::ElementWiseBinary::Kind::kAdd,
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
        .kind = mojom::ElementWiseBinary::Kind::kDiv,
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
        .kind = mojom::ElementWiseBinary::Kind::kDiv,
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
        .kind = mojom::ElementWiseBinary::Kind::kMax,
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
        .kind = mojom::ElementWiseBinary::Kind::kMax,
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
        .kind = mojom::ElementWiseBinary::Kind::kMin,
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
        .kind = mojom::ElementWiseBinary::Kind::kMin,
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
        .kind = mojom::ElementWiseBinary::Kind::kMul,
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
        .kind = mojom::ElementWiseBinary::Kind::kMul,
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
        .kind = mojom::ElementWiseBinary::Kind::kPow,
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
        .kind = mojom::ElementWiseBinary::Kind::kPow,
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
        .kind = mojom::ElementWiseBinary::Kind::kSub,
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
        .kind = mojom::ElementWiseBinary::Kind::kSub,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {-1, 0, 1, 2, 3, 4}}}
        .Test();
  }
  // Test building and computing a DML graph with single operator equal.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 3, 4, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kEqual,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 0, 1, 1, 0, 0}}}
        .Test();
  }

  // Test building and computing a DML graph with single operator equal using
  // broadcasting.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::ElementWiseBinary::Kind::kEqual,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 1, 0, 0, 0, 0}}}
        .Test();
  }

  // Test building and computing a DML graph with single operator greater.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 3, 4, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kGreater,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 0, 0, 0, 1, 1}}}
        .Test();
  }

  // Test building and computing a DML graph with single operator greater using
  // broadcasting.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::ElementWiseBinary::Kind::kGreater,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 0, 1, 1, 1, 1}}}
        .Test();
  }

  // Test building and computing a DML graph with single operator lesser.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 3, 4, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kLesser,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 1, 0, 0, 0, 0}}}
        .Test();
  }

  // Test building and computing a DML graph with single operator lesser using
  // broadcasting.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::ElementWiseBinary::Kind::kLesser,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 0, 0, 0, 0, 0}}}
        .Test();
  }
}

template <typename T, typename O = T>
struct ElementWiseUnaryTester {
  OperandInfo<T> input;
  mojom::ElementWiseUnary::Kind kind;
  OperandInfo<O> output;
  void Test(BuildAndComputeExpectation expectation =
                BuildAndComputeExpectation::kSuccess) {
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildElementWiseUnary(kind, input_operand_id, output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs, expectation);
    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(std::move(named_outputs["output"]), output);
    }
  }
};

// Test building and computing a DML graph with element-wise unary operator.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorElementWiseUnary) {
  OperandInfo<float_t> test_operand_info_float32_scalar{
      .type = mojom::Operand::DataType::kFloat32,
      .dimensions = {},
      .values = {2}};
  OperandInfo<float_t> test_operand_info_float32{
      .type = mojom::Operand::DataType::kFloat32,
      .dimensions = {1, 2, 3, 1},
      .values = {0, 2, 0, 4, 5, 120}};
  OperandInfo<float16> test_operand_info_float16{
      .type = mojom::Operand::DataType::kFloat16,
      .dimensions = {1, 2, 3, 1},
      .values = {0, 2, 0, 4, 5, 120}};
  OperandInfo<int32_t> test_operand_info_int32{
      .type = mojom::Operand::DataType::kInt32,
      .dimensions = {1, 2, 3, 1},
      .values = {0, 2, 0, 4, 5, 120}};
  OperandInfo<int8_t> test_operand_info_int8{
      .type = mojom::Operand::DataType::kInt8,
      .dimensions = {1, 2, 3, 1},
      .values = {0, 2, 0, 4, 5, 120}};
  OperandInfo<uint8_t> test_operand_info_uint8{
      .type = mojom::Operand::DataType::kUint8,
      .dimensions = {1, 2, 3, 1},
      .values = {0, 2, 0, 4, 5, 120}};
  {
    ElementWiseUnaryTester<float_t>{
        .input = test_operand_info_float32,
        .kind = mojom::ElementWiseUnary::Kind::kLogicalNot,
        .output = test_operand_info_float32}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<float16>{
        .input = test_operand_info_float16,
        .kind = mojom::ElementWiseUnary::Kind::kLogicalNot,
        .output = test_operand_info_float16}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<int32_t>{
        .input = test_operand_info_int32,
        .kind = mojom::ElementWiseUnary::Kind::kLogicalNot,
        .output = test_operand_info_int32}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<int8_t>{
        .input = test_operand_info_int8,
        .kind = mojom::ElementWiseUnary::Kind::kLogicalNot,
        .output = test_operand_info_int8}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<uint8_t>{
        .input = {.type = mojom::Operand::DataType::kUint8,
                  .dimensions = {1, 2, 3, 1},
                  .values = {0, 2, 0, 4, 5, 255}},
        .kind = mojom::ElementWiseUnary::Kind::kLogicalNot,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 0, 1, 0, 0, 0}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = test_operand_info_float32_scalar,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_float32_scalar}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = test_operand_info_float32,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_float32}
        .Test();
  }
  {
    ElementWiseUnaryTester<float16>{
        .input = test_operand_info_float16,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_float16}
        .Test();
  }
  {
    ElementWiseUnaryTester<int32_t>{
        .input = test_operand_info_int32,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_int32}
        .Test();
  }
  {
    ElementWiseUnaryTester<int8_t>{
        .input = test_operand_info_int8,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_int8}
        .Test();
  }
  {
    ElementWiseUnaryTester<uint8_t>{
        .input = test_operand_info_uint8,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_uint8}
        .Test();
  }
  {
    // Test Sqrt with 0-D scalar input.
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {},
                  .values = {4}},
        .kind = mojom::ElementWiseUnary::Kind::kSqrt,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {},
                   .values = {2}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 1},
                  .values = {0, 4, 25, 16, 64, 49}},
        .kind = mojom::ElementWiseUnary::Kind::kSqrt,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 2, 5, 4, 8, 7}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 3, 1},
                  .values = Float16FromFloat32({0, 4, 25, 16, 64, 49})},
        .kind = mojom::ElementWiseUnary::Kind::kSqrt,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 3, 1},
                   .values = Float16FromFloat32({0, 2, 5, 4, 8, 7})}}
        .Test();
  }
  {
    ElementWiseUnaryTester<int32_t>{
        .input = test_operand_info_int32,
        .kind = mojom::ElementWiseUnary::Kind::kSqrt,
        .output = test_operand_info_int32}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<int8_t>{.input = test_operand_info_int8,
                                   .kind = mojom::ElementWiseUnary::Kind::kSqrt,
                                   .output = test_operand_info_int8}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<uint8_t>{
        .input = test_operand_info_uint8,
        .kind = mojom::ElementWiseUnary::Kind::kSqrt,
        .output = test_operand_info_uint8}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 1},
                  .values = {0, 4, 0, 16, 64, -5}},
        .kind = mojom::ElementWiseUnary::Kind::kErf,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 1, 0, 1, 1, -1}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 3, 1},
                  .values = Float16FromFloat32({0, 4, 0, 16, 64, -5})},
        .kind = mojom::ElementWiseUnary::Kind::kErf,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 3, 1},
                   .values = Float16FromFloat32({0, 1, 0, 1, 1, -1})}}
        .Test();
  }
  {
    ElementWiseUnaryTester<int32_t>{.input = test_operand_info_int32,
                                    .kind = mojom::ElementWiseUnary::Kind::kErf,
                                    .output = test_operand_info_int32}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<int8_t>{.input = test_operand_info_int8,
                                   .kind = mojom::ElementWiseUnary::Kind::kErf,
                                   .output = test_operand_info_int8}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<uint8_t>{.input = test_operand_info_uint8,
                                    .kind = mojom::ElementWiseUnary::Kind::kErf,
                                    .output = test_operand_info_uint8}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 1},
                  .values = {1, 4, 2, 16, 64, 0}},
        .kind = mojom::ElementWiseUnary::Kind::kReciprocal,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 0.25, 0.5, 0.0625, 0.015625,
                              std::numeric_limits<float>::infinity()}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 3, 1},
                  .values = Float16FromFloat32({1, 4, 2, 16, 64, 0})},
        .kind = mojom::ElementWiseUnary::Kind::kReciprocal,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 3, 1},
                   .values = Float16FromFloat32(
                       {1, 0.25, 0.5, 0.0625, 0.015625,
                        std::numeric_limits<float>::infinity()})}}
        .Test();
  }
  {
    ElementWiseUnaryTester<int32_t>{
        .input = test_operand_info_int32,
        .kind = mojom::ElementWiseUnary::Kind::kReciprocal,
        .output = test_operand_info_int32}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<int8_t>{
        .input = test_operand_info_int8,
        .kind = mojom::ElementWiseUnary::Kind::kReciprocal,
        .output = test_operand_info_int8}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<uint8_t>{
        .input = test_operand_info_uint8,
        .kind = mojom::ElementWiseUnary::Kind::kReciprocal,
        .output = test_operand_info_uint8}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 1},
                  .values = {-1, 4, -2, 16, -64, 0}},
        .kind = mojom::ElementWiseUnary::Kind::kAbs,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 4, 2, 16, 64, 0}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 3},
                  .values = {-1.1, 0, 1.1, -2.2, 0, 2.2}},
        .kind = mojom::ElementWiseUnary::Kind::kCeil,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3},
                   .values = {-1, 0, 2, -2, 0, 3}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1, -2, 3, -4}},
        .kind = mojom::ElementWiseUnary::Kind::kCos,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {cos(1.f), cos(-2.f), cos(3.f), cos(-4.f)}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1, -2, 3, -4}},
        .kind = mojom::ElementWiseUnary::Kind::kExp,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {exp(1.f), exp(-2.f), exp(3.f), exp(-4.f)}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 3, 1},
                  .values = Float16FromFloat32({-1.1, 0, 1.1, -2.2, 0, 2.2})},
        .kind = mojom::ElementWiseUnary::Kind::kFloor,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 3, 1},
                   .values = Float16FromFloat32({-2, 0, 1, -3, 0, 2})}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {3},
                  .values = {0, 3, 10}},
        .kind = mojom::ElementWiseUnary::Kind::kLog,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3},
                   .values = {log(0.f), log(3.f), log(10.f)}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 1},
                  .values = {-1, 0, 1.1, -2.2, 0, 2}},
        .kind = mojom::ElementWiseUnary::Kind::kNeg,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 0, -1.1, 2.2, 0, -2}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1, -2, 3, -4}},
        .kind = mojom::ElementWiseUnary::Kind::kSin,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {sin(1.f), sin(-2.f), sin(3.f), sin(-4.f)}}}
        .Test();
  }
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1, -2, 3, -4}},
        .kind = mojom::ElementWiseUnary::Kind::kTan,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {tan(1.f), tan(-2.f), tan(3.f), tan(-4.f)}}}
        .Test();
  }
}

template <typename T>
struct ExpandTester {
  OperandInfo<T> input;
  OperandInfo<T> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildExpand(input_operand_id, output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);
    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }
};

TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorExpand) {
  {
    // Test building expand 0-D scalar to 3-D tensor.
    ExpandTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {},
                  .values = {6}},
        .output =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {2, 2, 4},
                // [[[ 6,  6,  6,  6],
                //   [ 6,  6,  6,  6]],
                //  [[ 6,  6,  6,  6],
                //   [ 6,  6,  6,  6]]] with shape (2, 3, 4)
                .values = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
            }}
        .Test();
  }
  {
    // Test building expand with the output shape that are the same as input.
    ExpandTester<float>{
        .input =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {3, 4},
                // [[ 1,  2,  3,  4],
                //  [ 5,  6,  7,  8],
                //  [ 9, 10, 11, 12]] with shape (3, 4)
                .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            },
        .output =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {3, 4},
                // [[ 1,  2,  3,  4],
                //  [ 5,  6,  7,  8],
                //  [ 9, 10, 11, 12]] with shape (3, 4)
                .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            }}
        .Test();
  }
  {
    // Test building expand with the input shape that are broadcastable.
    ExpandTester<float>{
        .input =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {2, 1, 5},
                // [[[1, 2, 3, 4, 5]],
                //  [[6, 7, 8, 9, 10]]] with shape (2, 1, 5)
                .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
            },
        .output =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {2, 3, 5},
                // [[[ 1, 2, 3, 4, 5],
                //   [ 1, 2, 3, 4, 5],
                //   [ 1, 2, 3, 4, 5]]],
                //  [[[6, 7, 8, 9, 10],
                //    [6, 7, 8, 9, 10],
                //    [6, 7, 8, 9, 10]]] with shape (2, 3, 5)
                .values = {1, 2, 3, 4, 5,  1, 2, 3, 4, 5,  1, 2, 3, 4, 5,
                           6, 7, 8, 9, 10, 6, 7, 8, 9, 10, 6, 7, 8, 9, 10},
            }}
        .Test();
  }
  {
    // Test building expand with the input shape that are broadcastable and the
    // rank of output shape larger than input.
    ExpandTester<float>{
        .input =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {2, 6},
                // [[[1, 2, 3, 4,  5,  6]],
                //  [[7, 8, 9, 10, 11, 12]]] with shape (2, 6)
                .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            },
        .output =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {3, 2, 6},
                // [[[ 1, 2, 3, 4,  5,  6],
                //   [ 7, 8, 9, 10, 11, 12]],
                //  [[ 1, 2, 3, 4,  5,  6],
                //   [ 7, 8, 9, 10, 11, 12]],
                //  [[ 1, 2, 3, 4,  5,  6],
                //   [ 7, 8, 9, 10, 11, 12]]] with shape (3, 2, 6)
                .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                           1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                           1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            }}
        .Test();
  }
}

TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorCast) {
  OperandInfo<float_t> test_operand_info_float32{
      .type = mojom::Operand::DataType::kFloat32,
      .dimensions = {1, 2, 3, 1},
      .values = {1, 0, 0, 0, 0, 0}};
  OperandInfo<float16> test_operand_info_float16{
      .type = mojom::Operand::DataType::kFloat16,
      .dimensions = {1, 2, 3, 1},
      .values = Float16FromFloat32({1.0f, 0, 0, 0, 0, 0})};
  OperandInfo<int32_t> test_operand_info_int32{
      .type = mojom::Operand::DataType::kInt32,
      .dimensions = {1, 2, 3, 1},
      .values = {1, 0, 0, 0, 0, 0}};
  OperandInfo<uint32_t> test_operand_info_uint32{
      .type = mojom::Operand::DataType::kUint32,
      .dimensions = {1, 2, 3, 1},
      .values = {1, 0, 0, 0, 0, 0}};
  OperandInfo<int8_t> test_operand_info_int8{
      .type = mojom::Operand::DataType::kInt8,
      .dimensions = {1, 2, 3, 1},
      .values = {1, 0, 0, 0, 0, 0}};
  OperandInfo<uint8_t> test_operand_info_uint8{
      .type = mojom::Operand::DataType::kUint8,
      .dimensions = {1, 2, 3, 1},
      .values = {1, 0, 0, 0, 0, 0}};

  // Test all combinations from float data type.
  {
    {
      ElementWiseUnaryTester<float, float16>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test();
    }

    {
      ElementWiseUnaryTester<float, int32_t>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test();
    }

    {
      ElementWiseUnaryTester<float, uint32_t>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test();
    }

    {
      ElementWiseUnaryTester<float, int8_t>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test();
    }

    {
      ElementWiseUnaryTester<float, uint8_t>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test();
    }
  }
  // Test all combinations from float16 data type.
  {
    {
      ElementWiseUnaryTester<float16, float>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test();
    }
    {
      ElementWiseUnaryTester<float16, int32_t>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test();
    }

    {
      ElementWiseUnaryTester<float16, uint32_t>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test();
    }

    {
      ElementWiseUnaryTester<float16, int8_t>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test();
    }

    {
      ElementWiseUnaryTester<float16, uint8_t>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test();
    }
  }
  // Test all combinations from int32 data type.
  {
    {
      ElementWiseUnaryTester<int32_t, float>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test();
    }

    {
      ElementWiseUnaryTester<int32_t, float16>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test();
    }

    {
      ElementWiseUnaryTester<int32_t, uint32_t>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test();
    }

    {
      ElementWiseUnaryTester<int32_t, int8_t>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test();
    }

    {
      ElementWiseUnaryTester<int32_t, uint8_t>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test();
    }
  }
  // Test all combinations from uint32 data type.
  {
    {
      ElementWiseUnaryTester<uint32_t, float>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test();
    }

    {
      ElementWiseUnaryTester<uint32_t, float16>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test();
    }

    {
      ElementWiseUnaryTester<uint32_t, int32_t>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test();
    }

    {
      ElementWiseUnaryTester<uint32_t, int8_t>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test();
    }

    {
      ElementWiseUnaryTester<uint32_t, uint8_t>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test();
    }
  }
  // Test all combinations from int8_t data type.
  {
    {
      ElementWiseUnaryTester<int8_t, float>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test();
    }

    {
      ElementWiseUnaryTester<int8_t, float16>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test();
    }

    {
      ElementWiseUnaryTester<int8_t, int32_t>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test();
    }

    {
      ElementWiseUnaryTester<int8_t, uint32_t>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test();
    }

    {
      ElementWiseUnaryTester<int8_t, uint8_t>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test();
    }
  }
  // Test all combinations from uint8_t data type.
  {
    {
      ElementWiseUnaryTester<uint8_t, float>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test();
    }

    {
      ElementWiseUnaryTester<uint8_t, float16>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test();
    }

    {
      ElementWiseUnaryTester<uint8_t, int32_t>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test();
    }

    {
      ElementWiseUnaryTester<uint8_t, uint32_t>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test();
    }

    {
      ElementWiseUnaryTester<uint8_t, int8_t>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test();
    }
  }
  // Test case where dimensions dont match
  {
    OperandInfo<int8_t> test_operand_info_int8_wrong_dimension{
        .type = mojom::Operand::DataType::kInt8,
        .dimensions = {1, 2, 3, 2},
        .values = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}};
    ElementWiseUnaryTester<uint8_t, int8_t>{
        .input = test_operand_info_uint8,
        .kind = mojom::ElementWiseUnary::Kind::kCast,
        .output = test_operand_info_int8_wrong_dimension}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
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
  OperandInfo<float> output;

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

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }
};

// Test building and computing a DML graph with single operator average
// pool2d.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorAveragePool2d) {
  {
    // Test average pool2d with nchw layout, float 32 data type.
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
    // Test average pool2d with nchw layout, float 16 data type.
    Pool2dTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 3, 3},
                  .values =
                      Float16FromFloat32({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                          13, 14, 15, 16, 17, 18})},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Pool2d::Kind::kAveragePool2d,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 2, 2},
                   .values = {3, 4, 6, 7, 12, 13, 15, 16}}}
        .Test();
  }
  {
    // Test average pool2d with nhwc layout,, float 32 data type.
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
  {
    // Test average pool2d with nhwc layout,, float 16 data type.
    Pool2dTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 3, 3, 2},
                  .values =
                      Float16FromFloat32({1, 10, 2, 11, 3, 12, 4, 13, 5, 14, 6,
                                          15, 7, 16, 8, 17, 9, 18})},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsLast},
        .kind = mojom::Pool2d::Kind::kAveragePool2d,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 2, 2},
                   .values = {3, 12, 4, 13, 6, 15, 7, 16}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator max pool2d
// with nchw layout.
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
struct PreluTester {
  OperandInfo<T> input;
  OperandInfo<T> slope;
  OperandInfo<float> output;
  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t slope_operand_id =
        builder.BuildInput("slope", slope.dimensions, slope.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildPrelu(input_operand_id, slope_operand_id, output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    named_inputs.insert({"slope", VectorToBigBuffer(slope.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }
};

TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorPrelu) {
  {
    // Test prelu when the input and slope have the same dimensions.
    PreluTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, 11, 12,
                             13, 14, 15, 16, 17, 18}},
        .slope = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08,
                             0.09, 0.1, 0.11, 0.12, 0.13, 0.14, 0.15, 0.16,
                             0.17, 0.18}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 3},
                   .values = {-0.01, -0.04, -0.09, -0.16, -0.25, -0.36, -0.49,
                              -0.64, -0.81, -1, 11, 12, 13, 14, 15, 16, 17,
                              18}}}
        .Test();
  }
  {
    // Test prelu with broadcastable slope.
    PreluTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, 11, 12,
                             13, 14, 15, 16, 17, 18}},
        .slope = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1},
                  .values = {0.01}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 3},
                   .values = {-0.01, -0.02, -0.03, -0.04, -0.05, -0.06, -0.07,
                              -0.08, -0.09, -0.1, 11, 12, 13, 14, 15, 16, 17,
                              18}}}
        .Test();
  }
}

template <typename T>
struct SplitTester {
  OperandInfo<T> input;
  uint32_t axis;
  std::vector<OperandInfo<T>> outputs;

  void Test() {
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    std::vector<uint64_t> output_operand_ids;
    output_operand_ids.reserve(outputs.size());
    for (size_t i = 0; i < outputs.size(); ++i) {
      const auto& output = outputs[i];
      output_operand_ids.push_back(builder.BuildOutput(
          "output" + base::NumberToString(i), output.dimensions, output.type));
    }
    builder.BuildSplit(input_operand_id, output_operand_ids, axis);
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    for (size_t i = 0; i < outputs.size(); ++i) {
      EXPECT_EQ(BigBufferToVector<float>(std::move(
                    named_outputs["output" + base::NumberToString(i)])),
                outputs[i].values);
    }
  }
};

template <typename T>
struct SliceTester {
  struct SliceAttributes {
    std::vector<uint32_t> starts;
    std::vector<uint32_t> sizes;
  };

  OperandInfo<T> input;
  SliceAttributes attributes;
  OperandInfo<T> output;

  void Test() {
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildSlice(input_operand_id, output_operand_id,
                       std::move(attributes.starts),
                       std::move(attributes.sizes));
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }
};

TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSliceOperator) {
  // DML_OPERATOR_SLICE support for dimensions other than 4 or 5 was
  // introduced in DML_FEATURE_LEVEL_3_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_3_0));
  {
    // Test a simple 2-dimension slice
    SliceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                 .dimensions = {2, 2},
                                 // [[1, 2],
                                 //  [3, 4]] with shape [2, 2]
                                 .values = {1, 2, 3, 4}},
                       .attributes = {.starts = {0, 0}, .sizes = {2, 2}},
                       .output = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 2},
                                  // [[1, 2],
                                  //  [3, 4]] with shape [2, 2]
                                  .values = {1, 2, 3, 4}}}
        .Test();
  }
  {
    // Test a complex 3-dimension slice
    SliceTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {3, 4, 5},
                  // [[[1 , 4 , 4 , -6, -3],
                  //   [-1, 7 , 3 , 1 , -8],
                  //   [1 , -1, -2, -3, 6 ],
                  //   [7 , 6 , 1 , -5, -7]],
                  //  [[1 , 1 , 5 , 3 , 3 ],
                  //   [3 , -3, -8, 2 , -1],
                  //   [8 , -1, -6, 1 , -7],
                  //   [1 , 4 , 1 , -5, 1 ]],
                  //  [[-8, 4 , 1 , -1, 9 ],
                  //   [-4, 1 , -5, -4, -1],
                  //   [4 , -1, -3, 7 , 1 ],
                  //   [9 , -4, -9, -8, -9]]] with shape [3, 4, 5]
                  .values = {1,  4,  4,  -6, -3, -1, 7,  3,  1,  -8, 1,  -1,
                             -2, -3, 6,  7,  6,  1,  -5, -7, 1,  1,  5,  3,
                             3,  3,  -3, -8, 2,  -1, 8,  -1, -6, 1,  -7, 1,
                             4,  1,  -5, 1,  -8, 4,  1,  -1, 9,  -4, 1,  -5,
                             -4, -1, 4,  -1, -3, 7,  1,  9,  -4, -9, -8, -9}},
        .attributes = {.starts = {0, 0, 1}, .sizes = {2, 3, 4}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3, 4},
                   // [[[4 , 4 , -6, -3],
                   //   [7 , 3 , 1 , -8],
                   //   [-1, -2, -3, 6 ]],
                   //  [[1 , 5 , 3 , 3 ],
                   //   [-3, -8, 2 , -1],
                   //   [-1, -6, 1 , -7]]] with shape [2, 3, 4]
                   .values = {4, 4, -6, -3, 7,  3,  1, -8, -1, -2, -3, 6,
                              1, 5, 3,  3,  -3, -8, 2, -1, -1, -6, 1,  -7}}}
        .Test();
  }
}

TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorSplit) {
  {
    SplitTester<float>{
        .input =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {2, 1, 3, 4},
                // [[[[ 1,  2,  3,  4],
                //    [ 5,  6,  7,  8],
                //    [ 9, 10, 11, 12]]],
                //  [[[13, 14, 15, 16],
                //    [17, 18, 19, 20],
                //    [21, 22, 23, 24]]]] with shape (2, 1, 3, 4)
                .values = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                           13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},

            },
        .axis = 0,
        .outputs = {{
                        .type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {1, 1, 3, 4},
                        // [[[[ 1,  2,  3,  4],
                        //    [ 5,  6,  7,  8],
                        //    [ 9, 10, 11, 12]]]] with shape (1, 1, 3, 4)
                        .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
                    },
                    {
                        .type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {1, 1, 3, 4},
                        // [[[[13, 14, 15, 16],
                        //    [17, 18, 19, 20],
                        //    [21, 22, 23, 24]]]] with shape (1, 1, 3, 4)
                        .values = {13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                   24},
                    }}}
        .Test();
  }
  {
    SplitTester<float>{
        .input =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 4},
                // [[[[ 1,  2,  3,  4],
                //    [ 5,  6,  7,  8],
                //    [ 9, 10, 11, 12]],
                //   [[13, 14, 15, 16],
                //    [17, 18, 19, 20],
                //    [21, 22, 23, 24]]]] with shape (1, 2, 3, 4)
                .values = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                           13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},

            },
        .axis = 1,
        .outputs = {{
                        .type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {1, 1, 3, 4},
                        // [[[[ 1,  2,  3,  4],
                        //    [ 5,  6,  7,  8],
                        //    [ 9, 10, 11, 12]]]] with shape (1, 1, 3, 4)
                        .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
                    },
                    {
                        .type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {1, 1, 3, 4},
                        // [[[[13, 14, 15, 16],
                        //    [17, 18, 19, 20],
                        //    [21, 22, 23, 24]]]] with shape (1, 1, 3, 4)
                        .values = {13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                   24},
                    }}}
        .Test();
  }
  {
    SplitTester<float>{
        .input =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {2, 1, 3, 4},
                // [[[[ 1,  2,  3,  4],
                //    [ 5,  6,  7,  8],
                //    [ 9, 10, 11, 12]]],
                //  [[[13, 14, 15, 16],
                //    [17, 18, 19, 20],
                //    [21, 22, 23, 24]]]] with shape (2, 1, 3, 4)
                .values = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                           13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},

            },
        .axis = 2,
        .outputs = {{
                        .type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 1, 1, 4},
                        // [[[[ 1,  2,  3,  4]]],
                        //  [[[13, 14, 15, 16]]]] with shape (2, 1, 1, 4)
                        .values = {1, 2, 3, 4, 13, 14, 15, 16},
                    },
                    {
                        .type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 1, 2, 4},
                        // [[[[ 5,  6,  7,  8],
                        //    [ 9, 10, 11, 12]]],
                        //  [[[17, 18, 19, 20],
                        //    [21, 22, 23, 24]]]] with shape (2, 1, 2, 4)
                        .values = {5, 6, 7, 8, 9, 10, 11, 12, 17, 18, 19, 20,
                                   21, 22, 23, 24},
                    }}}
        .Test();
  }
  {
    SplitTester<float>{
        .input =
            {
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {2, 1, 3, 4},
                // [[[[ 1,  2,  3,  4],
                //    [ 5,  6,  7,  8],
                //    [ 9, 10, 11, 12]]],
                //  [[[13, 14, 15, 16],
                //    [17, 18, 19, 20],
                //    [21, 22, 23, 24]]]] with shape (2, 1, 3, 4)
                .values = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                           13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},

            },
        .axis = 3,
        .outputs = {{
                        .type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 1, 3, 2},
                        // [[[[ 1,  2],
                        //    [ 5,  6],
                        //    [ 9, 10]]],
                        //  [[[13, 14],
                        //    [17, 18],
                        //    [21, 22]]]] with shape (2, 1, 3, 2)
                        .values = {1, 2, 5, 6, 9, 10, 13, 14, 17, 18, 21, 22},
                    },
                    {
                        .type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 1, 3, 2},
                        // [[[[ 3,  4],
                        //    [ 7,  8],
                        //    [11, 12]]],
                        //  [[[15, 16],
                        //    [19, 20],
                        //    [23, 24]]]] with shape (2, 1, 3, 2)
                        .values = {3, 4, 7, 8, 11, 12, 15, 16, 19, 20, 23, 24},
                    }}}
        .Test();
  }
}

// Test building and computing a DML graph in the following topology.
//         [input]
//            |
//          split
//        /       \
//   [output1]  reshape
//                 |
//             [output2]
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithSplitAndReshape) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {2, 5}, mojom::Operand::DataType::kFloat32);
  uint64_t output1_operand_id = builder.BuildOutput(
      "output1", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t split_operand_id = builder.BuildIntermediateOperand(
      {2, 3}, mojom::Operand::DataType::kFloat32);
  builder.BuildSplit(input_operand_id, {output1_operand_id, split_operand_id},
                     1);

  uint64_t output_operand_id = builder.BuildOutput(
      "output2", {3, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildReshape(split_operand_id, output_operand_id);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  // [[ 1  2  3  4  5]
  //  [ 6  7  8  9 10]] with shape (2, 5)
  std::vector<float> input_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  // [[1  2]
  //  [6  7]] with shape (2, 2)
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output1"])),
            std::vector<float>({1, 2, 6, 7}));
  // [[3  4]
  //  [5  8]
  //  [9  10]] with shape (3, 2)
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output2"])),
            std::vector<float>({3, 4, 5, 8, 9, 10}));
}

template <typename T>
struct PadTester {
  OperandInfo<T> input;
  std::vector<uint32_t> beginning_padding;
  std::vector<uint32_t> ending_padding;
  mojom::PaddingMode::Tag mode = mojom::PaddingMode::Tag::kConstant;
  float value = 0;
  OperandInfo<float> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildPad(input_operand_id, output_operand_id, beginning_padding,
                     ending_padding, mode, value);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }
};

// Test building and computing a DML graph with single operator pad.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorPad) {
  // Test pad with mode = "constant" and value = 0 by default.
  {
    PadTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 3},
                  // [[1 2 3]
                  //  [4 5 6]]]] with shape (2, 3)
                  .values = {1, 2, 3, 4, 5, 6}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {4, 7},
                   // [[0 0 0 0 0 0 0]
                   //  [0 0 1 2 3 0 0]
                   //  [0 0 4 5 6 0 0]
                   //  [0 0 0 0 0 0 0]] with shape ( 4, 7)
                   .values = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 0, 0,
                              0, 0, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0}}}
        .Test();
  }
  // Test pad with mode = "constant" and value = 1.
  {
    PadTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 3},
                  // [[1 2 3]
                  //  [4 5 6]]]] with shape (2, 3)
                  .values = {1, 2, 3, 4, 5, 6}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .value = 1,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {4, 7},
                   // [[1 1 1 1 1 1 1]
                   //  [1 1 1 2 3 1 1]
                   //  [1 1 4 5 6 1 1]
                   //  [1 1 1 1 1 1 1]] with shape ( 4, 7)
                   .values = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1,
                              1, 1, 4, 5, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1}}}
        .Test();
  }
  // Test pad with mode = "edge".
  {
    PadTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 3},
                  // [[1 2 3]
                  //  [4 5 6]]]] with shape (2, 3)
                  .values = {1, 2, 3, 4, 5, 6}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .mode = mojom::PaddingMode::Tag::kEdge,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {4, 7},
                   // [[1 1 1 2 3 3 3]
                   //  [1 1 1 2 3 3 3]
                   //  [4 4 4 5 6 6 6]
                   //  [4 4 4 5 6 6 6]] with shape ( 4, 7)
                   .values = {1, 1, 1, 2, 3, 3, 3, 1, 1, 1, 2, 3, 3, 3,
                              4, 4, 4, 5, 6, 6, 6, 4, 4, 4, 5, 6, 6, 6}}}
        .Test();
  }
  // Test pad with mode = "reflection".
  {
    PadTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 3},
                  // [[1 2 3]
                  //  [4 5 6]]]] with shape (2, 3)
                  .values = {1, 2, 3, 4, 5, 6}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .mode = mojom::PaddingMode::Tag::kReflection,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {4, 7},
                   // [[6 5 4 5 6 5 4]
                   //  [3 2 1 2 3 2 1]
                   //  [6 5 4 5 6 5 4]
                   //  [3 2 1 2 3 2 1]] with shape ( 4, 7)
                   .values = {6, 5, 4, 5, 6, 5, 4, 3, 2, 1, 2, 3, 2, 1,
                              6, 5, 4, 5, 6, 5, 4, 3, 2, 1, 2, 3, 2, 1}}}
        .Test();
  }
  // Test pad with mode = "symmetric".
  {
    PadTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 3},
                  // [[1 2 3]
                  //  [4 5 6]]]] with shape (2, 3)
                  .values = {1, 2, 3, 4, 5, 6}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .mode = mojom::PaddingMode::Tag::kSymmetric,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {4, 7},
                   // [[2 1 1 2 3 3 2]
                   //  [2 1 1 2 3 3 2]
                   //  [5 4 4 5 6 6 5]
                   //  [5 4 4 5 6 6 5]] with shape ( 4, 7)
                   .values = {2, 1, 1, 2, 3, 3, 2, 2, 1, 1, 2, 3, 3, 2,
                              5, 4, 4, 5, 6, 6, 5, 5, 4, 4, 5, 6, 6, 5}}}
        .Test();
  }
}

template <typename T>
struct UnaryOperatorTester {
  mojom::Operation::Tag tag;
  OperandInfo<T> input;
  absl::optional<float> clamp_min_value;
  absl::optional<float> clamp_max_value;
  absl::optional<float> elu_alpha;
  absl::optional<float> leaky_relu_alpha;
  OperandInfo<float> output;
  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    switch (tag) {
      case mojom::Operation::Tag::kClamp:
        CHECK(clamp_min_value);
        CHECK(clamp_max_value);
        builder.BuildClamp(input_operand_id, output_operand_id,
                           clamp_min_value.value(), clamp_max_value.value());
        break;
      case mojom::Operation::Tag::kElu:
        CHECK(elu_alpha);
        builder.BuildElu(input_operand_id, output_operand_id,
                         elu_alpha.value());
        break;
      case mojom::Operation::Tag::kLeakyRelu:
        CHECK(leaky_relu_alpha);
        builder.BuildLeakyRelu(input_operand_id, output_operand_id,
                               leaky_relu_alpha.value());
        break;
      case mojom::Operation::Tag::kRelu:
        builder.BuildRelu(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kSigmoid:
        builder.BuildSigmoid(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kSoftmax:
        builder.BuildSoftmax(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kTanh:
        builder.BuildTanh(input_operand_id, output_operand_id);
        break;
      default:
        NOTREACHED();
    }

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }
};

// Test building and computing a DML graph with single operator clamp.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorClamp) {
  {
    // Test clamp for 4-D tensor input.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kClamp,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 4},
                  .values = {-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22,  23,  24}},
        .clamp_min_value = 0,
        .clamp_max_value = 3,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 4},
                   .values = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}}}
        .Test();
  }
  {
    // Test clamp for 0-D scalar input.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kClamp,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {},
                  .values = {24}},
        .clamp_min_value = 0,
        .clamp_max_value = 3,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {},
                   .values = {3}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator sigmoid.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorSigmoid) {
  // Test sigmoid with a 0-D scalar input.
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSigmoid,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {},
                  .values = {0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {},
                   .values = {0.5}}}
        .Test();
  }
  // Test sigmoid with a 1d input.
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSigmoid,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {3},
                  .values = {-1, 0, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3},
                   .values = {0.26894143, 0.5, 0.7310586}}}
        .Test();
  }
  // Test sigmoid with a 3d input.
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSigmoid,
        .input =
            {.type = mojom::Operand::DataType::kFloat32,
             .dimensions = {3, 4, 5},
             .values = {-0.18371736, 0.4805392,   2.7183356,   0.03039639,
                        0.04197176,  -1.1536852,  -2.0124357,  -0.885673,
                        -0.25776535, 1.0151213,   -0.22013742, 0.13626824,
                        0.8574488,   -0.15987602, 0.7025059,   -0.8209337,
                        1.2621661,   0.4055987,   -0.65470445, 0.14290208,
                        1.6874043,   -0.7997532,  -1.0582826,  1.0813274,
                        -1.9656292,  -0.13285251, 0.87344545,  -0.07760263,
                        1.0503976,   -0.23713546, 0.21536243,  0.59599924,
                        -0.8221842,  0.10256762,  -0.67856175, 1.1891315,
                        -0.6567207,  -0.2958169,  -1.9581499,  -0.9223802,
                        -0.32011083, -0.31802705, 0.7264381,   1.0234208,
                        0.673269,    0.96394795,  0.6152301,   -0.4362364,
                        -1.2325221,  -0.11140272, -0.43866253, 0.5770897,
                        0.42372307,  -0.33066413, -0.46210232, -0.6456375,
                        2.0984166,   -1.2020895,  1.5637838,   -0.7114222}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3, 4, 5},
                   .values = {0.4541994,  0.61787516, 0.9381,     0.50759846,
                              0.5104914,  0.23981662, 0.11790343, 0.29200357,
                              0.43591312, 0.7340212,  0.44518682, 0.53401446,
                              0.7021274,  0.4601159,  0.66874313, 0.3055655,
                              0.77939874, 0.6000321,  0.34193018, 0.53566486,
                              0.8438825,  0.31007832, 0.2576378,  0.7467451,
                              0.12285913, 0.46683565, 0.70546216, 0.48060906,
                              0.7408512,  0.44099236, 0.55363345, 0.64474046,
                              0.3053002,  0.52561945, 0.33658236, 0.7665857,
                              0.34147665, 0.4265804,  0.12366741, 0.28447315,
                              0.42064875, 0.42115664, 0.67402315, 0.7356384,
                              0.6622347,  0.7239115,  0.64913297, 0.39263815,
                              0.2257403,  0.47217807, 0.39205968, 0.6403975,
                              0.6043738,  0.41807905, 0.38648725, 0.34397328,
                              0.89074916, 0.2311037,  0.8268956,  0.32928467}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator tanh.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorTanh) {
  // Test tanh with a 0-D scalar input.
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kTanh,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {},
                  .values = {-1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {},
                   .values = {-0.76159418}}}
        .Test();
  }
  // Test tanh with a 1d input.
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kTanh,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {3},
                  .values = {-1, 0, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3},
                   .values = {-0.76159418, 0., 0.76159418}}}
        .Test();
  }
  // Test tanh with a 3d input.
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kTanh,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3},
                  .values = {-2, -1, 0, 1, 2, 3}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3},
                   .values = {-0.9640275800758168, -0.7615941559557649, 0,
                              0.7615941559557649, 0.9640275800758169,
                              0.9950547536867305}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator softmax.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorSoftmax) {
  // DML_ACTIVATION_SOFTMAX_OPERATOR_DESC support for 2 dimensions was
  // introduced in DML_FEATURE_LEVEL_3_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_3_0));
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSoftmax,
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
        .tag = mojom::Operation::Tag::kRelu,
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
        .tag = mojom::Operation::Tag::kRelu,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 1},
                  .values = {-1, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 1},
                   .values = {0, 2}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator elu.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorElu) {
  {
    // Test elu with a 3d input and alpha = 1.0.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kElu,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2, 3},
                  .values = {0.4301911, 0.54719144, -1.1637765, 0.18390046,
                             0.58390397, 0.1735679, 0.539724, -0.953514,
                             -0.59202826, -0.17344485, 0.14395015,
                             -0.37920907}},
        .elu_alpha = 1.0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 3},
                   .values = {0.4301911, 0.54719144, -0.6876954670284463,
                              0.18390046, 0.58390397, 0.1735679, 0.539724,
                              -0.6146155995193658, -0.44679589568801814,
                              -0.15923648200867868, 0.14395015,
                              -0.3155974903251695}}}
        .Test();
  }
  {
    // Test elu with a 3d input and alpha = 0.8.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kElu,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2, 3},
                  .values = {0.4301911, 0.54719144, -1.1637765, 0.18390046,
                             0.58390397, 0.1735679, 0.539724, -0.953514,
                             -0.59202826, -0.17344485, 0.14395015,
                             -0.37920907}},
        .elu_alpha = 0.8,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 3},
                   .values = {0.4301911, 0.54719144, -0.550156373622757,
                              0.18390046, 0.58390397, 0.1735679, 0.539724,
                              -0.4916924796154927, -0.35743671655041453,
                              -0.12738918560694296, 0.14395015,
                              -0.2524779922601356}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator leakyRelu.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorLeakyRelu) {
  {
    // Test leakyRelu with a 3d input and alpha = 0.01.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kLeakyRelu,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2, 3},
                  .values = {0.4301911, 0.54719144, -1.1637765, 0.18390046,
                             0.58390397, 0.1735679, 0.539724, -0.953514,
                             -0.59202826, -0.17344485, 0.14395015,
                             -0.37920907}},
        .leaky_relu_alpha = 0.01,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 3},
                   .values = {0.4301911, 0.54719144, -0.011637765, 0.18390046,
                              0.58390397, 0.1735679, 0.539724, -0.00953514,
                              -0.0059202826, -0.0017344485000000001, 0.14395015,
                              -0.0037920907}}}
        .Test();
  }
  {
    // Test leakyRelu with a 3d input and alpha = 0.05.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kLeakyRelu,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2, 3},
                  .values = {0.4301911, 0.54719144, -1.1637765, 0.18390046,
                             0.58390397, 0.1735679, 0.539724, -0.953514,
                             -0.59202826, -0.17344485, 0.14395015,
                             -0.37920907}},
        .leaky_relu_alpha = 0.05,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 3},
                   .values = {0.4301911, 0.54719144, -0.058188825, 0.18390046,
                              0.58390397, 0.1735679, 0.539724, -0.0476757,
                              -0.029601413, -0.008672242500000002, 0.14395015,
                              -0.018960453500000002}}}
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
  builder.BuildRelu(input_operand_id, relu1_output_id);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(relu1_output_id, output_operand_id);

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
  builder.BuildReshape(input_operand_id, output_operand_id);

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
  builder.BuildRelu(input_operand_id, relu_output_id);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildReshape(relu_output_id, output_operand_id);

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
  builder.BuildReshape(input_operand_id, reshape_output_id);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 6, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(reshape_output_id, output_operand_id);

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
  builder.BuildReshape(input_operand_id, reshape_output_id);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildReshape(reshape_output_id, output_operand_id);

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
  builder.BuildReshape(input_operand_id, output1_operand_id);
  uint64_t output2_operand_id = builder.BuildOutput(
      "output2", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(input_operand_id, output2_operand_id);

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
struct ReduceTester {
  OperandInfo<T> input;
  std::vector<uint32_t> axes;
  bool keep_dimensions;
  mojom::Reduce::Kind kind;
  OperandInfo<float> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildReduce(kind, input_operand_id, output_operand_id, axes,
                        keep_dimensions);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }
};

// Test building and computing a DML graph with single operator reduce.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorReduce) {
  // Test reduceL1 with axes = {1} and keep_dimensions = true.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = true,
                        .kind = mojom::Reduce::Kind::kL1,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 1},
                                   .values = {6, 15}}}
        .Test();
  }
  // Test reduceL2 with axes = {1} and keep_dimensions = true.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = true,
                        .kind = mojom::Reduce::Kind::kL2,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 1},
                                   .values = {3.74165738, 8.77496438}}}
        .Test();
  }
  // Test reduceLogSum with axes = {1} and keep_dimensions = true.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = true,
                        .kind = mojom::Reduce::Kind::kLogSum,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 1},
                                   .values = {1.79175946, 2.70805020}}}
        .Test();
  }
  // Test reduceLosSumExp with axes = {1} and keep_dimensions = true.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = true,
                        .kind = mojom::Reduce::Kind::kLogSumExp,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 1},
                                   .values = {3.40760596, 6.40760596}}}
        .Test();
  }
  // Test reduceMax with axes = {1} and keep_dimensions = true.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = true,
                        .kind = mojom::Reduce::Kind::kMax,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 1},
                                   .values = {3, 6}}}
        .Test();
  }
  // Test reduceMean with axes = {1} and keep_dimensions = true.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = true,
                        .kind = mojom::Reduce::Kind::kMean,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 1},
                                   .values = {2, 5}}}
        .Test();
  }
  // Test reduceMin with axes = {1} and keep_dimensions = false.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = false,
                        .kind = mojom::Reduce::Kind::kMin,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {1, 4}}}
        .Test();
  }
  // Test reduceProduct with axes = {1} and keep_dimensions = false.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = false,
                        .kind = mojom::Reduce::Kind::kProduct,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {6, 120}}}
        .Test();
  }
  // Test reduceSum with axes = {1} and keep_dimensions = false.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = false,
                        .kind = mojom::Reduce::Kind::kSum,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {6, 15}}}
        .Test();
  }
  // Test reduceSumSquare with axes = {1} and keep_dimensions = false.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {1},
                        .keep_dimensions = false,
                        .kind = mojom::Reduce::Kind::kSumSquare,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {14, 77}}}
        .Test();
  }
  // Test reduceSum with all axes and keep_dimensions = true.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {0, 1},
                        .keep_dimensions = true,
                        .kind = mojom::Reduce::Kind::kSum,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 1},
                                   .values = {21}}}
        .Test();
  }
  // Test reduceSum with all axes and keep_dimensions = false.
  {
    ReduceTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 3},
                                  .values = {1, 2, 3, 4, 5, 6}},
                        .axes = {0, 1},
                        .keep_dimensions = false,
                        .kind = mojom::Reduce::Kind::kSum,
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {},
                                   .values = {21}}}
        .Test();
  }
}

template <typename InputOutputType, typename IndicesType>
struct GatherTester {
  OperandInfo<InputOutputType> input;
  OperandInfo<IndicesType> indices;
  uint32_t axis;
  OperandInfo<InputOutputType> output;
  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", indices.dimensions, indices.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildGather(input_operand_id, indices_operand_id, output_operand_id,
                        axis);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    named_inputs.insert({"indices", VectorToBigBuffer(indices.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    EXPECT_EQ(
        BigBufferToVector<InputOutputType>(std::move(named_outputs["output"])),
        output.values);
  }
};

TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorGather) {
  {
    // Test gather with 1-D input, 1-D indices and axis = 0 with data type
    // uint32.
    GatherTester<float, uint32_t>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {4},
                  .values = {1, 2, 3, 4}},
        .indices = {.type = mojom::Operand::DataType::kUint32,
                    .dimensions = {5},
                    .values = {2, 1, 3, 0, 1}},
        .axis = 0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {5},
                   .values = {3, 2, 4, 1, 2}}}
        .Test();
  }
  {
    // Test gather with 2-D input, 2-D indices and axis = 1 with data type
    // uint64.
    GatherTester<int32_t, uint64_t>{
        .input = {.type = mojom::Operand::DataType::kInt32,
                  .dimensions = {3, 3},
                  // [[1 2 3]
                  //  [4 5 6]
                  //  [7 8 9]] with shape (3, 3)
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .indices = {.type = mojom::Operand::DataType::kUint64,
                    .dimensions = {1, 2},
                    .values = {0, 2}},
        .axis = 1,
        .output = {.type = mojom::Operand::DataType::kInt32,
                   .dimensions = {3, 1, 2},
                   // [[[1 3]]
                   //  [[4 6]]
                   //  [[7 9]]] with shape (3, 1, 2)
                   .values = {1, 3, 4, 6, 7, 9}}}
        .Test();
  }
  {
    // Test gather with 4-D input, 1-D indices with negative index and axis = 1
    // with data type int64.
    GatherTester<uint32_t, int64_t>{
        .input = {.type = mojom::Operand::DataType::kUint32,
                  .dimensions = {2, 2, 2, 2},
                  // [[[[ 1  2]
                  //    [ 3  4]]
                  //   [[ 5  6]
                  //    [ 7  8]]]
                  //  [[[ 9 10]
                  //    [11 12]]
                  //   [[13 14]
                  //    [15 16]]]] with shape (2, 2, 2, 2)
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                             16}},
        .indices = {.type = mojom::Operand::DataType::kInt64,
                    .dimensions = {1},
                    .values = {-1}},
        .axis = 1,
        .output = {.type = mojom::Operand::DataType::kUint32,
                   .dimensions = {2, 1, 2, 2},
                   // [[[[ 5  6]
                   //    [ 7  8]]]
                   //  [[[13 14]
                   //    [15 16]]]] with shape (2, 1, 2, 2)
                   .values = {5, 6, 7, 8, 13, 14, 15, 16}}}
        .Test();
  }
  {
    // Test gather with 6-D input, 0-D indices and axis = 5 with data type
    // int32.
    GatherTester<float, int32_t>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 1, 1, 1, 5},
                  // [[[[[[1, 2, 3, 4, 5]]]]]] with shape (1, 1, 1, 1, 1, 5)
                  .values = {1, 2, 3, 4, 5}},
        .indices = {.type = mojom::Operand::DataType::kInt32,
                    .dimensions = {},
                    .values = {3}},
        .axis = 5,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 1, 1, 1},
                   // [[[[[4]]]]] with shape (1, 1, 1, 1, 1)
                   .values = {4}}}
        .Test();
  }
  {
    // Test gather with 1-D input, 0-D indices and axis = 0 with data type
    // uint32.
    GatherTester<int32_t, uint32_t>{
        .input = {.type = mojom::Operand::DataType::kInt32,
                  .dimensions = {3},
                  .values = {1, 2, 3}},
        .indices = {.type = mojom::Operand::DataType::kUint32,
                    .dimensions = {},
                    .values = {2}},
        .axis = 0,
        .output = {.type = mojom::Operand::DataType::kInt32,
                   .dimensions = {},
                   .values = {3}}}
        .Test();
  }
  {
    // Test gather with 2-D input, 2-D out-of-bound indices and axis = 1 with
    // data type uint32.
    GatherTester<float, uint32_t>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {3, 3},
                  // [[10 20 30]
                  //  [40 50 60]
                  //  [70 80 90]] with shape (3, 3)
                  .values = {10, 20, 30, 40, 50, 60, 70, 80, 90}},
        .indices = {.type = mojom::Operand::DataType::kUint32,
                    .dimensions = {1, 2},
                    .values = {0, 4}},
        .axis = 1,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3, 1, 2},
                   // [[[10 50]]
                   //  [[40 80]]
                   //  [[70 90]]] with shape (3, 1, 2)
                   .values = {10, 50, 40, 80, 70, 90}}}
        .Test();
  }
  {
    // Test gather with 1-D input, 2-D out-of-bound indices and axis = 0 with
    // data type int32.
    GatherTester<float, int32_t>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {4},
                  .values = {0, 1, 2, 3}},
        .indices = {.type = mojom::Operand::DataType::kInt32,
                    .dimensions = {2, 5},
                    .values = {0, 1, 2, 3, 4, -1, -2, -3, -4, -5}},
        .axis = 0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 5},
                   // [[0 1 2 3 3]
                   //  [3 2 1 0 3]] with shape (2, 5)
                   .values = {0, 1, 2, 3, 3, 3, 2, 1, 0, 3}}}
        .Test();
  }
}

struct GemmAttributes {
  absl::optional<uint64_t> c_operand_id;
  // TODO(crbug.com/1273291): Add test cases for below attributes.
  float alpha = 1.0;
  float beta = 1.0;
  bool a_transpose = false;
  bool b_transpose = false;
};

template <typename T>
struct GemmTester {
  OperandInfo<T> input_a;
  OperandInfo<T> input_b;
  absl::optional<OperandInfo<T>> input_c;
  GemmAttributes attributes;
  OperandInfo<float> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_a_operand_id =
        builder.BuildInput("input_a", input_a.dimensions, input_a.type);
    uint64_t input_b_operand_id =
        builder.BuildInput("input_b", input_b.dimensions, input_b.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    if (input_c.has_value()) {
      attributes.c_operand_id =
          builder.BuildInput("input_c", input_c->dimensions, input_c->type);
    }

    builder.BuildGemm(input_a_operand_id, input_b_operand_id, output_operand_id,
                      std::move(attributes));

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input_a", VectorToBigBuffer(input_a.values)});
    named_inputs.insert({"input_b", VectorToBigBuffer(input_b.values)});
    if (input_c.has_value()) {
      named_inputs.insert({"input_c", VectorToBigBuffer(input_c->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
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
        .input_c =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 2},
                               .values = {1, 1, 1, 1}},
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
        .input_c =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1, 2},
                               .values = {1, 2}},
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
        .input_c =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 1},
                               .values = {1, 2}},
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
        .input_c =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {},
                               .values = {1}},
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
  builder.BuildGemm(input_a_operand_id, input_b_operand_id,
                    intermediate_1_operand_id, GemmAttributes());
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildGemm(input_a_operand_id, input_b_operand_id,
                    intermediate_2_operand_id, GemmAttributes());
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildGemm(intermediate_1_operand_id, intermediate_2_operand_id,
                    output_operand_id, GemmAttributes());

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
  builder.BuildGemm(input_a_operand_id, input_b_operand_id, output_operand_id,
                    GemmAttributes());

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_a_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_a_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({12, 14, 12, 14}));
}

template <typename T>
struct MatmulTester {
  OperandInfo<T> input_a;
  OperandInfo<T> input_b;
  OperandInfo<T> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t a_operand_id =
        builder.BuildInput("input_a", input_a.dimensions, input_a.type);
    uint64_t b_operand_id =
        builder.BuildInput("input_b", input_b.dimensions, input_b.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildMatmul(a_operand_id, b_operand_id, output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input_a", VectorToBigBuffer(input_a.values)});
    named_inputs.insert({"input_b", VectorToBigBuffer(input_b.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }
};

// Test building and computing a DML graph with single operator matmul.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorMatmul) {
  // DML_GEMM_OPERATOR_DESC support for 2~4 dimensions was introduced in
  // DML_FEATURE_LEVEL_4_0.
  SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(DML_FEATURE_LEVEL_4_0));

  // Test matmul with 2-D * 2-D inputs.
  {
    MatmulTester<float>{.input_a = {.type = mojom::Operand::DataType::kFloat32,
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
  // Test matmul with 3-D * 3-D inputs using broadcasting.
  {
    MatmulTester<float>{.input_a = {.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {1, 2, 3},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2, 3, 1},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 2, 1},
                                   .values = {14, 32, 32, 77}}}
        .Test();
  }
  // Test matmul with 2-D * 3-D inputs using broadcasting.
  {
    MatmulTester<float>{.input_a = {.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2, 3},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2, 3, 1},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 2, 1},
                                   .values = {14, 32, 32, 77}}}
        .Test();
  }
  // Test matmul with 3-D * 4-D inputs using broadcasting.
  {
    MatmulTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2, 3},
                    .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 1, 3, 1},
                    .values = {1, 2, 3, 4, 5, 6}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 2, 1},
                   .values = {14, 32, 50, 68, 32, 77, 122, 167}}}
        .Test();
  }
  // Test matmul with 4-D * 4-D inputs.
  {
    MatmulTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 1, 2, 3},
                    .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 1, 3, 1},
                    .values = {1, 2, 3, 4, 5, 6}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 1, 2, 1},
                   .values = {14, 32, 122, 167}}}
        .Test();
  }
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
  builder.BuildGemm(input_a_operand_id, constant_a_operand_id,
                    intermediate_1_operand_id, GemmAttributes());
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildGemm(input_b_operand_id, constant_b_operand_id,
                    intermediate_2_operand_id, GemmAttributes());
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildGemm(intermediate_1_operand_id, intermediate_2_operand_id,
                    output_operand_id, GemmAttributes());

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
  builder.BuildGemm(constant_a_operand_id, input_a_operand_id,
                    intermediate_1_operand_id, GemmAttributes());
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildGemm(constant_b_operand_id, input_b_operand_id,
                    intermediate_2_operand_id, GemmAttributes());
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildGemm(intermediate_1_operand_id, intermediate_2_operand_id,
                    output_operand_id, GemmAttributes());

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
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 input_a_operand_id, input_b_operand_id,
                                 intermediate_1_operand_id);

  // Relu.
  uint64_t intermediate_2_operand_id = builder.BuildIntermediateOperand(
      {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(intermediate_1_operand_id, intermediate_2_operand_id);

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
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 input_a_operand_id, input_b_operand_id,
                                 intermediate_1_operand_id);

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
  builder.BuildRelu(intermediate_2_operand_id, output_operand_id);

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
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 intermediate_1_operand_id, input_b_operand_id,
                                 intermediate_2_operand_id);

  // Relu.
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(intermediate_2_operand_id, output_operand_id);

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

// Test building and computing a DML graph with single operator concat.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorConcat) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id1 = builder.BuildInput(
      "input_a", {1, 1, 2, 3}, mojom::Operand::DataType::kFloat32);
  uint64_t input_operand_id2 = builder.BuildInput(
      "input_b", {1, 1, 2, 3}, mojom::Operand::DataType::kFloat32);
  uint64_t input_operand_id3 = builder.BuildInput(
      "input_c", {1, 2, 2, 3}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 4, 2, 3}, mojom::Operand::DataType::kFloat32);
  builder.BuildConcat({input_operand_id1, input_operand_id2, input_operand_id3},
                      output_operand_id, 1);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  // [[[[-1 -2 -3]
  //    [-4 -5 -6]]]] with shape (1, 1, 2, 3)
  std::vector<float> input_data1 = {-1, -2, -3, -4, -5, -6};
  // [[[[0 0 0]
  //    [0 0 0]]]] with shape (1, 1, 2, 3)
  std::vector<float> input_data2 = {0, 0, 0, 0, 0, 0};
  // [[[[ 1  2  3]
  //    [ 4  5  6]]
  //   [[ 7  8  9]
  //    [10 11 12]]]] with shape (1, 2, 2, 3)
  std::vector<float> input_data3 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  named_inputs.insert({"input_a", VectorToBigBuffer(input_data1)});
  named_inputs.insert({"input_b", VectorToBigBuffer(input_data2)});
  named_inputs.insert({"input_c", VectorToBigBuffer(input_data3)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  // [[[[-1 -2 -3]
  //    [-4 -5 -6]]
  //   [[ 0  0  0]
  //    [ 0  0  0]]
  //   [[ 1  2  3]
  //    [ 4  5  6]]
  //   [[ 7  8  9]
  //    [10 11 12]]]] with shape (1, 4, 2, 3)
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({-1, -2, -3, -4, -5, -6, 0, 0, 0, 0,  0,  0,
                                1,  2,  3,  4,  5,  6,  7, 8, 9, 10, 11, 12}));
}

// Test building and computing a DML graph with float 16 data type in the
// following topology.
//     [input_a]
//         |
//      reshape    [input_b]
//          \         /
//             concat
//               |
//             clamp
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeReshapeConcatAndClamp) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id1 =
      builder.BuildInput("input_a", {4, 3}, mojom::Operand::DataType::kFloat16);
  uint64_t input_operand_id2 = builder.BuildInput(
      "input_b", {1, 1, 2, 3}, mojom::Operand::DataType::kFloat16);

  uint64_t reshape_operand_id = builder.BuildIntermediateOperand(
      {1, 2, 2, 3}, mojom::Operand::DataType::kFloat16);
  builder.BuildReshape(input_operand_id1, reshape_operand_id);

  uint64_t concat_operand_id = builder.BuildIntermediateOperand(
      {1, 3, 2, 3}, mojom::Operand::DataType::kFloat16);
  builder.BuildConcat({reshape_operand_id, input_operand_id2},
                      concat_operand_id, 1);

  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 3, 2, 3}, mojom::Operand::DataType::kFloat16);
  builder.BuildClamp(concat_operand_id, output_operand_id, 1.25, 8.75);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  // [[ 1  2  3]
  //  [ 4  5  6]
  //  [ 7  8  9]
  //  [10 11 12]] with shape (4, 3)
  std::vector<float16> input_data1 =
      Float16FromFloat32({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  // [[[[-6 -5 -4]
  //    [-3 -2 -1]]]] with shape (1, 1, 2, 3)
  std::vector<float16> input_data2 =
      Float16FromFloat32({-6, -5, -4, -3, -2, -1});

  named_inputs.insert({"input_a", VectorToBigBuffer(input_data1)});
  named_inputs.insert({"input_b", VectorToBigBuffer(input_data2)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  // [[[[1.25 2.   3.  ]
  //    [4.   5.   6.  ]]
  //   [[7.   8.   8.75]
  //    [8.75 8.75 8.75]]
  //   [[1.25 1.25 1.25]
  //    [1.25 1.25 1.25]]]] with shape (1, 3, 2, 3)
  EXPECT_EQ(GetFloatOutputData(std::move(named_outputs["output"]),
                               mojom::Operand::DataType::kFloat16),
            std::vector<float>({1.25, 2, 3, 4, 5, 6, 7, 8, 8.75, 8.75, 8.75,
                                8.75, 1.25, 1.25, 1.25, 1.25, 1.25, 1.25}));
}

// Test building and computing a DML graph in the following topology.
//      [input]   [constant_a]
//          \          /
//             concat   [constant_b]
//               \           /
//                   concat
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeConcatWithConstants) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 1, 1, 3}, mojom::Operand::DataType::kFloat32);

  // [[[[1 2 3]]]] with shape (1, 1, 1, 3)
  std::vector<float> constant_data_a = {1, 2, 3};
  uint64_t constant_a_operand_id =
      builder.BuildConstant({1, 1, 1, 3}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data_a)));

  // [[[[-1 -2 -3]
  //    [-4 -5 -6]]]] with shape (1, 1, 2, 3)
  std::vector<float> constant_data_b = {-1, -2, -3, -4, -5, -6};
  uint64_t constant_b_operand_id =
      builder.BuildConstant({1, 1, 2, 3}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data_b)));

  uint64_t concat_operand_id = builder.BuildIntermediateOperand(
      {1, 1, 2, 3}, mojom::Operand::DataType::kFloat32);
  builder.BuildConcat({input_operand_id, constant_a_operand_id},
                      concat_operand_id, 2);

  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 2, 3}, mojom::Operand::DataType::kFloat32);
  builder.BuildConcat({concat_operand_id, constant_b_operand_id},
                      output_operand_id, 1);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  // [[[[0 0 0]]]] with shape (1, 1, 1, 3)
  std::vector<float> input_data = {0, 0, 0};

  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  // [[[[ 0  0  0]
  //    [ 1  2  3]]
  //   [[-1 -2 -3]
  //    [-4 -5 -6]]]] with shape (1, 2, 2, 3)
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({0, 0, 0, 1, 2, 3, -1, -2, -3, -4, -5, -6}));
}

template <typename T>
struct Resample2dTester {
  OperandInfo<T> input;
  struct Resample2dAttributes {
    mojom::Resample2d::InterpolationMode mode =
        mojom::Resample2d::InterpolationMode::kNearestNeighbor;
    absl::optional<std::vector<float>> scales;
    std::vector<uint32_t> axes = {2, 3};
  };
  Resample2dAttributes attributes;
  OperandInfo<float> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildResample2d(input_operand_id, output_operand_id, attributes);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }
};

// Test building and computing a DML graph with single operator resample2d.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorResample2d) {
  // Test resample2d with "NearestNeighbor" mode and axes = [2, 3].
  {
    Resample2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 2, 2},
                  // [[[[1 2]
                  //    [3 4]]]] with shape (1, 1, 2, 2)
                  .values = {1, 2, 3, 4}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 4, 6},
                   // [[[[1 1 1 2 2 2]
                   //    [1 1 1 2 2 2]
                   //    [3 3 3 4 4 4]
                   //    [3 3 3 4 4 4]]]] with shape (1, 1, 4, 6)
                   .values = {1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2,
                              3, 3, 3, 4, 4, 4, 3, 3, 3, 4, 4, 4}}}
        .Test();
  }
  // Test resample2d with "NearestNeighbor" mode, explicit scales = [2, 3] and
  // axes = [2, 3].
  {
    Resample2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 2, 2},
                  // [[[[1 2]
                  //    [3 4]]]] with shape (1, 1, 2, 2)
                  .values = {1, 2, 3, 4}},
        .attributes = {.scales = std::vector<float>{2, 3}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 4, 6},
                   // [[[[1 1 1 2 2 2]
                   //    [1 1 1 2 2 2]
                   //    [3 3 3 4 4 4]
                   //    [3 3 3 4 4 4]]]] with shape (1, 1, 4, 6)
                   .values = {1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2,
                              3, 3, 3, 4, 4, 4, 3, 3, 3, 4, 4, 4}}}
        .Test();
  }
  // Test resample2d with "NearestNeighbor" mode and axes = [1, 2].
  {
    Resample2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  // [[[[1] [2]]
                  //   [[3] [4]]]] with shape (1, 2, 2, 1)
                  .values = {1, 2, 3, 4}},
        .attributes = {.axes = {1, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 4, 6, 1},
                   // [[[[1] [1] [1] [2] [2] [2]]
                   //   [[1] [1] [1] [2] [2] [2]]
                   //   [[3] [3] [3] [4] [4] [4]]
                   //   [[3] [3] [3] [4] [4] [4]]]] with shape (1, 4, 6, 1)
                   .values = {1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2,
                              3, 3, 3, 4, 4, 4, 3, 3, 3, 4, 4, 4}}}
        .Test();
  }
  // Test resample2d with "Linear" mode and axes = [2, 3].
  {
    Resample2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 2, 2},
                  // [[[[1 2]
                  //    [3 4]]]] with shape (1, 1, 2, 2)
                  .values = {1, 2, 3, 4}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 4, 4},
                   // [[[[1   1.25 1.75 2  ]
                   //    [1.5 1.75 2.25 2.5]
                   //    [2.5 2.75 3.25 3.5]
                   //    [3   3.25 3.75 4]]]] with shape (1, 1, 4, 4)
                   .values = {1, 1.25, 1.75, 2, 1.5, 1.75, 2.25, 2.5, 2.5, 2.75,
                              3.25, 3.5, 3, 3.25, 3.75, 4}}}
        .Test();
  }
  // Test resample2d with "NearestNeighbor" mode, axes = [2, 3] and output sizes
  // larger but not divisible to input sizes.
  {
    Resample2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 2, 3},
                  // [[[[1 2 3]
                  //    [4 5 6]]]] with shape (1, 1, 2, 3)
                  .values = {1, 2, 3, 4, 5, 6}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 4, 5},
                   // [[[[1 1 2 3 3]
                   //    [1 1 2 3 3]
                   //    [4 4 5 6 6]
                   //    [4 4 5 6 6]]]] with shape (1, 1, 4, 5)
                   .values = {1, 1, 2, 3, 3, 1, 1, 2, 3, 3,
                              4, 4, 5, 6, 6, 4, 4, 5, 6, 6}}}
        .Test();
  }
  // Test resample2d with "NearestNeighbor" mode , axes = [2, 3] and output
  // sizes smaller than input sizes.
  {
    Resample2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 3, 3},
                  // [[[[1 2 3]
                  //    [4 5 6]
                  //    [7 8 9]]]] with shape (1, 1, 3, 3)
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   // [[[[1 3]
                   //    [7 9]]]] with shape (1, 1, 2, 2)
                   .values = {1, 3, 7, 9}}}
        .Test();
  }
  // Test resample2d with "Linear" mode , axes = [2, 3] and output sizes smaller
  // than input sizes.
  {
    Resample2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 3, 3},
                  // [[[[1 2 3]
                  //    [4 5 6]
                  //    [7 8 9]]]] with shape (1, 1, 3, 3)
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   // [[[[2   3.5]
                   //    [6.5 8  ]]]] with shape (1, 1, 2, 2)
                   .values = {2, 3.5, 6.5, 8}}}
        .Test();
  }
}

// Test building and computing a DML graph with single operator transpose.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorTranspose) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {2, 3}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {3, 2}, mojom::Operand::DataType::kFloat32);

  builder.BuildTranspose(input_operand_id, output_operand_id, {1, 0});

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {-1, -2, -3, -4, -5, -6};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({-1, -4, -2, -5, -3, -6}));
}

// Test building and computing a DML graph in the following topology.
//      [input]
//         |
//     transpose
//         |
//     transpose
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithTwoTranspose) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);

  uint64_t transpose_operand_id = builder.BuildIntermediateOperand(
      {2, 1, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildTranspose(input_operand_id, transpose_operand_id, {1, 0, 2, 3});

  uint64_t output_operand_id = builder.BuildOutput(
      "output", {4, 3, 1, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildTranspose(transpose_operand_id, output_operand_id, {3, 2, 1, 0});

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  // [[[[ -1  -2  -3  -4]
  //    [ -5  -6  -7  -8]
  //    [ -9 -10 -11 -12]]
  //   [[ 13  14  15  16]
  //    [ 17  18  19  20]
  //    [ 21  22  23  24]]]] with shape (1, 2, 3, 4)
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  // [[[[ -1  13]]
  //   [[ -5  17]]
  //   [[ -9  21]]]
  //  [[[ -2  14]]
  //   [[ -6  18]]
  //   [[-10  22]]]
  //  [[[ -3  15]]
  //   [[ -7  19]]
  //   [[-11  23]]]
  //  [[[ -4  16]]
  //   [[ -8  20]]
  //   [[-12  24]]]] with shape (4, 3, 1, 2)
  EXPECT_EQ(
      BigBufferToVector<float>(std::move(named_outputs["output"])),
      std::vector<float>({-1, 13, -5, 17, -9,  21, -2, 14, -6, 18, -10, 22,
                          -3, 15, -7, 19, -11, 23, -4, 16, -8, 20, -12, 24}));
}

// Test building and computing a DML graph in the following topology.
//      [input]
//         |
//     transpose
//         |
//       relu
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithTransposeAndRelu) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);

  uint64_t transpose_operand_id = builder.BuildIntermediateOperand(
      {4, 3, 1, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildTranspose(input_operand_id, transpose_operand_id, {3, 2, 0, 1});

  uint64_t output_operand_id = builder.BuildOutput(
      "output", {4, 3, 1, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(transpose_operand_id, output_operand_id);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  // [[[[ -1  -2  -3  -4]
  //    [ -5  -6  -7  -8]
  //    [ -9 -10 -11 -12]]
  //   [[ 13  14  15  16]
  //    [ 17  18  19  20]
  //    [ 21  22  23  24]]]] with shape (1, 2, 3, 4)
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  // [[[[ 0  13]]
  //   [[ 0  17]]
  //   [[ 0  21]]]
  //  [[[ 0  14]]
  //   [[ 0  18]]
  //   [[ 0  22]]]
  //  [[[ 0  15]]
  //   [[ 0  19]]
  //   [[ 0  23]]]
  //  [[[ 0  16]]
  //   [[ 0  20]]
  //   [[ 0  24]]]] wit shape (4, 3, 1, 2)
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({0, 13, 0, 17, 0, 21, 0, 14, 0, 18, 0, 22,
                                0, 15, 0, 19, 0, 23, 0, 16, 0, 20, 0, 24}));
}

// Test building and computing a DML graph in the following topology.
//      [input]
//         |
//     transpose
//         |
//      reshape
//         |
//      reshape
//         |
//     transpose
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithTransposeAndTwoReshape) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);

  uint64_t transpose_operand_id = builder.BuildIntermediateOperand(
      {4, 3, 1, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildTranspose(input_operand_id, transpose_operand_id, {3, 2, 0, 1});

  uint64_t reshape_operand_id1 = builder.BuildIntermediateOperand(
      {2, 2, 6}, mojom::Operand::DataType::kFloat32);
  builder.BuildReshape(transpose_operand_id, reshape_operand_id1);

  uint64_t reshape_operand_id2 = builder.BuildIntermediateOperand(
      {12, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildReshape(reshape_operand_id1, reshape_operand_id2);

  uint64_t output_operand_id = builder.BuildOutput(
      "output", {2, 12}, mojom::Operand::DataType::kFloat32);
  builder.BuildTranspose(reshape_operand_id2, output_operand_id, {1, 0});

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  // [[[[ -1  -2  -3  -4]
  //    [ -5  -6  -7  -8]
  //    [ -9 -10 -11 -12]]
  //   [[ 13  14  15  16]
  //    [ 17  18  19  20]
  //    [ 21  22  23  24]]]] with shape (1, 2, 3, 4)
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  // [[ -1  -5  -9  -2  -6 -10  -3  -7 -11  -4  -8 -12]
  //  [ 13  17  21  14  18  22  15  19  23  16  20  24]] wit shape (2, 12)
  EXPECT_EQ(
      BigBufferToVector<float>(std::move(named_outputs["output"])),
      std::vector<float>({-1, -5, -9, -2, -6, -10, -3, -7, -11, -4, -8, -12,
                          13, 17, 21, 14, 18, 22,  15, 19, 23,  16, 20, 24}));
}

// Test building and computing a DML graph in the following topology.
//         [input]
//            |
//           relu
//          /    \
//     reshape    transpose
//        |           |
//    [output1]   [output2]
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeGraphWithTransposeAndTwoOutputs) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t relu_operand_id = builder.BuildIntermediateOperand(
      {1, 2, 3, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(input_operand_id, relu_operand_id);

  uint64_t output1_operand_id = builder.BuildOutput(
      "output1", {3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output2_operand_id = builder.BuildOutput(
      "output2", {1, 2, 2, 3}, mojom::Operand::DataType::kFloat32);
  builder.BuildReshape(relu_operand_id, output1_operand_id);
  builder.BuildTranspose(relu_operand_id, output2_operand_id, {0, 3, 1, 2});

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  // [[[[ -1  -2]
  //    [ -5 -10]
  //    [ -7   0]]
  //   [[  1   2]
  //    [  3   6]
  //    [ 10  20]]]] with shape (1, 2, 3, 2)
  std::vector<float> input_data = {-1, -2, -5, -10, -7, 0, 1, 2, 3, 6, 10, 20};
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  // [[ 0  0  0  0]
  //  [ 0  0  1  2]
  //  [ 3  6 10 20]] with shape (3, 4)
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output1"])),
            std::vector<float>({0, 0, 0, 0, 0, 0, 1, 2, 3, 6, 10, 20}));
  // [[[[ 0  0  0]
  //    [ 1  3 10]]
  //   [[ 0  0  0]
  //    [ 2  6 20]]]] with shape (1, 2, 2, 3)
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output2"])),
            std::vector<float>({0, 0, 0, 1, 3, 10, 0, 0, 0, 2, 6, 20}));
}

template <typename T>
struct WhereTester {
  OperandInfo<uint8_t> condition;
  OperandInfo<T> true_value;
  OperandInfo<T> false_value;
  OperandInfo<T> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t condition_operand_id =
        builder.BuildInput("condition", condition.dimensions, condition.type);
    uint64_t true_value_operand_id = builder.BuildInput(
        "true_value", true_value.dimensions, true_value.type);
    uint64_t false_value_operand_id = builder.BuildInput(
        "false_value", false_value.dimensions, false_value.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildWhere(condition_operand_id, true_value_operand_id,
                       false_value_operand_id, output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"condition", VectorToBigBuffer(condition.values)});
    named_inputs.insert({"true_value", VectorToBigBuffer(true_value.values)});
    named_inputs.insert({"false_value", VectorToBigBuffer(false_value.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }
};

// Test building and computing a DML graph with single operator where.
TEST_F(WebNNGraphDMLImplTest, BuildAndComputeSingleOperatorWhere) {
  // Test where with 2-D condition, 2-D true_value and 2-D false_value.
  {
    WhereTester<float>{
        .condition = {.type = mojom::Operand::DataType::kUint8,
                      .dimensions = {2, 3},
                      .values = {1, 1, 0, 0, 1, 0}},
        .true_value = {.type = mojom::Operand::DataType::kFloat32,
                       .dimensions = {2, 3},
                       .values = {1, 2, 3, 4, 5, 64}},
        .false_value = {.type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 3},
                        .values = {6, 3, 5, 7, 8, 0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3},
                   .values = {1, 2, 5, 7, 5, 0}}}
        .Test();
  }
  // Test where with 1-D condition, 2-D true_value and 2-D false_value using
  // broadcast.
  {
    WhereTester<float>{
        .condition = {.type = mojom::Operand::DataType::kUint8,
                      .dimensions = {3},
                      .values = {1, 1, 0}},
        .true_value = {.type = mojom::Operand::DataType::kFloat32,
                       .dimensions = {2, 3},
                       .values = {1, 2, 3, 4, 5, 64}},
        .false_value = {.type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 3},
                        .values = {7, 8, 9, 10, 11, 12}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3},
                   .values = {1, 2, 9, 4, 5, 12}}}
        .Test();
  }
  // Test where with 2-D condition, 2-D true_value and 1-D false_value using
  // broadcast.
  {
    WhereTester<float>{
        .condition = {.type = mojom::Operand::DataType::kUint8,
                      .dimensions = {2, 3},
                      .values = {1, 1, 0, 0, 0, 1}},
        .true_value = {.type = mojom::Operand::DataType::kFloat32,
                       .dimensions = {2, 3},
                       .values = {1, 2, 3, 4, 5, 64}},
        .false_value = {.type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {3},
                        .values = {7, 8, 9}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3},
                   .values = {1, 2, 9, 7, 8, 64}}}
        .Test();
  }
  // Test where with 1-D condition, 2-D true_value and 3-D false_value using
  // broadcast.
  {
    WhereTester<float>{
        .condition = {.type = mojom::Operand::DataType::kUint8,
                      .dimensions = {3},
                      .values = {1, 1, 0}},
        .true_value = {.type = mojom::Operand::DataType::kFloat32,
                       .dimensions = {2, 3},
                       .values = {1, 2, 3, 4, 5, 64}},
        .false_value = {.type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 2, 3},
                        .values = {7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                                   18}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 3},
                   .values = {1, 2, 9, 4, 5, 12, 1, 2, 15, 4, 5, 18}}}
        .Test();
  }
  // Test where with 3-D condition, 2-D true_value and 1-D false_value using
  // broadcast.
  {
    WhereTester<float>{
        .condition = {.type = mojom::Operand::DataType::kUint8,
                      .dimensions = {2, 2, 3},
                      .values = {1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0}},
        .true_value = {.type = mojom::Operand::DataType::kFloat32,
                       .dimensions = {2, 3},
                       .values = {1, 2, 3, 4, 5, 64}},
        .false_value = {.type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {3},
                        .values = {7, 8, 9}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 3},
                   .values = {1, 2, 9, 4, 5, 9, 1, 2, 9, 4, 5, 9}}}
        .Test();
  }
  // Test where with 2-D condition, 2-D true_value and 2-D false_value, and
  // condition value !=0 should be true.
  {
    WhereTester<float>{
        .condition = {.type = mojom::Operand::DataType::kUint8,
                      .dimensions = {2, 3},
                      .values = {2, 3, 0, 0, 5, 0}},
        .true_value = {.type = mojom::Operand::DataType::kFloat32,
                       .dimensions = {2, 3},
                       .values = {1, 2, 3, 4, 5, 64}},
        .false_value = {.type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 3},
                        .values = {6, 3, 5, 7, 8, 0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3},
                   .values = {1, 2, 5, 7, 5, 0}}}
        .Test();
  }
  // Test where with 2-D condition, 0-D scalar true_value and 2-D false_value
  // using broadcast.
  {
    WhereTester<float>{
        .condition = {.type = mojom::Operand::DataType::kUint8,
                      .dimensions = {2, 3},
                      .values = {1, 1, 0, 0, 1, 0}},
        .true_value = {.type = mojom::Operand::DataType::kFloat32,
                       .dimensions = {},
                       .values = {6}},
        .false_value = {.type = mojom::Operand::DataType::kFloat32,
                        .dimensions = {2, 3},
                        .values = {6, 3, 5, 7, 8, 0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3},
                   .values = {6, 6, 5, 7, 6, 0}}}
        .Test();
  }
}

}  // namespace webnn::dml
