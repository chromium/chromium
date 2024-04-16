// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cmath>
#include <type_traits>

#include "base/containers/fixed_flat_set.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/ml/webnn/features.mojom-features.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fp16/src/include/fp16.h"

#if BUILDFLAG(IS_WIN)
#include <DirectML.h>
#include <wrl.h>

#include "base/containers/fixed_flat_map.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/context_impl.h"
#include "services/webnn/dml/graph_impl.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/dml/utils.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#endif

namespace webnn::test {

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
        BuildAndComputeExpectation::kSuccess,
    mojom::CreateContextOptions::Device device =
        mojom::CreateContextOptions::Device::kGpu) {
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  mojo::AssociatedRemote<mojom::WebNNGraph> webnn_graph_remote;

  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  // Create the ContextImpl through context provider.
  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  webnn_provider_remote->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          device, mojom::CreateContextOptions::PowerPreference::kDefault),
      create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  if (create_context_result->is_context_remote()) {
    webnn_context_remote.Bind(
        std::move(create_context_result->get_context_remote()));
  }
  EXPECT_FALSE(create_context_result->is_error())
      << create_context_result->get_error()->message;
  EXPECT_TRUE(webnn_context_remote.is_bound());

  // The GraphImpl should be built successfully.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  webnn_context_remote->CreateGraph(std::move(graph_info),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  if (!create_graph_result->is_error()) {
    webnn_graph_remote.Bind(std::move(create_graph_result->get_graph_remote()));
  }

  if (expectation == BuildAndComputeExpectation::kCreateGraphFailure) {
    EXPECT_TRUE(create_graph_result->is_error());
    EXPECT_FALSE(webnn_graph_remote.is_bound());
    EXPECT_TRUE(webnn_context_remote.is_bound());
    webnn_graph_remote.reset();
    webnn_context_remote.reset();
    webnn_provider_remote.reset();
    base::RunLoop().RunUntilIdle();
    return;
  }
  EXPECT_FALSE(create_graph_result->is_error())
      << create_graph_result->get_error()->message;
  EXPECT_TRUE(webnn_graph_remote.is_bound());

  // The GraphImpl should compute successfully.
  base::test::TestFuture<mojom::ComputeResultPtr> compute_future;
  webnn_graph_remote->Compute(std::move(named_inputs),
                              compute_future.GetCallback());
  mojom::ComputeResultPtr compute_result = compute_future.Take();
  ASSERT_TRUE(compute_result->is_named_outputs());
  EXPECT_FALSE(compute_result->get_named_outputs().empty());
  named_outputs = std::move(compute_result->get_named_outputs());

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

// Get the output data from a `mojo_base::BigBuffer` as 32-bit floating-point
// number.
std::vector<float> GetFloatOutputData(mojo_base::BigBuffer big_buffer,
                                      mojom::Operand::DataType type) {
  switch (type) {
    case mojom::Operand_DataType::kFloat32:
      return BigBufferToVector<float>(std::move(big_buffer));
    case mojom::Operand_DataType::kFloat16:
      return Float16ToFloat32(
          BigBufferToVector<float16>(std::move(big_buffer)));
    case mojom::Operand_DataType::kInt32:
    case mojom::Operand_DataType::kUint32:
    case mojom::Operand_DataType::kInt64:
    case mojom::Operand_DataType::kUint64:
    case mojom::Operand_DataType::kInt8:
    case mojom::Operand_DataType::kUint8:
      NOTREACHED_NORETURN();
  }
}

template <typename T>
struct OperandInfo {
  mojom::Operand::DataType type;
  std::vector<uint32_t> dimensions;
  std::vector<T> values;
#if BUILDFLAG(IS_MAC)
  OperandInfo<int32_t> ToInt32() {
    return OperandInfo<int32_t>{
        .type = mojom::Operand::DataType::kInt32,
        .dimensions = dimensions,
        .values = std::vector<int32_t>(values.begin(), values.end())};
  }
#endif  // BUILDFLAG(IS_MAC)
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

#if BUILDFLAG(IS_WIN)
class WebNNGraphImplBackendTest : public dml::TestBase {
 public:
  WebNNGraphImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}

  void SetUp() override;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<dml::Adapter> adapter_;
};

void WebNNGraphImplBackendTest::SetUp() {
  SKIP_TEST_IF(!dml::UseGPUInTests());

  dml::Adapter::EnableDebugLayerForTesting();
  auto adapter_creation_result = dml::Adapter::GetInstanceForTesting();
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
       {"BuildSingleOperatorBatchNormalization", DML_FEATURE_LEVEL_3_1},
       {"FuseStandaloneActivationIntoBatchNormalization",
        DML_FEATURE_LEVEL_3_1},
       // DML_OPERATOR_SLICE support for dimensions other than 4 or 5 was
       // introduced in DML_FEATURE_LEVEL_3_0.
       {"BuildAndComputeSliceOperator", DML_FEATURE_LEVEL_3_0},
       // DML_ACTIVATION_SOFTMAX_OPERATOR_DESC support for 2 dimensions was
       // introduced in DML_FEATURE_LEVEL_3_0.
       {"BuildAndComputeSingleOperatorSoftmax", DML_FEATURE_LEVEL_3_0},
       // DML_GATHER_OPERATOR_DESC support for 1~8 dimensions was introduced in
       // DML_FEATURE_LEVEL_3_0.
       {"BuildAndComputeSingleOperatorGather", DML_FEATURE_LEVEL_3_0},
       // DML_GEMM_OPERATOR_DESC support for 2 dimensions was introduced in
       // DML_FEATURE_LEVEL_4_0.
       {"BuildSingleOperatorGemmOnNpu", DML_FEATURE_LEVEL_4_0},
       {"BuildSingleOperatorGemmOnGpu", DML_FEATURE_LEVEL_4_0},
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
      "BuildAndComputeSingleOperatorCast",
      "BuildAndComputeSingleOperatorClamp",
      "BuildAndComputeSingleOperatorConcat",
      "BuildAndComputeConcatWithConstants",
      "BuildAndComputeSingleOperatorElementWiseBinary",
      "BuildAndComputeSingleOperatorAveragePool2d",
      "BuildAndComputeSingleOperatorL2Pool2d",
      "BuildAndComputeSingleOperatorMaxPool2d",
      "BuildAndComputeSingleOperatorRelu",
      "BuildAndComputeSingleOperatorResample2d",
      "BuildAndComputeSingleOperatorSigmoid",
      "BuildAndComputeSingleOperatorSoftsign",
      "BuildAndComputeSingleOperatorTanh",
      "BuildAndComputeSingleOperatorTranspose",
      "BuildAndComputeGraphWithTwoTranspose",
  });
  if (!kSupportedTests.contains(current_test_name)) {
    GTEST_SKIP() << "Skipping test because the operator is not yet supported.";
  }
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(WEBNN_USE_TFLITE)
class WebNNGraphImplBackendTest : public testing::Test {
 public:
  WebNNGraphImplBackendTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {
#if BUILDFLAG(IS_CHROMEOS)
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
#endif
  }

  void SetUp() override;

#if BUILDFLAG(IS_CHROMEOS)
  template <typename DataType>
  void SetComputeResult(std::string output_name,
                        std::vector<DataType> output_data) {
    base::flat_map<std::string, std::vector<uint8_t>> output_tensors;
    auto output_data_in_byte = base::as_bytes(base::make_span(output_data));
    output_tensors[output_name] = std::vector<uint8_t>(
        output_data_in_byte.begin(), output_data_in_byte.end());
    fake_service_connection_.SetOutputWebPlatformModelCompute(output_tensors);
  }
#endif

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
#endif
};

void WebNNGraphImplBackendTest::SetUp() {
  const std::string_view current_test_name =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  static auto kSupportedTests = base::MakeFixedFlatSet<std::string_view>({
      "BuildAndComputeSingleOperatorElementWiseBinary",
      "BuildAndComputeSingleOperatorElementWiseUnary",
      "BuildAndComputeSingleOperatorConcat",
      "BuildAndComputeConcatWithConstants",
  });
  if (!kSupportedTests.contains(current_test_name)) {
    GTEST_SKIP() << "Skipping test because the operator is not yet supported.";
  }
}
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

template <typename T>
struct ArgMinMaxTester {
  OperandInfo<T> input;
  std::vector<uint32_t> axes;
  bool keep_dimensions = false;
  bool select_last_index = false;
  mojom::ArgMinMax::Kind kind;
  OperandInfo<int64_t> output;

  void Test() {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildArgMinMax(kind, input_operand_id, output_operand_id, axes,
                           keep_dimensions, select_last_index);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual<int64_t>(std::move(named_outputs["output"]), output);
  }
};

// Test building and computing a graph with single operator ArgMinMax.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorArgMinMax) {
  // Test argMax with axes = {0} and select_last_index = false.
  {
    ArgMinMaxTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                     .dimensions = {2, 3},
                                     .values = {1, 2, 3, 4, 5, 6}},
                           .axes = {0},
                           .keep_dimensions = true,
                           .select_last_index = false,
                           .kind = mojom::ArgMinMax::Kind::kMax,
                           .output = {.type = mojom::Operand::DataType::kInt64,
                                      .dimensions = {1, 3},
                                      .values = {1, 1, 1}}}
        .Test();
  }
  // Test argMax with axes = {0, 1} and select_last_index = false. The index is
  // into the flattened array: [1, 2, 3, 4, 5, 6].
  {
    ArgMinMaxTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                     .dimensions = {2, 3},
                                     .values = {1, 2, 3, 4, 5, 6}},
                           .axes = {0, 1},
                           .keep_dimensions = true,
                           .select_last_index = false,
                           .kind = mojom::ArgMinMax::Kind::kMax,
                           .output = {.type = mojom::Operand::DataType::kInt64,
                                      .dimensions = {1, 1},
                                      .values = {5}}}
        .Test();
  }
  // Test argMax with axes = {1} and select_last_index = false.
  {
    ArgMinMaxTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                     .dimensions = {3, 3},
                                     .values = {1, 2, 3, 4, 3, 4, 3, 2, 1}},
                           .axes = {1},
                           .keep_dimensions = true,
                           .select_last_index = false,
                           .kind = mojom::ArgMinMax::Kind::kMax,
                           .output = {.type = mojom::Operand::DataType::kInt64,
                                      .dimensions = {3, 1},
                                      .values = {2, 0, 0}}}
        .Test();
  }
  // Test argMax with axes = {1}, keep_dimensions = false and select_last_index
  // = true.
  {
    ArgMinMaxTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                     .dimensions = {3, 3},
                                     .values = {1, 2, 3, 4, 3, 4, 3, 2, 1}},
                           .axes = {1},
                           .keep_dimensions = false,
                           .select_last_index = true,
                           .kind = mojom::ArgMinMax::Kind::kMax,
                           .output = {.type = mojom::Operand::DataType::kInt64,
                                      .dimensions = {3},
                                      .values = {2, 2, 0}}}
        .Test();
  }
  // Test argMin with axes = {1} and select_last_index = false.
  {
    ArgMinMaxTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                     .dimensions = {3, 3},
                                     .values = {1, 2, 3, 4, 3, 4, 3, 2, 1}},
                           .axes = {1},
                           .keep_dimensions = true,
                           .select_last_index = false,
                           .kind = mojom::ArgMinMax::Kind::kMin,
                           .output = {.type = mojom::Operand::DataType::kInt64,
                                      .dimensions = {3, 1},
                                      .values = {0, 1, 2}}}
        .Test();
  }
  // Test argMin with axes = {1, 2} and select_last_index = false. The indexes
  // are into the partially flattened array: [[ 1, 2, 3, 4] ], [1, 2, 3, 4]].
  {
    ArgMinMaxTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                     .dimensions = {2, 2, 2},
                                     .values = {1, 2, 3, 4, 1, 2, 3, 4}},
                           .axes = {1, 2},
                           .keep_dimensions = true,
                           .select_last_index = false,
                           .kind = mojom::ArgMinMax::Kind::kMin,
                           .output = {.type = mojom::Operand::DataType::kInt64,
                                      .dimensions = {2, 1, 1},
                                      .values = {0, 0}}}
        .Test();
  }
  // Test argMin with axes = {0, 2} and select_last_index = true. The indexes
  // are into the partially flattened array: [[1, 2, 1, 2], [3, 4, 3, 4]].
  {
    ArgMinMaxTester<float>{.input = {.type = mojom::Operand::DataType::kFloat32,
                                     .dimensions = {2, 2, 2},
                                     .values = {1, 2, 3, 4, 1, 2, 3, 4}},
                           .axes = {0, 2},
                           .keep_dimensions = false,
                           .select_last_index = true,
                           .kind = mojom::ArgMinMax::Kind::kMin,
                           .output = {.type = mojom::Operand::DataType::kInt64,
                                      .dimensions = {2},
                                      .values = {2, 2}}}
        .Test();
  }
  // Test argMin with axes = {0}, keep_dimensions = false and select_last_index
  // = true.
  {
    ArgMinMaxTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {3, 3},
                  .values = {1, 2, 3, 4, 3, 4, 3, 2, 1}},
        .axes = {0},
        .keep_dimensions = false,
        .select_last_index = true,
        .kind = mojom::ArgMinMax::Kind::kMin,
        .output = {.type = mojom::Operand::DataType::kInt64,
                   .dimensions = {3},
                   .values = {0, 2, 2}}}
        .Test();
  }
}

struct ClampAttributes {
  float min_value;
  float max_value;
};

struct Activation {
  mojom::Activation::Tag kind;
  std::optional<ClampAttributes> clamp_attributes;
  std::optional<float> elu_alpha;
  std::optional<float> hard_sigmoid_alpha;
  std::optional<float> hard_sigmoid_beta;
  std::optional<float> leaky_relu_alpha;
  std::optional<float> linear_alpha;
  std::optional<float> linear_beta;
  std::optional<float> softplus_steepness;
};

void BuildStandaloneActivation(GraphInfoBuilder& builder,
                               const Activation& activation,
                               uint64_t input_operand_id,
                               uint64_t output_operand_id) {
  switch (activation.kind) {
    case mojom::Activation::Tag::kElu: {
      CHECK(activation.elu_alpha.has_value());
      builder.BuildElu(input_operand_id, output_operand_id,
                       activation.elu_alpha.value());
      return;
    }
    case mojom::Activation::Tag::kHardSigmoid: {
      CHECK(activation.hard_sigmoid_alpha.has_value());
      CHECK(activation.hard_sigmoid_beta.has_value());
      builder.BuildHardSigmoid(input_operand_id, output_operand_id,
                               activation.hard_sigmoid_alpha.value(),
                               activation.hard_sigmoid_beta.value());
      return;
    }
    case mojom::Activation::Tag::kLeakyRelu: {
      CHECK(activation.leaky_relu_alpha.has_value());
      builder.BuildLeakyRelu(input_operand_id, output_operand_id,
                             activation.leaky_relu_alpha.value());
      return;
    }
    case mojom::Activation::Tag::kLinear: {
      CHECK(activation.linear_alpha.has_value());
      CHECK(activation.linear_beta.has_value());
      builder.BuildLinear(input_operand_id, output_operand_id,
                          activation.linear_alpha.value(),
                          activation.linear_beta.value());
      return;
    }
    case mojom::Activation::Tag::kRelu:
      builder.BuildRelu(input_operand_id, output_operand_id);
      return;
    case mojom::Activation::Tag::kSigmoid:
      builder.BuildSigmoid(input_operand_id, output_operand_id);
      return;
    case mojom::Activation::Tag::kSoftplus: {
      CHECK(activation.softplus_steepness.has_value());
      builder.BuildSoftplus(input_operand_id, output_operand_id,
                            activation.softplus_steepness.value());
      return;
    }
    case mojom::Activation::Tag::kSoftsign:
      builder.BuildSoftsign(input_operand_id, output_operand_id);
      return;
    case mojom::Activation::Tag::kTanh:
      builder.BuildTanh(input_operand_id, output_operand_id);
      return;
    default:
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
    std::optional<Activation> activation;
  };
  BatchNormalizationAttributes attributes;
  OperandInfo<T> output;

  void Test(BuildAndComputeExpectation expectation =
                BuildAndComputeExpectation::kSuccess) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t mean_operand_id =
        builder.BuildInput("mean", mean.dimensions, mean.type);
    uint64_t variance_operand_id =
        builder.BuildInput("variance", variance.dimensions, variance.type);
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

    builder.BuildBatchNormalization(input_operand_id, mean_operand_id,
                                    variance_operand_id, output_operand_id,
                                    std::move(attributes));

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    named_inputs.insert({"mean", VectorToBigBuffer(mean.values)});
    named_inputs.insert({"variance", VectorToBigBuffer(variance.values)});
    if (scale.has_value()) {
      named_inputs.insert({"scale", VectorToBigBuffer(scale->values)});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs, expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(std::move(named_outputs["output"]), output);
    }
  }

  void TestFusingStandaloneActivation(const Activation& activation) {
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
    BuildStandaloneActivation(builder, activation, intermediate_operand_id,
                              output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    named_inputs.insert({"mean", VectorToBigBuffer(mean.values)});
    named_inputs.insert({"variance", VectorToBigBuffer(variance.values)});
    if (scale.has_value()) {
      named_inputs.insert({"scale", VectorToBigBuffer(scale->values)});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }
};

// Test building and computing a graph of fusing a standalone activation into
// batchNormalization automatically.
TEST_F(WebNNGraphImplBackendTest,
       FuseStandaloneActivationIntoBatchNormalization) {
  {  // Test batchNormalization with 4-D input, default axis and activation =
    // linear.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-8.999950000374997, 1, 10.999950000374997,
                              -1.2474078892909666, 11, 23.24740788929097}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kLinear,
                       .linear_alpha = 10,
                       .linear_beta = 1});
  }
  {
    // Test batchNormalization with 4-D input with activation = hardsigmoid.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {1, 1, 1, 1, 1, 1}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kHardSigmoid,
                       .hard_sigmoid_alpha = 1,
                       .hard_sigmoid_beta = 3});
  }
  {
    // Test batchNormalization with 4-D input with activation = relu.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {0, 0, 0.9999950000374997, 0, 1,
                              2.224740788929097}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kRelu});
  }
  {
    // Test batchNormalization with 4-D input with activation = softplus.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-100, -50, 100, 101, 102, 103}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1, 4}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1, 2}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.epsilon = 0},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {0, 0, 100, 99, 100, 101}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kSoftplus,
                       .softplus_steepness = 3.0});
  }
  {
    // Test batchNormalization with 1-D input with activation = softsign.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2},
                  .values = {-1, 1}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {-1, 1}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.axis = 0},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2},
                   .values = {0, 0.5}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kSoftsign});
  }
}

// Test building and computing a graph with single operator batchNormalization.
TEST_F(WebNNGraphImplBackendTest, BuildSingleOperatorBatchNormalization) {
  {
    // Test batchNormalization with 4-D input with default axis.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-0.9999950000374997, 0, 0.9999950000374997,
                              -0.22474078892909666, 1, 2.224740788929097}}}
        .Test();
  }
  {  // Test batchNormalization with 4-D input, default axis and activation =
    // linear.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.activation =
                           Activation{.kind = mojom::Activation::Tag::kLinear,
                                      .linear_alpha = 10,
                                      .linear_beta = 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-8.999950000374997, 1, 10.999950000374997,
                              -1.2474078892909666, 11, 23.24740788929097}}}
        .Test();
  }
  {
    // Test batchNormalization with 4-D input with activation = hardsigmoid.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.activation =
                           Activation{
                               .kind = mojom::Activation::Tag::kHardSigmoid,
                               .hard_sigmoid_alpha = 1,
                               .hard_sigmoid_beta = 3}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {1, 1, 1, 1, 1, 1}}}
        .Test();
  }
  {
    // Test batchNormalization with 4-D input with activation = relu.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.activation =
                           Activation{.kind = mojom::Activation::Tag::kRelu}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {0, 0, 0.9999950000374997, 0, 1,
                              2.224740788929097}}}
        .Test();
  }
  {
    // Test batchNormalization with 4-D input with activation = softplus.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-100, -50, 100, 101, 102, 103}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1, 4}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1, 2}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.epsilon = 0,
                       .activation =
                           Activation{.kind = mojom::Activation::Tag::kSoftplus,
                                      .softplus_steepness = 3.0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {0, 0, 100, 99, 100, 101}}}
        .Test();
  }
  {
    // Test batchNormalization with 1-D input with activation = softsign.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2},
                  .values = {-1, 1}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {-1, 1}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.axis = 0,
                       .activation =
                           Activation{.kind =
                                          mojom::Activation::Tag::kSoftsign}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2},
                   .values = {0, 0.5}}}
        .Test();
  }
  {
    // Test batchNormalization with 4-D input with axis = 3.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 3, 2},
                  .values = {-1, 2, 0, 3, 1, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {0, 3}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.axis = 3},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 2},
                   .values = {-0.9999950000374997, -0.22474078892909666, 0, 1,
                              0.9999950000374997, 2.224740788929097}}}
        .Test();
  }
  {
    // Test batchNormalization with 1-D input with axis = 0.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2},
                  .values = {-1, 1}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {-1, 1}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.axis = 0},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2},
                   .values = {0, 1}}}
        .Test();
  }
  {
    // Test batchNormalization with 3-D input with axis = 2, epsilon = 1e-3.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {3},
                 .values = {0, 3, 6}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {3},
                     .values = {1.0, 1.5, 2.0}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {3},
                                    .values = {1.0, 1.5, 2.0}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {3},
                                   .values = {0, 1, 2}},
        .attributes = {.axis = 2, .epsilon = 1e-3},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 1, 3},
                   .values =
                       {
                           -0.9995003746877732,
                           -2.6730104813358024,
                           -5.069300707549023,
                           1.9990007493755464,
                           1,
                           -0.8277202830196093,
                       }}}
        .Test();
  }
  {
    // Test batchNormalization with 1-D input with axis = 0 when scale operand
    // is missing.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2},
                  .values = {-1, 1}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {-1, 1}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0, 1}},
        .attributes = {.axis = 0},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2},
                   .values = {0, 1}}}
        .Test();
  }
  {
    // Test batchNormalization with 1-D input with axis = 0 when bias operand is
    // missing.
    BatchNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2},
                  .values = {-1, 1}},
        .mean = {.type = mojom::Operand::DataType::kFloat32,
                 .dimensions = {2},
                 .values = {-1, 1}},
        .variance = {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {2},
                     .values = {1.0, 1.5}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {1.0, 1.5}},
        .attributes = {.axis = 0},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2},
                   .values = {0, 0}}}
        .Test();
  }
  {
    // Test batchNormalization with 1-D input with axis = 0 and float16 data
    // type when scale and bias operands are both missing.
    BatchNormalizationTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {2},
                  .values = Float16FromFloat32({-1, 1})},
        .mean = {.type = mojom::Operand::DataType::kFloat16,
                 .dimensions = {2},
                 .values = Float16FromFloat32({-1, 1})},
        .variance = {.type = mojom::Operand::DataType::kFloat16,
                     .dimensions = {2},
                     .values = Float16FromFloat32({1.0, 1.5})},
        .attributes = {.axis = 0},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {2},
                   .values = Float16FromFloat32({0, 0})}}
        .Test();
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
    std::optional<Activation> activation;
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

    std::optional<uint64_t> bias_operand_id;
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

  void TestFusingStandaloneActivation(const Activation& activation) {
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
    BuildStandaloneActivation(builder, activation, conv2d_output_operand_id,
                              output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;

    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
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
                               .values = {-5}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {-0.7946096424007316, -0.7853474888890126,
                              -0.7601703453057089, -0.6917317734107099,
                              -0.5056964470628461, 0, 1, 2, 3}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kElu, .elu_alpha = 0.8});
  }
  // Test conv2d with NCHW layout, float 32 data type, bias and fusing with
  // leakyRelu activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
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
                               .values = {-60}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = {-0.3, -0.12, 21, 30}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kLeakyRelu,
                       .leaky_relu_alpha = 0.02});
  }
  // Test conv2d with NCHW layout, float 32 data type, fusing with bias and
  // linear activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
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
                   .values = {1.13, 1.22, 1.28, 1.34, 1.25, 1.34, 1.55,
                              1.64, 1.73, 1.52, 1.64, 2,    2.09, 2.18,
                              1.82, 1.94, 2.45, 2.54, 2.63, 2.12, 1.73,
                              2.12, 2.18, 2.24, 1.85}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kLinear,
                       .linear_alpha = 0.01,
                       .linear_beta = 1});
  }
  // Test conv2d with NHWC layout, float 32 data type, fusing with bias and relu
  // activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 5, 5, 1},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 3, 3, 1},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout = mojom::InputOperandLayout::kChannelsLast,
                       .bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {-100}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 5, 1},
                   .values = {0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kRelu});
  }
  // Test conv2d with NHWC layout, float 16 data type, fusing with bias and relu
  // activation.
  {
    Conv2dTester<float16>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 5, 5, 1},
                  .values = Float16FromFloat32(
                      {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})},
        .filter = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 3, 3, 1},
                   .values = Float16FromFloat32(std::vector<float>(9, 1))},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout = mojom::InputOperandLayout::kChannelsLast,
                       .bias =
                           OperandInfo<float16>{
                               .type = mojom::Operand::DataType::kFloat16,
                               .dimensions = {1},
                               .values = Float16FromFloat32({-100})}},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 5, 5, 1},
                   .values = {0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kRelu});
  }
  // Test conv2d with NCHW layout, fusing with hardSigmoid activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
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
                   .values = {0,    0,    0, 0,    0,    0,    0, 0,    0,
                              0,    0,    0, 0.09, 0.18, 0,    0, 0.45, 0.54,
                              0.63, 0.12, 0, 0.12, 0.18, 0.24, 0}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kHardSigmoid,
                       .hard_sigmoid_alpha = 0.01,
                       .hard_sigmoid_beta = -1});
  }
  // Test conv2d with NCHW layout, fusing with sigmoid activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
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
                           mojom::InputOperandLayout::kChannelsFirst},
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
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kSigmoid});
  }
  // Test conv2d with NCHW layout, float 32 data type, bias and fusing with
  // softplus activation.
  {
    Conv2dTester<float>{.type = mojom::Conv2d::Kind::kDirect,
                        .input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {1, 1, 2, 2},
                                  .values = {5, 6, 7, 8}},
                        .filter = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 1, 1, 1},
                                   .values = {1}},
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 1, 2, 2},
                                   .values = {5, 6, 7, 8}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kSoftplus,
                       .softplus_steepness = 8.0});
  }
  // Test conv2d with NCHW layout, float 32 data type, fusing with softsign
  // activation.
  {
    Conv2dTester<float>{.type = mojom::Conv2d::Kind::kDirect,
                        .input = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {1, 1, 3, 3},
                                  .values = {-3, -2, -1, -4, 0, 2, 1, 3, 4}},
                        .filter = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 1, 2, 2},
                                   .values = std::vector<float>(4, 1)},
                        .output = {.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 1, 2, 2},
                                   .values = {-0.9, -0.5, 0, 0.9}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kSoftsign});
  }
  // Test conv2d with NCHW layout, fusing with tanh activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 5, 5},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = std::vector<float>(9, 0.05)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout =
                           mojom::InputOperandLayout::kChannelsFirst},
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
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kTanh});
  }
}

// Test building and computing a graph with single operator conv2d.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorConv2d) {
  // Test conv2d with NCHW input layout, OIHW filter layout, padding = {1, 1, 1,
  // 1}, float 32 data type, fusing with bias.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
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
  // Test conv2d with NCHW input layout, OIHW filter layout, padding = {1, 1, 1,
  // 1}, float 16 data type, fusing with bias.
  {
    Conv2dTester<float16>{
        .type = mojom::Conv2d::Kind::kDirect,
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
  // Test conv2d with NCHW input layout, OIHW filter layout, padding = {1, 1, 1,
  // 1}, float 32 data type, without bias.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
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
  // Test conv2d with NHWC input layout, OHWI filter layout, padding = {1, 1, 1,
  // 1}, float 32 data type, without bias.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 5, 5, 1},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 3, 3, 1},
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
  // Test conv2d with NHWC input layout, OHWI filter layout, float 16 data type,
  // padding = {1, 1, 1, 1}, without bias.
  {
    Conv2dTester<float16>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 5, 5, 1},
                  .values = Float16FromFloat32(
                      {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})},
        .filter = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 3, 3, 1},
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
  // Test depthwise conv2d with NHWC input layout, IHWO filter layout, float 32
  // data type, groups = 2.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 2, 2},
                  .values = {0, 1, 2, 3, 4, 5, 6, 7}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 2, 2},
                   .values = std::vector<float>(8, 1)},
        .attributes = {.groups = 2,
                       .input_layout =
                           mojom::InputOperandLayout::kChannelsLast},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 1, 2},
                   .values = {12, 16}}}
        .Test();
  }
  // Test conv2d with NCHW input layout, OIHW filter layout, float 32 data type,
  // bias and fusing with elu activation.
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
                       .activation =
                           Activation{.kind = mojom::Activation::Tag::kElu,
                                      .elu_alpha = 0.8}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 3, 3},
                   .values = {-0.7946096424007316, -0.7853474888890126,
                              -0.7601703453057089, -0.6917317734107099,
                              -0.5056964470628461, 0, 1, 2, 3}}}
        .Test();
  }
  // Test conv2d with NCHW input layout, OIHW filter layout, float 32 data type,
  // bias and fusing with leakyRelu activation.
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
                       .activation =
                           Activation{
                               .kind = mojom::Activation::Tag::kLeakyRelu,
                               .leaky_relu_alpha = 0.02}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = {-0.3, -0.12, 21, 30}}}
        .Test();
  }
  // Test conv2d with NCHW input layout, OIHW filter layout, float 32 data type,
  // fusing with bias and linear activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
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
                               .values = {1}},
                       .activation =
                           Activation{.kind = mojom::Activation::Tag::kLinear,
                                      .linear_alpha = 0.01,
                                      .linear_beta = 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 5, 5},
                   .values = {1.13, 1.22, 1.28, 1.34, 1.25, 1.34, 1.55,
                              1.64, 1.73, 1.52, 1.64, 2,    2.09, 2.18,
                              1.82, 1.94, 2.45, 2.54, 2.63, 2.12, 1.73,
                              2.12, 2.18, 2.24, 1.85}}}
        .Test();
  }
  // Test conv2d with NHWC input layout, OHWI filter layout, float 32 data type,
  // fusing with bias and relu activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 5, 5, 1},
                  .values = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 3, 3, 1},
                   .values = std::vector<float>(9, 1)},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout = mojom::InputOperandLayout::kChannelsLast,
                       .bias =
                           OperandInfo<float>{
                               .type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {1},
                               .values = {-100}},
                       .activation =
                           Activation{.kind = mojom::Activation::Tag::kRelu}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 5, 5, 1},
                   .values = {0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0}}}
        .Test();
  }
  // Test conv2d with NHWC input layout, OHWI filter layout, float 16 data type,
  // fusing with bias and relu activation.
  {
    Conv2dTester<float16>{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 5, 5, 1},
                  .values = Float16FromFloat32(
                      {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})},
        .filter = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 3, 3, 1},
                   .values = Float16FromFloat32(std::vector<float>(9, 1))},
        .attributes = {.padding = {1, 1, 1, 1},
                       .input_layout = mojom::InputOperandLayout::kChannelsLast,
                       .bias =
                           OperandInfo<float16>{
                               .type = mojom::Operand::DataType::kFloat16,
                               .dimensions = {1},
                               .values = Float16FromFloat32({-100})},
                       .activation =
                           Activation{.kind = mojom::Activation::Tag::kRelu}},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 5, 5, 1},
                   .values = {0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                              17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0}}}
        .Test();
  }
  // Test conv2d with NCHW input layout, OIHW filter layout, fusing with
  // hardSigmoid activation.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kDirect,
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
                               .values = {1}},
                       .activation =
                           Activation{
                               .kind = mojom::Activation::Tag::kHardSigmoid,
                               .hard_sigmoid_alpha = 0.01,
                               .hard_sigmoid_beta = -1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 5, 5},
                   .values = {0,    0,    0, 0,    0,    0,    0, 0,    0,
                              0,    0,    0, 0.09, 0.18, 0,    0, 0.45, 0.54,
                              0.63, 0.12, 0, 0.12, 0.18, 0.24, 0}}}
        .Test();
  }
  // Test conv2d with NCHW input layout, OIHW filter layout, fusing with sigmoid
  // activation.
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
                       .activation =
                           Activation{.kind =
                                          mojom::Activation::Tag::kSigmoid}},
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
  // Test conv2d with NCHW input layout, OIHW filter layout, float 32 data type,
  // bias and fusing with softplus activation.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 2, 2},
                  .values = {5, 6, 7, 8}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 1, 1},
                   .values = {1}},
        .attributes = {.activation =
                           Activation{.kind = mojom::Activation::Tag::kSoftplus,
                                      .softplus_steepness = 8.0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = {5, 6, 7, 8}}}
        .Test();
  }
  // Test conv2d with NCHW input layout, OIHW filter layout, float 32 data type,
  // fusing with softsign activation.
  {
    Conv2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 3, 3},
                  .values = {-3, -2, -1, -4, 0, 2, 1, 3, 4}},
        .filter = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = std::vector<float>(4, 1)},
        .attributes = {.activation =
                           Activation{.kind =
                                          mojom::Activation::Tag::kSoftsign}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = {-0.9, -0.5, 0, 0.9}}}
        .Test();
  }
  // Test conv2d with NCHW input layout, OIHW filter layout, fusing with tanh
  // activation.
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
                       .activation =
                           Activation{.kind = mojom::Activation::Tag::kTanh}},
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

// Test building and computing a graph with single operator convTranspose2d.
TEST_F(WebNNGraphImplBackendTest,
       BuildAndComputeSingleOperatorConvTranspose2d) {
  // Test convTranspose2d with default attributes.
  {
    Conv2dTester<float>{
        .type = mojom::Conv2d::Kind::kTransposed,
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
        .type = mojom::Conv2d::Kind::kTransposed,
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
    Conv2dTester<float>{.type = mojom::Conv2d::Kind::kTransposed,
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
        .type = mojom::Conv2d::Kind::kTransposed,
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
        .type = mojom::Conv2d::Kind::kTransposed,
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
        .type = mojom::Conv2d::Kind::kTransposed,
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
        .type = mojom::Conv2d::Kind::kTransposed,
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
        .type = mojom::Conv2d::Kind::kTransposed,
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
                       .activation =
                           Activation{.kind = mojom::Activation::Tag::kRelu}},
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
  void Test(WebNNGraphImplBackendTest& helper) {
#if BUILDFLAG(IS_CHROMEOS)
    helper.SetComputeResult("output", output.values);
#endif

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t lhs_operand_id =
        builder.BuildInput("lhs", lhs.dimensions, lhs.type);
    uint64_t rhs_operand_id =
        builder.BuildInput("rhs", rhs.dimensions, rhs.type);
    auto graph_output_type = output.type;
#if BUILDFLAG(IS_MAC)
    if (output.type == mojom::Operand::DataType::kUint8) {
      // macOS only supports FP16,FP32,DOUBLE,INT32 as outputs of graph.
      // For testing, we cast the output of the element-wise logical
      // operators to Int32 and set the graph output to Int32.
      graph_output_type = mojom::Operand::DataType::kInt32;
    }
#endif  // BUILD_FLAG(IS_MAC)
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, graph_output_type);
    uint64_t element_wise_binary_output_operand_id = output_operand_id;
#if BUILDFLAG(IS_MAC)
    if (output.type == mojom::Operand::DataType::kUint8) {
      element_wise_binary_output_operand_id = builder.BuildIntermediateOperand(
          output.dimensions, mojom::Operand::DataType::kUint8);
    }
#endif  // BUILD_FLAG(IS_MAC)
    builder.BuildElementWiseBinary(kind, lhs_operand_id, rhs_operand_id,
                                   element_wise_binary_output_operand_id);
#if BUILDFLAG(IS_MAC)
    if (output.type == mojom::Operand::DataType::kUint8) {
      builder.BuildElementWiseUnary(mojom::ElementWiseUnary::Kind::kCast,
                                    element_wise_binary_output_operand_id,
                                    output_operand_id);
    }
#endif  // BUILD_FLAG(IS_MAC)

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"lhs", VectorToBigBuffer(lhs.values)});
    named_inputs.insert({"rhs", VectorToBigBuffer(rhs.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

#if BUILDFLAG(IS_MAC)
    if (output.type == mojom::Operand::DataType::kUint8) {
      VerifyIsEqual(std::move(named_outputs["output"]), output.ToInt32());
      return;
    }
#endif  // BUILD_FLAG(IS_MAC)

    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }

  void TestFusingStandaloneActivation(const Activation& activation) {
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
    BuildStandaloneActivation(builder, activation, intermediate_operand_id,
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

// Test building and computing a graph of fusing a standalone activation
// into elementwise binary add automatically.
TEST_F(WebNNGraphImplBackendTest,
       FuseStandaloneActivationIntoElementWiseBinaryAdd) {
  // Test add with linear activation.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {0, 5.1, 4, 3, 2, 0}},
        .kind = mojom::ElementWiseBinary::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {11, 72, 71, 71, 71, 61}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kLinear,
                       .linear_alpha = 10,
                       .linear_beta = 1});
  }
  // Test add with relu activation.
  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {1, 2, 3, 4, 5, 6}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-6, 5, 4, 3, 2, -7}},
        .kind = mojom::ElementWiseBinary::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 7, 7, 7, 7, 0}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kRelu});
  }
}

// Test building and computing a graph with single operator element-wise
// binary.
TEST_F(WebNNGraphImplBackendTest,
       BuildAndComputeSingleOperatorElementWiseBinary) {
  // Test building and computing a graph with single operator add for 0-D
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator add.
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator add using
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator add using
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator div.
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator div using
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator max.
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator max using
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator min.
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator min using
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator mul.
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator mul using
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator pow.
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator pow using
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator sub.
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
        .Test(*this);
  }
  // Test building and computing a graph with single operator sub using
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
        .Test(*this);
  }
  // TODO(https://crbug.com/326356909): Enable these tests when using TFLite,
  // after adding support for other binary operators.
#if !BUILDFLAG(WEBNN_USE_TFLITE)
  // Test building and computing a graph with single operator equal.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, -3, 4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 3, 4, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kEqual,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 0, 0, 1, 0, 0}}}
        .Test(*this);
  }
  // Test building and computing a graph with single operator equal using
  // broadcasting.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, 3, 4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::ElementWiseBinary::Kind::kEqual,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 1, 0, 0, 0, 0}}}
        .Test(*this);
  }
  // Test building and computing a graph with single operator greater.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, -2, 3, 4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, -5, 3, 4, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kGreater,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 1, 0, 0, 1, 1}}}
        .Test(*this);
  }
  // Test building and computing a graph with single operator greater using
  // broadcasting.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, 3, 4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::ElementWiseBinary::Kind::kGreater,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 0, 1, 1, 1, 1}}}
        .Test(*this);
  }
  // Test building and computing graph with single operator greaterOrEqual.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, -3, 4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 3, 4, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kGreaterOrEqual,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 0, 0, 1, 1, 1}}}
        .Test(*this);
  }
  // Test building and computing a graph with single operator
  // greaterOrEqual using broadcasting.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, -3, 4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::ElementWiseBinary::Kind::kGreaterOrEqual,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {0, 1, 0, 1, 1, 1}}}
        .Test(*this);
  }
  // Test building and computing a graph with single operator lesser.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, 3, -4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, 3, 3, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kLesser,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 1, 0, 1, 0, 0}}}
        .Test(*this);
  }
  // Test building and computing a graph with single operator lesser using
  // broadcasting.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, -3, 4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::ElementWiseBinary::Kind::kLesser,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 0, 1, 0, 0, 0}}}
        .Test(*this);
  }
  // Test building and computing a graph with single operator lesserOrEqual.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, 3, 4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {6, 5, -3, 4, 2, 1}},
        .kind = mojom::ElementWiseBinary::Kind::kLesserOrEqual,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 1, 0, 1, 0, 0}}}
        .Test(*this);
  }
  // Test building and computing a graph with single operator lesserOrEqual
  // using broadcasting.
  {
    ElementWiseBinaryTester<float, uint8_t>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 2, 3, 1},
                .values = {-1, 2, -2, -4, 5,
                           std::numeric_limits<float>::infinity()}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1, 1, 1, 1},
                .values = {2}},
        .kind = mojom::ElementWiseBinary::Kind::kLesserOrEqual,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 1, 1, 1, 0, 0}}}
        .Test(*this);
  }
#endif  // !BUILDFLAG(WEBNN_USE_TFLITE)
}

template <typename T, typename O = T>
struct ElementWiseUnaryTester {
  OperandInfo<T> input;
  mojom::ElementWiseUnary::Kind kind;
  OperandInfo<O> output;
  void Test(WebNNGraphImplBackendTest& helper) {
#if BUILDFLAG(IS_CHROMEOS)
    helper.SetComputeResult("output", output.values);
#endif

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
                    named_outputs);
    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }
};

// Test building and computing a graph with element-wise unary operator.
TEST_F(WebNNGraphImplBackendTest,
       BuildAndComputeSingleOperatorElementWiseUnary) {
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

  // TODO(https://crbug.com/326356909): Enable these tests when using TFLite,
  // after adding support for other unary operators.
#if !BUILDFLAG(WEBNN_USE_TFLITE)
  {
    ElementWiseUnaryTester<uint8_t>{
        .input = {.type = mojom::Operand::DataType::kUint8,
                  .dimensions = {1, 2, 3, 1},
                  .values = {0, 2, 0, 4, 5, 255}},
        .kind = mojom::ElementWiseUnary::Kind::kLogicalNot,
        .output = {.type = mojom::Operand::DataType::kUint8,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 0, 1, 0, 0, 0}}}
        .Test(*this);
  }
  {
    ElementWiseUnaryTester<float>{
        .input = test_operand_info_float32_scalar,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_float32_scalar}
        .Test(*this);
  }
  {
    ElementWiseUnaryTester<float>{
        .input = test_operand_info_float32,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_float32}
        .Test(*this);
  }
  {
    ElementWiseUnaryTester<float16>{
        .input = test_operand_info_float16,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_float16}
        .Test(*this);
  }
  {
    ElementWiseUnaryTester<int32_t>{
        .input = test_operand_info_int32,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_int32}
        .Test(*this);
  }
  {
    ElementWiseUnaryTester<int8_t>{
        .input = test_operand_info_int8,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_int8}
        .Test(*this);
  }
  {
    ElementWiseUnaryTester<uint8_t>{
        .input = test_operand_info_uint8,
        .kind = mojom::ElementWiseUnary::Kind::kIdentity,
        .output = test_operand_info_uint8}
        .Test(*this);
  }
#endif  // !BUILDFLAG(WEBNN_USE_TFLITE)

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
        .Test(*this);
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
        .Test(*this);
  }

  // TODO(https://crbug.com/326356909): Enable these tests when using TFLite,
  // after adding support for float16 and other unary operators.
#if !BUILDFLAG(WEBNN_USE_TFLITE)
  {
    ElementWiseUnaryTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 3, 1},
                  .values = Float16FromFloat32({0, 4, 25, 16, 64, 49})},
        .kind = mojom::ElementWiseUnary::Kind::kSqrt,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 3, 1},
                   .values = Float16FromFloat32({0, 2, 5, 4, 8, 7})}}
        .Test(*this);
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
        .Test(*this);
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
        .Test(*this);
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
        .Test(*this);
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
        .Test(*this);
  }
#endif  // !BUILDFLAG(WEBNN_USE_TFLITE)

  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 3, 1},
                  .values = {-1, 4, -2, 16, -64, 0}},
        .kind = mojom::ElementWiseUnary::Kind::kAbs,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 3, 1},
                   .values = {1, 4, 2, 16, 64, 0}}}
        .Test(*this);
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
        .Test(*this);
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
        .Test(*this);
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
        .Test(*this);
  }

  // TODO(https://crbug.com/326356909): Enable these tests when using TFLite,
  // after adding support for other unary operators.
#if !BUILDFLAG(WEBNN_USE_TFLITE)
  {
    ElementWiseUnaryTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 3, 1},
                  .values = Float16FromFloat32({-1.1, 0, 1.1, -2.2, 0, 2.2})},
        .kind = mojom::ElementWiseUnary::Kind::kFloor,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 3, 1},
                   .values = Float16FromFloat32({-2, 0, 1, -3, 0, 2})}}
        .Test(*this);
  }
#endif  // !BUILDFLAG(WEBNN_USE_TFLITE)

  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {3},
                  .values = {0, 3, 10}},
        .kind = mojom::ElementWiseUnary::Kind::kLog,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3},
                   .values = {log(0.f), log(3.f), log(10.f)}}}
        .Test(*this);
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
        .Test(*this);
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
        .Test(*this);
  }

  // TODO(https://crbug.com/326356909): Enable these tests when using TFLite,
  // after adding support for other unary operators.
#if !BUILDFLAG(WEBNN_USE_TFLITE)
  {
    ElementWiseUnaryTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1, -2, 3, -4}},
        .kind = mojom::ElementWiseUnary::Kind::kTan,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {tan(1.f), tan(-2.f), tan(3.f), tan(-4.f)}}}
        .Test(*this);
  }
#endif  // !BUILDFLAG(WEBNN_USE_TFLITE)
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

TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorExpand) {
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

TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorCast) {
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
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<float, int32_t>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test(*this);
    }
// TODO(https://issues.chromium.org/41481333): Enable these tests on Mac,
// after adding support for testing uint32/int8/uint8 outputs.
#if !BUILDFLAG(IS_MAC)
    {
      ElementWiseUnaryTester<float, uint32_t>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<float, int8_t>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<float, uint8_t>{
          .input = test_operand_info_float32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test(*this);
    }
#endif  // !BUILDFLAG(IS_MAC)
  }
  // Test all combinations from float16 data type.
  {
    {
      ElementWiseUnaryTester<float16, float>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test(*this);
    }
    {
      ElementWiseUnaryTester<float16, int32_t>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test(*this);
    }
// TODO(https://issues.chromium.org/41481333): Enable these tests on Mac,
// after adding support for testing uint32/int8/uint8 outputs.
#if !BUILDFLAG(IS_MAC)
    {
      ElementWiseUnaryTester<float16, uint32_t>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<float16, int8_t>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<float16, uint8_t>{
          .input = test_operand_info_float16,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test(*this);
    }
#endif  // !BUILDFLAG(IS_MAC)
  }
  // Test all combinations from int32 data type.
  {
    {
      ElementWiseUnaryTester<int32_t, float>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<int32_t, float16>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test(*this);
    }
// TODO(https://issues.chromium.org/41481333): Enable these tests on Mac,
// after adding support for testing uint32/int8/uint8 outputs.
#if !BUILDFLAG(IS_MAC)
    {
      ElementWiseUnaryTester<int32_t, uint32_t>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<int32_t, int8_t>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<int32_t, uint8_t>{
          .input = test_operand_info_int32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test(*this);
    }
#endif  // !BUILDFLAG(IS_MAC)
  }
// TODO(https://issues.chromium.org/41481333): Enable these tests on Mac,
// after adding support for testing uint32/int8/uint8 inputs.
#if !BUILDFLAG(IS_MAC)
  // Test all combinations from uint32 data type.
  {
    {
      ElementWiseUnaryTester<uint32_t, float>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<uint32_t, float16>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<uint32_t, int32_t>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<uint32_t, int8_t>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<uint32_t, uint8_t>{
          .input = test_operand_info_uint32,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test(*this);
    }
  }
  // Test all combinations from int8_t data type.
  {
    {
      ElementWiseUnaryTester<int8_t, float>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<int8_t, float16>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<int8_t, int32_t>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<int8_t, uint32_t>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<int8_t, uint8_t>{
          .input = test_operand_info_int8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint8}
          .Test(*this);
    }
  }
  // Test all combinations from uint8_t data type.
  {
    {
      ElementWiseUnaryTester<uint8_t, float>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<uint8_t, float16>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_float16}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<uint8_t, int32_t>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<uint8_t, uint32_t>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_uint32}
          .Test(*this);
    }

    {
      ElementWiseUnaryTester<uint8_t, int8_t>{
          .input = test_operand_info_uint8,
          .kind = mojom::ElementWiseUnary::Kind::kCast,
          .output = test_operand_info_int8}
          .Test(*this);
    }
  }
#endif  // !BUILDFLAG(IS_MAC)
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

// Test building and computing a graph with single operator average
// pool2d.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorAveragePool2d) {
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
    // Test average pool2d with nhwc layout, float 32 data type.
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
    // Test average pool2d with nhwc layout, float 16 data type.
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

// Test building and computing a graph with single operator l2Pool2d.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorL2Pool2d) {
  {
    // Test l2Pool2d with nchw layout, float 32 data type.
    Pool2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 4, 2, 2},
                  .values = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4}},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {2, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Pool2d::Kind::kL2Pool2d,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 4, 1, 1},
                   .values = {2, 4, 6, 8}}}
        .Test();
  }
  {
    // Test l2Pool2d with nchw layout, float 16 data type.
    Pool2dTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 4, 2, 2},
                  .values = Float16FromFloat32(
                      {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4})},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {2, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Pool2d::Kind::kL2Pool2d,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 4, 1, 1},
                   .values = {2, 4, 6, 8}}}
        .Test();
  }
  {
    // Test l2Pool2d with nhwc layout, float 32 data type.
    Pool2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 2, 4},
                  .values = {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4}},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {2, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsLast},
        .kind = mojom::Pool2d::Kind::kL2Pool2d,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 1, 4},
                   .values = {2, 4, 6, 8}}}
        .Test();
  }
  {
    // Test l2Pool2d with nhwc layout, float 16 data type.
    Pool2dTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 2, 4},
                  .values = Float16FromFloat32(
                      {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4})},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {2, 1},
                       .dilations = {1, 1},
                       .layout = mojom::InputOperandLayout::kChannelsLast},
        .kind = mojom::Pool2d::Kind::kL2Pool2d,
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 1, 1, 4},
                   .values = {2, 4, 6, 8}}}
        .Test();
  }
}

// Test building and computing a graph with single operator max pool2d
// with nchw layout.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorMaxPool2d) {
  {
    // Test max pool2d with nchw layout, strides=1, padding=0, dilations={1,1}
    // and floor rounding.
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
  {
    // Test max pool2d with nchw layout, strides=1, padding=0, dilations={2,2}
    // and floor rounding.
    Pool2dTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 4, 4},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                             16}},
        .attributes = {.window_dimensions = {2, 2},
                       .padding = {0, 0, 0, 0},
                       .strides = {1, 1},
                       .dilations = {2, 2},
                       .layout = mojom::InputOperandLayout::kChannelsFirst},
        .kind = mojom::Pool2d::Kind::kMaxPool2d,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 2},
                   .values = {11, 12, 15, 16}}}
        .Test();
  }
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

TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorPrelu) {
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

TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSliceOperator) {
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

TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorSplit) {
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

// Test building and computing a graph with single operator pad.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorPad) {
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
  std::optional<float> clamp_min_value;
  std::optional<float> clamp_max_value;
  std::optional<float> hard_sigmoid_alpha;
  std::optional<float> hard_sigmoid_beta;
  std::optional<float> elu_alpha;
  std::optional<float> leaky_relu_alpha;
  std::optional<float> linear_alpha;
  std::optional<float> linear_beta;
  std::optional<float> softplus_steepness;
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
      case mojom::Operation::Tag::kSoftmax:
        builder.BuildSoftmax(input_operand_id, output_operand_id);
        break;
      case mojom::Operation::Tag::kSoftplus:
        CHECK(softplus_steepness);
        builder.BuildSoftplus(input_operand_id, output_operand_id,
                              softplus_steepness.value());
        break;
      case mojom::Operation::Tag::kSoftsign:
        builder.BuildSoftsign(input_operand_id, output_operand_id);
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
                    named_outputs, expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(std::move(named_outputs["output"]), output);
    }
  }
};

// Test building and computing a graph with single operator clamp.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorClamp) {
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

// Test building and computing a graph with single operator hardSigmoid.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorHardSigmoid) {
  {
    // Test hardSigmoid with default alpha = 0.2 and beta = 0.5.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kHardSigmoid,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-1, -2, 0, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 2, 1},
                   .values = {0.3, 0.1, 0.5, 0.9}}}
        .Test();
  }
  {
    // Test hardSigmoid for 4-D tensor input.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kHardSigmoid,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-1, -2, 0, 2}},
        .hard_sigmoid_alpha = 0.1,
        .hard_sigmoid_beta = 0.2,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 2, 1},
                   .values = {0.1, 0, 0.2, 0.4}}}
        .Test();
  }
  {
    // Test sigmoid for 0-D scalar input.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kHardSigmoid,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {},
                  .values = {24}},
        .hard_sigmoid_alpha = 0.1,
        .hard_sigmoid_beta = 3,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {},
                   .values = {1}}}
        .Test();
  }
}

// Test building and computing a graph with single operator hardSwish.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorHardSwish) {
  // Test hardSwish with a 0-D scalar input.
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kHardSwish,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {},
                  .values = {7.0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {},
                   .values = {7.0}}}
        .Test();
  }
  // Test hardSwish with a 4-D input.
  {
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kHardSwish,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 2, 2},
                  .values = {-6, -5, -4, -3, 0, 4, 5, 6}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 2, 2},
                   .values = {0, 0, 0, 0, 0, 4, 5, 6}}}
        .Test();
  }
}

// Test building and computing a graph with single operator sigmoid.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorSigmoid) {
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

// Test building and computing a graph with single operator softplus.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorSoftplus) {
  {
    // Test softplus with steepness = 1.0.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSoftplus,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 2, 3},
                  .values = {-100, -50, 40, 50, 100, 150}},
        .softplus_steepness = 1.0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 2, 3},
                   .values = {0, 0, 40, 50, 100, 150}}}
        .Test();
  }
  {
    // Test softplus with steepness = 5.0.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSoftplus,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-10, -8, 8, 10}},
        .softplus_steepness = 5.0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 2, 1},
                   .values = {0, 0, 8, 10}}}
        .Test();
  }
  {
    // Test softplus with steepness = 10.0.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSoftplus,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 3, 2, 1},
                  .values = {-10, -5, 7, 10, 15, 20}},
        .softplus_steepness = 10.0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 3, 2, 1},
                   .values = {0, 0, 7, 10, 15, 20}}}
        .Test();
  }
  {
    // Test graph creation failure when steepness < 1.0.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSoftplus,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 1, 1, 1},
                  .values = {200}},
        .softplus_steepness = 0.5,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 1, 1},
                   .values = {200}}}
        .Test(BuildAndComputeExpectation::kCreateGraphFailure);
  }
}

// Test building and computing a graph with single operator softsign.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorSoftsign) {
  {
    // Test softsign with a float32 input.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kSoftsign,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-9, -7, -4, -3, -1, 0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-0.9, -0.875, -0.8, -0.75, -0.5, 0}}}
        .Test();
  }
  {
    // Test softsign with a float16 input.
    UnaryOperatorTester<float16>{
        .tag = mojom::Operation::Tag::kSoftsign,
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 3, 1},
                  .values = Float16FromFloat32({0, 1, 3, 4, 7, 9})},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 3, 1},
                   .values =
                       Float16FromFloat32({0, 0.5, 0.75, 0.8, 0.875, 0.9})}}
        .Test();
  }
}

// Test building and computing a graph with single operator tanh.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorTanh) {
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

// Test building and computing a graph with single operator softmax.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorSoftmax) {
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

// Test building and computing a graph with single operator relu.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorRelu) {
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

// Test building and computing a graph with single operator elu.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorElu) {
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

// Test building and computing a graph with single operator leakyRelu.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorLeakyRelu) {
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

// Test building and computing a graph with single operator linear.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorLinear) {
  {
    // Test linear with a 3d input and alpha = 0.01, beta = 1.0.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kLinear,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2, 3},
                  .values = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}},
        .linear_alpha = 0.01,
        .linear_beta = 1.0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 3},
                   .values = {0.99, 1, 1.01, 1.02, 1.03, 1.04, 1.05, 1.06, 1.07,
                              1.08, 1.09, 1.1}}}
        .Test();
  }
  {
    // Test linear with a 2d input and alpha = 0.02, beta = 2.0.
    UnaryOperatorTester<float>{
        .tag = mojom::Operation::Tag::kLinear,
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .linear_alpha = 0.02,
        .linear_beta = 2.0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 3},
                   .values = {1.98, 2, 2.02, 2.04, 2.06, 2.08}}}
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

// Test building and computing a graph with single operator reshape.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorReshape) {
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

// Test building and computing a graph with two reshape operators
//    [input]
//       |
//    reshape1
//       |
//    reshape2
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTwoReshape) {
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

// Test building and computing a graph with two operators and two outputs
//      [input]
//       /   \
//  reshape   relu
//     |        |
// [output1] [output2]
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTwoOutputs) {
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

// Test building and computing a graph with single operator reduce.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorReduce) {
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

TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorGather) {
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
    // Test gather with 6-D input, 0-D indices and axis = 0 with data type
    // int32.
    GatherTester<float, int32_t>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 1, 1, 1, 1, 5},
                  // [[[[[[1 2 3 4  5]]]]]
                  //  [[[[[6 7 8 9 10]]]]]] with shape (2, 1, 1, 1, 1, 5)
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}},
        .indices = {.type = mojom::Operand::DataType::kInt32,
                    .dimensions = {},
                    .values = {1}},
        .axis = 0,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 1, 1, 1, 5},
                   // [[[[[6 7 8 9 10]]]]] with shape (1, 1, 1, 1, 5)
                   .values = {6, 7, 8, 9, 10}}}
        .Test();
  }
  {
    // Test gather with 3-D input, 0-D indices and axis = 1 with data type
    // int64.
    GatherTester<float, int64_t>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {3, 4, 2},
                  // [[[ 1  2]
                  //   [ 3  4]
                  //   [ 5  6]
                  //   [ 7  8]]
                  //  [[ 9 10]
                  //   [11 12]
                  //   [13 14]
                  //   [15 16]]
                  //  [[17 18]
                  //   [19 20]
                  //   [21 22]
                  //   [23 24]]] with shape (3, 4, 2)
                  .values = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                             13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}},
        .indices = {.type = mojom::Operand::DataType::kInt64,
                    .dimensions = {},
                    .values = {2}},
        .axis = 1,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3, 2},
                   // [[ 5  6]
                   //  [13 14]
                   //  [21 22]] with shape (3, 2)
                   .values = {5, 6, 13, 14, 21, 22}}}
        .Test();
  }
  {
    // Test gather with 5-D input, 0-D indices and axis = 4 with data type
    // int32.
    GatherTester<int32_t, int32_t>{
        .input = {.type = mojom::Operand::DataType::kInt32,
                  .dimensions = {2, 1, 1, 3, 2},
                  // [[[[[ 1  2]
                  //     [ 3  4]
                  //     [ 5  6]]]]
                  //  [[[[ 7  8]
                  //     [ 9 10]
                  //     [11 12]]]]] with shape (2, 1, 1, 3, 2)
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}},
        .indices = {.type = mojom::Operand::DataType::kInt32,
                    .dimensions = {},
                    .values = {1}},
        .axis = 4,
        .output = {.type = mojom::Operand::DataType::kInt32,
                   .dimensions = {2, 1, 1, 3},
                   // [[[[ 2  4  6]]]
                   //  [[[ 8 10 12]]]] with shape (2, 1, 1, 3)
                   .values = {2, 4, 6, 8, 10, 12}}}
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
  std::optional<uint64_t> c_operand_id;
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
  std::optional<OperandInfo<T>> input_c;
  GemmAttributes attributes;
  OperandInfo<float> output;

  void Test(mojom::CreateContextOptions::Device device =
                mojom::CreateContextOptions::Device::kGpu) {
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
                    named_outputs, BuildAndComputeExpectation::kSuccess,
                    device);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output"]), output.type),
        output.values);
  }

  void TestFusingStandaloneActivation(const Activation& activation) {
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
    BuildStandaloneActivation(builder, activation, intermediate_operand_id,
                              output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input_a", VectorToBigBuffer(input_a.values)});
    named_inputs.insert({"input_b", VectorToBigBuffer(input_b.values)});
    if (input_c.has_value()) {
      named_inputs.insert({"input_c", VectorToBigBuffer(input_c->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }
};

// Test building and computing a graph of fusing a standalone activation
// into gemm automatically.
TEST_F(WebNNGraphImplBackendTest, FuseStandaloneActivationIntoGemm) {
  // Test gemm without a third input, activation = linear.
  {
    GemmTester<float>{.input_a = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 2},
                                  .values = {1, 2, 3, 4}},
                      .input_b = {.type = mojom::Operand::DataType::kFloat32,
                                  .dimensions = {2, 2},
                                  .values = {1, 2, 3, 4}},
                      .output = {.type = mojom::Operand::DataType::kFloat32,
                                 .dimensions = {2, 2},
                                 .values = {71, 101, 151, 221}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kLinear,
                       .linear_alpha = 10,
                       .linear_beta = 1});
  }

  // Test gemm with a third input, activation = relu.
  {
    GemmTester<float>{
        .input_a = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, -4}},
        .input_b = {.type = mojom::Operand::DataType::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1, 2, 3, 4}},
        .input_c =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 2},
                               .values = {1, 1, 1, 1}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2},
                   .values = {8, 11, 0, 0}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kRelu});
  }
}

#if BUILDFLAG(IS_WIN)
// Test building and computing a graph with single operator gemm on npu.
TEST_F(WebNNGraphImplBackendTest, BuildSingleOperatorGemmOnNpu) {
  // Skip if failed to get NPU instance since not all platforms support NPU.
  SKIP_TEST_IF(
      !dml::Adapter::GetNpuInstance(DML_FEATURE_LEVEL_4_0).has_value());
  // Test gemm without a third input.
  GemmTester<float>{.input_a = {.type = mojom::Operand::DataType::kFloat32,
                                .dimensions = {2, 2},
                                .values = {1, 2, 3, 4}},
                    .input_b = {.type = mojom::Operand::DataType::kFloat32,
                                .dimensions = {2, 2},
                                .values = {1, 2, 3, 4}},
                    .output = {.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 2},
                               .values = {7, 10, 15, 22}}}
      .Test(mojom::CreateContextOptions::Device::kNpu);
}
#endif  // #if BUILDFLAG(IS_WIN)

// Test building and computing a graph with single operator gemm on gpu.
TEST_F(WebNNGraphImplBackendTest, BuildSingleOperatorGemmOnGpu) {
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
    std::vector<Activation> activations{
        Activation{.kind = mojom::Activation::Tag::kSigmoid},
        Activation{.kind = mojom::Activation::Tag::kTanh}};
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

  void Test(BuildAndComputeExpectation expectation =
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

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    named_inputs.insert({"weight", VectorToBigBuffer(weight.values)});
    named_inputs.insert(
        {"recurrentWeight", VectorToBigBuffer(recurrent_weight.values)});
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    if (recurrent_bias.has_value()) {
      named_inputs.insert(
          {"recurrentBias", VectorToBigBuffer(recurrent_bias->values)});
    }

    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs, expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      for (size_t i = 0; i < outputs.size(); ++i) {
        VerifyIsEqual(
            std::move(named_outputs["output" + base::NumberToString(i)]),
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
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .attributes = {.activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {-30., -30., -30., -30., -30., -210., -210.,
                                -210., -210., -210., -552., -552., -552., -552.,
                                -552.}}}}
        .Test();
  }
  // Test gru with number directions = 2.
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 2;
    GruTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .attributes = {.direction = mojom::RecurrentNetworkDirection::kBoth,
                       .activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {-30.,  -30.,  -30.,  -30.,  -30.,  -210.,
                                -210., -210., -210., -210., -552., -552.,
                                -552., -552., -552., -30.,  -30.,  -30.,
                                -30.,  -30.,  -210., -210., -210., -210.,
                                -210., -552., -552., -552., -552., -552.}}}}
        .Test();
  }
  // Test gru with steps = 2.
  {
    const uint32_t steps = 2;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 2;
    GruTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7, 8,
                             9}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .attributes = {.direction = mojom::RecurrentNetworkDirection::kBoth,
                       .activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {6.,  6.,  6.,  6.,  6.,  15., 15., 15.,
                                15., 15., 24., 24., 24., 24., 24., 6.,
                                6.,  6.,  6.,  6.,  15., 15., 15., 15.,
                                15., 24., 24., 24., 24., 24.}}}}
        .Test();
  }
  // Test gru with bias and recurrentbias.
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 1;
    GruTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 1)},
        .recurrent_bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 0)},
        .attributes = {.activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {-42., -42., -42., -42., -42., -240., -240.,
                                -240., -240., -240., -600., -600., -600., -600.,
                                -600.}}}}
        .Test();
  }
  // Test gru with bias and initial hidden state.
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 1;
    GruTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 1)},
        .initial_hidden_state =
            OperandInfo<float>{
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {num_directions, batch_size, hidden_size},
                .values = std::vector<float>(
                    num_directions * batch_size * hidden_size, 1)},
        .attributes = {.activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size},
                     .values = {-725., -725., -725., -725., -725., -2399.,
                                -2399., -2399., -2399., -2399., -5045., -5045.,
                                -5045., -5045., -5045.}}}}
        .Test();
  }
  // Test gru with return_sequence = true;
  {
    const uint32_t steps = 1;
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    const uint32_t num_directions = 1;
    GruTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size},
                   .values = std::vector<float>(
                       num_directions * 3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(
                                 num_directions * 3 * hidden_size * hidden_size,
                                 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 1)},
        .recurrent_bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {num_directions, 3 * hidden_size},
                               .values = std::vector<float>(
                                   num_directions * 3 * hidden_size, 0)},
        .initial_hidden_state =
            OperandInfo<float>{
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {num_directions, batch_size, hidden_size},
                .values = std::vector<float>(
                    num_directions * batch_size * hidden_size, 1)},
        .attributes = {.return_sequence = true,
                       .activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .outputs =
            {{.type = mojom::Operand::DataType::kFloat32,
              .dimensions = {num_directions, batch_size, hidden_size},
              .values = {-725., -725., -725., -725., -725., -2399., -2399.,
                         -2399., -2399., -2399., -5045., -5045., -5045., -5045.,
                         -5045.}},
             {.type = mojom::Operand::DataType::kFloat32,
              .dimensions = {steps, num_directions, batch_size, hidden_size},
              .values = {-725., -725., -725., -725., -725., -2399., -2399.,
                         -2399., -2399., -2399., -5045., -5045., -5045., -5045.,
                         -5045.}}}}
        .Test();
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
    std::vector<Activation> activations{
        Activation{.kind = mojom::Activation::Tag::kSigmoid},
        Activation{.kind = mojom::Activation::Tag::kTanh}};
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

  void Test(BuildAndComputeExpectation expectation =
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

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    named_inputs.insert({"weight", VectorToBigBuffer(weight.values)});
    named_inputs.insert(
        {"recurrentWeight", VectorToBigBuffer(recurrent_weight.values)});
    named_inputs.insert(
        {"hiddenState", VectorToBigBuffer(hidden_state.values)});
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    if (recurrent_bias.has_value()) {
      named_inputs.insert(
          {"recurrentBias", VectorToBigBuffer(recurrent_bias->values)});
    }

    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs, expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(std::move(named_outputs["output"]), output);
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
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3 * hidden_size, input_size},
                   .values =
                       std::vector<float>(3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {3 * hidden_size, hidden_size},
                             .values = std::vector<float>(
                                 3 * hidden_size * hidden_size, 1)},
        .hidden_state = {.type = mojom::Operand::DataType::kFloat32,
                         .dimensions = {batch_size, hidden_size},
                         .values =
                             std::vector<float>(batch_size * hidden_size, 0)},
        .hidden_size = hidden_size,
        .attributes = {.activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {batch_size, hidden_size},
                   .values = {-30., -30., -30., -30., -30., -210., -210., -210.,
                              -210., -210., -552., -552., -552., -552., -552.}}}
        .Test();
  }
  // Test gruCell with bias and recurrentbias.
  {
    const uint32_t batch_size = 3;
    const uint32_t input_size = 3;
    const uint32_t hidden_size = 5;
    GruCellTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {batch_size, input_size},
                  .values = {1, 2, 3, 4, 5, 6, 7, 8, 9}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {3 * hidden_size, input_size},
                   .values =
                       std::vector<float>(3 * hidden_size * input_size, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {3 * hidden_size, hidden_size},
                             .values = std::vector<float>(
                                 3 * hidden_size * hidden_size, 1)},
        .hidden_state = {.type = mojom::Operand::DataType::kFloat32,
                         .dimensions = {batch_size, hidden_size},
                         .values =
                             std::vector<float>(batch_size * hidden_size, 0)},
        .hidden_size = hidden_size,
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {3 * hidden_size},
                                   .values =
                                       std::vector<float>(3 * hidden_size, 1)},
        .recurrent_bias =
            OperandInfo<float>{
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {3 * hidden_size},
                .values = std::vector<float>(3 * hidden_size, 0)},
        .attributes = {.activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {batch_size, hidden_size},
                   .values = {-42., -42., -42., -42., -42., -240., -240., -240.,
                              -240., -240., -600., -600., -600., -600., -600.}}}
        .Test();
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

// Test building and computing a graph with one input and one constant.
TEST_F(WebNNGraphImplBackendTest, BuildOneInputAndOneConstantOperand) {
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

// Test building a graph with one input and one constant to compute for
// multiple times.
TEST_F(WebNNGraphImplBackendTest, BuildOneGraphToComputeMultipleTimes) {
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

  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  mojo::AssociatedRemote<mojom::WebNNGraph> webnn_graph_remote;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  // Create the ContextImpl through context provider.
  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  webnn_provider_remote->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          mojom::CreateContextOptions::Device::kGpu,
          mojom::CreateContextOptions::PowerPreference::kDefault),
      create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  if (create_context_result->is_context_remote()) {
    webnn_context_remote.Bind(
        std::move(create_context_result->get_context_remote()));
  }
  EXPECT_TRUE(webnn_context_remote.is_bound());

  // The GraphImpl should be built successfully.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  webnn_context_remote->CreateGraph(builder.CloneGraphInfo(),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  EXPECT_FALSE(create_graph_result->is_error());
  webnn_graph_remote.Bind(std::move(create_graph_result->get_graph_remote()));
  EXPECT_TRUE(webnn_graph_remote.is_bound());
  {
    // Compute for the first time.
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input_a", VectorToBigBuffer<float>({1, 1, 1, 1})});

    // The GraphImpl should compute successfully.
    base::test::TestFuture<mojom::ComputeResultPtr> compute_future;
    webnn_graph_remote->Compute(std::move(named_inputs),
                                compute_future.GetCallback());
    mojom::ComputeResultPtr compute_result = compute_future.Take();
    ASSERT_TRUE(compute_result->is_named_outputs());
    EXPECT_FALSE(compute_result->get_named_outputs().empty());
    named_outputs = std::move(compute_result->get_named_outputs());

    EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
              std::vector<float>({12, 14, 12, 14}));
  }
  {
    // Compute for the second time.
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input_a", VectorToBigBuffer<float>({1, 1, 1, 1})});

    // The GraphImpl should compute successfully.
    base::test::TestFuture<mojom::ComputeResultPtr> compute_future;
    webnn_graph_remote->Compute(std::move(named_inputs),
                                compute_future.GetCallback());
    mojom::ComputeResultPtr compute_result = compute_future.Take();
    ASSERT_TRUE(compute_result->is_named_outputs());
    EXPECT_FALSE(compute_result->get_named_outputs().empty());
    named_outputs = std::move(compute_result->get_named_outputs());

    EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
              std::vector<float>({12, 14, 12, 14}));
  }
  {
    // Compute for the third time.
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input_a", VectorToBigBuffer<float>({2, 2, 2, 2})});

    // The GraphImpl should compute successfully.
    base::test::TestFuture<mojom::ComputeResultPtr> compute_future;
    webnn_graph_remote->Compute(std::move(named_inputs),
                                compute_future.GetCallback());
    mojom::ComputeResultPtr compute_result = compute_future.Take();
    ASSERT_TRUE(compute_result->is_named_outputs());
    EXPECT_FALSE(compute_result->get_named_outputs().empty());
    named_outputs = std::move(compute_result->get_named_outputs());

    EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
              std::vector<float>({24, 28, 24, 28}));
  }
  webnn_graph_remote.reset();
  webnn_context_remote.reset();
  webnn_provider_remote.reset();
  base::RunLoop().RunUntilIdle();
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

    builder.BuildInstanceNormalization(input_operand_id, output_operand_id,
                                       std::move(attributes));

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    if (scale.has_value()) {
      named_inputs.insert({"scale", VectorToBigBuffer(scale->values)});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs, expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(std::move(named_outputs["output"]), output);
    }
  }

  void TestFusingStandaloneActivation(const Activation& activation) {
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
    BuildStandaloneActivation(builder, activation, intermediate_operand_id,
                              output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    if (scale.has_value()) {
      named_inputs.insert({"scale", VectorToBigBuffer(scale->values)});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
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
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {1, 2, 3, 4, 5, 6}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {0, 0, 1.2247356859083902, 0, 0,
                              1.2247356859083902}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kRelu});
  }
}

// Test building and computing a graph with single operator
// instanceNormalization.
TEST_F(WebNNGraphImplBackendTest, BuildSingleOperatorInstanceNormalization) {
  {
    // Test instanceNormalization with 4-D input with default scale and bias.
    InstanceNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {1, 2, 3, 4, 5, 6}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-1.2247356859083902, 0, 1.2247356859083902,
                              -1.2247356859083902, 0, 1.2247356859083902}}}
        .Test();
  }
  {
    // Test instanceNormalization with 4-D input with layout = nchw and
    // non-default scale and bias.
    InstanceNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {1, 2, 3, 4, 5, 6}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2},
                                    .values = {0.5, -0.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0.1, 0.2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-0.5123678429541951, 0.1, 0.7123678429541951,
                              0.8123678429541952, 0.2, -0.4123678429541951}}}
        .Test();
  }
  {
    // Test instanceNormalization with 4-D input with layout = nhwc and
    // non-default scale and bias.
    InstanceNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {1, 2, 3, 4, 5, 6}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {3},
                                    .values = {0.5, 1, -0.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {3},
                                   .values = {0.1, 0.2, 0.3}},
        .attributes = {.layout = mojom::InputOperandLayout::kChannelsLast},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-0.3999988888925926, -0.7999977777851852,
                              0.7999988888925926, 0.5999988888925926,
                              1.1999977777851851, -0.1999988888925926}}}
        .Test();
  }
  {
    // Test instanceNormalization with 4-D input with float16 data type, given
    // scale only.
    InstanceNormalizationTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 1, 3},
                  .values = Float16FromFloat32({1, 2, 3, 4, 5, 6})},
        .scale =
            OperandInfo<float16>{.type = mojom::Operand::DataType::kFloat16,
                                 .dimensions = {2},
                                 .values = Float16FromFloat32({0.5, -0.5})},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 1, 3},
                   .values = Float16FromFloat32(
                       {-0.6123678429541951, 0, 0.6123678429541951,
                        0.6123678429541952, 0, -0.6123678429541951})}}
        .Test();
  }
  {
    // Test instanceNormalization with 4-D input with float32 data type, given
    // bias only.
    InstanceNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {1, 2, 3, 4, 5, 6}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2},
                                   .values = {0.5, -0.5}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-0.7247356859083902, 0.5, 1.7247356859083902,
                              -1.7247356859083902, -0.5, 0.7247356859083902}}}
        .Test();
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

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    if (scale.has_value()) {
      named_inputs.insert({"scale", VectorToBigBuffer(scale->values)});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs, expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      VerifyIsEqual(std::move(named_outputs["output"]), output);
    }
  }

  void TestFusingStandaloneActivation(const Activation& activation) {
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
    BuildStandaloneActivation(builder, activation, intermediate_operand_id,
                              output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    if (scale.has_value()) {
      named_inputs.insert({"scale", VectorToBigBuffer(scale->values)});
    }
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
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
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {5},
                  .values = {0, 1, 2, 3, 4}},
        .attributes = {.axes = {0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {5},
                   .values = {0, 0, 0, 0.7071050134262237, 1.4142100268524473}}}
        .TestFusingStandaloneActivation(
            Activation{.kind = mojom::Activation::Tag::kRelu});
  }
}

// Test building and computing a graph with single operator
// layerNormalization.
TEST_F(WebNNGraphImplBackendTest, BuildSingleOperatorLayerNormalization) {
  {
    // Test layerNormalization with a scalar input with default scale and bias.
    LayerNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {},
                  .values = {5}},
        .attributes = {.axes = {}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {},
                   .values = {0}}}
        .Test();
  }
  {
    // Test layerNormalization with 1-D input with axes = [0] and default scale
    // and bias.
    LayerNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {5},
                  .values = {0, 1, 2, 3, 4}},
        .attributes = {.axes = {0}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {5},
                   .values = {-1.4142100268524473, -0.7071050134262237, 0,
                              0.7071050134262237, 1.4142100268524473}}}
        .Test();
  }
  {
    // Test layerNormalization with 4-D input with axes = [1, 2, 3] and default
    // scale and bias.
    LayerNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .attributes = {.axes = {1, 2, 3}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-1.4638475999719223, -0.8783085599831534,
                              -0.29276951999438444, 0.29276951999438444,
                              0.8783085599831534, 1.4638475999719223}}}
        .Test();
  }
  {
    // Test layerNormalization with 4-D input with axes = [2, 3].
    LayerNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {1, 3},
                                    .values = {0.5, 1, -0.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 3},
                                   .values = {0.1, 0.2, 0.3}},
        .attributes = {.axes = {2, 3}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-0.5123678429541951, 0.2, -0.3123678429541951,
                              -0.5123678429541951, 0.2, -0.3123678429541951}}}
        .Test();
  }
  {
    // Test layerNormalization with 3-D input with axes = [0, 1, 2] and default
    // scale and bias.
    LayerNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {2, 2, 2},
                  .values = {-4, -3, -2, -1, 1, 2, 3, 4}},
        .attributes = {.axes = {0, 1, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {2, 2, 2},
                   .values = {-1.4605925129524255, -1.0954443847143192,
                              -0.7302962564762128, -0.3651481282381064,
                              0.3651481282381064, 0.7302962564762128,
                              1.0954443847143192, 1.4605925129524255}}}
        .Test();
  }
  {
    // Test layerNormalization with 6-D input with permuted axes = [4, 1, 2].
    LayerNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3, 2, 1},
                  .values = {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7}},
        .scale = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                    .dimensions = {2, 2, 1},
                                    .values = {0.5, 0, 1, -0.5}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 2, 1},
                                   .values = {0.1, 0.2, 0.3, 0.4}},
        .attributes = {.axes = {4, 1, 2}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3, 2, 1},
                   .values = {-0.47539614454389156, -0.5219944922055593,
                              -0.47539614454389156, -0.5219944922055593,
                              -0.47539614454389156, -0.5219944922055593, 0.2,
                              -0.17539614454389152, 0.2, -0.17539614454389152,
                              0.2, -0.17539614454389152}}}
        .Test();
  }
  {
    // Test layerNormalization with 4-D input with axes = [1, 2, 3], float16
    // data type, given scale only.
    LayerNormalizationTester<float16>{
        .input = {.type = mojom::Operand::DataType::kFloat16,
                  .dimensions = {1, 2, 1, 2},
                  .values = Float16FromFloat32({-2, -2, 2, 2})},
        .scale =
            OperandInfo<float16>{.type = mojom::Operand::DataType::kFloat16,
                                 .dimensions = {2, 1, 2},
                                 .values = Float16FromFloat32({1, 1, 1, 1})},
        .attributes = {.axes = {1, 2, 3}, .epsilon = 0},
        .output = {.type = mojom::Operand::DataType::kFloat16,
                   .dimensions = {1, 2, 1, 2},
                   .values = Float16FromFloat32({-1, -1, 1, 1})}}
        .Test();
  }
  {
    // Test layerNormalization with 4-D input with axes = [1, 2, 3], float32
    // data type, given bias only.
    LayerNormalizationTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {1, 2, 1, 3},
                  .values = {-1, 0, 1, 2, 3, 4}},
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2, 1, 3},
                                   .values = {0, 0.1, 0.2, 0.3, 0.4, 0.5}},
        .attributes = {.axes = {1, 2, 3}},
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1, 2, 1, 3},
                   .values = {-1.4638475999719223, -0.7783085599831534,
                              -0.09276951999438444, 0.59276951999438444,
                              1.2783085599831534, 1.9638475999719223}}}
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
    std::vector<Activation> activations{
        Activation{.kind = mojom::Activation::Tag::kSigmoid},
        Activation{.kind = mojom::Activation::Tag::kTanh},
        Activation{.kind = mojom::Activation::Tag::kTanh}};
  };
  LstmAttributes attributes;
  std::vector<OperandInfo<T>> outputs;

  void Test(BuildAndComputeExpectation expectation =
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

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input", VectorToBigBuffer(input.values)});
    named_inputs.insert({"weight", VectorToBigBuffer(weight.values)});
    named_inputs.insert(
        {"recurrentWeight", VectorToBigBuffer(recurrent_weight.values)});
    if (bias.has_value()) {
      named_inputs.insert({"bias", VectorToBigBuffer(bias->values)});
    }
    if (recurrent_bias.has_value()) {
      named_inputs.insert(
          {"recurrentBias", VectorToBigBuffer(recurrent_bias->values)});
    }
    if (peephole_weight.has_value()) {
      named_inputs.insert(
          {"peepholeWeight", VectorToBigBuffer(peephole_weight->values)});
    }
    if (initial_hidden_state.has_value()) {
      named_inputs.insert({"initialHiddenState",
                           VectorToBigBuffer(initial_hidden_state->values)});
    }
    if (initial_cell_state.has_value()) {
      named_inputs.insert(
          {"initialCellState", VectorToBigBuffer(initial_cell_state->values)});
    }

    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs, expectation);

    if (expectation == BuildAndComputeExpectation::kSuccess) {
      for (size_t i = 0; i < outputs.size(); ++i) {
        VerifyIsEqual(
            std::move(named_outputs["output" + base::NumberToString(i)]),
            outputs[i]);
      }
    }
  }
};

// Test building and computing a graph with single operator lstm.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorLstm) {
  {
    // Test lstm with bidirection and activations = {relu, relu, linear}.
    uint32_t steps = 1;
    uint32_t batch_size = 2;
    uint32_t input_size = 2;
    uint32_t direction_count = 2;
    uint32_t hidden_size = 1;
    LstmTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size, input_size},
                   .values = std::vector<float>(16, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(8, 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .attributes =
            {.direction = mojom::RecurrentNetworkDirection::kBoth,
             .activations = {Activation{.kind = mojom::Activation::Tag::kRelu},
                             Activation{.kind = mojom::Activation::Tag::kRelu},
                             Activation{.kind = mojom::Activation::Tag::kLinear,
                                        .linear_alpha = 2,
                                        .linear_beta = 0}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {54, 686, 54, 686}},
                    {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {9, 49, 9, 49}}}}
        .Test();
  }
  {
    // Test lstm with given bias and recurrent bias, activations = {relu, relu,
    // relu}.
    uint32_t steps = 2;
    uint32_t batch_size = 2;
    uint32_t input_size = 2;
    uint32_t direction_count = 1;
    uint32_t hidden_size = 1;
    LstmTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {-4, -3, -2, -1, 0, 1, 2, 3}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size, input_size},
                   .values = std::vector<float>(8, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(4, 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {direction_count, 4 * hidden_size},
                               .values = std::vector<float>(4, 0.5)},
        .recurrent_bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {direction_count, 4 * hidden_size},
                               .values = std::vector<float>(4, 0.5)},
        .attributes = {.activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {8, 216}},
                    {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {4, 36}}}}
        .Test();
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
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {1, 2, 3, 4}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size, input_size},
                   .values = std::vector<float>(16, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(16, 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {direction_count, 4 * hidden_size},
                               .values = std::vector<float>(8, 1)},
        .peephole_weight =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {direction_count, 3 * hidden_size},
                               .values = std::vector<float>(6, 0)},
        .attributes = {.activations =
                           {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {2811392, 2811392}},
                    {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {20672, 20672}}}}
        .Test();
  }
  {
    // Test lstm with given recurrent bias, initial hidden state and initial
    // cell state, return_sequence = true, activations = {linear, linear,
    // linear}.
    uint32_t steps = 1;
    uint32_t batch_size = 2;
    uint32_t input_size = 1;
    uint32_t direction_count = 1;
    uint32_t hidden_size = 2;
    LstmTester<float>{
        .input = {.type = mojom::Operand::DataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size},
                  .values = {0, 1}},
        .weight = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size, input_size},
                   .values = std::vector<float>(8, 1)},
        .recurrent_weight = {.type = mojom::Operand::DataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size},
                             .values = std::vector<float>(16, 1)},
        .steps = steps,
        .hidden_size = hidden_size,
        .recurrent_bias =
            OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {direction_count, 4 * hidden_size},
                               .values = std::vector<float>(8, 2)},
        .initial_hidden_state =
            OperandInfo<float>{
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {direction_count, batch_size, hidden_size},
                .values = std::vector<float>(4, 1)},
        .initial_cell_state =
            OperandInfo<float>{
                .type = mojom::Operand::DataType::kFloat32,
                .dimensions = {direction_count, batch_size, hidden_size},
                .values = std::vector<float>(4, 0)},
        .attributes =
            {.return_sequence = true,
             .activations = {Activation{.kind = mojom::Activation::Tag::kLinear,
                                        .linear_alpha = 1,
                                        .linear_beta = 0},
                             Activation{.kind = mojom::Activation::Tag::kLinear,
                                        .linear_alpha = 1,
                                        .linear_beta = 1},
                             Activation{.kind = mojom::Activation::Tag::kLinear,
                                        .linear_alpha = 1,
                                        .linear_beta = 2}}},
        .outputs = {{.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {88, 88, 160, 160}},
                    {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size},
                     .values = {20, 20, 30, 30}},
                    {.type = mojom::Operand::DataType::kFloat32,
                     .dimensions = {steps, direction_count, batch_size,
                                    hidden_size},
                     .values = {88, 88, 160, 160}}}}
        .Test();
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

    GraphInfoBuilder builder;
    uint64_t input_operand_id = builder.BuildConstant(
        {steps, batch_size, input_size}, mojom::Operand::DataType::kFloat32,
        base::as_bytes(base::make_span(input_data)));
    uint64_t weight_operand_id =
        builder.BuildConstant({direction_count, 4 * hidden_size, input_size},
                              mojom::Operand::DataType::kFloat32,
                              base::as_bytes(base::make_span(weight_data)));
    uint64_t recurrent_weight_operand_id = builder.BuildConstant(
        {direction_count, 4 * hidden_size, hidden_size},
        mojom::Operand::DataType::kFloat32,
        base::as_bytes(base::make_span(recurrent_weight_data)));

    LstmTester<float>::LstmAttributes attributes;
    attributes.peephole_weight_operand_id = builder.BuildConstant(
        {direction_count, 3 * hidden_size}, mojom::Operand::DataType::kFloat32,
        base::as_bytes(base::make_span(peephole_weight_data)));
    attributes.initial_hidden_state_operand_id = builder.BuildConstant(
        {direction_count, batch_size, hidden_size},
        mojom::Operand::DataType::kFloat32,
        base::as_bytes(base::make_span(initial_hidden_state_data)));
    attributes.initial_cell_state_operand_id = builder.BuildConstant(
        {direction_count, batch_size, hidden_size},
        mojom::Operand::DataType::kFloat32,
        base::as_bytes(base::make_span(initial_cell_state_data)));
    attributes.activations = {
        Activation{.kind = mojom::Activation::Tag::kRelu},
        Activation{.kind = mojom::Activation::Tag::kRelu},
        Activation{.kind = mojom::Activation::Tag::kRelu}};

    uint64_t output_a_operand_id = builder.BuildOutput(
        "output0", {direction_count, batch_size, hidden_size},
        mojom::Operand::DataType::kFloat32);
    uint64_t output_b_operand_id = builder.BuildOutput(
        "output1", {direction_count, batch_size, hidden_size},
        mojom::Operand::DataType::kFloat32);
    std::vector<uint64_t> output_operand_ids{output_a_operand_id,
                                             output_b_operand_id};
    builder.BuildLstm(input_operand_id, weight_operand_id,
                      recurrent_weight_operand_id,
                      std::move(output_operand_ids), steps, hidden_size,
                      std::move(attributes));

    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
    BuildAndCompute(builder.CloneGraphInfo(), /*named_inputs=*/{},
                    named_outputs);

    ASSERT_EQ(named_outputs.size(), 2u);
    EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output0"])),
              std::vector<float>({0, 0, 2, 2}));
    EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output1"])),
              std::vector<float>({0, 0, 2, 2}));
  }
}

struct LstmCellAttributes {
  std::optional<uint64_t> bias_operand_id;
  std::optional<uint64_t> recurrent_bias_operand_id;
  std::optional<uint64_t> peephole_weight_operand_id;
  mojom::LstmWeightLayout layout = mojom::LstmWeightLayout::kIofg;
  std::vector<Activation> activations;
};

// TODO(crbug.com/331250158): Remove this test after the WPT conformance tests
// are completed.
// Test building and computing a graph with single operator lstmCell.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorLstmCell) {
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
      "input", {batch_size, input_size}, mojom::Operand::DataType::kFloat32);
  uint64_t weight_operand_id =
      builder.BuildInput("weight", {4 * hidden_size, input_size},
                         mojom::Operand::DataType::kFloat32);
  uint64_t recurrent_weight_operand_id =
      builder.BuildInput("recurrentWeight", {4 * hidden_size, hidden_size},
                         mojom::Operand::DataType::kFloat32);
  uint64_t hidden_state_operand_id =
      builder.BuildInput("hiddenState", {batch_size, hidden_size},
                         mojom::Operand::DataType::kFloat32);
  uint64_t cell_state_operand_id =
      builder.BuildInput("cellState", {batch_size, hidden_size},
                         mojom::Operand::DataType::kFloat32);

  LstmCellAttributes attributes;
  attributes.activations = {Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu},
                            Activation{.kind = mojom::Activation::Tag::kRelu}};

  uint64_t output_a_operand_id = builder.BuildOutput(
      "output0", {batch_size, hidden_size}, mojom::Operand::DataType::kFloat32);
  uint64_t output_b_operand_id = builder.BuildOutput(
      "output1", {batch_size, hidden_size}, mojom::Operand::DataType::kFloat32);
  std::vector<uint64_t> output_operand_ids{output_a_operand_id,
                                           output_b_operand_id};
  builder.BuildLstmCell(input_operand_id, weight_operand_id,
                        recurrent_weight_operand_id, hidden_state_operand_id,
                        cell_state_operand_id, std::move(output_operand_ids),
                        hidden_size, std::move(attributes));

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  named_inputs.insert({"input", VectorToBigBuffer(input_data)});
  named_inputs.insert({"weight", VectorToBigBuffer(weight_data)});
  named_inputs.insert(
      {"recurrentWeight", VectorToBigBuffer(recurrent_weight_data)});
  named_inputs.insert(
      {"hiddenState", VectorToBigBuffer(initial_hidden_state_data)});
  named_inputs.insert(
      {"cellState", VectorToBigBuffer(initial_cell_state_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  ASSERT_EQ(named_outputs.size(), 2u);
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output0"])),
            std::vector<float>({150, 150, 810, 810}));
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output1"])),
            std::vector<float>({30, 30, 90, 90}));
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

  void TestFusingStandaloneActivation(const Activation& activation) {
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_a_operand_id =
        builder.BuildInput("input_a", input_a.dimensions, input_a.type);
    uint64_t input_b_operand_id =
        builder.BuildInput("input_b", input_b.dimensions, input_b.type);
    uint64_t intermediate_operand_id =
        builder.BuildIntermediateOperand(output.dimensions, output.type);

    builder.BuildMatmul(input_a_operand_id, input_b_operand_id,
                        intermediate_operand_id);

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    BuildStandaloneActivation(builder, activation, intermediate_operand_id,
                              output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
    named_inputs.insert({"input_a", VectorToBigBuffer(input_a.values)});
    named_inputs.insert({"input_b", VectorToBigBuffer(input_b.values)});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyIsEqual(std::move(named_outputs["output"]), output);
  }
};

// Test building and computing a graph of fusing a standalone activation
// into matmul automatically.
TEST_F(WebNNGraphImplBackendTest, FuseStandaloneActivationIntoMatmul) {
  // Test matmul with 2-D * 2-D inputs, activation = linear.
  GemmTester<float>{.input_a = {.type = mojom::Operand::DataType::kFloat32,
                                .dimensions = {2, 2},
                                .values = {1, 2, 3, 4}},
                    .input_b = {.type = mojom::Operand::DataType::kFloat32,
                                .dimensions = {2, 2},
                                .values = {1, 2, 3, 4}},
                    .output = {.type = mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 2},
                               .values = {71, 101, 151, 221}}}
      .TestFusingStandaloneActivation(
          Activation{.kind = mojom::Activation::Tag::kLinear,
                     .linear_alpha = 10,
                     .linear_beta = 1});
}

// Test building and computing a graph with single operator matmul.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorMatmul) {
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
      builder.BuildInput("input_a", {2, 2}, mojom::Operand::DataType::kFloat32);
  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, mojom::Operand::DataType::kFloat32);
  std::vector<float> constant_data = {1, 1};
  uint64_t constant_c_operand_id =
      builder.BuildConstant({2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  // Reshape constant_c from [2] to [1, 2] and use it as operand c for gemm.
  uint64_t reshape_operand_id = builder.BuildIntermediateOperand(
      {1, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildReshape(constant_c_operand_id, reshape_operand_id);
  GemmAttributes gemm_attributes;
  gemm_attributes.c_operand_id = reshape_operand_id;
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildGemm(input_a_operand_id, input_b_operand_id, output_operand_id,
                    gemm_attributes);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1, 2, 3, 4};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_data)});
  named_inputs.insert({"input_b", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);

  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
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
  uint64_t input_a_operand_id = builder.BuildInput(
      "input_a", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  std::vector<float> constant_data = {1, 1};
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  // Reshape constant_b from [2] to [1, 2] and use it as operand b for add.
  uint64_t reshape_operand_id = builder.BuildIntermediateOperand(
      {1, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildReshape(constant_b_operand_id, reshape_operand_id);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 1, 2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 input_a_operand_id, reshape_operand_id,
                                 output_operand_id);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  std::vector<float> input_data = {1, 1, 1, 1};
  named_inputs.insert({"input_a", VectorToBigBuffer(input_data)});
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
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
      builder.BuildConstant({3}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_data)));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {3}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(constant_operand_id, output_operand_id);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
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
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_a_data)));
  std::vector<float> constant_b_data = {2, 2, 2, 2};
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_b_data)));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 constant_a_operand_id, constant_b_operand_id,
                                 output_operand_id);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
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
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_a_data)));
  std::vector<float> constant_b_data = {2, 2, 2, 2};
  uint64_t constant_b_operand_id =
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_b_data)));
  uint64_t intermediate_operand_id = builder.BuildIntermediateOperand(
      {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 constant_a_operand_id, constant_b_operand_id,
                                 intermediate_operand_id);
  std::vector<float> constant_c_data = {3, 3, 3, 3};
  uint64_t constant_c_operand_id =
      builder.BuildConstant({2, 2}, mojom::Operand::DataType::kFloat32,
                            base::as_bytes(base::make_span(constant_c_data)));
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, mojom::Operand::DataType::kFloat32);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kMul,
                                 intermediate_operand_id, constant_c_operand_id,
                                 output_operand_id);

  base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;
  base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
  BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                  named_outputs);
  EXPECT_EQ(BigBufferToVector<float>(std::move(named_outputs["output"])),
            std::vector<float>({9, 9, 9, 9}));
}

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

// Test building and computing a graph with single operator concat.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorConcat) {
  std::vector<float> expected_output = {-1, -2, -3, -4, -5, -6, 0,  0,
                                        0,  0,  0,  0,  1,  2,  3,  4,
                                        5,  6,  7,  8,  9,  10, 11, 12};
#if BUILDFLAG(IS_CHROMEOS)
  SetComputeResult("output", expected_output);
#endif
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
            expected_output);
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

// Test building and computing a graph in the following topology.
//      [input]   [constant_a]
//          \          /
//             concat   [constant_b]
//               \           /
//                   concat
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeConcatWithConstants) {
  std::vector<float> expected_output = {0,  0,  0,  1,  2,  3,
                                        -1, -2, -3, -4, -5, -6};
#if BUILDFLAG(IS_CHROMEOS)
  SetComputeResult("output", expected_output);
#endif

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
            expected_output);
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

// Test building and computing a graph with single operator resample2d.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorResample2d) {
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

// Test building and computing a graph with single operator transpose.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorTranspose) {
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

// Test building and computing a graph in the following topology.
//      [input]
//         |
//     transpose
//         |
//     transpose
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTwoTranspose) {
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

// Test building and computing a graph in the following topology.
//      [input]
//         |
//     transpose
//         |
//       relu
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeGraphWithTransposeAndRelu) {
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

// Test building and computing a graph with single operator where.
TEST_F(WebNNGraphImplBackendTest, BuildAndComputeSingleOperatorWhere) {
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
    uint64_t input_operand_id = builder.BuildInput(
        "input", {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);
    uint64_t filter_operand_id = builder.BuildConstant(
        {1, 3, 3, 1}, mojom::Operand::DataType::kFloat32,
        base::as_bytes(base::make_span(std::vector<float>(9, 1))));
    uint64_t conv2d_output_operand_id = builder.BuildIntermediateOperand(
        {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);

    Conv2dTester<float>::Conv2dAttributes attributes{
        .padding = {1, 1, 1, 1},
        .input_layout = mojom::InputOperandLayout::kChannelsLast,
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
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

    uint64_t relu1_output_operand_id = builder.BuildOutput(
        "output1", {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);
    builder.BuildRelu(conv2d_output_operand_id, relu1_output_operand_id);

    uint64_t relu2_output_operand_id = builder.BuildOutput(
        "output2", {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);
    builder.BuildRelu(conv2d_output_operand_id, relu2_output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;

    named_inputs.insert(
        {"input", VectorToBigBuffer(std::vector<float>{
                      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    std::vector<float> expected_output_data{0,  0,  0, 0,  0,  0,  0, 0,  0,
                                            0,  0,  0, 8,  17, 0,  0, 44, 53,
                                            62, 11, 0, 11, 17, 23, 0};
    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output1"]),
                           mojom::Operand::DataType::kFloat32),
        expected_output_data);
    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output2"]),
                           mojom::Operand::DataType::kFloat32),
        expected_output_data);
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
    uint64_t input_operand_id = builder.BuildInput(
        "input", {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);
    uint64_t filter_operand_id = builder.BuildConstant(
        {1, 3, 3, 1}, mojom::Operand::DataType::kFloat32,
        base::as_bytes(base::make_span(std::vector<float>(9, 1))));
    uint64_t conv2d_output_operand_id = builder.BuildIntermediateOperand(
        {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);

    Conv2dTester<float>::Conv2dAttributes attributes{
        .padding = {1, 1, 1, 1},
        .input_layout = mojom::InputOperandLayout::kChannelsLast,
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
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

    uint64_t reshape_output_operand_id = builder.BuildOutput(
        "output1", {1, 5, 1, 5}, mojom::Operand::DataType::kFloat32);
    builder.BuildReshape(conv2d_output_operand_id, reshape_output_operand_id);

    uint64_t relu_output_operand_id = builder.BuildOutput(
        "output2", {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);
    builder.BuildRelu(conv2d_output_operand_id, relu_output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;

    named_inputs.insert(
        {"input", VectorToBigBuffer(std::vector<float>{
                      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output1"]),
                           mojom::Operand::DataType::kFloat32),
        std::vector<float>{-88, -79, -73, -67, -76, -67, -46, -37, -28,
                           -49, -37, -1,  8,   17,  -19, -7,  44,  53,
                           62,  11,  -28, 11,  17,  23,  -16});
    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output2"]),
                           mojom::Operand::DataType::kFloat32),
        std::vector<float>{0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
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
    uint64_t input_operand_id = builder.BuildInput(
        "input", {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);
    uint64_t filter_operand_id = builder.BuildConstant(
        {1, 3, 3, 1}, mojom::Operand::DataType::kFloat32,
        base::as_bytes(base::make_span(std::vector<float>(9, 1))));
    uint64_t conv2d_output_operand_id = builder.BuildIntermediateOperand(
        {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);

    Conv2dTester<float>::Conv2dAttributes attributes{
        .padding = {1, 1, 1, 1},
        .input_layout = mojom::InputOperandLayout::kChannelsLast,
        .bias = OperandInfo<float>{.type = mojom::Operand::DataType::kFloat32,
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

    uint64_t relu_output_operand_id = builder.BuildOutput(
        "output1", {1, 5, 5, 1}, mojom::Operand::DataType::kFloat32);
    builder.BuildRelu(conv2d_output_operand_id, relu_output_operand_id);

    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs;

    named_inputs.insert(
        {"input", VectorToBigBuffer(std::vector<float>{
                      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24})});
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;

    BuildAndCompute(builder.CloneGraphInfo(), std::move(named_inputs),
                    named_outputs);

    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output1"]),
                           mojom::Operand::DataType::kFloat32),
        std::vector<float>{0,  0, 0, 0,  0,  0,  0,  0, 0,  0,  0,  0, 8,
                           17, 0, 0, 44, 53, 62, 11, 0, 11, 17, 23, 0});
    VerifyFloatDataIsEqual(
        GetFloatOutputData(std::move(named_outputs["output2"]),
                           mojom::Operand::DataType::kFloat32),
        std::vector<float>{-88, -79, -73, -67, -76, -67, -46, -37, -28,
                           -49, -37, -1,  8,   17,  -19, -7,  44,  53,
                           62,  11,  -28, 11,  17,  23,  -16});
  }
}

}  // namespace webnn::test
