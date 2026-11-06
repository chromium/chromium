// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_builder_ort.h"

#include <cstdint>
#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/ort/test_base_ort.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::ort {

namespace {

// A small tensor byte length limit for testing.
constexpr uint64_t kTensorByteLengthLimit = 128;
constexpr uint32_t kMaxInt32ElementCount =
    kTensorByteLengthLimit / sizeof(int32_t);
constexpr uint32_t kMaxInt64ElementCount =
    kTensorByteLengthLimit / sizeof(int64_t);

}  // namespace

class WebNNOrtGraphBuilderTest : public TestBaseOrt {
 protected:
  base::expected<std::unique_ptr<ModelEditor::ModelInfo>, mojom::ErrorPtr>
  Build(const GraphInfoBuilder& builder) {
    ContextProperties context_properties = GetContextPropertiesForTesting();
    context_properties.tensor_byte_length_limit = kTensorByteLengthLimit;
    return GraphBuilderOrt::CreateAndBuild(
        builder.GetGraphInfo(), std::move(context_properties), EpWorkarounds(),
        /*constant_operands=*/{});
  }

  mojo::Remote<mojom::WebNNGraphBuilder> graph_builder_remote_;
};

// The int32 indices are within the byte length limit, but the int64 indices
// inserted for ONNX GatherND exceed it.
TEST_F(WebNNOrtGraphBuilderTest, GatherNDInt64IndicesOverByteLengthLimit) {
  const uint32_t n = kMaxInt32ElementCount;
  GraphInfoBuilder builder(graph_builder_remote_);
  OperandId input = builder.BuildInput("input", {n}, OperandDataType::kFloat32);
  OperandId indices =
      builder.BuildInput("indices", {n, 1}, OperandDataType::kInt32);
  OperandId output =
      builder.BuildOutput("output", {n}, OperandDataType::kFloat32);
  builder.BuildGatherND(input, indices, output);

  auto result = Build(builder);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojom::Error::Code::kNotSupportedError);
}

// The int64 indices inserted for ONNX GatherND are within the byte length
// limit.
TEST_F(WebNNOrtGraphBuilderTest, GatherNDInt64IndicesWithinByteLengthLimit) {
  const uint32_t n = kMaxInt64ElementCount;
  GraphInfoBuilder builder(graph_builder_remote_);
  OperandId input = builder.BuildInput("input", {n}, OperandDataType::kFloat32);
  OperandId indices =
      builder.BuildInput("indices", {n, 1}, OperandDataType::kInt32);
  OperandId output =
      builder.BuildOutput("output", {n}, OperandDataType::kFloat32);
  builder.BuildGatherND(input, indices, output);

  EXPECT_TRUE(Build(builder).has_value());
}

// The int32 indices are within the byte length limit, but the int64 indices
// inserted for ONNX ScatterND exceed it.
TEST_F(WebNNOrtGraphBuilderTest, ScatterNDInt64IndicesOverByteLengthLimit) {
  const uint32_t n = kMaxInt32ElementCount;
  GraphInfoBuilder builder(graph_builder_remote_);
  OperandId input = builder.BuildInput("input", {n}, OperandDataType::kFloat32);
  OperandId indices =
      builder.BuildInput("indices", {n, 1}, OperandDataType::kInt32);
  OperandId updates =
      builder.BuildInput("updates", {n}, OperandDataType::kFloat32);
  OperandId output =
      builder.BuildOutput("output", {n}, OperandDataType::kFloat32);
  builder.BuildScatterND(input, indices, updates, output);

  auto result = Build(builder);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojom::Error::Code::kNotSupportedError);
}

// The int32 output is within the byte length limit, but the inserted int64
// output of ONNX ArgMax exceeds it.
TEST_F(WebNNOrtGraphBuilderTest, ArgMaxInt64OutputOverByteLengthLimit) {
  const uint32_t n = kMaxInt32ElementCount;
  GraphInfoBuilder builder(graph_builder_remote_);
  OperandId input =
      builder.BuildInput("input", {n, 1}, OperandDataType::kFloat32);
  OperandId output =
      builder.BuildOutput("output", {n, 1}, OperandDataType::kInt32);
  builder.BuildArgMinMax(mojom::ArgMinMax::Kind::kMax, input, output,
                         /*axis=*/1, /*keep_dimensions=*/true);

  auto result = Build(builder);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojom::Error::Code::kNotSupportedError);
}

}  // namespace webnn::ort
