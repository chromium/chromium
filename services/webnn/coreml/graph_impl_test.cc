// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/ml/webnn/features.mojom-features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::coreml {

namespace {

// TODO(https://crbug.com/1522285): Share this code with
// services/webnn/dml/graph_impl_test.cc
// The DML tests can be shared with CoreML with some refactoring.

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

  // Create the coreml::ContextImpl through context provider.
  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  webnn_provider_remote->CreateWebNNContext(
      mojom::CreateContextOptions::New(), create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  if (create_context_result->is_context_remote()) {
    webnn_context_remote.Bind(
        std::move(create_context_result->get_context_remote()));
  }
  EXPECT_TRUE(webnn_context_remote.is_bound());

  // The coreml::GraphImpl should be built successfully.
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
  EXPECT_TRUE(webnn_graph_remote.is_bound());

  // The coreml::GraphImpl should compute successfully.
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
struct OperandInfo {
  mojom::Operand::DataType type;
  std::vector<uint32_t> dimensions;
  std::vector<T> values;
};

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

template <typename T>
void VerifyIsEqual(mojo_base::BigBuffer actual,
                   const OperandInfo<T>& expected) {
  EXPECT_EQ(BigBufferToVector<T>(std::move(actual)), expected.values);
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

}  // namespace

class WebNNGraphCoreMLImplTest : public testing::Test {
 public:
  void SetUp() override;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

void WebNNGraphCoreMLImplTest::SetUp() {
  if (base::mac::MacOSVersion() < 13'00'00) {
    GTEST_SKIP() << "Skipping test because WebNN is not supported on Mac OS "
                 << base::mac::MacOSVersion();
  }

  scoped_feature_list_.InitAndEnableFeature(
      webnn::mojom::features::kWebMachineLearningNeuralNetwork);
}

// Test building and computing a graph with single operator element-wise
// binary.
TEST_F(WebNNGraphCoreMLImplTest,
       BuildAndComputeSingleOperatorElementWiseBinary) {
  // Test building and computing a graph with single operator add for 0-D
  // scalars.

  // TODO(https://crbug.com/1522285): Enable this failing test, with
  // coreml when dimension is not specified, we don't call add_shape in
  // GraphBuilder::PopulateFeatureDescription. This requires discussing if
  // dimensions when left unspecified should default to 1 across all platforms.
  //   {
  //     ElementWiseBinaryTester<float>{
  //         .lhs = {.type = mojom::Operand::DataType::kFloat32,
  //                 .dimensions = {},
  //                 .values = {1}},
  //         .rhs = {.type = mojom::Operand::DataType::kFloat32,
  //                 .dimensions = {},
  //                 .values = {6}},
  //         .kind = mojom::ElementWiseBinary::Kind::kAdd,
  //         .output = {.type = mojom::Operand::DataType::kFloat32,
  //                    .dimensions = {},
  //                    .values = {7}}}
  //         .Test();
  //   }

  {
    ElementWiseBinaryTester<float>{
        .lhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1},
                .values = {1}},
        .rhs = {.type = mojom::Operand::DataType::kFloat32,
                .dimensions = {1},
                .values = {6}},
        .kind = mojom::ElementWiseBinary::Kind::kAdd,
        .output = {.type = mojom::Operand::DataType::kFloat32,
                   .dimensions = {1},
                   .values = {7}}}
        .Test();
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
        .Test();
  }
}

}  // namespace webnn::coreml
