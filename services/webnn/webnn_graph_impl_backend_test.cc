// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cmath>
#include <type_traits>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_test_utils.h"
#include "services/webnn/webnn_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fp16/src/include/fp16.h"

#if BUILDFLAG(IS_WIN)
#include "base/containers/fixed_flat_map.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/context_impl_dml.h"
#include "services/webnn/dml/graph_impl_dml.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/dml/utils.h"
#include "third_party/microsoft_dxheaders/include/directml.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace webnn::test {

namespace {

// Since there is no float16 data type in C++, use uint16_t to represent the
// binary data.
using float16 = uint16_t;

template <typename T>
std::vector<T> BigBufferToVector(const mojo_base::BigBuffer& big_buffer) {
  std::vector<T> data(big_buffer.size() / sizeof(T));
  memcpy(data.data(), big_buffer.data(), big_buffer.size());
  return data;
}

enum class BuildAndComputeExpectation { kSuccess, kCreateGraphFailure };

template <typename InputDataType, typename OutputDataType = InputDataType>
[[nodiscard]] base::flat_map<std::string, std::vector<OutputDataType>>
BuildAndCompute(
    mojom::GraphInfoPtr graph_info,
    base::flat_map<std::string, base::span<const InputDataType>> named_inputs,
    BuildAndComputeExpectation expectation =
        BuildAndComputeExpectation::kSuccess,
    mojom::CreateContextOptions::Device device =
        mojom::CreateContextOptions::Device::kGpu) {
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  mojo::AssociatedRemote<mojom::WebNNGraph> webnn_graph_remote;
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> webnn_graph_builder_remote;

  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  // Create the ContextImpl through context provider.
  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  webnn_provider_remote->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          device, mojom::CreateContextOptions::PowerPreference::kDefault,
          /*thread_count_hint=*/0),
      create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  if (create_context_result->is_success()) {
    webnn_context_remote.Bind(
        std::move(create_context_result->get_success()->context_remote));
  }
  EXPECT_FALSE(create_context_result->is_error())
      << create_context_result->get_error()->message;
  EXPECT_TRUE(webnn_context_remote.is_bound());

  // Create the GraphBuilder through the context.
  webnn_context_remote->CreateGraphBuilder(
      webnn_graph_builder_remote.BindNewEndpointAndPassReceiver());

  // The GraphImpl should be built successfully.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  webnn_graph_builder_remote->CreateGraph(std::move(graph_info),
                                          create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  if (!create_graph_result->is_error()) {
    webnn_graph_remote.Bind(std::move(create_graph_result->get_graph_remote()));
  }

  if (expectation == BuildAndComputeExpectation::kCreateGraphFailure) {
    EXPECT_TRUE(create_graph_result->is_error());
    EXPECT_FALSE(webnn_graph_remote.is_bound());
    EXPECT_TRUE(webnn_context_remote.is_bound());
    EXPECT_TRUE(webnn_graph_builder_remote.is_bound());
    webnn_graph_remote.reset();
    webnn_graph_builder_remote.reset();
    webnn_context_remote.reset();
    webnn_provider_remote.reset();
    base::RunLoop().RunUntilIdle();
    return {};
  }
  EXPECT_FALSE(create_graph_result->is_error())
      << create_graph_result->get_error()->message;
  EXPECT_TRUE(webnn_graph_remote.is_bound());

  std::vector<std::pair<std::string, mojo_base::BigBuffer>> named_input_buffers;
  named_input_buffers.reserve(named_inputs.size());
  base::ranges::transform(
      named_inputs, std::back_inserter(named_input_buffers),
      [](const auto& name_and_data) {
        return std::make_pair(
            name_and_data.first,
            mojo_base::BigBuffer(base::as_byte_span(name_and_data.second)));
      });

  // The GraphImpl should compute successfully.
  base::test::TestFuture<mojom::ComputeResultPtr> compute_future;
  webnn_graph_remote->Compute(base::flat_map<std::string, mojo_base::BigBuffer>(
                                  std::move(named_input_buffers)),
                              compute_future.GetCallback());
  mojom::ComputeResultPtr compute_result = compute_future.Take();
  EXPECT_TRUE(compute_result->is_named_outputs());
  EXPECT_FALSE(compute_result->get_named_outputs().empty());
  auto named_outputs = std::move(compute_result->get_named_outputs());

  // Read back the results from the output buffers.
  std::vector<std::pair<std::string, std::vector<OutputDataType>>>
      named_output_results;
  named_output_results.reserve(named_outputs.size());
  base::ranges::transform(
      named_outputs, std::back_inserter(named_output_results),
      [](auto& output) {
        return std::make_pair(output.first,
                              BigBufferToVector<OutputDataType>(output.second));
      });

  webnn_graph_remote.reset();
  webnn_graph_builder_remote.reset();
  webnn_context_remote.reset();
  webnn_provider_remote.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expectation, BuildAndComputeExpectation::kSuccess);

  return base::flat_map<std::string, std::vector<OutputDataType>>(
      std::move(named_output_results));
}

void VerifyFloatDataIsEqual(base::span<const float> data,
                            base::span<const float> expected_data) {
  EXPECT_THAT(data, testing::Pointwise(testing::FloatEq(), expected_data));
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

template <typename T>
struct OperandInfo {
  OperandDataType type;
  std::vector<uint32_t> dimensions;
  std::vector<T> values;
#if BUILDFLAG(IS_MAC)
  OperandInfo<int32_t> ToInt32() {
    return OperandInfo<int32_t>{
        .type = OperandDataType::kInt32,
        .dimensions = dimensions,
        .values = std::vector<int32_t>(values.begin(), values.end())};
  }
#endif  // BUILDFLAG(IS_MAC)
};

void VerifyIsEqual(base::span<const float> actual,
                   const OperandInfo<float>& expected) {
  VerifyFloatDataIsEqual(actual, expected.values);
}

template <typename T>
void VerifyIsEqual(base::span<const T> actual, const OperandInfo<T>& expected) {
  EXPECT_EQ(actual, expected.values);
}

}  // namespace

#if BUILDFLAG(IS_WIN)
class WebNNGraphImplBackendTest : public dml::TestBase {
 public:
  WebNNGraphImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}

  void SetUp() override;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<dml::Adapter> adapter_;
};

void WebNNGraphImplBackendTest::SetUp() {
  SKIP_TEST_IF(!dml::UseGPUInTests());

  dml::Adapter::EnableDebugLayerForTesting();
  auto adapter_creation_result = dml::Adapter::GetGpuInstanceForTesting();
  // If the adapter creation result has no value, it's most likely because
  // platform functions were not properly loaded.
  SKIP_TEST_IF(!adapter_creation_result.has_value());
  adapter_ = adapter_creation_result.value();
  // Graph compilation relies on IDMLDevice1::CompileGraph introduced in
  // DirectML version 1.2 or DML_FEATURE_LEVEL_2_1, so skip the tests if the
  // DirectML version doesn't support this feature.
  SKIP_TEST_IF(!adapter_->IsDMLDeviceCompileGraphSupportedForTesting());

  // Skip a test if the required feature level is not supported for the
  // operator being tested.
  auto kRequiredFeatureLevels = base::MakeFixedFlatMap<std::string_view,
                                                       DML_FEATURE_LEVEL>(
      {// DML_BATCHNORMALIZATION_OPERATOR_DESC support for 1~8 dimension counts
       // was introduced in DML_FEATURE_LEVEL_3_1.
       {"FuseStandaloneActivationIntoBatchNormalization",
        DML_FEATURE_LEVEL_3_1},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"FuseStandaloneActivationIntoGemm", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildAndComputeMultipleOperatorGemm", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildOneInputAndOneConstantOperand", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildOneGraphToComputeMultipleTimes", DML_FEATURE_LEVEL_4_0},
       // DML_MEAN_VARIANCE_NORMALIZATION1_OPERATOR_DESC support for 1~8
       // dimension
       // counts was introduced in DML_FEATURE_LEVEL_3_1.
       {"BuildSingleOperatorLayerNormalization", DML_FEATURE_LEVEL_3_1},
       // DML_GEMM_OPERATOR_DESC support for 2~4 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildAndComputeSingleOperatorMatmul", DML_FEATURE_LEVEL_4_0},
       {"FuseStandaloneOperationsIntoMatmul", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildMultipleInputsAppendingConstants", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildMultipleConstantsAppendingInputs", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildGemmWithReshapedConstantOperand", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildMaxPooingAsThirdOperator", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildMaxPooingAsSecondOperator", DML_FEATURE_LEVEL_4_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildMaxPooingAsFirstOperator", DML_FEATURE_LEVEL_4_0}});
  auto it = kRequiredFeatureLevels.find(
      ::testing::UnitTest::GetInstance()->current_test_info()->name());
  if (it != kRequiredFeatureLevels.end()) {
    const auto& required_feature_level = it->second;
    SKIP_TEST_IF(!adapter_->IsDMLFeatureLevelSupported(required_feature_level));
  }
}
#endif  // #if BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
class WebNNGraphImplBackendTest : public testing::Test {
 public:
  WebNNGraphImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}

  void SetUp() override;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

void WebNNGraphImplBackendTest::SetUp() {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP() << "Skipping test because WebNN is not supported on Mac OS "
                 << base::mac::MacOSVersion();
  }
  const std::string_view current_test_name =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  // Keep this list sorted by the operator being tested.
  static auto kSupportedTests = base::MakeFixedFlatSet<std::string_view>({
      "BuildAndComputeSingleOperatorClamp",
      "BuildAndComputeConcatWithConstants",
      "BuildAndComputeSingleOperatorRelu",
      "BuildAndComputeSingleOperatorTanh",
      "BuildAndComputeGraphWithTwoTranspose",
  });
  if (!kSupportedTests.contains(current_test_name)) {
    GTEST_SKIP() << "Skipping test because the operator is not yet supported.";
  }
}
#endif  // BUILDFLAG(IS_MAC)

// TODO(crbug.com/325612086): Parameterize these tests for different backends.
#if BUILDFLAG(WEBNN_USE_TFLITE) && !BUILDFLAG(IS_WIN)
class WebNNGraphImplBackendTest : public testing::Test {
 public:
  WebNNGraphImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {
  }

  void SetUp() override;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

void WebNNGraphImplBackendTest::SetUp() {
  const std::string_view current_test_name =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  static auto kSupportedTests = base::MakeFixedFlatSet<std::string_view>({
      "BuildAndComputeConcatWithConstants",
      "BuildAndComputeSingleOperatorGruCell",
      "BuildAndComputeSingleOperatorGru",
      "BuildAndComputeSingleOperatorLstmCell",
      "BuildAndComputeSingleOperatorLstm",
  });
  if (!kSupportedTests.contains(current_test_name)) {
    GTEST_SKIP() << "Skipping test because the operator is not yet supported.";
  }
}
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) && !BUILDFLAG(IS_WIN)

struct FusibleOperationDescriptor {
  mojom::Operation::Tag kind;
  std::optional<float> alpha;
  std::optional<float> beta;
};

void BuildFusibleOperation(GraphInfoBuilder& builder,
                           const FusibleOperationDescriptor& operation,
                           uint64_t input_operand_id,
                           uint64_t output_operand_id) {
  switch (operation.kind) {
    case mojom::Operation::Tag::kElu: {
      CHECK(operation.alpha.has_value());
      builder.BuildElu(input_operand_id, output_operand_id, *operation.alpha);
      return;
    }
    case mojom::Operation::Tag::kHardSigmoid: {
      CHECK(operation.alpha.has_value());
      CHECK(operation.beta.has_value());
      builder.BuildHardSigmoid(input_operand_id, output_operand_id,
                               *operation.alpha, *operation.beta);
      return;
    }
    case mojom::Operation::Tag::kLeakyRelu: {
      CHECK(operation.alpha.has_value());
      builder.BuildLeakyRelu(input_operand_id, output_operand_id,
                             *operation.alpha);
      return;
    }
    case mojom::Operation::Tag::kLinear: {
      CHECK(operation.alpha.has_value());
      CHECK(operation.beta.has_value());
      builder.BuildLinear(input_operand_id, output_operand_id, *operation.alpha,
                          *operation.beta);
      return;
    }
    case mojom::Operation::Tag::kRelu:
      builder.BuildRelu(input_operand_id, output_operand_id);
      return;
    case mojom::Operation::Tag::kSigmoid:
      builder.BuildSigmoid(input_operand_id, output_operand_id);
      return;
    case mojom::Operation::Tag::kSoftplus:
      builder.BuildSoftplus(input_operand_id, output_operand_id);
      return;
    case mojom::Operation::Tag::kSoftsign:
      builder.BuildSoftsign(input_operand_id, output_operand_id);
      return;
    case mojom::Operation::Tag::kTanh:
      builder.BuildTanh(input_operand_id, output_operand_id);
      return;
    default:
      // TODO(crbug.com/345640552): Support fusing gelu.
      NOTREACHED();
  }
}

template <typename T>
struct BatchNormalizationTester {
  OperandInfo<T> input;
  OperandInfo<T> mean;
  OperandInfo<T> variance;
  std::optional<OperandInfo<T>> scale;
  std::optional<OperandInfo<T>> bias;
  struct BatchNormalizationAttributes {
    std::optional<uint64_t> scale_operand_id;
    std::optional<uint64_t> bias_operand_id;
    uint32_t axis = 1;
    float epsilon = 1e-5;
  };
  BatchNormalizationAttributes attributes;
  OperandInfo<T> output;

  void TestFusingOperation(
      const FusibleOperationDescriptor& fusible_operation) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t mean_operand_id =
        builder.BuildInput("mean", mean.dimensions, mean.type);
    uint64_t variance_operand_id =
        builder.BuildInput("variance", variance.dimensions, variance.type);
    uint64_t intermediate_operand_id =
        builder.BuildIntermediateOperand(output.dimensions, output.type);
    if (scale.has_value()) {
      attributes.scale_operand_id =
          builder.BuildInput("scale", scale->dimensions, scale->type);
    }
    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }

    builder.BuildBatchNormalization(
        input_operand_id, mean_operand_id, variance_operand_id,
        intermediate_operand_id, std::move(attributes));

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    BuildFusibleOperation(builder, fusible_operation, intermediate_operand_id,
                          output_operand_id);

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    named_inputs.insert({"mean", mean.values});
    named_inputs.insert({"variance", variance.values});
    if (scale.has_value()) {
      named_inputs.insert({"scale", scale->values});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", bias->values});
    }

    base::flat_map<std::string, std::vector<T>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyIsEqual(named_outputs["output"], output);
  }
};

// Test building and computing a graph of fusing a standalone activation into
// batchNormalization automatically.
TEST_F(WebNNGraphImplBackendTest,
       FuseStandaloneActivationIntoBatchNormalization) {
  {  // Test batchNormalization with 4-D input, default axis and activation =
    // linear.
    BatchNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = OperandDataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = OperandDataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-8.999950000374997, 1, 10.999950000374997,
                              -1.2474078892909666, 11, 23.24740788929097}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kLinear, .alpha = 10, .beta = 1});
  }
  {
    // Test batchNormalization with 4-D input with activation = hardsigmoid.
    BatchNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = OperandDataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = OperandDataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {1, 1, 1, 1, 1, 1}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kHardSigmoid,
            .alpha = 1,
            .beta = 3});
  }
  {
    // Test batchNormalization with 4-D input with activation = relu.
    BatchNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = OperandDataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = OperandDataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {0, 0, 0.9999950000374997, 0, 1,
                              2.224740788929097}}}
        .TestFusingOperation(
            FusibleOperationDescriptor{.kind = mojom::Operation::Tag::kRelu});
  }
  {
    // Test batchNormalization with 4-D input with activation = softplus.
    BatchNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-100, -50, 100, 101, 102, 103}},
        .mean = {.type = OperandDataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = OperandDataType::kFloat32,
                     .dimensions = {2},
                     .values = {1, 4}},
        .scale = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1, 2}},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.epsilon = 0},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {0, 0, 100, 99, 100, 101}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kSoftplus});
  }
  {
    // Test batchNormalization with 1-D input with activation = softsign.
    BatchNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {2},
                  .values = {-1, 1}},
        .mean = {.type = OperandDataType::kFloat32,
                 .dimensions = {2},
                 .values = {-1, 1}},
        .variance = {.type = OperandDataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.axis = 0},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {2},
                   .values = {0, 0.5}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kSoftsign});
  }
}

template <typename T>
struct Conv2dTester {
  mojom::Conv2d::Kind type;
  OperandInfo<T> input;
  OperandInfo<T> filter;
  struct Conv2dAttributes {
    std::vector<uint32_t> padding = {0, 0, 0, 0};
    std::vector<uint32_t> strides = {1, 1};
    std::vector<uint32_t> dilations = {1, 1};
    uint32_t groups = 1;
    mojom::InputOperandLayout input_layout =
        mojom::InputOperandLayout::kChannelsFirst;
    std::optional<OperandInfo<T>> bias;
  };
  Conv2dAttributes attributes;
  OperandInfo<float> output;

  void TestFusingOperation(
      const FusibleOperationDescriptor& fusible_operation) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t filter_operand_id =
        builder.BuildConstant(filter.dimensions, filter.type,
                              base::as_bytes(base::make_span(filter.values)));
    uint64_t conv2d_output_operand_id =
        builder.BuildIntermediateOperand(output.dimensions, output.type);

    std::optional<uint64_t> bias_operand_id;
    if (attributes.bias.has_value()) {
      bias_operand_id = builder.BuildConstant(
          attributes.bias->dimensions, attributes.bias->type,
          base::as_bytes(base::make_span(attributes.bias->values)));
    }

    builder.BuildConv2d(type, input_operand_id, filter_operand_id,
                        conv2d_output_operand_id, std::move(attributes),
                        bias_operand_id);

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    BuildFusibleOperation(builder, fusible_operation, conv2d_output_operand_id,
                          output_operand_id);

    base::flat_map<std::string, base::span<const T>> named_inputs;

    named_inputs.insert({"input", input.values});
    base::flat_map<std::string, std::vector<T>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyIsEqual(named_outputs["output"], output);
  }
};

// Test building and computing a graph of fusing a standalone activation
// into conv2d automatically.
TEST_F(WebNNGraphImplBackendTest, FuseStandaloneActivationIntoConv2d) {
  // Test conv2d with NCHW layout, float 32 data type, bias and fusing with elu
  // activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 3, 3},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7, 8}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 1, 1},
                   .values = {1}},
        .attributes = {.bias =
                           OperandInfo<float>{.type = OperandDataType::kFloat32,
                                              .dimensions = {1},
                                              .values = {-5}}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {-0.7946096424007316, -0.7853474888890126,
                              -0.7601703453057089, -0.6917317734107099,
                              -0.5056964470628461, 0, 1, 2, 3}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kElu, .alpha = 0.8});
  }
  // Test conv2d with NCHW layout, float 32 data type, bias and fusing with
  // leakyRelu activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 4, 4},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                             15}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.bias =
                           OperandInfo<float>{.type = OperandDataType::kFloat32,
                                              .dimensions = {1},
                                              .values = {-60}}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = {-0.3, -0.12, 21, 30}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kLeakyRelu, .alpha = 0.02});
  }
  // Test conv2d with NCHW layout, float 32 data type, fusing with bias and
  // linear activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .bias =
                           OperandInfo<float>{.type = OperandDataType::kFloat32,
                                              .dimensions = {1},
                                              .values = {1}}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 5, 5},
                   .values = {1.13, 1.22, 1.28, 1.34, 1.25, 1.34, 1.55,
                              1.64, 1.73, 1.52, 1.64, 2,    2.09, 2.18,
                              1.82, 1.94, 2.45, 2.54, 2.63, 2.12, 1.73,
                              2.12, 2.18, 2.24, 1.85}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kLinear, .alpha = 0.01, .beta = 1});
  }
  // Test conv2d with NCHW layout, fusing with hardSigmoid activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .bias =
                           OperandInfo<float>{.type = OperandDataType::kFloat32,
                                              .dimensions = {1},
                                              .values = {1}}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 5, 5},
                   .values = {0,    0,    0, 0,    0,    0,    0, 0,    0,
                              0,    0,    0, 0.09, 0.18, 0,    0, 0.45, 0.54,
                              0.63, 0.12, 0, 0.12, 0.18, 0.24, 0}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kHardSigmoid,
            .alpha = 0.01,
            .beta = -1});
  }
  // Test conv2d with NCHW layout, fusing with sigmoid activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
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
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 1, 2, 2},
                   .values = {0.14543837927656278, 0.9671129790291346,
                              0.10836050336762582, 0.320230810822804,
                              0.6952692250382182, 0.5070913293589028,
                              0.0813970738017622, 0.5303338853508432,
                              0.30721364807734, 0.4324123448833208,
                              0.9849002194630809, 0.4281076188358701}},
        .output = {.type = OperandDataType::kFloat32,
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
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kSigmoid});
  }
  // Test conv2d with NCHW layout, float 32 data type, bias and fusing with
  // softplus activation.
  {
    Conv2dTester<float>{.type = mojom::Conv2d::Kind::kDirect,
                        .input = {.type = OperandDataType::kFloat32,
                                  .dimensions = {1, 1, 2, 2},
                                  .values = {40, 48, 56, 64}},
                        .filter = {.type = OperandDataType::kFloat32,
                                   .dimensions = {1, 1, 1, 1},
                                   .values = {1}},
                        .output = {.type = OperandDataType::kFloat32,
                                   .dimensions = {1, 1, 2, 2},
                                   .values = {40, 48, 56, 64}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kSoftplus});
  }
  // Test conv2d with NCHW layout, float 32 data type, fusing with softsign
  // activation.
  {
    Conv2dTester<float>{.type = mojom::Conv2d::Kind::kDirect,
                        .input = {.type = OperandDataType::kFloat32,
                                  .dimensions = {1, 1, 3, 3},
                                  .values = {-3, -2, -1, -4, 0, 2, 1, 3, 4}},
                        .filter = {.type = OperandDataType::kFloat32,
                                   .dimensions = {1, 1, 2, 2},
                                   .values = std::vector<float>(4, 1)},
                        .output = {.type = OperandDataType::kFloat32,
                                   .dimensions = {1, 1, 2, 2},
                                   .values = {-0.9, -0.5, 0, 0.9}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kSoftsign});
  }
  // Test conv2d with NCHW layout, fusing with tanh activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 0.05)},
        .attributes = {.padding = {1, 1, 1, 1}},
        .output = {.type = OperandDataType::kFloat32,
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
        .TestFusingOperation(
            FusibleOperationDescriptor{.kind = mojom::Operation::Tag::kTanh});
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
  void Test(WebNNGraphImplBackendTest& helper) {

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t lhs_operand_id =
        builder.BuildInput("lhs", lhs.dimensions, lhs.type);
    uint64_t rhs_operand_id =
        builder.BuildInput("rhs", rhs.dimensions, rhs.type);
    auto graph_output_type = output.type;
#if BUILDFLAG(IS_MAC)
    if (output.type == OperandDataType::kUint8) {
      // macOS only supports FP16,FP32,DOUBLE,INT32 as outputs of graph.
      // For testing, we cast the output of the element-wise logical
      // operators to Int32 and set the graph output to Int32.
      graph_output_type = OperandDataType::kInt32;
    }
#endif  // BUILD_FLAG(IS_MAC)
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, graph_output_type);
    uint64_t element_wise_binary_output_operand_id = output_operand_id;
#if BUILDFLAG(IS_MAC)
    if (output.type == OperandDataType::kUint8) {
      element_wise_binary_output_operand_id = builder.BuildIntermediateOperand(
          output.dimensions, OperandDataType::kUint8);
    }
#endif  // BUILD_FLAG(IS_MAC)
    builder.BuildElementWiseBinary(kind, lhs_operand_id, rhs_operand_id,
                                   element_wise_binary_output_operand_id);
#if BUILDFLAG(IS_MAC)
    if (output.type == OperandDataType::kUint8) {
      builder.BuildElementWiseUnary(mojom::ElementWiseUnary::Kind::kCast,
                                    element_wise_binary_output_operand_id,
                                    output_operand_id);
    }
#endif  // BUILD_FLAG(IS_MAC)

    base::flat_map<std::string, base::span<const I>> named_inputs;
    named_inputs.insert({"lhs", lhs.values});
    named_inputs.insert({"rhs", rhs.values});
    base::flat_map<std::string, std::vector<O>> named_outputs =
        BuildAndCompute<O>(builder.TakeGraphInfo(), std::move(named_inputs));

#if BUILDFLAG(IS_MAC)
    if (output.type == OperandDataType::kUint8) {
      VerifyIsEqual(named_outputs["output"], output.ToInt32());
      return;
    }
#endif  // BUILD_FLAG(IS_MAC)

    VerifyIsEqual(named_outputs["output"], output);
  }

  void TestFusingOperation(
      const FusibleOperationDescriptor& fusible_operation) {
    // Now only binary add supports fusing standalone activation.
    CHECK_EQ(kind, mojom::ElementWiseBinary::Kind::kAdd);
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t lhs_operand_id =
        builder.BuildInput("lhs", lhs.dimensions, lhs.type);
    uint64_t rhs_operand_id =
        builder.BuildInput("rhs", rhs.dimensions, rhs.type);
    uint64_t intermediate_operand_id =
        builder.BuildIntermediateOperand(output.dimensions, output.type);
    builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                   lhs_operand_id, rhs_operand_id,
                                   intermediate_operand_id);

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    BuildFusibleOperation(builder, fusible_operation, intermediate_operand_id,
                          output_operand_id);

    base::flat_map<std::string, base::span<const I>> named_inputs;
    named_inputs.insert({"lhs", lhs.values});
    named_inputs.insert({"rhs", rhs.values});
    base::flat_map<std::string, std::vector<O>> named_outputs =
        BuildAndCompute<O>(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyIsEqual(named_outputs["output"], output);
  }
};

// Test building and computing a graph of fusing a standalone activation
// into elementwise binary add automatically.
TEST_F(WebNNGraphImplBackendTest,
       FuseStandaloneActivationIntoElementWiseBinaryAdd) {
  // Test add with linear activation.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = OperandDataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = OperandDataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {0, 5.1, 4, 3, 2, 0}},
        .kind = mojom::ElementWiseBinary::Kind::kAdd,
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {11, 72, 71, 71, 71, 61}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kLinear, .alpha = 10, .beta = 1});
  }
  // Test add with relu activation.
  {
    ElementWiseBinaryTester<float>{.lhs = {.type = OperandDataType::kFloat32,
                                           .dimensions = {1, 2, 3, 1},
                                           .values = {1, 2, 3, 4, 5, 6}},
                                   .rhs = {.type = OperandDataType::kFloat32,
                                           .dimensions = {1, 2, 3, 1},
                                           .values = {-6, 5, 4, 3, 2, -7}},
                                   .kind = mojom::ElementWiseBinary::Kind::kAdd,
                                   .output = {.type = OperandDataType::kFloat32,
                                              .dimensions = {1, 2, 3, 1},
                                              .values = {0, 7, 7, 7, 7, 0}}}
        .TestFusingOperation(
            FusibleOperationDescriptor{.kind = mojom::Operation::Tag::kRelu});
  }
}

// Test building and computing a graph in the following topology.
//         [input]
//            |
//          split
//        /       \
//   [output1]  reshape
//                 |
//             [output2]
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithSplitAndReshape) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {2, 5}, OperandDataType::kFloat32);
  uint64_t output1_operand_id =
      builder.BuildOutput("output1", {2, 2}, OperandDataType::kFloat32);
  uint64_t split_operand_id =
      builder.BuildIntermediateOperand({2, 3}, OperandDataType::kFloat32);
  builder.BuildSplit(input_operand_id, {output1_operand_id, split_operand_id},
                     1);

  uint64_t output_operand_id =
      builder.BuildOutput("output2", {3, 2}, OperandDataType::kFloat32);
  builder.BuildReshape(split_operand_id, output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  // [[ 1  2  3  4  5]
  //  [ 6  7  8  9 10]] with shape (2, 5)
  std::vector<float> input_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  // [[1  2]
  //  [6  7]] with shape (2, 2)
  VerifyFloatDataIsEqual(named_outputs["output1"],
                         std::array<float, 4>{1, 2, 6, 7});
  // [[3  4]
  //  [5  8]
  //  [9  10]] with shape (3, 2)
  VerifyFloatDataIsEqual(named_outputs["output2"],
                         std::array<float, 6>{3, 4, 5, 8, 9, 10});
}

template <typename T>
struct UnaryOperatorTester {
  mojom::Operation::Tag tag;
  OperandInfo<T> input;
  std::optional<float> clamp_min_value;
  std::optional<float> clamp_max_value;
  std::optional<float> hard_sigmoid_alpha;
  std::optional<float> hard_sigmoid_beta;
  std::optional<float> elu_alpha;
  std::optional<float> leaky_relu_alpha;
  std::optional<float> linear_alpha;
  std::optional<float> linear_beta;
  OperandInfo<T> output;
  void Test(BuildAndComputeExpectation expectation =
                BuildAndComputeExpectation::kSuccess) {
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
      case mojom::Operation::Tag::kHardSigmoid:
        builder.BuildHardSigmoid(input_operand_id, output_operand_id,
                                 hard_sigmoid_alpha, hard_sigmoid_beta);
        break;
      case mojom::Operation::Tag::kHardSwish:
        builder.BuildHardSwish(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kLeakyRelu:
        CHECK(leaky_relu_alpha);
        builder.BuildLeakyRelu(input_operand_id, output_operand_id,
                               leaky_relu_alpha.value());
        break;
      case mojom::Operation::Tag::kLinear:
        CHECK(linear_alpha);
        CHECK(linear_beta);
        builder.BuildLinear(input_operand_id, output_operand_id,
                            linear_alpha.value(), linear_beta.value());
        break;
      case mojom::Operation::Tag::kRelu:
        builder.BuildRelu(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kSigmoid:
        builder.BuildSigmoid(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kSoftplus:
        builder.BuildSoftplus(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kSoftsign:
        builder.BuildSoftsign(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kTanh:
        builder.BuildTanh(input_operand_id, output_operand_id);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    base::flat_map<std::string, std::vector<T>> named_outputs = BuildAndCompute(
        builder.TakeGraphInfo(), std::move(named_inputs), expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(named_outputs["output"], output);
    }
  }
};

// Test building and computing a graph with single operator clamp.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorClamp) {
  {
    // Test clamp for 0-D scalar input.
    UnaryOperatorTester<float>{.tag = mojom::Operation::Tag::kClamp,
                               .input = {.type = OperandDataType::kFloat32,
                                         .dimensions = {},
                                         .values = {24}},
                               .clamp_min_value = 0,
                               .clamp_max_value = 3,
                               .output = {.type = OperandDataType::kFloat32,
                                          .dimensions = {},
                                          .values = {3}}}
        .Test();
  }
}

// Test building and computing a graph with single operator hardSigmoid.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorHardSigmoid) {
  {
    // Test sigmoid for 0-D scalar input.
    UnaryOperatorTester<float>{.tag = mojom::Operation::Tag::kHardSigmoid,
                               .input = {.type = OperandDataType::kFloat32,
                                         .dimensions = {},
                                         .values = {24}},
                               .hard_sigmoid_alpha = 0.1,
                               .hard_sigmoid_beta = 3,
                               .output = {.type = OperandDataType::kFloat32,
                                          .dimensions = {},
                                          .values = {1}}}
        .Test();
  }
}

// Test building and computing a graph with single operator hardSwish.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorHardSwish) {
  // Test hardSwish with a 0-D scalar input.
  {
    UnaryOperatorTester<float>{.tag = mojom::Operation::Tag::kHardSwish,
                               .input = {.type = OperandDataType::kFloat32,
                                         .dimensions = {},
                                         .values = {7.0}},
                               .output = {.type = OperandDataType::kFloat32,
                                          .dimensions = {},
                                          .values = {7.0}}}
        .Test();
  }
}

// Test building and computing a graph with single operator tanh.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorTanh) {
  // Test tanh with a 0-D scalar input.
  {
    UnaryOperatorTester<float>{.tag = mojom::Operation::Tag::kTanh,
                               .input = {.type = OperandDataType::kFloat32,
                                         .dimensions = {},
                                         .values = {-1}},
                               .output = {.type = OperandDataType::kFloat32,
                                          .dimensions = {},
                                          .values = {-0.76159418}}}
        .Test();
  }
}

// Test building and computing a graph with two relu operators.
//    [input]
//       |
//      relu1
//       |
//      relu2
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTwoRelu) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
  uint64_t relu1_output_id =
      builder.BuildIntermediateOperand({1, 2, 3, 4}, OperandDataType::kFloat32);
  builder.BuildRelu(input_operand_id, relu1_output_id);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 2, 3, 4}, OperandDataType::kFloat32);
  builder.BuildRelu(relu1_output_id, output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(
      named_outputs["output"],
      std::array<float, 24>({0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}));
}

// Test building and computing a graph with two operators (reshape as the
// last node).
//    [input]
//       |
//      relu
//       |
//     reshape
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithReshapeAsLastNode) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
  uint64_t relu_output_id =
      builder.BuildIntermediateOperand({1, 2, 3, 4}, OperandDataType::kFloat32);
  builder.BuildRelu(input_operand_id, relu_output_id);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 1, 6, 4}, OperandDataType::kFloat32);
  builder.BuildReshape(relu_output_id, output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1,  2,  3,  4,  5,  6,  7,  8,
                                   9,  10, 11, 12, 13, 14, 15, 16,
                                   17, 18, 19, 20, 21, 22, 23, 24};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(named_outputs["output"], input_data);
}

// Test building and computing a graph with two operators (reshape as an
// intermediate node).
//    [input]
//       |
//    reshape
//       |
//      relu
TEST_F(WebNNGraphImplBackendTest,
       BuildAndComputeGraphWithReshapeAsIntermediateNode) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
  uint64_t reshape_output_id =
      builder.BuildIntermediateOperand({1, 1, 6, 4}, OperandDataType::kFloat32);
  builder.BuildReshape(input_operand_id, reshape_output_id);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 1, 6, 4}, OperandDataType::kFloat32);
  builder.BuildRelu(reshape_output_id, output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1,  2,  3,  4,  5,  6,  7,  8,
                                   9,  10, 11, 12, 13, 14, 15, 16,
                                   17, 18, 19, 20, 21, 22, 23, 24};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(named_outputs["output"], input_data);
}

// Test building and computing a graph with two reshape operators
//    [input]
//       |
//    reshape1
//       |
//    reshape2
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTwoReshape) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
  uint64_t reshape_output_id =
      builder.BuildIntermediateOperand({1, 1, 6, 4}, OperandDataType::kFloat32);
  builder.BuildReshape(input_operand_id, reshape_output_id);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 2, 3, 4}, OperandDataType::kFloat32);
  builder.BuildReshape(reshape_output_id, output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1,  2,  3,  4,  5,  6,  7,  8,
                                   9,  10, 11, 12, 13, 14, 15, 16,
                                   17, 18, 19, 20, 21, 22, 23, 24};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(named_outputs["output"], input_data);
}

// Test building and computing a graph with two operators and two outputs
//      [input]
//       /   \
//  reshape   relu
//     |        |
// [output1] [output2]
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTwoOutputs) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
  uint64_t output1_operand_id =
      builder.BuildOutput("output1", {1, 1, 6, 4}, OperandDataType::kFloat32);
  builder.BuildReshape(input_operand_id, output1_operand_id);
  uint64_t output2_operand_id =
      builder.BuildOutput("output2", {1, 2, 3, 4}, OperandDataType::kFloat32);
  builder.BuildRelu(input_operand_id, output2_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(
      named_outputs["output1"],
      std::vector<float>({-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12,
                          13, 14, 15, 16, 17, 18, 19, 20, 21, 22,  23,  24}));
  VerifyFloatDataIsEqual(
      named_outputs["output2"],
      std::vector<float>({0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                          13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}));
}

struct GemmAttributes {
  std::optional<uint64_t> c_operand_id;
  // TODO(crbug.com/40206287): Add test cases for below attributes.
  float alpha = 1.0;
  float beta = 1.0;
  bool a_transpose = false;
  bool b_transpose = false;
};

template <typename T>
struct GemmTester {
  OperandInfo<T> input_a;
  OperandInfo<T> input_b;
  std::optional<OperandInfo<T>> input_c;
  GemmAttributes attributes;
  OperandInfo<float> output;

  void TestFusingOperation(
      const FusibleOperationDescriptor& fusible_operation) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_a_operand_id =
        builder.BuildInput("input_a", input_a.dimensions, input_a.type);
    uint64_t input_b_operand_id =
        builder.BuildInput("input_b", input_b.dimensions, input_b.type);
    uint64_t intermediate_operand_id =
        builder.BuildIntermediateOperand(output.dimensions, output.type);
    if (input_c.has_value()) {
      attributes.c_operand_id =
          builder.BuildInput("input_c", input_c->dimensions, input_c->type);
    }

    builder.BuildGemm(input_a_operand_id, input_b_operand_id,
                      intermediate_operand_id, std::move(attributes));

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    BuildFusibleOperation(builder, fusible_operation, intermediate_operand_id,
                          output_operand_id);

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input_a", input_a.values});
    named_inputs.insert({"input_b", input_b.values});
    if (input_c.has_value()) {
      named_inputs.insert({"input_c", input_c->values});
    }
    base::flat_map<std::string, std::vector<float>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyIsEqual(named_outputs["output"], output);
  }
};

// Test building and computing a graph of fusing a standalone activation
// into gemm automatically.
TEST_F(WebNNGraphImplBackendTest, FuseStandaloneActivationIntoGemm) {
  // Test gemm without a third input, activation = linear.
  {
    GemmTester<float>{.input_a = {.type = OperandDataType::kFloat32,
                                  .dimensions = {2, 2},
                                  .values = {1, 2, 3, 4}},
                      .input_b = {.type = OperandDataType::kFloat32,
                                  .dimensions = {2, 2},
                                  .values = {1, 2, 3, 4}},
                      .output = {.type = OperandDataType::kFloat32,
                                 .dimensions = {2, 2},
                                 .values = {71, 101, 151, 221}}}
        .TestFusingOperation(FusibleOperationDescriptor{
            .kind = mojom::Operation::Tag::kLinear, .alpha = 10, .beta = 1});
  }

  // Test gemm with a third input, activation = relu.
  {
    GemmTester<float>{
        .input_a = {.type = OperandDataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, -4}},
        .input_b = {.type = OperandDataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .input_c = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                      .dimensions = {2, 2},
                                      .values = {1, 1, 1, 1}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {8, 11, 0, 0}}}
        .TestFusingOperation(
            FusibleOperationDescriptor{.kind = mojom::Operation::Tag::kRelu});
  }
}

template <typename T>
struct GruTester {
  struct GruAttributes {
    std::optional<uint64_t> bias_operand_id;
    std::optional<uint64_t> recurrent_bias_operand_id;
    std::optional<uint64_t> initial_hidden_state_operand_id;
    bool reset_after = true;
    bool return_sequence = false;
    mojom::RecurrentNetworkDirection direction =
        mojom::RecurrentNetworkDirection::kForward;
    mojom::GruWeightLayout layout = mojom::GruWeightLayout::kZrn;
    std::vector<mojom::RecurrentNetworkActivation> activations{
        mojom::RecurrentNetworkActivation::kSigmoid,
        mojom::RecurrentNetworkActivation::kTanh};
  };

  OperandInfo<T> input;
  OperandInfo<T> weight;
  OperandInfo<T> recurrent_weight;
  uint32_t steps;
  uint32_t hidden_size;
  std::optional<OperandInfo<T>> bias;
  std::optional<OperandInfo<T>> recurrent_bias;
  std::optional<OperandInfo<T>> initial_hidden_state;
  GruAttributes attributes;
  std::vector<OperandInfo<T>> outputs;

  void Test(WebNNGraphImplBackendTest& helper,
            BuildAndComputeExpectation expectation =
                BuildAndComputeExpectation::kSuccess) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t weight_operand_id =
        builder.BuildInput("weight", weight.dimensions, weight.type);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", recurrent_weight.dimensions, recurrent_weight.type);

    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    if (recurrent_bias.has_value()) {
      attributes.recurrent_bias_operand_id = builder.BuildInput(
          "recurrentBias", recurrent_bias->dimensions, recurrent_bias->type);
    }
    if (initial_hidden_state.has_value()) {
      attributes.initial_hidden_state_operand_id = builder.BuildConstant(
          initial_hidden_state->dimensions, initial_hidden_state->type,
          base::as_bytes(base::make_span(initial_hidden_state->values)));
    }

    std::vector<uint64_t> output_operand_ids;
    output_operand_ids.reserve(outputs.size());
    for (size_t i = 0; i < outputs.size(); ++i) {
      const auto& output = outputs[i];
      output_operand_ids.push_back(builder.BuildOutput(
          "output" + base::NumberToString(i), output.dimensions, output.type));
    }

    builder.BuildGru(input_operand_id, weight_operand_id,
                     recurrent_weight_operand_id, std::move(output_operand_ids),
                     steps, hidden_size, std::move(attributes));

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    named_inputs.insert({"weight", weight.values});
    named_inputs.insert({"recurrentWeight", recurrent_weight.values});
    if (bias.has_value()) {
      named_inputs.insert({"bias", bias->values});
    }
    if (recurrent_bias.has_value()) {
      named_inputs.insert({"recurrentBias", recurrent_bias->values});
    }

    base::flat_map<std::string, std::vector<T>> named_outputs = BuildAndCompute(
        builder.TakeGraphInfo(), std::move(named_inputs), expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      for (size_t i = 0; i < outputs.size(); ++i) {
        VerifyIsEqual(named_outputs["output" + base::NumberToString(i)],
                      outputs[i]);
      }
    }
  }
};

// Test building and computing a graph with single operator gru.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorGru) {
  // Test gru without bias and initial hidden state.
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 1;
    GruTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .attributes =
            {.activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {-30., -30., -30., -30., -30., -210., -210.,
                                -210., -210., -210., -552., -552., -552., -552.,
                                -552.}}}}
        .Test(*this);
  }
  // Test gru with number directions = 2.
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 2;
    GruTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .attributes =
            {.direction = mojom::RecurrentNetworkDirection::kBoth,
             .activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {-30.,  -30.,  -30.,  -30.,  -30.,  -210.,
                                -210., -210., -210., -210., -552., -552.,
                                -552., -552., -552., -30.,  -30.,  -30.,
                                -30.,  -30.,  -210., -210., -210., -210.,
                                -210., -552., -552., -552., -552., -552.}}}}
        .Test(*this);
  }
  // Test gru with steps = 2.
  {
    const uint32_t steps = 2;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 2;
    GruTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7, 8,
                             9}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .attributes =
            {.direction = mojom::RecurrentNetworkDirection::kBoth,
             .activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {6.,  6.,  6.,  6.,  6.,  15., 15., 15.,
                                15., 15., 24., 24., 24., 24., 24., 6.,
                                6.,  6.,  6.,  6.,  15., 15., 15., 15.,
                                15., 24., 24., 24., 24., 24.}}}}
        .Test(*this);
  }
  // Test gru with bias and recurrentbias.
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 1;
    GruTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 1)},
        .recurrent_bias =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 0)},
        .attributes =
            {.activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {-42., -42., -42., -42., -42., -240., -240.,
                                -240., -240., -240., -600., -600., -600., -600.,
                                -600.}}}}
        .Test(*this);
  }
  // Test gru with bias and initial hidden state.
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 1;
    GruTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 1)},
        .initial_hidden_state =
            OperandInfo<float>{
                .type = OperandDataType::kFloat32,
                .dimensions = {num_directions, batch_size, hidden_size},
                .values = std::vector<float>(
                    num_directions * batch_size * hidden_size, 1)},
        .attributes =
            {.activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {-725., -725., -725., -725., -725., -2399.,
                                -2399., -2399., -2399., -2399., -5045., -5045.,
                                -5045., -5045., -5045.}}}}
        .Test(*this);
  }
  // Test gru with return_sequence = true;
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 1;
    GruTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 1)},
        .recurrent_bias =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 0)},
        .initial_hidden_state =
            OperandInfo<float>{
                .type = OperandDataType::kFloat32,
                .dimensions = {num_directions, batch_size, hidden_size},
                .values = std::vector<float>(
                    num_directions * batch_size * hidden_size, 1)},
        .attributes =
            {.return_sequence = true,
             .activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .outputs =
            {{.type = OperandDataType::kFloat32,
              .dimensions = {num_directions, batch_size, hidden_size},
              .values = {-725., -725., -725., -725., -725., -2399., -2399.,
                         -2399., -2399., -2399., -5045., -5045., -5045., -5045.,
                         -5045.}},
             {.type = OperandDataType::kFloat32,
              .dimensions = {steps, num_directions, batch_size, hidden_size},
              .values = {-725., -725., -725., -725., -725., -2399., -2399.,
                         -2399., -2399., -2399., -5045., -5045., -5045., -5045.,
                         -5045.}}}}
        .Test(*this);
  }
}

// TODO(https://issues.chromium.org/issues/331250158): Delete the test cases
// after the WPT conformance tests are completed.
template <typename T>
struct GruCellTester {
  struct GruCellAttributes {
    std::optional<uint64_t> bias_operand_id;
    std::optional<uint64_t> recurrent_bias_operand_id;
    bool reset_after = true;
    mojom::GruWeightLayout layout = mojom::GruWeightLayout::kZrn;
    std::vector<mojom::RecurrentNetworkActivation> activations{
        mojom::RecurrentNetworkActivation::kSigmoid,
        mojom::RecurrentNetworkActivation::kTanh};
  };

  OperandInfo<T> input;
  OperandInfo<T> weight;
  OperandInfo<T> recurrent_weight;
  OperandInfo<T> hidden_state;
  uint32_t hidden_size;
  std::optional<OperandInfo<T>> bias;
  std::optional<OperandInfo<T>> recurrent_bias;
  GruCellAttributes attributes;
  OperandInfo<T> output;

  void Test(WebNNGraphImplBackendTest& helper,
            BuildAndComputeExpectation expectation =
                BuildAndComputeExpectation::kSuccess) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t weight_operand_id =
        builder.BuildInput("weight", weight.dimensions, weight.type);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", recurrent_weight.dimensions, recurrent_weight.type);
    uint64_t hidden_state_operand_id = builder.BuildInput(
        "hiddenState", hidden_state.dimensions, hidden_state.type);

    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    if (recurrent_bias.has_value()) {
      attributes.recurrent_bias_operand_id = builder.BuildInput(
          "recurrentBias", recurrent_bias->dimensions, recurrent_bias->type);
    }

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);

    builder.BuildGruCell(input_operand_id, weight_operand_id,
                         recurrent_weight_operand_id, hidden_state_operand_id,
                         output_operand_id, hidden_size, std::move(attributes));

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    named_inputs.insert({"weight", weight.values});
    named_inputs.insert({"recurrentWeight", recurrent_weight.values});
    named_inputs.insert({"hiddenState", hidden_state.values});
    if (bias.has_value()) {
      named_inputs.insert({"bias", bias->values});
    }
    if (recurrent_bias.has_value()) {
      named_inputs.insert({"recurrentBias", recurrent_bias->values});
    }

    base::flat_map<std::string, std::vector<T>> named_outputs = BuildAndCompute(
        builder.TakeGraphInfo(), std::move(named_inputs), expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(named_outputs["output"], output);
    }
  }
};

// Test building and computing a graph with single operator gruCell.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorGruCell) {
  // Test gruCell without bias and initial hidden state.
  {
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    GruCellTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {3 * hidden_size, input_size},
                   .values =
                       std::vector<float>(3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {3 * hidden_size, hidden_size},
                             .values = std::vector<float>(
                                 3 * hidden_size * hidden_size, 1)},
        .hidden_state = {.type = OperandDataType::kFloat32,
                         .dimensions = {batch_size, hidden_size},
                         .values =
                             std::vector<float>(batch_size * hidden_size, 0)},
        .hidden_size = hidden_size,
        .attributes =
            {.activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {batch_size, hidden_size},
                   .values = {-30., -30., -30., -30., -30., -210., -210., -210.,
                              -210., -210., -552., -552., -552., -552., -552.}}}
        .Test(*this);
  }
  // Test gruCell with bias and recurrentbias.
  {
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    GruCellTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {3 * hidden_size, input_size},
                   .values =
                       std::vector<float>(3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {3 * hidden_size, hidden_size},
                             .values = std::vector<float>(
                                 3 * hidden_size * hidden_size, 1)},
        .hidden_state = {.type = OperandDataType::kFloat32,
                         .dimensions = {batch_size, hidden_size},
                         .values =
                             std::vector<float>(batch_size * hidden_size, 0)},
        .hidden_size = hidden_size,
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {3 * hidden_size},
                                   .values =
                                       std::vector<float>(3 * hidden_size, 1)},
        .recurrent_bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                             .dimensions = {3 * hidden_size},
                                             .values = std::vector<float>(
                                                 3 * hidden_size, 0)},
        .attributes =
            {.activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {batch_size, hidden_size},
                   .values = {-42., -42., -42., -42., -42., -240., -240., -240.,
                              -240., -240., -600., -600., -600., -600., -600.}}}
        .Test(*this);
  }
}

// Test building and computing a graph with three gemm operations.
//    [input_a] [input_b] [input_a] [input_b]
//           \    /                \    /
//            gemm                  gemm
//                \                /
//                       gemm
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeMultipleOperatorGemm) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, OperandDataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, OperandDataType::kFloat32);
  uint64_t intermediate_1_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(input_a_operand_id, input_b_operand_id,
                    intermediate_1_operand_id, GemmAttributes());
  uint64_t intermediate_2_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(input_a_operand_id, input_b_operand_id,
                    intermediate_2_operand_id, GemmAttributes());
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(intermediate_1_operand_id, intermediate_2_operand_id,
                    output_operand_id, GemmAttributes());

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_a_data = {1, 2, 3, 4};
  named_inputs.insert({"input_a", input_a_data});
  std::vector<float> input_b_data = {1, 1, 1, 1};
  named_inputs.insert({"input_b", input_b_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({30, 30, 70, 70}));
}

// Test building and computing a graph with one input and one constant.
TEST_F(WebNNGraphImplBackendTest, BuildOneInputAndOneConstantOperand) {
  // Build the mojom graph info.
  std::vector<float> constant_data = {5, 6, 7, 8};
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, OperandDataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(input_a_operand_id, input_b_operand_id, output_operand_id,
                    GemmAttributes());

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_a_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", input_a_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({12, 14, 12, 14}));
}

template <typename T>
struct InstanceNormalizationTester {
  OperandInfo<T> input;
  std::optional<OperandInfo<T>> scale;
  std::optional<OperandInfo<T>> bias;
  struct InstanceNormalizationAttributes {
    std::optional<uint64_t> scale_operand_id;
    std::optional<uint64_t> bias_operand_id;
    mojom::InputOperandLayout layout =
        mojom::InputOperandLayout::kChannelsFirst;
    float epsilon = 1e-5;
  };
  InstanceNormalizationAttributes attributes;
  OperandInfo<T> output;

  void TestFusingOperation(
      const FusibleOperationDescriptor& fusible_operation) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t intermediate_operand_id =
        builder.BuildIntermediateOperand(output.dimensions, output.type);
    if (scale.has_value()) {
      attributes.scale_operand_id =
          builder.BuildInput("scale", scale->dimensions, scale->type);
    }
    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }

    builder.BuildInstanceNormalization(
        input_operand_id, intermediate_operand_id, std::move(attributes));

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    BuildFusibleOperation(builder, fusible_operation, intermediate_operand_id,
                          output_operand_id);

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    if (scale.has_value()) {
      named_inputs.insert({"scale", scale->values});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", bias->values});
    }
    base::flat_map<std::string, std::vector<T>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyIsEqual(named_outputs["output"], output);
  }
};

// Test building and computing a graph of fusing a standalone activation into
// instanceNormalization automatically.
TEST_F(WebNNGraphImplBackendTest,
       FuseStandaloneActivationIntoInstanceNormalization) {
  {
    // Test instanceNormalization with 4-D input with default scale and bias and
    // activation = relu.
    InstanceNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {1, 2, 3, 4, 5, 6}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {0, 0, 1.2247356859083902, 0, 0,
                              1.2247356859083902}}}
        .TestFusingOperation(
            FusibleOperationDescriptor{.kind = mojom::Operation::Tag::kRelu});
  }
}

template <typename T>
struct LayerNormalizationTester {
  OperandInfo<T> input;
  std::optional<OperandInfo<T>> scale;
  std::optional<OperandInfo<T>> bias;
  struct LayerNormalizationAttributes {
    std::optional<uint64_t> scale_operand_id;
    std::optional<uint64_t> bias_operand_id;
    std::vector<uint32_t> axes;
    float epsilon = 1e-5;
  };
  LayerNormalizationAttributes attributes;
  OperandInfo<T> output;

  void Test(BuildAndComputeExpectation expectation =
                BuildAndComputeExpectation::kSuccess) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    if (scale.has_value()) {
      attributes.scale_operand_id =
          builder.BuildInput("scale", scale->dimensions, scale->type);
    }
    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }

    builder.BuildLayerNormalization(input_operand_id, output_operand_id,
                                    std::move(attributes));

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    if (scale.has_value()) {
      named_inputs.insert({"scale", scale->values});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", bias->values});
    }
    base::flat_map<std::string, std::vector<T>> named_outputs = BuildAndCompute(
        builder.TakeGraphInfo(), std::move(named_inputs), expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(named_outputs["output"], output);
    }
  }

  void TestFusingOperation(
      const FusibleOperationDescriptor& fusible_operation) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t intermediate_operand_id =
        builder.BuildIntermediateOperand(output.dimensions, output.type);
    if (scale.has_value()) {
      attributes.scale_operand_id =
          builder.BuildInput("scale", scale->dimensions, scale->type);
    }
    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }

    builder.BuildLayerNormalization(input_operand_id, intermediate_operand_id,
                                    std::move(attributes));

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    BuildFusibleOperation(builder, fusible_operation, intermediate_operand_id,
                          output_operand_id);

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    if (scale.has_value()) {
      named_inputs.insert({"scale", scale->values});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", bias->values});
    }
    base::flat_map<std::string, std::vector<T>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyIsEqual(named_outputs["output"], output);
  }
};

// Test building and computing a graph of fusing a standalone activation into
// layerNormalization automatically.
TEST_F(WebNNGraphImplBackendTest,
       FuseStandaloneActivationIntoLayerNormalization) {
  {
    // Test layerNormalization with 1-D input with axes = [0] and default scale
    // and bias and activation = relu.
    LayerNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {5},
                  .values = {0, 1, 2, 3, 4}},
        .attributes = {.axes = {0}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {5},
                   .values = {0, 0, 0, 0.7071050134262237, 1.4142100268524473}}}
        .TestFusingOperation(
            FusibleOperationDescriptor{.kind = mojom::Operation::Tag::kRelu});
  }
}

// Test building and computing a graph with single operator
// layerNormalization.
TEST_F(WebNNGraphImplBackendTest, BuildSingleOperatorLayerNormalization) {
  {
    // Test layerNormalization with a scalar input with default scale and bias.
    LayerNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {},
                  .values = {5}},
        .attributes = {.axes = {}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {},
                   .values = {0}}}
        .Test();
  }
  {
    // Test layerNormalization with 6-D input with permuted axes = [4, 1, 2].
    LayerNormalizationTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 1, 3, 2, 1},
                  .values = {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7}},
        .scale = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                    .dimensions = {2, 2, 1},
                                    .values = {0.5, 0, 1, -0.5}},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {2, 2, 1},
                                   .values = {0.1, 0.2, 0.3, 0.4}},
        .attributes = {.axes = {4, 1, 2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 1, 3, 2, 1},
                   .values = {-0.47539614454389156, -0.5219944922055593,
                              -0.47539614454389156, -0.5219944922055593,
                              -0.47539614454389156, -0.5219944922055593, 0.2,
                              -0.17539614454389152, 0.2, -0.17539614454389152,
                              0.2, -0.17539614454389152}}}
        .Test();
  }
}

template <typename T>
struct LstmTester {
  OperandInfo<T> input;
  OperandInfo<T> weight;
  OperandInfo<T> recurrent_weight;
  uint32_t steps;
  uint32_t hidden_size;
  std::optional<OperandInfo<T>> bias;
  std::optional<OperandInfo<T>> recurrent_bias;
  std::optional<OperandInfo<T>> peephole_weight;
  std::optional<OperandInfo<T>> initial_hidden_state;
  std::optional<OperandInfo<T>> initial_cell_state;
  struct LstmAttributes {
    std::optional<uint64_t> bias_operand_id;
    std::optional<uint64_t> recurrent_bias_operand_id;
    std::optional<uint64_t> peephole_weight_operand_id;
    std::optional<uint64_t> initial_hidden_state_operand_id;
    std::optional<uint64_t> initial_cell_state_operand_id;
    bool return_sequence = false;
    mojom::RecurrentNetworkDirection direction =
        mojom::RecurrentNetworkDirection::kForward;
    mojom::LstmWeightLayout layout = mojom::LstmWeightLayout::kIofg;
    std::vector<mojom::RecurrentNetworkActivation> activations{
        mojom::RecurrentNetworkActivation::kSigmoid,
        mojom::RecurrentNetworkActivation::kTanh,
        mojom::RecurrentNetworkActivation::kTanh};
  };
  LstmAttributes attributes;
  std::vector<OperandInfo<T>> outputs;

  void Test(WebNNGraphImplBackendTest& helper,
            BuildAndComputeExpectation expectation =
                BuildAndComputeExpectation::kSuccess) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t weight_operand_id =
        builder.BuildInput("weight", weight.dimensions, weight.type);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", recurrent_weight.dimensions, recurrent_weight.type);

    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    if (recurrent_bias.has_value()) {
      attributes.recurrent_bias_operand_id = builder.BuildInput(
          "recurrentBias", recurrent_bias->dimensions, recurrent_bias->type);
    }
    if (peephole_weight.has_value()) {
      attributes.peephole_weight_operand_id = builder.BuildInput(
          "peepholeWeight", peephole_weight->dimensions, peephole_weight->type);
    }
    if (initial_hidden_state.has_value()) {
      attributes.initial_hidden_state_operand_id = builder.BuildInput(
          "initialHiddenState", initial_hidden_state->dimensions,
          initial_hidden_state->type);
    }
    if (initial_cell_state.has_value()) {
      attributes.initial_cell_state_operand_id =
          builder.BuildInput("initialCellState", initial_cell_state->dimensions,
                             initial_cell_state->type);
    }

    std::vector<uint64_t> output_operand_ids;
    output_operand_ids.reserve(outputs.size());
    for (size_t i = 0; i < outputs.size(); ++i) {
      const auto& output = outputs[i];
      output_operand_ids.push_back(builder.BuildOutput(
          "output" + base::NumberToString(i), output.dimensions, output.type));
    }

    builder.BuildLstm(input_operand_id, weight_operand_id,
                      recurrent_weight_operand_id,
                      std::move(output_operand_ids), steps, hidden_size,
                      std::move(attributes));

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    named_inputs.insert({"weight", weight.values});
    named_inputs.insert({"recurrentWeight", recurrent_weight.values});
    if (bias.has_value()) {
      named_inputs.insert({"bias", bias->values});
    }
    if (recurrent_bias.has_value()) {
      named_inputs.insert({"recurrentBias", recurrent_bias->values});
    }
    if (peephole_weight.has_value()) {
      named_inputs.insert({"peepholeWeight", peephole_weight->values});
    }
    if (initial_hidden_state.has_value()) {
      named_inputs.insert({"initialHiddenState", initial_hidden_state->values});
    }
    if (initial_cell_state.has_value()) {
      named_inputs.insert({"initialCellState", initial_cell_state->values});
    }

    base::flat_map<std::string, std::vector<T>> named_outputs = BuildAndCompute(
        builder.TakeGraphInfo(), std::move(named_inputs), expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      for (size_t i = 0; i < outputs.size(); ++i) {
        VerifyIsEqual(named_outputs["output" + base::NumberToString(i)],
                      outputs[i]);
      }
    }
  }
};

// Test building and computing a graph with single operator lstm.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorLstm) {
  {
    // Test lstm with given bias and recurrent bias, activations = {relu, relu,
    // relu}.
    uint32_t steps = 2;
    uint32_t batch_size = 2;
    uint32_t input_size = 2;
    uint32_t direction_count = 1;
    uint32_t hidden_size = 1;
    LstmTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {-4, -3, -2, -1, 0, 1, 2, 3}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size, input_size},
                   .values = std::vector<float>(8, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(4, 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {direction_count, 4 * hidden_size},
                               .values = std::vector<float>(4, 0.5)},
        .recurrent_bias =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {direction_count, 4 * hidden_size},
                               .values = std::vector<float>(4, 0.5)},
        .attributes =
            {.activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {8, 216}},
                    {.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {4, 36}}}}
        .Test(*this);
  }
  {
    // Test lstm with given bias and peephole weight, activations = {relu, relu,
    // relu}.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 2;
    uint32_t direction_count = 1;
    uint32_t hidden_size = 2;
    LstmTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size, input_size},
                   .values = std::vector<float>(16, 1)},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(16, 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {direction_count, 4 * hidden_size},
                               .values = std::vector<float>(8, 1)},
        .peephole_weight =
            OperandInfo<float>{.type = OperandDataType::kFloat32,
                               .dimensions = {direction_count, 3 * hidden_size},
                               .values = std::vector<float>(6, 0)},
        .attributes =
            {.activations = {mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu,
                             mojom::RecurrentNetworkActivation::kRelu}},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {2811392, 2811392}},
                    {.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {20672, 20672}}}}
        .Test(*this);
  }
  {
    // Test lstm with constant operands.
    uint32_t steps = 1;
    uint32_t batch_size = 2;
    uint32_t input_size = 1;
    uint32_t direction_count = 1;
    uint32_t hidden_size = 2;
    std::array<float, 2> input_data = {0, 1};
    std::array<float, 8> weight_data = {1, 1, 1, 1, 1, 1, 1, 1};
    std::array<float, 16> recurrent_weight_data = {1, 1, 1, 1, 1, 1, 1, 1,
                                                   1, 1, 1, 1, 1, 1, 1, 1};
    std::array<float, 6> peephole_weight_data = {0, 0, 0, 0, 0, 0};
    std::array<float, 4> initial_hidden_state_data = {0, 0, 0, 0};
    std::array<float, 4> initial_cell_state_data = {1, 1, 1, 1};
    std::vector<float> expected_data = {0, 0, 2, 2};

    GraphInfoBuilder builder;
    uint64_t input_operand_id = builder.BuildConstant(
        {steps, batch_size, input_size}, OperandDataType::kFloat32,
        base::as_bytes(base::make_span(input_data)));
    uint64_t weight_operand_id =
        builder.BuildConstant({direction_count, 4 * hidden_size, input_size},
                              OperandDataType::kFloat32,
                              base::as_bytes(base::make_span(weight_data)));
    uint64_t recurrent_weight_operand_id = builder.BuildConstant(
        {direction_count, 4 * hidden_size, hidden_size},
        OperandDataType::kFloat32,
        base::as_bytes(base::make_span(recurrent_weight_data)));

    LstmTester<float>::LstmAttributes attributes;
    attributes.peephole_weight_operand_id = builder.BuildConstant(
        {direction_count, 3 * hidden_size}, OperandDataType::kFloat32,
        base::as_bytes(base::make_span(peephole_weight_data)));
    attributes.initial_hidden_state_operand_id = builder.BuildConstant(
        {direction_count, batch_size, hidden_size}, OperandDataType::kFloat32,
        base::as_bytes(base::make_span(initial_hidden_state_data)));
    attributes.initial_cell_state_operand_id = builder.BuildConstant(
        {direction_count, batch_size, hidden_size}, OperandDataType::kFloat32,
        base::as_bytes(base::make_span(initial_cell_state_data)));
    attributes.activations = {mojom::RecurrentNetworkActivation::kRelu,
                              mojom::RecurrentNetworkActivation::kRelu,
                              mojom::RecurrentNetworkActivation::kRelu};

    uint64_t output_a_operand_id = builder.BuildOutput(
        "output0", {direction_count, batch_size, hidden_size},
        OperandDataType::kFloat32);
    uint64_t output_b_operand_id = builder.BuildOutput(
        "output1", {direction_count, batch_size, hidden_size},
        OperandDataType::kFloat32);
    std::vector<uint64_t> output_operand_ids{output_a_operand_id,
                                             output_b_operand_id};
    builder.BuildLstm(input_operand_id, weight_operand_id,
                      recurrent_weight_operand_id,
                      std::move(output_operand_ids), steps, hidden_size,
                      std::move(attributes));

    base::flat_map<std::string, std::vector<float>> named_outputs =
        BuildAndCompute<float>(builder.TakeGraphInfo(),
                               /*named_inputs=*/{});

    ASSERT_EQ(named_outputs.size(), 2u);
    VerifyFloatDataIsEqual(named_outputs["output0"], expected_data);
    VerifyFloatDataIsEqual(named_outputs["output1"], expected_data);
  }
}

struct LstmCellAttributes {
  std::optional<uint64_t> bias_operand_id;
  std::optional<uint64_t> recurrent_bias_operand_id;
  std::optional<uint64_t> peephole_weight_operand_id;
  mojom::LstmWeightLayout layout = mojom::LstmWeightLayout::kIofg;
  std::vector<mojom::RecurrentNetworkActivation> activations = {
      mojom::RecurrentNetworkActivation::kSigmoid,
      mojom::RecurrentNetworkActivation::kTanh,
      mojom::RecurrentNetworkActivation::kTanh};
};

// TODO(crbug.com/331250158): Remove this test after the WPT conformance tests
// are completed.
// Test building and computing a graph with single operator lstmCell.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorLstmCell) {
  std::vector<float> expected_output0 = {150, 150, 810, 810};
  std::vector<float> expected_output1 = {30, 30, 90, 90};
  uint32_t batch_size = 2;
  uint32_t input_size = 2;
  uint32_t hidden_size = 2;
  std::vector<float> input_data = {1, 2, 3, 4};
  std::vector<float> weight_data(16, 1);
  std::vector<float> recurrent_weight_data(16, 1);
  std::vector<float> initial_hidden_state_data(4, 1);
  std::vector<float> initial_cell_state_data(4, 1);

  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {batch_size, input_size}, OperandDataType::kFloat32);
  uint64_t weight_operand_id = builder.BuildInput(
      "weight", {4 * hidden_size, input_size}, OperandDataType::kFloat32);
  uint64_t recurrent_weight_operand_id =
      builder.BuildInput("recurrentWeight", {4 * hidden_size, hidden_size},
                         OperandDataType::kFloat32);
  uint64_t hidden_state_operand_id = builder.BuildInput(
      "hiddenState", {batch_size, hidden_size}, OperandDataType::kFloat32);
  uint64_t cell_state_operand_id = builder.BuildInput(
      "cellState", {batch_size, hidden_size}, OperandDataType::kFloat32);

  LstmCellAttributes attributes;
  attributes.activations = {mojom::RecurrentNetworkActivation::kRelu,
                            mojom::RecurrentNetworkActivation::kRelu,
                            mojom::RecurrentNetworkActivation::kRelu};

  uint64_t output_a_operand_id = builder.BuildOutput(
      "output0", {batch_size, hidden_size}, OperandDataType::kFloat32);
  uint64_t output_b_operand_id = builder.BuildOutput(
      "output1", {batch_size, hidden_size}, OperandDataType::kFloat32);
  std::vector<uint64_t> output_operand_ids{output_a_operand_id,
                                           output_b_operand_id};
  builder.BuildLstmCell(input_operand_id, weight_operand_id,
                        recurrent_weight_operand_id, hidden_state_operand_id,
                        cell_state_operand_id, std::move(output_operand_ids),
                        hidden_size, std::move(attributes));

  base::flat_map<std::string, base::span<const float>> named_inputs;
  named_inputs.insert({"input", input_data});
  named_inputs.insert({"weight", weight_data});
  named_inputs.insert({"recurrentWeight", recurrent_weight_data});
  named_inputs.insert({"hiddenState", initial_hidden_state_data});
  named_inputs.insert({"cellState", initial_cell_state_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  ASSERT_EQ(named_outputs.size(), 2u);
  VerifyFloatDataIsEqual(named_outputs["output0"], expected_output0);
  VerifyFloatDataIsEqual(named_outputs["output1"], expected_output1);
}

template <typename T>
struct MatmulTester {
  OperandInfo<T> input_a;
  OperandInfo<T> input_b;
  OperandInfo<T> output;

  void TestFusion(
      std::optional<std::vector<uint32_t>> permutation_a,
      std::optional<std::vector<uint32_t>> permutation_b,
      std::optional<const FusibleOperationDescriptor> fusible_operation) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_a_operand_id =
        builder.BuildInput("input_a", input_a.dimensions, input_a.type);
    if (permutation_a) {
      std::vector<uint32_t> transposed_input_a_shape =
          PermuteArray(input_a.dimensions, permutation_a.value());
      uint64_t transposed_input_a_id = builder.BuildIntermediateOperand(
          transposed_input_a_shape, input_a.type);
      builder.BuildTranspose(input_a_operand_id, transposed_input_a_id,
                             permutation_a.value());
      input_a_operand_id = transposed_input_a_id;
    }
    uint64_t input_b_operand_id =
        builder.BuildInput("input_b", input_b.dimensions, input_b.type);
    if (permutation_b) {
      std::vector<uint32_t> transposed_input_b_shape =
          PermuteArray(input_b.dimensions, permutation_b.value());
      uint64_t transposed_input_b_id = builder.BuildIntermediateOperand(
          transposed_input_b_shape, input_b.type);
      builder.BuildTranspose(input_b_operand_id, transposed_input_b_id,
                             permutation_b.value());
      input_b_operand_id = transposed_input_b_id;
    }

    uint64_t output_operand_id;
    if (fusible_operation) {
      output_operand_id =
          builder.BuildIntermediateOperand(output.dimensions, output.type);
    } else {
      output_operand_id =
          builder.BuildOutput("output", output.dimensions, output.type);
    }

    builder.BuildMatmul(input_a_operand_id, input_b_operand_id,
                        output_operand_id);

    if (fusible_operation) {
      uint64_t intermediate_operand_id = output_operand_id;
      output_operand_id =
          builder.BuildOutput("output", output.dimensions, output.type);
      BuildFusibleOperation(builder, fusible_operation.value(),
                            intermediate_operand_id, output_operand_id);
    }

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input_a", input_a.values});
    named_inputs.insert({"input_b", input_b.values});
    base::flat_map<std::string, std::vector<T>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyIsEqual(named_outputs["output"], output);
  }
};

// Test building and computing a graph of fusing standalone operations
// into matmul when possible.
TEST_F(WebNNGraphImplBackendTest, FuseStandaloneOperationsIntoMatmul) {
  // Test matmul with fusible transpose for input a.
  {
    MatmulTester<float>{
        .input_a = {.type = OperandDataType::kFloat32,
                    .dimensions = {1, 2, 3},
                    .values = {1, 2, 3, 4, 5, 6}},
        .input_b = {.type = OperandDataType::kFloat32,
                    .dimensions = {1, 2, 3},
                    .values = {1, 2, 3, 4, 5, 6}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 3, 3},
                   .values = {17, 22, 27, 22, 29, 36, 27, 36, 45}}}
        .TestFusion(
            /*transpose_a*/ std::vector<uint32_t>({0, 2, 1}),
            /*transpose_b*/ std::nullopt,
            /*activation*/ std::nullopt);
  }

  // Test matmul with fusible transpose for input b.
  {
    MatmulTester<float>{.input_a = {.type = OperandDataType::kFloat32,
                                    .dimensions = {1, 2, 3},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .input_b = {.type = OperandDataType::kFloat32,
                                    .dimensions = {1, 2, 3},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .output = {.type = OperandDataType::kFloat32,
                                   .dimensions = {1, 2, 2},
                                   .values = {14, 32, 32, 77}}}
        .TestFusion(
            /*transpose_a*/ std::nullopt,
            /*transpose_b*/ std::vector<uint32_t>({0, 2, 1}),
            /*activation*/ std::nullopt);
  }

  // Test matmul with fusible transpose for both input a and b.
  {
    MatmulTester<float>{.input_a = {.type = OperandDataType::kFloat32,
                                    .dimensions = {1, 3, 2},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .input_b = {.type = OperandDataType::kFloat32,
                                    .dimensions = {1, 2, 3},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .output = {.type = OperandDataType::kFloat32,
                                   .dimensions = {1, 2, 2},
                                   .values = {22, 49, 28, 64}}}
        .TestFusion(
            /*transpose_a*/ std::vector<uint32_t>({0, 2, 1}),
            /*transpose_b*/ std::vector<uint32_t>({0, 2, 1}),
            /*activation*/ std::nullopt);
  }

  // Test matmul with unfusible transpose for input a.
  {
    MatmulTester<float>{
        .input_a = {.type = OperandDataType::kFloat32,
                    .dimensions = {2, 3, 1},
                    .values = {1, 2, 3, 4, 5, 6}},
        .input_b = {.type = OperandDataType::kFloat32,
                    .dimensions = {1, 2, 3},
                    .values = {1, 2, 3, 4, 5, 6}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 3, 3},
                   .values = {17, 22, 27, 22, 29, 36, 27, 36, 45}}}
        .TestFusion(
            /*transpose_a*/ std::vector<uint32_t>({2, 1, 0}),
            /*transpose_b*/ std::nullopt, /*activation*/ std::nullopt);
  }

  // Test matmul with 2-D * 2-D inputs, activation = linear.
  {
    MatmulTester<float>{.input_a = {.type = OperandDataType::kFloat32,
                                    .dimensions = {2, 2},
                                    .values = {1, 2, 3, 4}},
                        .input_b = {.type = OperandDataType::kFloat32,
                                    .dimensions = {2, 2},
                                    .values = {1, 2, 3, 4}},
                        .output = {.type = OperandDataType::kFloat32,
                                   .dimensions = {2, 2},
                                   .values = {71, 101, 151, 221}}}
        .TestFusion(
            /*transpose_a*/ std::nullopt, /*transpose_b*/ std::nullopt,
            /*activation*/
            FusibleOperationDescriptor{.kind = mojom::Operation::Tag::kLinear,
                                       .alpha = 10,
                                       .beta = 1});
  }

  // Test matmul that can fuse transpose a, b and linear.
  {
    MatmulTester<float>{.input_a = {.type = OperandDataType::kFloat32,
                                    .dimensions = {1, 3, 2},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .input_b = {.type = OperandDataType::kFloat32,
                                    .dimensions = {1, 2, 3},
                                    .values = {1, 2, 3, 4, 5, 6}},
                        .output = {.type = OperandDataType::kFloat32,
                                   .dimensions = {1, 2, 2},
                                   .values = {221, 491, 281, 641}}}
        .TestFusion(
            /*transpose_a*/ std::vector<uint32_t>({0, 2, 1}),
            /*transpose_b*/ std::vector<uint32_t>({0, 2, 1}),
            /*activation*/
            FusibleOperationDescriptor{.kind = mojom::Operation::Tag::kLinear,
                                       .alpha = 10,
                                       .beta = 1});
  }
}

// Test building and computing a graph with two inputs and two constant in
// the following topology.
//    [input_a] [constant_a] [input_b] [constant_b]
//           \    /                \    /
//            gemm                  gemm
//                \                /
//                       gemm
TEST_F(WebNNGraphImplBackendTest, BuildMultipleInputsAppendingConstants) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, OperandDataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, OperandDataType::kFloat32);
  std::vector<float> constant_data = {1, 1, 1, 1};
  uint64_t constant_a_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));

  // The order of inputs are [input_a, constant_a, input_b, constant_b].
  uint64_t intermediate_1_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(input_a_operand_id, constant_a_operand_id,
                    intermediate_1_operand_id, GemmAttributes());
  uint64_t intermediate_2_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(input_b_operand_id, constant_b_operand_id,
                    intermediate_2_operand_id, GemmAttributes());
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(intermediate_1_operand_id, intermediate_2_operand_id,
                    output_operand_id, GemmAttributes());

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1, 2, 3, 4};
  named_inputs.insert({"input_a", input_data});
  named_inputs.insert({"input_b", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({30, 30, 70, 70}));
}

// Test building and computing a graph with two inputs and two constant in
// the following topology.
//    [constant_a] [input_a] [constant_b] [input_b]
//           \    /                \    /
//            gemm                  gemm
//                \                /
//                       gemm
TEST_F(WebNNGraphImplBackendTest, BuildMultipleConstantsAppendingInputs) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, OperandDataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, OperandDataType::kFloat32);
  std::vector<float> constant_data = {1, 2, 3, 4};
  uint64_t constant_a_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));

  // The order of inputs are [constant_a, input_a, constant_b, input_b].
  uint64_t intermediate_1_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(constant_a_operand_id, input_a_operand_id,
                    intermediate_1_operand_id, GemmAttributes());
  uint64_t intermediate_2_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(constant_b_operand_id, input_b_operand_id,
                    intermediate_2_operand_id, GemmAttributes());
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(intermediate_1_operand_id, intermediate_2_operand_id,
                    output_operand_id, GemmAttributes());

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", input_data});
  named_inputs.insert({"input_b", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({30, 30, 70, 70}));
}

// Test building and computing a graph whose gemm operator takes a reshaped
// constant operand c in the following topology:
//                        [constant_c]
//                         |
//     [input_a] [input_b] reshape
//             \    |     /
//                 gemm
// This test case could reproduce the issue of ResNetV2 50 model of WebNN image
// classification sample:
// https://bugs.chromium.org/p/chromium/issues/detail?id=1509747
TEST_F(WebNNGraphImplBackendTest, BuildGemmWithReshapedConstantOperand) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, OperandDataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, OperandDataType::kFloat32);
  std::vector<float> constant_data = {1, 1};
  uint64_t constant_c_operand_id =
      builder.BuildConstant({2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  // Reshape constant_c from [2] to [1, 2] and use it as operand c for gemm.
  uint64_t reshape_operand_id =
      builder.BuildIntermediateOperand({1, 2}, OperandDataType::kFloat32);
  builder.BuildReshape(constant_c_operand_id, reshape_operand_id);
  GemmAttributes gemm_attributes;
  gemm_attributes.c_operand_id = reshape_operand_id;
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(input_a_operand_id, input_b_operand_id, output_operand_id,
                    gemm_attributes);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1, 2, 3, 4};
  named_inputs.insert({"input_a", input_data});
  named_inputs.insert({"input_b", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({8, 11, 16, 23}));
}

// Test building a graph whose add operator takes a reshaped
// constant operand b in the following topology:
//              [constant_b]
//                 |
//    [input_a]  reshape
//           \    /
//            add
TEST_F(WebNNGraphImplBackendTest, BuildAddWithReshapedConstantOperand) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {1, 1, 2, 2}, OperandDataType::kFloat32);
  std::vector<float> constant_data = {1, 1};
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  // Reshape constant_b from [2] to [1, 2] and use it as operand b for add.
  uint64_t reshape_operand_id =
      builder.BuildIntermediateOperand({1, 2}, OperandDataType::kFloat32);
  builder.BuildReshape(constant_b_operand_id, reshape_operand_id);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 input_a_operand_id, reshape_operand_id,
                                 output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));
  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({2, 2, 2, 2}));
}

// Test building and computing a graph whose relu operator only has a
// constant operand input, as the following topology:
//    [constant]
//         |
//       relu
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeReluWithOnlyConstantInput) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  std::vector<float> constant_data = {-1, 0, 1};
  uint64_t constant_operand_id =
      builder.BuildConstant({3}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {3}, OperandDataType::kFloat32);
  builder.BuildRelu(constant_operand_id, output_operand_id);

  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute<float>(builder.TakeGraphInfo(),
                             /*named_inputs=*/{});
  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({0, 0, 1}));
}

// Test building and computing a graph whose add operator only has constant
// operand inputs, as the following topology:
//    [constant_a]  [constant_b]
//               \  /
//               add
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeAddWithOnlyConstantInputs) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  std::vector<float> constant_a_data = {1, 1, 1, 1};
  uint64_t constant_a_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_a_data)));
  std::vector<float> constant_b_data = {2, 2, 2, 2};
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_b_data)));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 constant_a_operand_id, constant_b_operand_id,
                                 output_operand_id);

  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute<float>(builder.TakeGraphInfo(),
                             /*named_inputs=*/{});
  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({3, 3, 3, 3}));
}

// Test building and computing a graph whose add and mul operators only have
// constant and intermediate operand inputs, as the following topology:
//    [constant_a]  [constant_b]
//               \  /
//               add    [constant_c]
//                  \  /
//                   mul
TEST_F(WebNNGraphImplBackendTest,
       BuildAndComputeAddAndMulWithOnlyConstantInputs) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  std::vector<float> constant_a_data = {1, 1, 1, 1};
  uint64_t constant_a_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_a_data)));
  std::vector<float> constant_b_data = {2, 2, 2, 2};
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_b_data)));
  uint64_t intermediate_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 constant_a_operand_id, constant_b_operand_id,
                                 intermediate_operand_id);
  std::vector<float> constant_c_data = {3, 3, 3, 3};
  uint64_t constant_c_operand_id =
      builder.BuildConstant({2, 2}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_c_data)));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kMul,
                                 intermediate_operand_id, constant_c_operand_id,
                                 output_operand_id);

  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute<float>(builder.TakeGraphInfo(),
                             /*named_inputs=*/{});
  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({9, 9, 9, 9}));
}

struct Pool2dAttributes {
  std::vector<uint32_t> window_dimensions;
  std::vector<uint32_t> padding;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> dilations;
  mojom::InputOperandLayout layout;
};

// Test building a graph in the following topology.
//    [input_a] [input_b]
//           \    /
//            add
//             |
//            relu
//             |
//          max pooling
TEST_F(WebNNGraphImplBackendTest, BuildMaxPooingAsThirdOperator) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {1, 1, 2, 2}, OperandDataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {1, 1, 2, 2}, OperandDataType::kFloat32);
  uint64_t intermediate_1_operand_id =
      builder.BuildIntermediateOperand({1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 input_a_operand_id, input_b_operand_id,
                                 intermediate_1_operand_id);

  // Relu.
  uint64_t intermediate_2_operand_id =
      builder.BuildIntermediateOperand({1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildRelu(intermediate_1_operand_id, intermediate_2_operand_id);

  // Max pooling.
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildPool2d(
      mojom::Pool2d::Kind::kMaxPool2d, intermediate_2_operand_id,
      output_operand_id,
      Pool2dAttributes{.window_dimensions = {1, 1},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst});

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", input_data});
  named_inputs.insert({"input_b", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));
  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({2, 2, 2, 2}));
}

// Test building a graph in the following topology.
//    [input_a] [input_b]
//           \    /
//            add
//             |
//          max pooling
//             |
//            relu
TEST_F(WebNNGraphImplBackendTest, BuildMaxPooingAsSecondOperator) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {1, 1, 2, 2}, OperandDataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {1, 1, 2, 2}, OperandDataType::kFloat32);
  uint64_t intermediate_1_operand_id =
      builder.BuildIntermediateOperand({1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 input_a_operand_id, input_b_operand_id,
                                 intermediate_1_operand_id);

  // Max pooling.
  uint64_t intermediate_2_operand_id =
      builder.BuildIntermediateOperand({1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildPool2d(
      mojom::Pool2d::Kind::kMaxPool2d, intermediate_1_operand_id,
      intermediate_2_operand_id,
      Pool2dAttributes{.window_dimensions = {1, 1},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst});

  // Relu.
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildRelu(intermediate_2_operand_id, output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", input_data});
  named_inputs.insert({"input_b", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));
  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({2, 2, 2, 2}));
}

// Test building a graph in the following topology.
//      [input_a]
//          |
//      max pooling
//                  [input_b]
//           \        /
//               add
//                |
//               relu
TEST_F(WebNNGraphImplBackendTest, BuildMaxPooingAsFirstOperator) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {1, 1, 2, 2}, OperandDataType::kFloat32);
  uint64_t intermediate_1_operand_id =
      builder.BuildIntermediateOperand({1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildPool2d(
      mojom::Pool2d::Kind::kMaxPool2d, input_a_operand_id,
      intermediate_1_operand_id,
      Pool2dAttributes{.window_dimensions = {1, 1},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst});

  // Add operation.
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {1, 1, 2, 2}, OperandDataType::kFloat32);
  uint64_t intermediate_2_operand_id =
      builder.BuildIntermediateOperand({1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 intermediate_1_operand_id, input_b_operand_id,
                                 intermediate_2_operand_id);

  // Relu.
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 1, 2, 2}, OperandDataType::kFloat32);
  builder.BuildRelu(intermediate_2_operand_id, output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", input_data});
  named_inputs.insert({"input_b", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));
  VerifyFloatDataIsEqual(named_outputs["output"],
                         std::vector<float>({2, 2, 2, 2}));
}

// Test building and computing a graph with float 16 data type in the
// following topology.
//     [input_a]
//         |
//      reshape    [input_b]
//          \         /
//             concat
//               |
//             clamp
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeReshapeConcatAndClamp) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id1 =
      builder.BuildInput("input_a", {4, 3}, OperandDataType::kFloat16);
  uint64_t input_operand_id2 =
      builder.BuildInput("input_b", {1, 1, 2, 3}, OperandDataType::kFloat16);

  uint64_t reshape_operand_id =
      builder.BuildIntermediateOperand({1, 2, 2, 3}, OperandDataType::kFloat16);
  builder.BuildReshape(input_operand_id1, reshape_operand_id);

  uint64_t concat_operand_id =
      builder.BuildIntermediateOperand({1, 3, 2, 3}, OperandDataType::kFloat16);
  builder.BuildConcat({reshape_operand_id, input_operand_id2},
                      concat_operand_id, 1);

  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 3, 2, 3}, OperandDataType::kFloat16);
  builder.BuildClamp(concat_operand_id, output_operand_id, 1.25, 8.75);

  base::flat_map<std::string, base::span<const float16>> named_inputs;
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

  named_inputs.insert({"input_a", input_data1});
  named_inputs.insert({"input_b", input_data2});
  base::flat_map<std::string, std::vector<float16>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  // [[[[1.25 2.   3.  ]
  //    [4.   5.   6.  ]]
  //   [[7.   8.   8.75]
  //    [8.75 8.75 8.75]]
  //   [[1.25 1.25 1.25]
  //    [1.25 1.25 1.25]]]] with shape (1, 3, 2, 3)
  EXPECT_EQ(Float16ToFloat32(named_outputs["output"]),
            std::vector<float>({1.25, 2, 3, 4, 5, 6, 7, 8, 8.75, 8.75, 8.75,
                                8.75, 1.25, 1.25, 1.25, 1.25, 1.25, 1.25}));
}

// Test building and computing a graph in the following topology.
//      [input]   [constant_a]
//          \          /
//             concat   [constant_b]
//               \           /
//                   concat
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeConcatWithConstants) {
  std::vector<float> expected_output = {0,  0,  0,  1,  2,  3,
                                        -1, -2, -3, -4, -5, -6};

  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 1, 1, 3}, OperandDataType::kFloat32);

  // [[[[1 2 3]]]] with shape (1, 1, 1, 3)
  std::vector<float> constant_data_a = {1, 2, 3};
  uint64_t constant_a_operand_id =
      builder.BuildConstant({1, 1, 1, 3}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data_a)));

  // [[[[-1 -2 -3]
  //    [-4 -5 -6]]]] with shape (1, 1, 2, 3)
  std::vector<float> constant_data_b = {-1, -2, -3, -4, -5, -6};
  uint64_t constant_b_operand_id =
      builder.BuildConstant({1, 1, 2, 3}, OperandDataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data_b)));

  uint64_t concat_operand_id =
      builder.BuildIntermediateOperand({1, 1, 2, 3}, OperandDataType::kFloat32);
  builder.BuildConcat({input_operand_id, constant_a_operand_id},
                      concat_operand_id, 2);

  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 2, 2, 3}, OperandDataType::kFloat32);
  builder.BuildConcat({concat_operand_id, constant_b_operand_id},
                      output_operand_id, 1);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  // [[[[0 0 0]]]] with shape (1, 1, 1, 3)
  std::vector<float> input_data = {0, 0, 0};

  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  // [[[[ 0  0  0]
  //    [ 1  2  3]]
  //   [[-1 -2 -3]
  //    [-4 -5 -6]]]] with shape (1, 2, 2, 3)
  VerifyFloatDataIsEqual(named_outputs["output"], expected_output);
}

template <typename T>
struct Resample2dTester {
  OperandInfo<T> input;
  struct Resample2dAttributes {
    mojom::Resample2d::InterpolationMode mode =
        mojom::Resample2d::InterpolationMode::kNearestNeighbor;
    std::optional<std::vector<float>> scales;
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

    base::flat_map<std::string, base::span<const T>> named_inputs;
    named_inputs.insert({"input", input.values});
    base::flat_map<std::string, std::vector<float>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyFloatDataIsEqual(named_outputs["output"], output.values);
  }
};

// Test building and computing a graph with single operator resample2d.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorResample2d) {
  // Test resample2d with "NearestNeighbor" mode, explicit scales = [2, 3] and
  // axes = [2, 3].
  {
    Resample2dTester<float>{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 2, 2},
                  // [[[[1 2]
                  //    [3 4]]]] with shape (1, 1, 2, 2)
                  .values = {1, 2, 3, 4}},
        .attributes = {.scales = std::vector<float>{2, 3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 4, 6},
                   // [[[[1 1 1 2 2 2]
                   //    [1 1 1 2 2 2]
                   //    [3 3 3 4 4 4]
                   //    [3 3 3 4 4 4]]]] with shape (1, 1, 4, 6)
                   .values = {1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 2, 2,
                              3, 3, 3, 4, 4, 4, 3, 3, 3, 4, 4, 4}}}
        .Test();
  }
}

// Test building and computing a graph in the following topology.
//      [input]
//         |
//     transpose
//         |
//     transpose
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTwoTranspose) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);

  uint64_t transpose_operand_id =
      builder.BuildIntermediateOperand({2, 1, 3, 4}, OperandDataType::kFloat32);
  builder.BuildTranspose(input_operand_id, transpose_operand_id, {1, 0, 2, 3});

  uint64_t output_operand_id =
      builder.BuildOutput("output", {4, 3, 1, 2}, OperandDataType::kFloat32);
  builder.BuildTranspose(transpose_operand_id, output_operand_id, {3, 2, 1, 0});

  base::flat_map<std::string, base::span<const float>> named_inputs;
  // [[[[ -1  -2  -3  -4]
  //    [ -5  -6  -7  -8]
  //    [ -9 -10 -11 -12]]
  //   [[ 13  14  15  16]
  //    [ 17  18  19  20]
  //    [ 21  22  23  24]]]] with shape (1, 2, 3, 4)
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

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
  VerifyFloatDataIsEqual(
      named_outputs["output"],
      std::vector<float>({-1, 13, -5, 17, -9,  21, -2, 14, -6, 18, -10, 22,
                          -3, 15, -7, 19, -11, 23, -4, 16, -8, 20, -12, 24}));
}

// Test building and computing a graph in the following topology.
//      [input]
//         |
//     transpose
//         |
//       relu
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTransposeAndRelu) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);

  uint64_t transpose_operand_id =
      builder.BuildIntermediateOperand({4, 3, 1, 2}, OperandDataType::kFloat32);
  builder.BuildTranspose(input_operand_id, transpose_operand_id, {3, 2, 0, 1});

  uint64_t output_operand_id =
      builder.BuildOutput("output", {4, 3, 1, 2}, OperandDataType::kFloat32);
  builder.BuildRelu(transpose_operand_id, output_operand_id);

  base::flat_map<std::string, base::span<const float>> named_inputs;
  // [[[[ -1  -2  -3  -4]
  //    [ -5  -6  -7  -8]
  //    [ -9 -10 -11 -12]]
  //   [[ 13  14  15  16]
  //    [ 17  18  19  20]
  //    [ 21  22  23  24]]]] with shape (1, 2, 3, 4)
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));
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
  VerifyFloatDataIsEqual(
      named_outputs["output"],
      std::vector<float>({0, 13, 0, 17, 0, 21, 0, 14, 0, 18, 0, 22,
                          0, 15, 0, 19, 0, 23, 0, 16, 0, 20, 0, 24}));
}

// Test building and computing a graph in the following topology.
//      [input]
//         |
//     transpose
//         |
//      reshape
//         |
//      reshape
//         |
//     transpose
TEST_F(WebNNGraphImplBackendTest,
       BuildAndComputeGraphWithTransposeAndTwoReshape) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);

  uint64_t transpose_operand_id =
      builder.BuildIntermediateOperand({4, 3, 1, 2}, OperandDataType::kFloat32);
  builder.BuildTranspose(input_operand_id, transpose_operand_id, {3, 2, 0, 1});

  uint64_t reshape_operand_id1 =
      builder.BuildIntermediateOperand({2, 2, 6}, OperandDataType::kFloat32);
  builder.BuildReshape(transpose_operand_id, reshape_operand_id1);

  uint64_t reshape_operand_id2 =
      builder.BuildIntermediateOperand({12, 2}, OperandDataType::kFloat32);
  builder.BuildReshape(reshape_operand_id1, reshape_operand_id2);

  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 12}, OperandDataType::kFloat32);
  builder.BuildTranspose(reshape_operand_id2, output_operand_id, {1, 0});

  base::flat_map<std::string, base::span<const float>> named_inputs;
  // [[[[ -1  -2  -3  -4]
  //    [ -5  -6  -7  -8]
  //    [ -9 -10 -11 -12]]
  //   [[ 13  14  15  16]
  //    [ 17  18  19  20]
  //    [ 21  22  23  24]]]] with shape (1, 2, 3, 4)
  std::vector<float> input_data = {-1, -2,  -3,  -4,  -5, -6, -7, -8,
                                   -9, -10, -11, -12, 13, 14, 15, 16,
                                   17, 18,  19,  20,  21, 22, 23, 24};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

  // [[ -1  -5  -9  -2  -6 -10  -3  -7 -11  -4  -8 -12]
  //  [ 13  17  21  14  18  22  15  19  23  16  20  24]] wit shape (2, 12)
  VerifyFloatDataIsEqual(
      named_outputs["output"],
      std::vector<float>({-1, -5, -9, -2, -6, -10, -3, -7, -11, -4, -8, -12,
                          13, 17, 21, 14, 18, 22,  15, 19, 23,  16, 20, 24}));
}

// Test building and computing a graph in the following topology.
//         [input]
//            |
//           relu
//          /    \
//     reshape    transpose
//        |           |
//    [output1]   [output2]
TEST_F(WebNNGraphImplBackendTest,
       BuildAndComputeGraphWithTransposeAndTwoOutputs) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 2}, OperandDataType::kFloat32);
  uint64_t relu_operand_id =
      builder.BuildIntermediateOperand({1, 2, 3, 2}, OperandDataType::kFloat32);
  builder.BuildRelu(input_operand_id, relu_operand_id);

  uint64_t output1_operand_id =
      builder.BuildOutput("output1", {3, 4}, OperandDataType::kFloat32);
  uint64_t output2_operand_id =
      builder.BuildOutput("output2", {1, 2, 2, 3}, OperandDataType::kFloat32);
  builder.BuildReshape(relu_operand_id, output1_operand_id);
  builder.BuildTranspose(relu_operand_id, output2_operand_id, {0, 3, 1, 2});

  base::flat_map<std::string, base::span<const float>> named_inputs;
  // [[[[ -1  -2]
  //    [ -5 -10]
  //    [ -7   0]]
  //   [[  1   2]
  //    [  3   6]
  //    [ 10  20]]]] with shape (1, 2, 3, 2)
  std::vector<float> input_data = {-1, -2, -5, -10, -7, 0, 1, 2, 3, 6, 10, 20};
  named_inputs.insert({"input", input_data});
  base::flat_map<std::string, std::vector<float>> named_outputs =
      BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));
  // [[ 0  0  0  0]
  //  [ 0  0  1  2]
  //  [ 3  6 10 20]] with shape (3, 4)
  VerifyFloatDataIsEqual(
      named_outputs["output1"],
      std::vector<float>({0, 0, 0, 0, 0, 0, 1, 2, 3, 6, 10, 20}));
  // [[[[ 0  0  0]
  //    [ 1  3 10]]
  //   [[ 0  0  0]
  //    [ 2  6 20]]]] with shape (1, 2, 2, 3)
  VerifyFloatDataIsEqual(
      named_outputs["output2"],
      std::vector<float>({0, 0, 0, 1, 3, 10, 0, 0, 0, 2, 6, 20}));
}

// Test building and computing a graph which can't be automatically fused
// because the output of conv2d is used by two operations or as graph's output.
TEST_F(WebNNGraphImplBackendTest,
       MultipleOutputsCanNotFuseStandaloneActivation) {
  //     [input]
  //        |
  //       conv
  //      /    \
  //     /      \
  //   relu1    relu2
  //     |        |
  // [output1][output2]
  {
    // Build the mojom graph info.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 5, 5}, OperandDataType::kFloat32);
    uint64_t filter_operand_id = builder.BuildConstant(
        {1, 1, 3, 3}, OperandDataType::kFloat32,
        base::as_bytes(base::make_span(std::vector<float>(9, 1))));
    uint64_t conv2d_output_operand_id = builder.BuildIntermediateOperand(
        {1, 1, 5, 5}, OperandDataType::kFloat32);

    Conv2dTester<float>::Conv2dAttributes attributes{
        .padding = {1, 1, 1, 1},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {1},
                                   .values = {-100}},
    };

    std::optional<uint64_t> bias_operand_id;
    if (attributes.bias.has_value()) {
      bias_operand_id = builder.BuildConstant(
          attributes.bias->dimensions, attributes.bias->type,
          base::as_bytes(base::make_span(attributes.bias->values)));
    }

    builder.BuildConv2d(mojom::Conv2d::Kind::kDirect, input_operand_id,
                        filter_operand_id, conv2d_output_operand_id,
                        std::move(attributes), bias_operand_id);

    uint64_t relu1_output_operand_id =
        builder.BuildOutput("output1", {1, 1, 5, 5}, OperandDataType::kFloat32);
    builder.BuildRelu(conv2d_output_operand_id, relu1_output_operand_id);

    uint64_t relu2_output_operand_id =
        builder.BuildOutput("output2", {1, 1, 5, 5}, OperandDataType::kFloat32);
    builder.BuildRelu(conv2d_output_operand_id, relu2_output_operand_id);

    base::flat_map<std::string, base::span<const float>> named_inputs;

    named_inputs.insert(
        {"input", std::array<float, 25>{0,  1,  2,  3,  4,  5,  6,  7,  8,
                                        9,  10, 11, 12, 13, 14, 15, 16, 17,
                                        18, 19, 20, 21, 22, 23, 24}});
    base::flat_map<std::string, std::vector<float>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    std::vector<float> expected_output_data{0,  0,  0, 0,  0,  0,  0, 0,  0,
                                            0,  0,  0, 8,  17, 0,  0, 44, 53,
                                            62, 11, 0, 11, 17, 23, 0};
    VerifyFloatDataIsEqual(named_outputs["output1"], expected_output_data);
    VerifyFloatDataIsEqual(named_outputs["output2"], expected_output_data);
  }
  //     [input]
  //        |
  //       conv
  //      /    \
  //     /      \
  //  reshape   relu
  //     |        |
  // [output1][output2]
  {
    // Build the mojom graph info.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 5, 5}, OperandDataType::kFloat32);
    uint64_t filter_operand_id = builder.BuildConstant(
        {1, 1, 3, 3}, OperandDataType::kFloat32,
        base::as_bytes(base::make_span(std::vector<float>(9, 1))));
    uint64_t conv2d_output_operand_id = builder.BuildIntermediateOperand(
        {1, 1, 5, 5}, OperandDataType::kFloat32);

    Conv2dTester<float>::Conv2dAttributes attributes{
        .padding = {1, 1, 1, 1},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {1},
                                   .values = {-100}},
    };

    std::optional<uint64_t> bias_operand_id;
    if (attributes.bias.has_value()) {
      bias_operand_id = builder.BuildConstant(
          attributes.bias->dimensions, attributes.bias->type,
          base::as_bytes(base::make_span(attributes.bias->values)));
    }

    builder.BuildConv2d(mojom::Conv2d::Kind::kDirect, input_operand_id,
                        filter_operand_id, conv2d_output_operand_id,
                        std::move(attributes), bias_operand_id);

    uint64_t reshape_output_operand_id =
        builder.BuildOutput("output1", {1, 5, 1, 5}, OperandDataType::kFloat32);
    builder.BuildReshape(conv2d_output_operand_id, reshape_output_operand_id);

    uint64_t relu_output_operand_id =
        builder.BuildOutput("output2", {1, 1, 5, 5}, OperandDataType::kFloat32);
    builder.BuildRelu(conv2d_output_operand_id, relu_output_operand_id);

    base::flat_map<std::string, base::span<const float>> named_inputs;

    named_inputs.insert(
        {"input", std::array<float, 25>{0,  1,  2,  3,  4,  5,  6,  7,  8,
                                        9,  10, 11, 12, 13, 14, 15, 16, 17,
                                        18, 19, 20, 21, 22, 23, 24}});
    base::flat_map<std::string, std::vector<float>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyFloatDataIsEqual(
        named_outputs["output1"],
        std::array<float, 25>{-88, -79, -73, -67, -76, -67, -46, -37, -28,
                              -49, -37, -1,  8,   17,  -19, -7,  44,  53,
                              62,  11,  -28, 11,  17,  23,  -16});
    VerifyFloatDataIsEqual(
        named_outputs["output2"],
        std::array<float, 25>{0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0});
  }
  //     [input]
  //        |
  //      conv2d
  //      /    \
  //     /      \
  //   relu      \
  //     |        \
  // [output1] [output2]
  {
    // Build the mojom graph info.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 5, 5}, OperandDataType::kFloat32);
    uint64_t filter_operand_id = builder.BuildConstant(
        {1, 1, 3, 3}, OperandDataType::kFloat32,
        base::as_bytes(base::make_span(std::vector<float>(9, 1))));
    uint64_t conv2d_output_operand_id = builder.BuildIntermediateOperand(
        {1, 1, 5, 5}, OperandDataType::kFloat32);

    Conv2dTester<float>::Conv2dAttributes attributes{
        .padding = {1, 1, 1, 1},
        .bias = OperandInfo<float>{.type = OperandDataType::kFloat32,
                                   .dimensions = {1},
                                   .values = {-100}},
    };

    std::optional<uint64_t> bias_operand_id;
    if (attributes.bias.has_value()) {
      bias_operand_id = builder.BuildConstant(
          attributes.bias->dimensions, attributes.bias->type,
          base::as_bytes(base::make_span(attributes.bias->values)));
    }

    builder.BuildConv2d(mojom::Conv2d::Kind::kDirect, input_operand_id,
                        filter_operand_id, conv2d_output_operand_id,
                        std::move(attributes), bias_operand_id);
    builder.AddOutput("output2", conv2d_output_operand_id);

    uint64_t relu_output_operand_id =
        builder.BuildOutput("output1", {1, 1, 5, 5}, OperandDataType::kFloat32);
    builder.BuildRelu(conv2d_output_operand_id, relu_output_operand_id);

    base::flat_map<std::string, base::span<const float>> named_inputs;

    named_inputs.insert(
        {"input", std::array<float, 25>{0,  1,  2,  3,  4,  5,  6,  7,  8,
                                        9,  10, 11, 12, 13, 14, 15, 16, 17,
                                        18, 19, 20, 21, 22, 23, 24}});
    base::flat_map<std::string, std::vector<float>> named_outputs =
        BuildAndCompute(builder.TakeGraphInfo(), std::move(named_inputs));

    VerifyFloatDataIsEqual(
        named_outputs["output1"],
        std::array<float, 25>{0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0});
    VerifyFloatDataIsEqual(
        named_outputs["output2"],
        std::array<float, 25>{-88, -79, -73, -67, -76, -67, -46, -37, -28,
                              -49, -37, -1,  8,   17,  -19, -7,  44,  53,
                              62,  11,  -28, 11,  17,  23,  -16});
  }
}

}  // namespace webnn::test
