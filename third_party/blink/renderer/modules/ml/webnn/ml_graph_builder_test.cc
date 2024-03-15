// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/containers/fixed_flat_set.h"
#include "base/numerics/checked_math.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

const uint32_t kSquareRootOfSizeMax = base::saturated_cast<uint32_t>(
    std::sqrt(std::numeric_limits<size_t>::max()));

class MLGraphBuilderTest : public testing::Test {
 public:
  MLGraphBuilderTest() = default;
  ~MLGraphBuilderTest() override = default;
  test::TaskEnvironment task_environment_;
};

MLOperand* BuildArgMinMax(V8TestingScope& scope,
                          MLGraphBuilder* builder,
                          webnn::mojom::blink::ArgMinMax::Kind kind,
                          const MLOperand* input,
                          const MLArgMinMaxOptions* options) {
  MLOperand* output = nullptr;
  switch (kind) {
    case webnn::mojom::blink::ArgMinMax::Kind::kMin:
      output = builder->argMin(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::ArgMinMax::Kind::kMax:
      output = builder->argMax(input, options, scope.GetExceptionState());
      break;
  }
  return output;
}

void CheckArgMinMaxOutput(const MLOperand* input,
                          const MLOperand* output,
                          webnn::mojom::blink::ArgMinMax::Kind kind) {
  ASSERT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kInt64);
  auto* arg_max_min = output->Operator();
  ASSERT_THAT(arg_max_min, testing::NotNull());
  EXPECT_EQ(arg_max_min->Kind(),
            webnn::mojom::blink::Operation::Tag::kArgMinMax);
  EXPECT_EQ(arg_max_min->SubKind<webnn::mojom::blink::ArgMinMax::Kind>(), kind);
  EXPECT_TRUE(arg_max_min->IsConnected());
  EXPECT_THAT(arg_max_min->Options(), testing::NotNull());
}

String ArgMinMaxKindToString(webnn::mojom::blink::ArgMinMax::Kind kind) {
  switch (kind) {
    case webnn::mojom::blink::ArgMinMax::Kind::kMin:
      return "argMin";
    case webnn::mojom::blink::ArgMinMax::Kind::kMax:
      return "argMax";
  }
}

TEST_F(MLGraphBuilderTest, ArgMinMaxTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  const auto kArgMinMaxKinds = {webnn::mojom::blink::ArgMinMax::Kind::kMin,
                                webnn::mojom::blink::ArgMinMax::Kind::kMax};
  for (const auto kind : kArgMinMaxKinds) {
    SCOPED_TRACE(testing::Message()
                 << "Testing for: " << ArgMinMaxKindToString(kind));
    {
      // Test argMinMax with default options.
      auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLArgMinMaxOptions::Create();
      EXPECT_FALSE(options->hasAxes());
      EXPECT_TRUE(options->hasKeepDimensions());
      EXPECT_FALSE(options->keepDimensions());
      EXPECT_TRUE(options->hasSelectLastIndex());
      EXPECT_FALSE(options->selectLastIndex());
      auto* output = BuildArgMinMax(scope, builder, kind, input, options);
      CheckArgMinMaxOutput(input, output, kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({}));
    }
    {
      // Test argMinMax with axes = {1}.
      auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLArgMinMaxOptions::Create();
      options->setAxes({1});
      auto* output = BuildArgMinMax(scope, builder, kind, input, options);
      CheckArgMinMaxOutput(input, output, kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4}));
    }
    {
      // Test argMinMax with axes = {1, 3} and keepDimensions = true.
      auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLArgMinMaxOptions::Create();
      options->setAxes({1, 3});
      options->setKeepDimensions(true);
      auto* output = BuildArgMinMax(scope, builder, kind, input, options);
      CheckArgMinMaxOutput(input, output, kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 1}));
    }
    {
      // Test argMinMax with axes = {1} and selectLastIndex = true.
      auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLArgMinMaxOptions::Create();
      options->setAxes({1});
      options->setSelectLastIndex(true);
      auto* output = BuildArgMinMax(scope, builder, kind, input, options);
      CheckArgMinMaxOutput(input, output, kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4}));
    }
    {
      // Test throwing exception if the value in axes is greater than or equal
      // to input rank.
      auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLArgMinMaxOptions::Create();
      options->setAxes({4});
      auto* output = BuildArgMinMax(scope, builder, kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The values in axes must be in the range [0, 4).");
    }
    {
      // Test throwing exception if two or more values are same in the axes
      // sequence.
      auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLArgMinMaxOptions::Create();
      options->setAxes({1, 1});
      auto* output = BuildArgMinMax(scope, builder, kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Two or more values are same in the axes sequence.");
    }
  }
}

MLOperand* BuildBatchNormalization(V8TestingScope& scope,
                                   MLGraphBuilder* builder,
                                   const MLOperand* input,
                                   const MLOperand* mean,
                                   const MLOperand* variance,
                                   const MLBatchNormalizationOptions* options) {
  auto* output = builder->batchNormalization(input, mean, variance, options,
                                             scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* batch_normalization = output->Operator();
  EXPECT_THAT(batch_normalization, testing::NotNull());
  EXPECT_EQ(batch_normalization->Kind(),
            webnn::mojom::blink::Operation::Tag::kBatchNormalization);
  EXPECT_TRUE(batch_normalization->IsConnected());
  EXPECT_THAT(batch_normalization->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, BatchNormalizationTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test batchNormalization with default options.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    EXPECT_FALSE(options->hasScale());
    EXPECT_FALSE(options->hasBias());
    EXPECT_FALSE(options->hasActivation());
    EXPECT_TRUE(options->hasAxis());
    EXPECT_EQ(options->axis(), 1u);
    EXPECT_TRUE(options->hasEpsilon());
    EXPECT_FLOAT_EQ(options->epsilon(), 1e-5);
    auto* output =
        BuildBatchNormalization(scope, builder, input, mean, variance, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 5, 5}));
  }
  {
    // Test batchNormalization with axis = 2 and epsilon = 0.0001.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    options->setAxis(2);
    options->setEpsilon(0.0001);
    auto* output =
        BuildBatchNormalization(scope, builder, input, mean, variance, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 5, 5}));
  }
  {
    // Test throwing exception when the input data type is not one of floating
    // point types.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* mean = BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kInt32,
                               scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kInt32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input type must be one of the floating point types.");
  }
  {
    // Test throwing exception when the mean data type is not the same as the
    // input data type.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat16,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "For mean operand: the data type doesn't match the input data type.");
  }
  {
    // Test throwing exception when the mean operand is not a 1-D tensor.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {1, 2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For mean operand: the operand should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the size of mean operand is not equal to
    // the size of the input dimension denoted by axis.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {3}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For mean operand: the size of operand must be equal to the size "
              "of the feature dimension of the input.");
  }
  {
    // Test throwing exception when the variance data type is not the same as
    // the input data type.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kInt32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For variance operand: the data type doesn't match the input "
              "data type.");
  }
  {
    // Test throwing exception when the variance operand is not a 1-D tensor.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For variance operand: the operand should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the size of variance operand is not
    // equal to the size of the input dimension denoted by axis.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    options->setAxis(2);
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For variance operand: the size of operand must be equal to the "
              "size of the feature dimension of the input.");
  }
  {
    // Test throwing exception when the scale data type is not the same as the
    // input data type.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat16,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat16,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* scale =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setScale(scale);
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "For scale operand: the data type doesn't match the input data type.");
  }
  {
    // Test throwing exception when the scale operand is not a 1-D tensor.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* scale =
        BuildConstant(builder, {2, 1}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setScale(scale);
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For scale operand: the operand should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the size of scale operand is not equal
    // to the size of the input dimension denoted by axis.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    options->setAxis(2);
    auto* scale =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setScale(scale);
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For scale operand: the size of operand must be equal to the "
              "size of the feature dimension of the input.");
  }
  {
    // Test throwing exception when the bias data type is not the same as the
    // input data type.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat16,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat16,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* bias =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "For bias operand: the data type doesn't match the input data type.");
  }
  {
    // Test throwing exception when the bias operand is not a 1-D tensor.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    auto* bias =
        BuildConstant(builder, {2, 1}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For bias operand: the operand should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the size of bias operand is not equal to
    // the size of the input dimension denoted by axis.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    options->setAxis(2);
    auto* bias =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For bias operand: the size of operand must be equal to the size "
              "of the feature dimension of the input.");
  }
  {
    // Test throwing exception when the value of axis is not in the range of [0,
    // N-1].
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* mean =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* variance =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLBatchNormalizationOptions::Create();
    options->setAxis(4);
    auto* output = builder->batchNormalization(input, mean, variance, options,
                                               scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The value of axis must be in the range [0, N-1] where N is the "
              "rank of the input tensor.");
  }
}

TEST_F(MLGraphBuilderTest, ConcatTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building Concat with one input.
    Vector<uint32_t> input_a_shape({4, 4, 3});
    Vector<uint32_t> output_shape({4, 4, 3});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    uint32_t axis = 2;
    auto* output = builder->concat({input_a}, axis, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), output_shape);
    const MLOperator* concat = output->Operator();
    ASSERT_THAT(concat, testing::NotNull());
    EXPECT_EQ(concat->Kind(), webnn::mojom::blink::Operation::Tag::kConcat);
    EXPECT_TRUE(concat->IsConnected());
    EXPECT_THAT(concat->Options(), testing::IsNull());
  }
  {
    // Test building Concat with two inputs.
    Vector<uint32_t> input_a_shape({3, 1, 5});
    Vector<uint32_t> input_b_shape({3, 2, 5});
    Vector<uint32_t> output_shape({3, 3, 5});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_b = BuildInput(builder, "input_b", input_b_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    uint32_t axis = 1;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), output_shape);
    const MLOperator* concat = output->Operator();
    ASSERT_THAT(concat, testing::NotNull());
    EXPECT_EQ(concat->Kind(), webnn::mojom::blink::Operation::Tag::kConcat);
    EXPECT_TRUE(concat->IsConnected());
    EXPECT_THAT(concat->Options(), testing::IsNull());
  }
  {
    // Test building Concat with three inputs.
    Vector<uint32_t> input_a_shape({3, 5, 1});
    Vector<uint32_t> input_b_shape({3, 5, 2});
    Vector<uint32_t> input_c_shape({3, 5, 3});
    Vector<uint32_t> output_shape({3, 5, 6});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_b = BuildInput(builder, "input_b", input_b_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_c = BuildInput(builder, "input_c", input_c_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    uint32_t axis = 2;
    auto* output = builder->concat({input_a, input_b, input_c}, axis,
                                   scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), output_shape);
    const MLOperator* concat = output->Operator();
    ASSERT_THAT(concat, testing::NotNull());
    EXPECT_EQ(concat->Kind(), webnn::mojom::blink::Operation::Tag::kConcat);
    EXPECT_TRUE(concat->IsConnected());
    EXPECT_THAT(concat->Options(), testing::IsNull());
  }
  {
    // Test building Concat with two 1D inputs.
    Vector<uint32_t> input_a_shape({1});
    Vector<uint32_t> input_b_shape({1});
    Vector<uint32_t> output_shape({2});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_b = BuildInput(builder, "input_b", input_b_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    uint32_t axis = 0;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), output_shape);
    const MLOperator* concat = output->Operator();
    ASSERT_THAT(concat, testing::NotNull());
    EXPECT_EQ(concat->Kind(), webnn::mojom::blink::Operation::Tag::kConcat);
    EXPECT_TRUE(concat->IsConnected());
    EXPECT_THAT(concat->Options(), testing::IsNull());
  }
  {
    // Test throwing exception when the inputs are empty.
    uint32_t axis = 0;
    auto* output = builder->concat({}, axis, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The inputs should not be empty.");
  }
  {
    // Test throwing exception when the argument types are inconsistent.
    Vector<uint32_t> input_a_shape({1, 1});
    Vector<uint32_t> input_b_shape({1, 1});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_b = BuildInput(builder, "input_b", input_b_shape,
                               V8MLOperandDataType::Enum::kInt32,
                               scope.GetExceptionState());
    uint32_t axis = 0;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data types don't match.");
  }
  {
    // Test throwing exception when the inputs have different dimension.
    Vector<uint32_t> input_a_shape({1, 1});
    Vector<uint32_t> input_b_shape({1, 1, 1});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_b = BuildInput(builder, "input_b", input_b_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    uint32_t axis = 0;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All input tensors must have the same dimension.");
  }
  {
    // Test throwing exception when the axis is equal to or greater than the
    // size of dimension.
    Vector<uint32_t> input_a_shape({1, 1});
    Vector<uint32_t> input_b_shape({1, 1});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_b = BuildInput(builder, "input_b", input_b_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    uint32_t axis = 2;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The axis must be in the range [0, N-1] where N is the rank of "
              "input tensor.");
  }
  {
    // Test throwing exception when concat with two 0-D scalars.
    Vector<uint32_t> input_a_shape({});
    Vector<uint32_t> input_b_shape({});
    Vector<uint32_t> output_shape({2});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_b = BuildInput(builder, "input_b", input_b_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    uint32_t axis = 0;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The axis must be in the range [0, N-1] where N is the rank of "
              "input tensor.");
  }
  {
    // Test throwing exception when the inputs have other axes with different
    // sizes except on the axis.
    Vector<uint32_t> input_a_shape({1, 1, 1});
    Vector<uint32_t> input_b_shape({1, 2, 3});
    auto* input_a = BuildInput(builder, "input_a", input_a_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    auto* input_b = BuildInput(builder, "input_b", input_b_shape,
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
    uint32_t axis = 1;
    auto* output =
        builder->concat({input_a, input_b}, axis, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All input tensors must have the same shape, except for the size "
              "of the dimension to concatenate on.");
  }
}

MLOperand* BuildConv2d(V8TestingScope& scope,
                       MLGraphBuilder* builder,
                       const MLOperand* input,
                       const MLOperand* filter,
                       const MLConv2dOptions* options) {
  auto* output =
      builder->conv2d(input, filter, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* conv2d = output->Operator();
  EXPECT_THAT(conv2d, testing::NotNull());
  EXPECT_EQ(conv2d->Kind(), webnn::mojom::blink::Operation::Tag::kConv2d);
  EXPECT_TRUE(conv2d->IsConnected());
  EXPECT_THAT(conv2d->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, Conv2dTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test conv2d with default options.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    EXPECT_FALSE(options->hasBias());
    EXPECT_FALSE(options->hasDilations());
    EXPECT_FALSE(options->hasActivation());
    EXPECT_TRUE(options->hasFilterLayout());
    EXPECT_EQ(options->filterLayout(),
              V8MLConv2dFilterOperandLayout::Enum::kOihw);
    EXPECT_TRUE(options->hasInputLayout());
    EXPECT_EQ(options->inputLayout(), V8MLInputOperandLayout::Enum::kNchw);
    EXPECT_TRUE(options->hasGroups());
    EXPECT_EQ(options->groups(), 1u);
    EXPECT_FALSE(options->hasPadding());
    EXPECT_FALSE(options->hasStrides());
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with padding=1.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test conv2d with strides=2 and padding=1.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with strides=2 and asymmetric padding.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 4, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 2, 0, 1});
    options->setStrides({2, 2});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test depthwise conv2d by setting groups to input channels.
    auto* input = BuildInput(builder, "input", {1, 4, 2, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {4, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(4);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 4, 1, 1}));
  }
  {
    // Test depthwise conv2d with groups=4, inputLayout="nhwc" and
    // filterLayout="ihwo".
    auto* input = BuildInput(builder, "input", {1, 2, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 2, 4},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(4);
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 1, 4}));
  }
  {
    // Test conv2d with dilations=4, inputLayout="nhwc" and
    // filterLayout="ihwo".
    auto* input = BuildInput(builder, "input", {1, 65, 65, 1},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 3, 3, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    options->setDilations({4, 4});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 57, 57, 1}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="oihw".
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOihw);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="hwio".
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {3, 3, 2, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kHwio);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ohwi".
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 3, 3, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ihwo".
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {2, 3, 3, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="oihw".
    auto* input = BuildInput(builder, "input", {1, 5, 5, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOihw);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="hwio".
    auto* input = BuildInput(builder, "input", {1, 5, 5, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {3, 3, 2, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kHwio);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="ohwi".
    auto* input = BuildInput(builder, "input", {1, 5, 5, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 3, 3, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="ihwo".
    auto* input = BuildInput(builder, "input", {1, 5, 5, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {2, 3, 3, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test throwing exception if the output operand's number of elements is too
    // large.
    // Set the input and filter dimensions that let the output's number of
    // lements be 2 * SIZE_MAX.
    auto* input = BuildInput(
        builder, "input",
        {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
        V8MLOperandDataType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {8, 1, 1, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* output = builder->conv2d(input, filter, MLConv2dOptions::Create(),
                                   scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Invalid operand descriptor: The number of elements is too large.");
  }
  {
    // Test throwing exception if the output operand's byte length is too large.
    // Set the dimensions and data type of input and filter that let the
    // output's byte length be 4 * SIZE_MAX.
    auto* input = BuildInput(
        builder, "input",
        {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
        V8MLOperandDataType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {4, 1, 1, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* output = builder->conv2d(input, filter, MLConv2dOptions::Create(),
                                   scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The byte length is too large.");
  }
  {
    // Test throwing exception when the input is not a 4-D tensor.
    auto* input = BuildInput(builder, "input", {1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 2, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the filter is not a 4-D tensor.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The filter should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the filter data type doesn't match the input
    // data type.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandDataType::Enum::kInt32,
                      scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The filter data type doesn't match the input data type.");
  }
  {
    // Test throwing exception when the length of padding is not 4.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setPadding({2, 2});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of padding should be 4.");
  }
  {
    // Test throwing exception when the length of strides is not 2.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setStrides({2});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of strides should be 2.");
  }
  {
    // Test throwing exception when one stride value is smaller than 1.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setStrides({1, 0});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All strides should be greater than 0.");
  }
  {
    // Test throwing exception when the length of dilations is not 2.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({1});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of dilations should be 2.");
  }
  {
    // Test throwing exception when the one dilation value is smaller than 1.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({1, 0});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All dilations should be greater than 0.");
  }
  {
    // Test throwing exception when input_channels % groups() != 0.
    auto* input = BuildInput(builder, "input", {1, 4, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(3);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups must evenly divide the input "
              "channels to filter input channels.");
  }
  {
    // Test throwing exception when filter_input_channels != input_channels /
    // groups().
    auto* input = BuildInput(builder, "input", {1, 4, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(2);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups must evenly divide the input "
              "channels to filter input channels.");
  }
  {
    // Test throwing exception when the groups is smaller than 1.
    auto* input = BuildInput(builder, "input", {1, 4, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setGroups(0);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups should be greater than 0.");
  }
  {
    // Test throwing exception due to overflow when calculating the effective
    // filter height.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 434983, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({328442, 1});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Failed to calculate the output height: The effective filter size is "
        "too large.");
  }
  {
    // Test throwing exception due to overflow when calculating the effective
    // filter width.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 234545},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({2, 843452});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Failed to calculate the output width: The effective filter size is "
        "too large.");
  }
  {
    // Test throwing exception due to underflow when calculating the output
    // height.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 4, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({4, 1});
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output height: The input size is too "
              "small to fill the window.");
  }
  {
    // Test throwing exception due to underflow when calculating the output
    // width.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 8},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    options->setDilations({1, 4});
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output width: The input size is too "
              "small to fill the window.");
  }
  {
    // Test throwing exception when the bias is not a 1-D tensor.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* bias =
        BuildConstant(builder, {1, 2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the bias shape is not equal to
    // [output_channels].
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* bias =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias shape should be [1].");
  }
  {
    // Test throwing exception when the bias data type doesn't match input data
    // type.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConv2dOptions::Create();
    auto* bias = BuildConstant(builder, {1}, V8MLOperandDataType::Enum::kInt32,
                               scope.GetExceptionState());
    options->setBias(bias);
    auto* output =
        builder->conv2d(input, filter, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias data type doesn't match input data type.");
  }
}

MLOperand* BuildConvTranspose2d(V8TestingScope& scope,
                                MLGraphBuilder* builder,
                                const MLOperand* input,
                                const MLOperand* filter,
                                const MLConvTranspose2dOptions* options) {
  auto* output = builder->convTranspose2d(input, filter, options,
                                          scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* convTranspose2d = output->Operator();
  EXPECT_THAT(convTranspose2d, testing::NotNull());
  EXPECT_EQ(convTranspose2d->Kind(),
            webnn::mojom::blink::Operation::Tag::kConv2d);
  EXPECT_EQ(convTranspose2d->SubKind<webnn::mojom::blink::Conv2d::Kind>(),
            webnn::mojom::blink::Conv2d::Kind::kTransposed);
  EXPECT_TRUE(convTranspose2d->IsConnected());
  EXPECT_THAT(convTranspose2d->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, ConvTranspose2dTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test convTranspose2d with default options.
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    EXPECT_FALSE(options->hasBias());
    EXPECT_FALSE(options->hasDilations());
    EXPECT_FALSE(options->hasActivation());
    EXPECT_TRUE(options->hasFilterLayout());
    EXPECT_EQ(options->filterLayout(),
              V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw);
    EXPECT_TRUE(options->hasInputLayout());
    EXPECT_EQ(options->inputLayout(), V8MLInputOperandLayout::Enum::kNchw);
    EXPECT_TRUE(options->hasGroups());
    EXPECT_EQ(options->groups(), 1u);
    EXPECT_FALSE(options->hasPadding());
    EXPECT_FALSE(options->hasStrides());
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test convTranspose2d with inputLayout="nchw" and filterLayout="hwoi".
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {3, 3, 2, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 5, 5}));
  }
  {
    // Test convTranspose2d with inputLayout="nchw" and filterLayout="ohwi".
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {2, 3, 3, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 5, 5}));
  }
  {
    // Test convTranspose2d with inputLayout="nhwc" and filterLayout="iohw".
    auto* input = BuildInput(builder, "input", {1, 3, 3, 1},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 5, 5, 2}));
  }
  {
    // Test convTranspose2d with inputLayout="nhwc" and filterLayout="hwoi".
    auto* input = BuildInput(builder, "input", {1, 3, 3, 1},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {3, 3, 2, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 5, 5, 2}));
  }
  {
    // Test convTranspose2d with inputLayout="nhwc" and filterLayout="ohwi".
    auto* input = BuildInput(builder, "input", {1, 3, 3, 1},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {2, 3, 3, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 5, 5, 2}));
  }
  {
    // Test convTranspose2d with strides=[3, 2], outputSizes=[10, 8].
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputSizes({10, 8});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 10, 8}));
  }
  {
    // Test convTranspose2d with strides=[3, 2], outputPadding=[1, 1].
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputPadding({1, 1});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 10, 8}));
  }
  {
    // Test convTranspose2d with padding=1.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test convTranspose2d with padding=1, groups=3.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setGroups(3);
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 5, 5}));
  }
  {
    // Test convTranspose2d with strides=2.
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({2, 2});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 7, 7}));
  }
  {
    // Test convTranspose2d with strides=2 and padding=1.
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test convTranspose2d with outputSizes and outputPadding. When the output
    // sizes are explicitly specified, the output padding values are ignored.
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputPadding({1, 1});
    options->setOutputSizes({10, 8});
    auto* output = BuildConvTranspose2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 10, 8}));
  }
  {
    // Test throwing exception if the output operand's number of elements is too
    // large.
    // Set the input and filter dimensions that let the output's number of
    // lements be 2 * SIZE_MAX.
    auto* input = BuildInput(
        builder, "input",
        {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
        V8MLOperandDataType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 8, 1, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Invalid operand descriptor: The number of elements is too large.");
  }
  {
    // Test throwing exception if the output operand's byte length is too large.
    // Set the dimensions and data type of input and filter that let the
    // output's byte length be 4 * SIZE_MAX.
    auto* input = BuildInput(
        builder, "input",
        {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
        V8MLOperandDataType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 4, 1, 1},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The byte length is too large.");
  }
  {
    // Test throwing exception when the input is not a 4-D tensor.
    auto* input = BuildInput(builder, "input", {1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the filter is not a 4-D tensor.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The filter should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the filter data type doesn't match the input
    // data type.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 2, 2}, V8MLOperandDataType::Enum::kInt32,
                      scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The filter data type doesn't match the input data type.");
  }
  {
    // Test throwing exception when the length of padding is not 4.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({2, 2});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of padding should be 4.");
  }
  {
    // Test throwing exception when the length of strides is not 2.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({2});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of strides should be 2.");
  }
  {
    // Test throwing exception when one stride value is smaller than 1.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({1, 0});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All strides should be greater than 0.");
  }
  {
    // Test throwing exception when the length of dilations is not 2.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setDilations({1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of dilations should be 2.");
  }
  {
    // Test throwing exception when the one dilation value is smaller than 1.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setDilations({1, 0});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All dilations should be greater than 0.");
  }
  {
    // Test throwing exception when the input channels is not equal to the
    // filter input channels.
    auto* input = BuildInput(builder, "input", {1, 4, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setGroups(3);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input channels should equal to filter input channels.");
  }
  {
    // Test throwing exception when output channels is too large.
    auto* input = BuildInput(builder, "input", {1, 4, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {4, 2, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setGroups(std::numeric_limits<uint32_t>::max());
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The output channels is too large.");
  }
  {
    // Test throwing exception when the groups is smaller than 1.
    auto* input = BuildInput(builder, "input", {1, 4, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setGroups(0);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The groups should be greater than 0.");
  }
  {
    // Test throwing exception due to overflow when calculating the effective
    // filter height.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 434983, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setDilations({328442, 1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Failed to calculate the output height: The effective filter size is "
        "too large.");
  }
  {
    // Test throwing exception due to overflow when calculating the effective
    // filter width.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 234545},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setDilations({2, 843452});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Failed to calculate the output width: The effective filter size is "
        "too large.");
  }
  {
    // Test throwing exception when the bias is not a 1-D tensor.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* bias =
        BuildConstant(builder, {1, 2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the bias shape is not equal to
    // [output_channels].
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* bias =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias shape should be [1].");
  }
  {
    // Test throwing exception when the bias data type doesn't match input data
    // type.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 2, 2},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    auto* bias = BuildConstant(builder, {1}, V8MLOperandDataType::Enum::kInt32,
                               scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The bias data type doesn't match input data type.");
  }
  {
    // Test throwing exception when the outputPadding is not a sequence of
    // length 2.
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputPadding({1, 1, 1, 1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of output padding should be 2.");
  }
  {
    // Test throwing exception when the outputPadding is greater than stride
    // along the same dimension.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({0, 0, 3, 3});
    options->setStrides({2, 2});
    options->setOutputPadding({0, 2});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The output padding must be smaller than the stride along the "
              "same dimension.");
  }
  {
    // Test throwing exception when the outputSizes is not a sequence of
    // length 2.
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 2, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setStrides({3, 2});
    options->setOutputSizes({1, 2, 10, 8});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of output sizes should be 2.");
  }
  {
    // Test throwing exception due to underflow when calculating the output
    // height.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({4, 4, 0, 0});
    options->setStrides({2, 2});
    options->setOutputPadding({1, 0});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output height: The stride is too large "
              "or the input size is too small for padding.");
  }
  {
    // Test throwing exception due to outputSizes values are smaller than the
    // output sizes calculated by not using outputPadding.
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    options->setOutputSizes({4, 4});
    options->setOutputPadding({1, 1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The height of output sizes is invalid.");
  }
  {
    // Test throwing exception due to outputSizes values are greater than the
    // output sizes calculated by not using outputPadding.
    auto* input = BuildInput(builder, "input", {1, 1, 3, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* options = MLConvTranspose2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    options->setOutputSizes({6, 8});
    options->setOutputPadding({1, 1});
    auto* output = builder->convTranspose2d(input, filter, options,
                                            scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The width of output sizes is invalid.");
  }
}

MLOperand* BuildPool2d(V8TestingScope& scope,
                       MLGraphBuilder* builder,
                       webnn::mojom::blink::Pool2d::Kind kind,
                       const MLOperand* input,
                       const MLPool2dOptions* options) {
  MLOperand* output = nullptr;
  switch (kind) {
    case webnn::mojom::blink::Pool2d::Kind::kAveragePool2d:
      output =
          builder->averagePool2d(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Pool2d::Kind::kL2Pool2d:
      output = builder->l2Pool2d(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Pool2d::Kind::kMaxPool2d:
      output = builder->maxPool2d(input, options, scope.GetExceptionState());
      break;
  }
  return output;
}

void CheckPool2dOutput(const MLOperand* input,
                       const MLOperand* output,
                       webnn::mojom::blink::Pool2d::Kind kind) {
  ASSERT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* pool2d = output->Operator();
  ASSERT_THAT(pool2d, testing::NotNull());
  EXPECT_EQ(pool2d->Kind(), webnn::mojom::blink::Operation::Tag::kPool2d);
  EXPECT_EQ(pool2d->SubKind<webnn::mojom::blink::Pool2d::Kind>(), kind);
  EXPECT_TRUE(pool2d->IsConnected());
  EXPECT_THAT(pool2d->Options(), testing::NotNull());
}

TEST_F(MLGraphBuilderTest, Pool2dTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  const auto kPool2dKinds = {webnn::mojom::blink::Pool2d::Kind::kAveragePool2d,
                             webnn::mojom::blink::Pool2d::Kind::kL2Pool2d,
                             webnn::mojom::blink::Pool2d::Kind::kMaxPool2d};
  static_assert(
      1 + static_cast<size_t>(webnn::mojom::blink::Pool2d::Kind::kMaxValue) ==
      3);
  for (const auto pool2d_kind : kPool2dKinds) {
    {
      // Test pool2d with default options.
      auto* input = BuildInput(builder, "input", {1, 3, 4, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      EXPECT_FALSE(options->hasWindowDimensions());
      EXPECT_FALSE(options->hasPadding());
      EXPECT_FALSE(options->hasStrides());
      EXPECT_FALSE(options->hasDilations());
      EXPECT_TRUE(options->hasLayout());
      EXPECT_EQ(options->layout(), V8MLInputOperandLayout::Enum::kNchw);
      EXPECT_TRUE(options->hasRoundingType());
      EXPECT_EQ(options->roundingType(), V8MLRoundingType::Enum::kFloor);
      EXPECT_FALSE(options->hasOutputSizes());
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 1, 1}));
    }
    {
      // Test pool2d without padding.
      auto* input = BuildInput(builder, "input", {1, 3, 4, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 2, 2}));
    }
    {
      // Test pool2d with padding=2.
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({5, 5});
      options->setPadding({2, 2, 2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 5, 5}));
    }
    {
      // Test pool2d with strides=2.
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 2});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 2, 2}));
    }
    {
      // Test pool2d with strides=2 and padding=1.
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2 and asymmetric padding.
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({2, 1, 2, 1});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with strides=2, padding=1 and roundingType="floor".
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setRoundingType(V8MLRoundingType::Enum::kFloor);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2, padding=1 and roundingType="ceil".
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setRoundingType(V8MLRoundingType::Enum::kCeil);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with strides=2, padding=1 and outputSizes=[3, 3].
      // When the output sizes are explicitly specified, the
      // options.roundingType is ignored.
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setRoundingType(V8MLRoundingType::Enum::kCeil);
      options->setOutputSizes({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2, padding=1 and outputSizes=[3, 3].
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setOutputSizes({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2, padding=1 and outputSizes=[4, 4].
      auto* input = BuildInput(builder, "input", {1, 3, 7, 7},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setOutputSizes({4, 4});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with layout="nchw".
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setLayout(V8MLInputOperandLayout::Enum::kNchw);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3, 3}));
    }
    {
      // Test pool2d with layout="nhwc".
      auto* input = BuildInput(builder, "input", {1, 5, 5, 2},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      CheckPool2dOutput(input, output, pool2d_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 2}));
    }
    {
      // Test throwing exception if the output operand's byte length is too
      // large.
      // Set the data type and sizes of input, padding and window that let the
      // output operands' byte length be greater than SIZE_MAX.
      auto* input = BuildInput(
          builder, "input",
          {1, 1, kSquareRootOfSizeMax / 2, kSquareRootOfSizeMax / 2},
          V8MLOperandDataType::Enum::kFloat32, scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({1, 1});
      options->setPadding({2, 2, 2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Invalid operand descriptor: The byte length is too large.");
    }
    {
      // Test throwing exception when the input is not a 4-D tensor.
      auto* input = BuildInput(builder, "input", {1, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The input should be a 4-D tensor.");
    }
    {
      // Test throwing exception when the output size is incorrect.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 2});
      options->setPadding({2, 2, 2, 2});
      options->setStrides({2, 2});
      options->setOutputSizes({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The output sizes should be either [4, 4] or [5, 5].");
    }
    {
      // Test throwing exception when the length of output size is not 2.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 2});
      options->setPadding({2, 2, 2, 2});
      options->setStrides({2, 2});
      options->setOutputSizes({1, 2, 4, 4});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of output sizes should be 2.");
    }
    {
      // Test throwing exception when the length of window dimensions is not 2.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({1, 1, 1, 1});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of window dimensions should be 2.");
    }
    {
      // Test throwing exception when not all window dimensions is greater than
      // or equal to 1.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({0, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "All window dimensions should be greater than 0.");
    }
    {
      // Test throwing exception when the input height is too small to fill the
      // pool window height.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({8, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Failed to calculate the output height: The input size is too "
                "small to fill the window.");
    }
    {
      // Test throwing exception when the input width is too small to fill the
      // pool window width.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 8});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Failed to calculate the output width: The input size is too "
                "small to fill the window.");
    }
    {
      // Test throwing exception when the calculated output height is equal to
      // 0.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({6, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(
          scope.GetExceptionState().Message(),
          "Invalid operand descriptor: All dimensions should be positive.");
    }
    {
      // Test throwing exception when the length of padding is not 4.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setPadding({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of padding should be 4.");
    }
    {
      // Test throwing exception when the length of strides is not 2.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setStrides({2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of strides should be 2.");
    }
    {
      // Test throwing exception when one stride value is smaller than 1.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setStrides({0, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "All strides should be greater than 0.");
    }
    {
      // Test throwing exception when the length of dilations is not 2.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setDilations({1, 1, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The length of dilations should be 2.");
    }
    {
      // Test throwing exception when one dilation value is smaller than 1.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLPool2dOptions::Create();
      options->setDilations({1, 0});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "All dilations should be greater than 0.");
    }
  }
}

TEST_F(MLGraphBuilderTest, PReluTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building prelu when slope_shape is the same as the input_shape.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {3, 2, 5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* p_relu = output->Operator();
    ASSERT_THAT(p_relu, testing::NotNull());
    EXPECT_EQ(p_relu->Kind(), webnn::mojom::blink::Operation::Tag::kPrelu);
    EXPECT_TRUE(p_relu->IsConnected());
    EXPECT_THAT(p_relu->Options(), testing::IsNull());
  }
  {
    // Test building prelu with input_shape = {3, 2, 5} and slope_shape = {5}.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* p_relu = output->Operator();
    ASSERT_THAT(p_relu, testing::NotNull());
    EXPECT_EQ(p_relu->Kind(), webnn::mojom::blink::Operation::Tag::kPrelu);
    EXPECT_TRUE(p_relu->IsConnected());
    EXPECT_THAT(p_relu->Options(), testing::IsNull());
  }
  {
    // Test building prelu with input_shape = {3, 2, 5} and slope_shape = {}.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* p_relu = output->Operator();
    ASSERT_THAT(p_relu, testing::NotNull());
    EXPECT_EQ(p_relu->Kind(), webnn::mojom::blink::Operation::Tag::kPrelu);
    EXPECT_TRUE(p_relu->IsConnected());
    EXPECT_THAT(p_relu->Options(), testing::IsNull());
  }
  {
    // Test building prelu with input_shape = {3, 2, 5} and slope_shape = {2,
    // 5}.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {2, 5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* p_relu = output->Operator();
    ASSERT_THAT(p_relu, testing::NotNull());
    EXPECT_EQ(p_relu->Kind(), webnn::mojom::blink::Operation::Tag::kPrelu);
    EXPECT_TRUE(p_relu->IsConnected());
    EXPECT_THAT(p_relu->Options(), testing::IsNull());
  }
  {
    // Test building prelu with input_shape = {3, 2, 5} and slope_shape = {2}.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The shape of slope is not broadcastable to the shape of input.",
              scope.GetExceptionState().Message());
  }
  {
    // Test building prelu with input_shape = {5, 1, 2} and slope_shape = {2,
    // 2}.
    Vector<uint32_t> input_shape({5, 1, 2});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* slope =
        BuildConstant(builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The shape of slope is not broadcastable to the shape of input.",
              scope.GetExceptionState().Message());
  }
  {
    // Test building prelu with input_type = float and slope_type = int32.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* slope = BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kInt32,
                                scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The data type of slope doesn't match the data type of input.",
              scope.GetExceptionState().Message());
  }
  {
    // Test building prelu with input_type = int32.
    Vector<uint32_t> input_shape({3, 2, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* slope = BuildConstant(builder, {5}, V8MLOperandDataType::Enum::kInt32,
                                scope.GetExceptionState());
    auto* output = builder->prelu(input, slope, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        "The data type of input and slope must be one of the floating point "
        "types.",
        scope.GetExceptionState().Message());
  }
}

TEST_F(MLGraphBuilderTest, ReluTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building relu with float32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->relu(input, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* relu = output->Operator();
    ASSERT_THAT(relu, testing::NotNull());
    EXPECT_EQ(relu->Kind(), webnn::mojom::blink::Operation::Tag::kRelu);
    EXPECT_TRUE(relu->IsConnected());
    EXPECT_THAT(relu->Options(), testing::IsNull());
  }
  {
    // Test building relu with int32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* output = builder->relu(input, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kInt32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* relu = output->Operator();
    ASSERT_THAT(relu, testing::NotNull());
    EXPECT_EQ(relu->Kind(), webnn::mojom::blink::Operation::Tag::kRelu);
    EXPECT_TRUE(relu->IsConnected());
    EXPECT_THAT(relu->Options(), testing::IsNull());
  }
  {
    // Test building relu activation.
    MLActivation* relu = builder->relu(scope.GetExceptionState());
    ASSERT_THAT(relu, testing::NotNull());
    ASSERT_THAT(relu->Operator(), testing::NotNull());
    EXPECT_EQ(relu->Kind(), webnn::mojom::blink::Activation::Tag::kRelu);
    EXPECT_FALSE(relu->Operator()->IsConnected());
    EXPECT_THAT(relu->Operator()->Options(), testing::IsNull());
  }
}

MLOperand* BuildHardSigmoid(V8TestingScope& scope,
                            MLGraphBuilder* builder,
                            const MLOperand* input,
                            const MLHardSigmoidOptions* options) {
  auto* output =
      builder->hardSigmoid(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  EXPECT_EQ(output->Dimensions(), input->Dimensions());
  auto* hard_sigmoid = output->Operator();
  EXPECT_THAT(hard_sigmoid, testing::NotNull());
  EXPECT_EQ(hard_sigmoid->Kind(),
            webnn::mojom::blink::Operation::Tag::kHardSigmoid);
  EXPECT_TRUE(hard_sigmoid->IsConnected());
  EXPECT_THAT(hard_sigmoid->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, HardSigmoidTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building hardSigmoid with a scalar input and default options.
    auto* input =
        BuildInput(builder, "input", {}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    BuildHardSigmoid(scope, builder, input);
  }
  {
    // Test building hardSigmoid with a 2-D input and alpha = 0.1, beta = 0.2.
    auto* input = BuildInput(builder, "input", {3, 4},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* options = MLHardSigmoidOptions::Create();
    options->setAlpha(0.1);
    options->setBeta(0.2);
    BuildHardSigmoid(scope, builder, input, options);
  }
  {
    // Test building hardSigmoid activation.
    auto* output = builder->hardSigmoid(MLHardSigmoidOptions::Create(),
                                        scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    const MLOperator* hard_sigmoid = output->Operator();
    ASSERT_THAT(hard_sigmoid, testing::NotNull());
    EXPECT_EQ(hard_sigmoid->Kind(),
              webnn::mojom::blink::Operation::Tag::kHardSigmoid);
    EXPECT_FALSE(hard_sigmoid->IsConnected());
    ASSERT_THAT(hard_sigmoid->Options(), testing::NotNull());
  }
  {
    // Test throwing exception when building hardSigmoid with int32 input.
    Vector<uint32_t> input_shape({3, 4});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* output = builder->hardSigmoid(input, MLHardSigmoidOptions::Create(),
                                        scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
}

TEST_F(MLGraphBuilderTest, HardSwishTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test building hard-swish with float32 input.
    auto* input = BuildInput(builder, "input", {3, 4, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->hardSwish(input, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({3, 4, 5}));
    auto* hard_swish = output->Operator();
    ASSERT_THAT(hard_swish, testing::NotNull());
    EXPECT_EQ(hard_swish->Kind(),
              webnn::mojom::blink::Operation::Tag::kHardSwish);
    EXPECT_TRUE(hard_swish->IsConnected());
    EXPECT_THAT(hard_swish->Options(), testing::IsNull());
  }
  {
    // Test throwing exception when building hard-swish with int32 input.
    auto* input = BuildInput(builder, "input", {3, 4, 5},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* output = builder->hardSwish(input, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The input data type must be one of the float32,float16 types.",
              scope.GetExceptionState().Message());
  }
}

MLOperand* BuildGemm(V8TestingScope& scope,
                     MLGraphBuilder* builder,
                     const MLOperand* a,
                     const MLOperand* b,
                     const MLGemmOptions* options) {
  auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), a->DataType());
  auto* gemm = output->Operator();
  EXPECT_THAT(gemm, testing::NotNull());
  EXPECT_EQ(gemm->Kind(), webnn::mojom::blink::Operation::Tag::kGemm);
  EXPECT_TRUE(gemm->IsConnected());
  EXPECT_THAT(gemm->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, GemmTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  ASSERT_THAT(builder, testing::NotNull());
  {
    // Test building gemm with default option.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    EXPECT_FALSE(options->hasC());
    EXPECT_TRUE(options->hasAlpha());
    EXPECT_EQ(options->alpha(), 1);
    EXPECT_TRUE(options->hasBeta());
    EXPECT_EQ(options->beta(), 1);
    EXPECT_TRUE(options->hasATranspose());
    EXPECT_FALSE(options->aTranspose());
    EXPECT_TRUE(options->hasBTranspose());
    EXPECT_FALSE(options->bTranspose());
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with two matrices - {2, 3} and {2, 4} that can't be
    // multiplied together due to incompatible dimensions.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {2, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (3) in the first matrix isn't equal to the "
        "number of rows (2) in the second matrix.");
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it's compatible with
    // b_dimensions {2, 4}.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {2, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    options->setATranspose(true);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({3, 4}));
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it can't be multiplied with
    // b_dimensions {3, 4}.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    options->setATranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (2) in the transposed first matrix isn't equal "
        "to the number of rows (3) in the second matrix.");
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {3, 4} and it's compatible with
    // a_dimensions {2, 3}.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {4, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    options->setBTranspose(true);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {4, 3} and it's incompatible with
    // a_dimensions {2, 3}.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (3) in the first matrix isn't equal to the "
        "number of rows (4) in the transposed second matrix.");
  }
  {
    // Test building gemm with a_dimensions = {2, 3, 1}.
    // Test throwing an error due to input_a is not a 2-D tensor.
    auto* a =
        BuildInput(builder, "a", {2, 3, 1}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {2, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The first input must be a 2-D tensor.");
  }
  {
    // Test building gemm with two mismatching input types.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {3, 4}, V8MLOperandDataType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The data types of first two inputs don't match.");
  }
  {
    // Test building gemm with setting optional input C.
    // The output dimensions of a * b would be {2, 4} and
    // c_dimensions {4} is able to broadcast to {2, 4}.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c = BuildInput(builder, "c", {4}, V8MLOperandDataType::Enum::kFloat32,
                         scope.GetExceptionState());
    options->setC(c);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with setting optional input C.
    // The output dimensions of a * b would be {2, 4} and
    // c_dimensions is a scalar.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c = BuildInput(builder, "c", {}, V8MLOperandDataType::Enum::kFloat32,
                         scope.GetExceptionState());
    options->setC(c);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // The output dimensions of a * b would be {2, 4} and
    // c_dimension {2, 3} is incompatible with {2, 4}.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    options->setC(c);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The third input tensor isn't unidirectionally broadcastable to "
              "the output tensor.");
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // Set optional input C with data type = int32 and it mismatches with input
    // data type float32.
    auto* a =
        BuildInput(builder, "a", {3, 2}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {4, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c =
        BuildInput(builder, "c", {2, 4}, V8MLOperandDataType::Enum::kInt32,
                   scope.GetExceptionState());
    options->setC(c);
    options->setATranspose(true);
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The third input data type doesn't match other inputs' data type.");
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // Set optional input C with dimensions = {2, 3, 4} and an error should be
    // thrown since c_dimensions is not a 2-D tensor.
    auto* a =
        BuildInput(builder, "a", {3, 2}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {4, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLGemmOptions::Create();
    auto* c =
        BuildInput(builder, "c", {2, 3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    options->setC(c);
    options->setATranspose(true);
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The third input tensor should be either a scalar or a 2-D tensor.");
  }
  {
    // Test throwing exception if the output operand's byte length is too large.
    // Set the type and dimensions of inputs that let the output operand's byte
    // length be 4 * SIZE_MAX.
    auto* a = BuildInput(builder, "a", {kSquareRootOfSizeMax, 2},
                         V8MLOperandDataType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {2, kSquareRootOfSizeMax},
                         V8MLOperandDataType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* output =
        builder->gemm(a, b, MLGemmOptions::Create(), scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The byte length is too large.");
  }
}

std::string ElementWiseBinaryKindToString(
    webnn::mojom::blink::ElementWiseBinary::Kind kind) {
  switch (kind) {
    case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
      return "add";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
      return "sub";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
      return "mul";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
      return "div";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
      return "min";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
      return "max";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
      return "pow";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kEqual:
      return "equal";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreater:
      return "greater";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual:
      return "greaterOrEqual";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesser:
      return "lesser";
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual:
      return "lesserOrEqual";
  }
}

std::string TestParamElementWiseBinaryKindToString(
    const ::testing::TestParamInfo<
        webnn::mojom::blink::ElementWiseBinary::Kind>& info) {
  return ElementWiseBinaryKindToString(info.param);
}

MLOperand* BuildElementWiseBinaryOperator(
    MLGraphBuilder* builder,
    V8TestingScope& scope,
    const MLOperand* a,
    const MLOperand* b,
    webnn::mojom::blink::ElementWiseBinary::Kind kind) {
  switch (kind) {
    case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
      return builder->add(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
      return builder->sub(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
      return builder->mul(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
      return builder->div(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
      return builder->min(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
      return builder->max(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
      return builder->pow(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kEqual:
      return builder->equal(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreater:
      return builder->greater(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual:
      return builder->greaterOrEqual(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesser:
      return builder->lesser(a, b, scope.GetExceptionState());
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual:
      return builder->lesserOrEqual(a, b, scope.GetExceptionState());
  }
}

// TODO: crbug.com/325598628 - Consolidate this with the method of the same name
// in third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.cc.
constexpr bool IsLogicalBinaryOperator(
    webnn::mojom::blink::ElementWiseBinary::Kind kind) {
  switch (kind) {
    case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
      return false;
    case webnn::mojom::blink::ElementWiseBinary::Kind::kEqual:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreater:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesser:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual:
      return true;
  }
}

MLOperand* BuildElementWiseBinary(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    const MLOperand* a,
    const MLOperand* b) {
  MLOperand* output =
      BuildElementWiseBinaryOperator(builder, scope, a, b, kind);
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);

  if (IsLogicalBinaryOperator(kind)) {
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kUint8);
  } else {
    EXPECT_EQ(output->DataType(), a->DataType());
  }

  auto* op = output->Operator();
  EXPECT_THAT(op, testing::NotNull());
  EXPECT_EQ(op->Kind(),
            webnn::mojom::blink::Operation::Tag::kElementWiseBinary);
  EXPECT_EQ(op->SubKind<webnn::mojom::blink::ElementWiseBinary::Kind>(), kind);
  EXPECT_TRUE(op->IsConnected());
  return output;
}

class MLGraphBuilderElementWiseBinaryTest
    : public testing::TestWithParam<
          webnn::mojom::blink::ElementWiseBinary::Kind> {
 public:
  void TestElementWiseBinary() {
    V8TestingScope scope;
    MLGraphBuilder* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    const auto& kind = GetParam();
    SCOPED_TRACE(testing::Message() << "Testing operator: "
                                    << ElementWiseBinaryKindToString(kind));
    {
      // Testing building with two input dimensions - {8, 1, 6, 1} and {7, 1,
      // 5}. Both the a and b dimensions have axes with length one that are
      // expanded to a larger size during the broadcast operation.
      // a_dimensions     (4d) 8 * 1 * 6 * 1
      // b_dimensions     (3d)     7 * 1 * 5
      // output_dimenions (4d) 8 * 7 * 6 * 5
      auto* a = BuildInput(builder, "a", {8, 1, 6, 1},
                           V8MLOperandDataType::Enum::kFloat32,
                           scope.GetExceptionState());
      auto* b = BuildInput(builder, "b", {7, 1, 5},
                           V8MLOperandDataType::Enum::kFloat32,
                           scope.GetExceptionState());
      auto* output = BuildElementWiseBinary(scope, builder, kind, a, b);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({8, 7, 6, 5}));
    }
    {
      // Testing building with two input dimensions - {4, 2, 1} and {4}.
      // a_dimensions     (3d) 4 * 2 * 1
      // b_dimensions     (1d)         4
      // output_dimenions (3d) 4 * 2 * 4
      auto* a = BuildInput(builder, "a", {4, 2, 1},
                           V8MLOperandDataType::Enum::kFloat32,
                           scope.GetExceptionState());
      auto* b =
          BuildInput(builder, "b", {4}, V8MLOperandDataType::Enum::kFloat32,
                     scope.GetExceptionState());
      auto* output = BuildElementWiseBinary(scope, builder, kind, a, b);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 2, 4}));
    }
    {
      // Testing scalar broadcasting by building with two input dimensions -
      // {4, 2, 4} and {}.
      auto* a = BuildInput(builder, "a", {4, 2, 4},
                           V8MLOperandDataType::Enum::kFloat32,
                           scope.GetExceptionState());
      auto* b =
          BuildInput(builder, "b", {}, V8MLOperandDataType::Enum::kFloat32,
                     scope.GetExceptionState());
      auto* output = BuildElementWiseBinary(scope, builder, kind, a, b);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 2, 4}));
    }
    {
      // Test throwing exception when the input shapes are not broadcastable.
      auto* a =
          BuildInput(builder, "a", {4, 2}, V8MLOperandDataType::Enum::kFloat32,
                     scope.GetExceptionState());
      auto* b =
          BuildInput(builder, "b", {4}, V8MLOperandDataType::Enum::kFloat32,
                     scope.GetExceptionState());
      auto* output = BuildElementWiseBinaryOperator(builder, scope, a, b, kind);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
                scope.GetExceptionState().Code());
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The input shapes are not broadcastable.");
    }
    {
      // Test throwing exception when the input types don't match.
      auto* a =
          BuildInput(builder, "a", {4, 2}, V8MLOperandDataType::Enum::kFloat32,
                     scope.GetExceptionState());
      auto* b = BuildInput(builder, "b", {1}, V8MLOperandDataType::Enum::kInt32,
                           scope.GetExceptionState());
      auto* output = BuildElementWiseBinaryOperator(builder, scope, a, b, kind);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
                scope.GetExceptionState().Code());
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The input operand data types don't match.");
    }
  }

 private:
  test::TaskEnvironment task_environment_;
};

TEST_P(MLGraphBuilderElementWiseBinaryTest, TestElementWiseBinary) {
  TestElementWiseBinary();
}

constexpr webnn::mojom::blink::ElementWiseBinary::Kind
    kAllElementWiseBinaryOperators[] = {
        webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
        webnn::mojom::blink::ElementWiseBinary::Kind::kSub,
        webnn::mojom::blink::ElementWiseBinary::Kind::kMul,
        webnn::mojom::blink::ElementWiseBinary::Kind::kDiv,
        webnn::mojom::blink::ElementWiseBinary::Kind::kMin,
        webnn::mojom::blink::ElementWiseBinary::Kind::kMax,
        webnn::mojom::blink::ElementWiseBinary::Kind::kPow,
        webnn::mojom::blink::ElementWiseBinary::Kind::kEqual,
        webnn::mojom::blink::ElementWiseBinary::Kind::kGreater,
        webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual,
        webnn::mojom::blink::ElementWiseBinary::Kind::kLesser,
        webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual,
};
static_assert(
    1 + static_cast<size_t>(
            webnn::mojom::blink::ElementWiseBinary::Kind::kMaxValue) ==
    12);

INSTANTIATE_TEST_SUITE_P(ElementWiseBinaryTest,
                         MLGraphBuilderElementWiseBinaryTest,
                         ::testing::ValuesIn(kAllElementWiseBinaryOperators),
                         TestParamElementWiseBinaryKindToString);

template <typename T>
struct ElementWiseUnaryTester {
  webnn::mojom::blink::ElementWiseUnary::Kind kind;
  OperandInfo<T> input_info;

  MLOperand* BuildElementWiseUnary(V8TestingScope& scope) {
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input = BuildInput(builder, "input", input_info.dimensions,
                             input_info.data_type, scope.GetExceptionState());
    MLOperand* output = nullptr;
    switch (kind) {
      case webnn::mojom::blink::ElementWiseUnary::Kind::kAbs:
        output = builder->abs(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kCeil:
        output = builder->ceil(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kCos:
        output = builder->cos(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kExp:
        output = builder->exp(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kFloor:
        output = builder->floor(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kLog:
        output = builder->log(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kNeg:
        output = builder->neg(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kSin:
        output = builder->sin(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kTan:
        output = builder->tan(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kErf:
        output = builder->erf(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kIdentity:
        output = builder->identity(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kLogicalNot:
        output = builder->logicalNot(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kReciprocal:
        output = builder->reciprocal(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kSqrt:
        output = builder->sqrt(input, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kCast:
        // TODO: crbug.com/325598628 - Add tests for this case.
        break;
    }
    return output;
  }

  // Test valid arguments of operators.
  void Test(V8TestingScope& scope) {
    MLOperand* output = BuildElementWiseUnary(scope);
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), input_info.data_type);
    EXPECT_EQ(output->Dimensions(), input_info.dimensions);
    auto* op = output->Operator();
    ASSERT_THAT(op, testing::NotNull());
    EXPECT_EQ(op->Kind(),
              webnn::mojom::blink::Operation::Tag::kElementWiseUnary);
    EXPECT_EQ(op->SubKind<webnn::mojom::blink::ElementWiseUnary::Kind>(), kind);
    EXPECT_TRUE(op->IsConnected());
    EXPECT_THAT(op->Options(), testing::IsNull());
  }
};

TEST_F(MLGraphBuilderTest, ElementWiseUnaryTest) {
  V8TestingScope scope;
  {
    // Test building element-wise exp for scalar input.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kExp,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {}}}
        .Test(scope);
  }
  {
    // Test building element-wise abs.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kAbs,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {2}}}
        .Test(scope);
  }
  {
    // Test building element-wise ceil.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kCeil,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {1, 2}}}
        .Test(scope);
  }
  {
    // Test building element-wise cos.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kCos,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {5, 6}}}
        .Test(scope);
  }
  {
    // Test building element-wise exp.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kExp,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {8, 5, 6}}}
        .Test(scope);
  }
  {
    // Test building element-wise floor.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kFloor,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {1, 2, 3}}}
        .Test(scope);
  }
  {
    // Test building element-wise log.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kLog,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {8, 6}}}
        .Test(scope);
  }
  {
    // Test building element-wise neg.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kNeg,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {1, 2, 3, 4}}}
        .Test(scope);
  }
  {
    // Test building element-wise sin.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kSin,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {6}}}
        .Test(scope);
  }
  {
    // Test building element-wise tan.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kTan,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {8, 6, 2}}}
        .Test(scope);
  }
  {
    // Test throwing exception when building ceil with int32 input.
    const MLOperand* output = ElementWiseUnaryTester<int32_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kCeil,
        .input_info = {
            .data_type = V8MLOperandDataType::Enum::kInt32,
            .dimensions = {3, 4}}}.BuildElementWiseUnary(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test throwing exception when building exp with int32 input.
    const MLOperand* output = ElementWiseUnaryTester<uint32_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kExp,
        .input_info = {
            .data_type = V8MLOperandDataType::Enum::kUint32,
            .dimensions = {3, 4}}}.BuildElementWiseUnary(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test throwing exception when building floor with int32 input.
    const MLOperand* output = ElementWiseUnaryTester<int32_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kFloor,
        .input_info = {
            .data_type = V8MLOperandDataType::Enum::kInt32,
            .dimensions = {3, 4}}}.BuildElementWiseUnary(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test throwing exception when building sin with int32 input.
    const MLOperand* output = ElementWiseUnaryTester<uint32_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kSin,
        .input_info = {
            .data_type = V8MLOperandDataType::Enum::kUint32,
            .dimensions = {3, 4}}}.BuildElementWiseUnary(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test throwing exception when building neg with uint32 input.
    const MLOperand* output = ElementWiseUnaryTester<uint32_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kNeg,
        .input_info = {
            .data_type = V8MLOperandDataType::Enum::kUint32,
            .dimensions = {3, 4}}}.BuildElementWiseUnary(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the "
              "float32,float16,int32,int64,int8 types.");
  }
  {
    // Test throwing exception when building abs with uint8 input.
    const MLOperand* output = ElementWiseUnaryTester<uint8_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kAbs,
        .input_info = {
            .data_type = V8MLOperandDataType::Enum::kUint8,
            .dimensions = {3, 4}}}.BuildElementWiseUnary(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the "
              "float32,float16,int32,int64,int8 types.");
  }
  {
    // Test building element-wise Erf.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kErf,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {8, 6, 2}}}
        .Test(scope);
  }
  {
    // Test building element-wise reciprocal.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kReciprocal,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {8, 6, 2}}}
        .Test(scope);
  }
  {
    // Test building element-wise sqrt.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kSqrt,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {8, 6, 2}}}
        .Test(scope);
  }
  {
    // Test building element-wise logical not.
    ElementWiseUnaryTester<uint8_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kLogicalNot,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kUint8,
                       .dimensions = {8, 6, 2}}}
        .Test(scope);
  }
  {
    // Test throwing exception when building logicalNot with uint32 input.
    const MLOperand* output = ElementWiseUnaryTester<uint32_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kLogicalNot,
        .input_info = {
            .data_type = V8MLOperandDataType::Enum::kUint32,
            .dimensions = {3, 4}}}.BuildElementWiseUnary(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the uint8 types.");
  }
  {
    // Test building element-wise identity.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kIdentity,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {8, 6, 2}}}
        .Test(scope);
  }
  {
    // Test building element-wise identity.
    ElementWiseUnaryTester<uint8_t>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kIdentity,
        .input_info = {.data_type = V8MLOperandDataType::Enum::kUint8,
                       .dimensions = {8, 6, 2}}}
        .Test(scope);
  }
}

TEST_F(MLGraphBuilderTest, Cast) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  auto int8_datatype = V8MLOperandDataType::Create("int8");
  auto* input = BuildInput(builder, "input", {8, 6, 2},
                           V8MLOperandDataType::Enum::kFloat32,
                           scope.GetExceptionState());
  MLOperand* output =
      builder->cast(input, int8_datatype.value(), scope.GetExceptionState());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kInt8);
  EXPECT_EQ(output->Dimensions(), input->Dimensions());
  auto* op = output->Operator();
  ASSERT_THAT(op, testing::NotNull());
  EXPECT_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kElementWiseUnary);
  EXPECT_EQ(op->SubKind<webnn::mojom::blink::ElementWiseUnary::Kind>(),
            webnn::mojom::blink::ElementWiseUnary::Kind::kCast);
  EXPECT_TRUE(op->IsConnected());
  EXPECT_THAT(op->Options(), testing::IsNull());
}

MLOperand* BuildReduce(V8TestingScope& scope,
                       MLGraphBuilder* builder,
                       webnn::mojom::blink::Reduce::Kind kind,
                       const MLOperand* input,
                       const MLReduceOptions* options) {
  MLOperand* output = nullptr;
  switch (kind) {
    case webnn::mojom::blink::Reduce::Kind::kL1:
      output = builder->reduceL1(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kL2:
      output = builder->reduceL2(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kLogSum:
      output = builder->reduceLogSum(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kLogSumExp:
      output =
          builder->reduceLogSumExp(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kMax:
      output = builder->reduceMax(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kMean:
      output = builder->reduceMean(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kMin:
      output = builder->reduceMin(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kProduct:
      output =
          builder->reduceProduct(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kSum:
      output = builder->reduceSum(input, options, scope.GetExceptionState());
      break;
    case webnn::mojom::blink::Reduce::Kind::kSumSquare:
      output =
          builder->reduceSumSquare(input, options, scope.GetExceptionState());
      break;
  }
  return output;
}

void CheckReduceOutput(const MLOperand* input,
                       const MLOperand* output,
                       webnn::mojom::blink::Reduce::Kind kind) {
  ASSERT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* reduce = output->Operator();
  ASSERT_THAT(reduce, testing::NotNull());
  EXPECT_EQ(reduce->Kind(), webnn::mojom::blink::Operation::Tag::kReduce);
  EXPECT_EQ(reduce->SubKind<webnn::mojom::blink::Reduce::Kind>(), kind);
  EXPECT_TRUE(reduce->IsConnected());
  ASSERT_THAT(reduce->Options(), testing::NotNull());
}

TEST_F(MLGraphBuilderTest, ReduceTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  const auto kinds = {webnn::mojom::blink::Reduce::Kind::kL1,
                      webnn::mojom::blink::Reduce::Kind::kL2,
                      webnn::mojom::blink::Reduce::Kind::kLogSum,
                      webnn::mojom::blink::Reduce::Kind::kLogSumExp,
                      webnn::mojom::blink::Reduce::Kind::kMax,
                      webnn::mojom::blink::Reduce::Kind::kMean,
                      webnn::mojom::blink::Reduce::Kind::kMin,
                      webnn::mojom::blink::Reduce::Kind::kProduct,
                      webnn::mojom::blink::Reduce::Kind::kSum,
                      webnn::mojom::blink::Reduce::Kind::kSumSquare};
  for (const auto reduce_kind : kinds) {
    {
      // Test reduce with default options.
      auto* input = BuildInput(builder, "input", {1, 3, 4, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLReduceOptions::Create();
      EXPECT_FALSE(options->hasAxes());
      EXPECT_TRUE(options->hasKeepDimensions());
      EXPECT_FALSE(options->keepDimensions());
      auto* output = BuildReduce(scope, builder, reduce_kind, input, options);
      CheckReduceOutput(input, output, reduce_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({}));
    }
    {
      // Test reduce with keepDimensions = true.
      auto* input = BuildInput(builder, "input", {1, 3, 4, 4},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLReduceOptions::Create();
      options->setKeepDimensions(true);
      auto* output = BuildReduce(scope, builder, reduce_kind, input, options);
      CheckReduceOutput(input, output, reduce_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 1, 1}));
    }
    {
      // Test reduce with axes = {0, 1} and keep_dimensions = false.
      auto* input = BuildInput(builder, "input", {1, 3, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLReduceOptions::Create();
      options->setAxes({0, 1});
      auto* output = BuildReduce(scope, builder, reduce_kind, input, options);
      CheckReduceOutput(input, output, reduce_kind);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({5, 5}));
    }
    {
      // Test throwing exception when a value in axes is out of range of [0,
      // N-1].
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLReduceOptions::Create();
      options->setAxes({4});
      auto* output = BuildReduce(scope, builder, reduce_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "The values in axes must be in the range [0, 4).");
    }
    {
      // Test throwing exception when the two values are same in axes sequence.
      auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                               V8MLOperandDataType::Enum::kFloat32,
                               scope.GetExceptionState());
      auto* options = MLReduceOptions::Create();
      options->setAxes({0, 1, 1});
      auto* output = BuildReduce(scope, builder, reduce_kind, input, options);
      EXPECT_THAT(output, testing::IsNull());
      EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                DOMExceptionCode::kDataError);
      EXPECT_EQ(scope.GetExceptionState().Message(),
                "Two or more values are same in the axes sequence.");
    }
  }
  // Test throw error when the input data type is not one of the floating point
  // types for these four reduce kind.
  const auto kFloatRestrictReduceKinds = {
      webnn::mojom::blink::Reduce::Kind::kL2,
      webnn::mojom::blink::Reduce::Kind::kLogSum,
      webnn::mojom::blink::Reduce::Kind::kLogSumExp,
      webnn::mojom::blink::Reduce::Kind::kMean};
  for (const auto reduce_kind : kFloatRestrictReduceKinds) {
    // Test throwing exception when the two values are same in axes sequence.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* options = MLReduceOptions::Create();
    options->setAxes({0, 1});
    auto* output = BuildReduce(scope, builder, reduce_kind, input, options);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the floating point types.");
  }
}

TEST_F(MLGraphBuilderTest, ReshapeTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building reshape with new shape = {3, 8}.
    auto* input = BuildInput(builder, "input", {2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->reshape(input, {3, 8}, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({3, 8}));
    auto* reshape = output->Operator();
    ASSERT_THAT(reshape, testing::NotNull());
    EXPECT_EQ(reshape->Kind(), webnn::mojom::blink::Operation::Tag::kReshape);
    EXPECT_TRUE(reshape->IsConnected());
  }
  {
    // Test building reshape with new shape = {24}, src shape = {2, 3, 4}.
    auto* input = BuildInput(builder, "input", {2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->reshape(input, {24}, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({24}));
    auto* reshape = output->Operator();
    ASSERT_THAT(reshape, testing::NotNull());
    EXPECT_EQ(reshape->Kind(), webnn::mojom::blink::Operation::Tag::kReshape);
    EXPECT_TRUE(reshape->IsConnected());
  }
  {
    // Test building reshape with new shape = {1}, src shape = {1}.
    auto* input =
        BuildInput(builder, "input", {1}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->reshape(input, {1}, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1}));
    auto* reshape = output->Operator();
    ASSERT_THAT(reshape, testing::NotNull());
    EXPECT_EQ(reshape->Kind(), webnn::mojom::blink::Operation::Tag::kReshape);
    EXPECT_TRUE(reshape->IsConnected());
  }
  {
    // Test reshaping a 1-D 1-element tensor to scalar.
    auto* input =
        BuildInput(builder, "input", {1}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->reshape(input, {}, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({}));
    auto* reshape = output->Operator();
    ASSERT_THAT(reshape, testing::NotNull());
    EXPECT_EQ(reshape->Kind(), webnn::mojom::blink::Operation::Tag::kReshape);
    EXPECT_TRUE(reshape->IsConnected());
  }
  {
    // Test reshaping a scalar to 1-D 1-element tensor.
    auto* input =
        BuildInput(builder, "input", {}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->reshape(input, {1}, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1}));
    auto* reshape = output->Operator();
    ASSERT_THAT(reshape, testing::NotNull());
    EXPECT_EQ(reshape->Kind(), webnn::mojom::blink::Operation::Tag::kReshape);
    EXPECT_TRUE(reshape->IsConnected());
  }
  {
    // Test throwing error when one value of new shape is 0.
    auto* input = BuildInput(builder, "input", {2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output =
        builder->reshape(input, {2, 4, 0}, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The value of new shape should not be 0.");
  }
  {
    // Setting new shape = {}.
    // Test throwing error since the number of elements implied by new shape is
    // not equal to the number of elements in the input tensor.
    auto* input = BuildInput(builder, "input", {2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->reshape(input, {}, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The number of elements (1) implied by new shape doesn't match "
              "the number of elements (24) in the input tensor.");
  }
  {
    // Test throwing error since the number of elements implied by new shape is
    // not equal to the number of elements in the input tensor.
    auto* input = BuildInput(builder, "input", {2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->reshape(input, {3, 9}, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The number of elements (27) implied by new shape doesn't match "
              "the number of elements (24) in the input tensor.");
  }
}

MLOperand* BuildResample2d(V8TestingScope& scope,
                           MLGraphBuilder* builder,
                           const MLOperand* input,
                           const MLResample2dOptions* options) {
  auto* output = builder->resample2d(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* resample2d = output->Operator();
  EXPECT_THAT(resample2d, testing::NotNull());
  EXPECT_EQ(resample2d->Kind(),
            webnn::mojom::blink::Operation::Tag::kResample2d);
  EXPECT_TRUE(resample2d->IsConnected());
  EXPECT_THAT(resample2d->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, Resample2dTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building resample2d with default options.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    EXPECT_TRUE(options->hasMode());
    EXPECT_EQ(options->mode(), V8MLInterpolationMode::Enum::kNearestNeighbor);
    EXPECT_FALSE(options->hasScales());
    EXPECT_FALSE(options->hasSizes());
    EXPECT_FALSE(options->hasAxes());
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 2, 4}));
  }
  {
    // Test building resample2d with scales = {2.0, 2.0}.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({2.0, 2.0});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 4, 8}));
  }
  {
    // Test building resample2d with scales = {0.5, 0.5}.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({0.5, 0.5});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 2, 2}));
  }
  {
    // Test building resample2d with sizes = {3, 6}.
    // When the target sizes are specified, scales argument is
    // ignored.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({2.0, 2.0});
    options->setSizes({3, 6});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 6}));
  }
  {
    // Test building resample2d with scales = {1.0, 2.0} and axes = {0, 1}.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({1.0, 2.0});
    options->setAxes({0, 1});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 2, 4}));
  }
  {
    // Test building resample2d with scales = {2.0, 2.0} and axes = {1, 2}.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({2.0, 2.0});
    options->setAxes({1, 2});
    auto* output = BuildResample2d(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 4, 4}));
  }
  {
    // Test throwing error when the input is not a 4-D tensor.
    auto* input = BuildInput(builder, "input", {2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input must be a 4-D tensor.");
  }
  {
    // Test throwing error when the length of scales is not 2.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({1.0, 1.0, 2.0, 2.0});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of scales should be 2.");
  }
  {
    // Test throwing error when the scale is negative.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({1.0, -2.0});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "All scales should be greater than 0.");
  }
  {
    // Test throwing error when the length of sizes is not 2.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setSizes({1, 1, 4, 6});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of sizes should be 2.");
  }
  {
    // Test throwing error when the scale height is too large.
    auto* input = BuildInput(builder, "input", {1, 1, 34902, 23243},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({232433, 4});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output height: The scale is too large.");
  }
  {
    // Test throwing error when the scale height is too small.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({0.02, 0.8});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output height: The scale is too small.");
  }
  {
    // Test throwing error when the scale width is too large.
    auto* input = BuildInput(builder, "input", {1, 1, 34902, 23243},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({20, 434324});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output width: The scale is too large.");
  }
  {
    // Test throwing error when the scale width is too small.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setScales({0.7, 0.1});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Failed to calculate the output width: The scale is too small.");
  }
  {
    // Test throwing error when the length of axes is not 2.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setAxes({0, 1, 2});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of axes should be 2.");
  }
  {
    // Test throwing error when the values of axes are inconsecutive.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLResample2dOptions::Create();
    options->setAxes({0, 2});
    auto* output =
        builder->resample2d(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The values of axes are invalid.");
  }
}

MLOperand* BuildTranspose(V8TestingScope& scope,
                          MLGraphBuilder* builder,
                          const MLOperand* input,
                          const MLTransposeOptions* options) {
  auto* output = builder->transpose(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* transpose = output->Operator();
  EXPECT_THAT(transpose, testing::NotNull());
  EXPECT_EQ(transpose->Kind(), webnn::mojom::blink::Operation::Tag::kTranspose);
  EXPECT_TRUE(transpose->IsConnected());
  EXPECT_THAT(transpose->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, TransposeTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building transpose with default options.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = BuildTranspose(scope, builder, input);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 3, 2, 1}));
  }
  {
    // Test building transpose with permutation = {0, 2, 3, 1}.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({0, 2, 3, 1});
    auto* output = BuildTranspose(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 2}));
  }
  {
    // Test throwing error when the number of values in permutation is not the
    // same as the rank of the input tensor.
    auto* input = BuildInput(builder, "input", {1, 2, 4},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({0, 2, 3, 1});
    auto* output =
        builder->transpose(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of values in permutation must be the same as the rank "
        "of the input tensor.");
  }
  {
    // Test throwing error when two values in permutation are same.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({0, 2, 3, 2});
    auto* output =
        builder->transpose(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Two or more values are same in the axes sequence.");
  }
  {
    // Test throwing error when one value in permutation is greater than
    // input_rank-1.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({0, 1, 2, 4});
    auto* output =
        builder->transpose(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The values in axes must be in the range [0, 4).");
  }
}

MLOperand* BuildClamp(V8TestingScope& scope,
                      MLGraphBuilder* builder,
                      const MLOperand* input,
                      const MLClampOptions* options) {
  auto* output = builder->clamp(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* clamp = output->Operator();
  EXPECT_THAT(clamp, testing::NotNull());
  EXPECT_EQ(clamp->Kind(), webnn::mojom::blink::Operation::Tag::kClamp);
  EXPECT_TRUE(clamp->IsConnected());
  EXPECT_THAT(clamp->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, ClampTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building clamp with default options.
    auto* input = BuildInput(builder, "input", {2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLClampOptions::Create();
    EXPECT_FALSE(options->hasMaxValue());
    EXPECT_FALSE(options->hasMinValue());
    auto* output = BuildClamp(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building clamp with max value = 0 and min value = 0.
    auto* input = BuildInput(builder, "input", {1, 2, 2, 7},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLClampOptions::Create();
    options->setMaxValue(0);
    options->setMinValue(0);
    auto* output = BuildClamp(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 2, 7}));
  }
  {
    // Test throwing error when the max value is less than the min value.
    auto* input =
        BuildInput(builder, "input", {2, 4}, V8MLOperandDataType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* options = MLClampOptions::Create();
    options->setMaxValue(-3.243432);
    options->setMinValue(4.432232);
    auto* output = builder->clamp(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The min value (4.432232) should be less than or equal to "
              "the max value (-3.243432).");
  }
  {
    // Test building clamp as an activation.
    MLActivation* clamp =
        builder->clamp(MLClampOptions::Create(), scope.GetExceptionState());
    ASSERT_THAT(clamp, testing::NotNull());
    ASSERT_THAT(clamp->Operator(), testing::NotNull());
    EXPECT_EQ(clamp->Kind(), webnn::mojom::blink::Activation::Tag::kClamp);
    EXPECT_FALSE(clamp->Operator()->IsConnected());
    ASSERT_THAT(clamp->Operator()->Options(), testing::NotNull());
  }
}

void TestBuildElu(V8TestingScope& scope,
                  MLGraphBuilder* builder,
                  const MLOperand* input,
                  const Vector<uint32_t>& output_shape,
                  const MLEluOptions* options) {
  auto* output = builder->elu(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  EXPECT_EQ(output->Dimensions(), output_shape);
  auto* elu = output->Operator();
  EXPECT_THAT(elu, testing::NotNull());
  EXPECT_EQ(elu->Kind(), webnn::mojom::blink::Operation::Tag::kElu);
  EXPECT_TRUE(elu->IsConnected());
  EXPECT_THAT(elu->Options(), testing::NotNull());
}

TEST_F(MLGraphBuilderTest, EluTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building elu with float32 input and default options.
    auto* input = BuildInput(builder, "input", {1, 2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLEluOptions::Create();
    EXPECT_TRUE(options->hasAlpha());
    EXPECT_EQ(options->alpha(), 1.0f);
    TestBuildElu(scope, builder, input, {1, 2, 3}, options);
  }
  {
    // Test building elu with float32 input and alpha = 0.1.
    auto* input = BuildInput(builder, "input", {2, 2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLEluOptions::Create();
    options->setAlpha(0.1);
    TestBuildElu(scope, builder, input, {2, 2, 3}, options);
  }
  {
    // Test throwing error when alpha = 0.
    auto* input = BuildInput(builder, "input", {2, 2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLEluOptions::Create();
    options->setAlpha(0);
    auto* output = builder->elu(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The value of alpha must be greater than 0.");
  }
  {
    // Test throwing error when alpha = -1.
    auto* input = BuildInput(builder, "input", {2, 2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLEluOptions::Create();
    options->setAlpha(-1);
    auto* output = builder->elu(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The value of alpha must be greater than 0.");
  }
  {
    // Test throwing error when input data type is int32.
    auto* input = BuildInput(builder, "input", {2, 2, 3},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* output =
        builder->elu(input, MLEluOptions::Create(), scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test building elu as an activation.
    MLActivation* elu =
        builder->elu(MLEluOptions::Create(), scope.GetExceptionState());
    ASSERT_THAT(elu, testing::NotNull());
    ASSERT_THAT(elu->Operator(), testing::NotNull());
    EXPECT_EQ(elu->Kind(), webnn::mojom::blink::Activation::Tag::kElu);
    EXPECT_FALSE(elu->Operator()->IsConnected());
    ASSERT_THAT(elu->Operator()->Options(), testing::NotNull());
  }
}

template <typename T>
struct ExpandTester {
  OperandInfo<T> input;
  Vector<uint32_t> new_shape;

  MLOperand* BuildExpandOperator(V8TestingScope& scope) {
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    return builder->expand(input_operand, new_shape, scope.GetExceptionState());
  }

  void Test(V8TestingScope& scope) {
    MLOperand* output = BuildExpandOperator(scope);
    EXPECT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), input.data_type);
    EXPECT_EQ(output->Dimensions(), new_shape);
    auto* op = output->Operator();
    EXPECT_THAT(op, testing::NotNull());
    EXPECT_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kExpand);
    EXPECT_TRUE(op->IsConnected());
    EXPECT_THAT(op->Options(), testing::IsNull());
  }
};

TEST_F(MLGraphBuilderTest, ExpandTest) {
  V8TestingScope scope;
  {
    // Test building expand 0-D scalar to 3-D tensor.
    ExpandTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .new_shape = {3, 4, 5}}
        .Test(scope);
  }
  {
    // Test building expand with the new shapes that are the same as input.
    ExpandTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {4}},
        .new_shape = {4}}
        .Test(scope);
  }
  {
    // Test building expand with the new shapes that are broadcastable.
    ExpandTester<int32_t>{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {3, 1, 5}},
        .new_shape = {3, 4, 5}}
        .Test(scope);
  }
  {
    // Test building expand with the new shapes that are broadcastable and the
    // number of new shapes larger than input.
    ExpandTester<int32_t>{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 5}},
        .new_shape = {3, 2, 5}}
        .Test(scope);
  }
  {
    // Test throwing exception when the input shapes are not the same as new
    // shape and not broadcastable.
    MLOperand* output = ExpandTester<uint32_t>{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {3, 6, 2}},
        .new_shape = {4, 3,
                      5}}.BuildExpandOperator(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input shape is not broadcastable to the new shape.");
  }
  {
    // Test throwing exception when the input shapes are not broadcastable.
    MLOperand* output = ExpandTester<uint32_t>{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {5, 4}},
        .new_shape = {5}}.BuildExpandOperator(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input shape is not broadcastable to the new shape.");
  }
  {
    // Test throwing exception if the number of new shapes is too large.
    // Set the dimensions that let the number of elements be 2 * SIZE_MAX.
    MLOperand* output =
        ExpandTester<float>{
            .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                      .dimensions = {1, 2, 1, 1}},
            .new_shape = {1, 2, kSquareRootOfSizeMax, kSquareRootOfSizeMax}}
            .BuildExpandOperator(scope);
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Invalid operand descriptor: The number of elements is too large.");
  }
}

MLOperand* BuildInstanceNormalization(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLInstanceNormalizationOptions* options) {
  auto* output =
      builder->instanceNormalization(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  EXPECT_EQ(output->Dimensions(), input->Dimensions());
  auto* instance_normalization = output->Operator();
  EXPECT_THAT(instance_normalization, testing::NotNull());
  EXPECT_EQ(instance_normalization->Kind(),
            webnn::mojom::blink::Operation::Tag::kInstanceNormalization);
  EXPECT_TRUE(instance_normalization->IsConnected());
  EXPECT_THAT(instance_normalization->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, InstanceNormalizationTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building instanceNormalization with default options for 4-D input.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    EXPECT_FALSE(options->hasScale());
    EXPECT_FALSE(options->hasBias());
    EXPECT_TRUE(options->hasLayout());
    EXPECT_EQ(options->layout(), V8MLInputOperandLayout::Enum::kNchw);
    EXPECT_TRUE(options->hasEpsilon());
    EXPECT_FLOAT_EQ(options->epsilon(), 1e-5);
    auto* output = BuildInstanceNormalization(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3, 4}));
  }
  {
    // Test building instanceNormalization with default layout NCHW.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    auto* scale =
        BuildInput(builder, "scale", {2}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    options->setScale(scale);
    auto* bias =
        BuildInput(builder, "bias", {2}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    options->setBias(bias);
    auto* output = BuildInstanceNormalization(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3, 4}));
  }
  {
    // Test building instanceNormalization with layout = Nhwc.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    auto* scale =
        BuildInput(builder, "scale", {4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    options->setScale(scale);
    auto* bias =
        BuildInput(builder, "bias", {4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    options->setScale(bias);
    auto* output = BuildInstanceNormalization(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3, 4}));
  }
  {
    // Test throwing exception when the input is not a 4-D tensor.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    auto* output = builder->instanceNormalization(input, options,
                                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input should be a 4-D tensor.");
  }
  {
    // Test throwing exception when the input data type is not one of floating
    // point types.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    auto* output = builder->instanceNormalization(input, options,
                                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input type must be one of the floating point types.");
  }
  {
    // Test throwing exception when the scale data type is not the same as the
    // input data type.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    auto* scale =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setScale(scale);
    auto* output = builder->instanceNormalization(input, options,
                                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "For scale operand: the data type doesn't match the input data type.");
  }
  {
    // Test throwing exception when the scale operand is not a 1-D tensor.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    auto* scale =
        BuildConstant(builder, {2, 1}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setScale(scale);
    auto* output = builder->instanceNormalization(input, options,
                                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For scale operand: the operand should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the size of scale operand is not equal
    // to the size of the feature dimension of the input.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    auto* scale =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setScale(scale);
    auto* output = builder->instanceNormalization(input, options,
                                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For scale operand: the size of operand must be equal to the "
              "size of the feature dimension of the input.");
  }
  {
    // Test throwing exception when the bias data type is not the same as the
    // input data type.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    auto* bias =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->instanceNormalization(input, options,
                                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "For bias operand: the data type doesn't match the input data type.");
  }
  {
    // Test throwing exception when the bias operand is not a 1-D tensor.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    auto* bias =
        BuildConstant(builder, {2, 1}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->instanceNormalization(input, options,
                                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For bias operand: the operand should be a 1-D tensor.");
  }
  {
    // Test throwing exception when the size of bias operand is not equal to
    // the size of the feature dimension of the input.
    auto* input = BuildInput(builder, "input", {1, 2, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLInstanceNormalizationOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    auto* bias =
        BuildConstant(builder, {2}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    options->setBias(bias);
    auto* output = builder->instanceNormalization(input, options,
                                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For bias operand: the size of operand must be equal to the size "
              "of the feature dimension of the input.");
  }
}

MLOperand* BuildLayerNormalization(V8TestingScope& scope,
                                   MLGraphBuilder* builder,
                                   const MLOperand* input,
                                   const MLLayerNormalizationOptions* options) {
  auto* output =
      builder->layerNormalization(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  EXPECT_EQ(output->Dimensions(), input->Dimensions());
  auto* layer_normalization = output->Operator();
  EXPECT_THAT(layer_normalization, testing::NotNull());
  EXPECT_EQ(layer_normalization->Kind(),
            webnn::mojom::blink::Operation::Tag::kLayerNormalization);
  EXPECT_TRUE(layer_normalization->IsConnected());
  EXPECT_THAT(layer_normalization->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, LayerNormalizationTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building layerNormalization with default options for scalar input.
    auto* input =
        BuildInput(builder, "input", {}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    BuildLayerNormalization(scope, builder, input);
  }
  {
    // Test building layerNormalization with given axes.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    options->setAxes({3});
    BuildLayerNormalization(scope, builder, input, options);
  }
  {
    // Test building layerNormalization with given scale.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    auto* scale = BuildInput(builder, "scale", {2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    options->setScale(scale);
    BuildLayerNormalization(scope, builder, input, options);
  }
  {
    // Test building layerNormalization with given axes, scale and bias.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    auto* scale = BuildInput(builder, "scale", {3, 4},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* bias =
        BuildInput(builder, "bias", {3, 4}, V8MLOperandDataType::Enum::kFloat16,
                   scope.GetExceptionState());
    options->setScale(scale);
    options->setBias(bias);
    options->setAxes({2, 3});
    BuildLayerNormalization(scope, builder, input, options);
  }
  {
    // Test building layerNormalization with inconsecutive axes.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4, 5, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    auto* scale = BuildInput(builder, "scale", {2, 4, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* bias = BuildInput(builder, "bias", {2, 4, 6},
                            V8MLOperandDataType::Enum::kFloat32,
                            scope.GetExceptionState());
    options->setScale(scale);
    options->setBias(bias);
    options->setAxes({1, 3, 5});
    BuildLayerNormalization(scope, builder, input, options);
  }
  {
    // Test building layerNormalization with axes in descending order.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4, 5, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    auto* scale = BuildInput(builder, "scale", {6, 5, 4, 3, 2},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* bias = BuildInput(builder, "bias", {6, 5, 4, 3, 2},
                            V8MLOperandDataType::Enum::kFloat32,
                            scope.GetExceptionState());
    options->setScale(scale);
    options->setBias(bias);
    options->setAxes({5, 4, 3, 2, 1});
    BuildLayerNormalization(scope, builder, input, options);
  }
  {
    // Test throwing error when the input data type is uint32.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kUint32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    auto* output =
        builder->layerNormalization(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input type must be one of the floating point types.");
  }
  {
    // Test throwing error when the axis is greater than the input rank.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    options->setAxes({1, 2, 4});
    auto* output =
        builder->layerNormalization(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The values in axes must be in the range [0, 4).");
  }
  {
    // Test throwing error when the axes have duplications.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    options->setAxes({3, 3});
    auto* output =
        builder->layerNormalization(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Two or more values are same in the axes sequence.");
  }
  {
    // Test throwing error when the scale data type doesn't match input data
    // type.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    auto* scale = BuildInput(builder, "scale", {3, 4},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* bias =
        BuildInput(builder, "bias", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    options->setScale(scale);
    options->setBias(bias);
    options->setAxes({2, 3});
    auto* output =
        builder->layerNormalization(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "For scale operand: the data type doesn't match the input data type.");
  }
  {
    // Test throwing error when the bias dimensions doesn't match axis
    // dimensions.
    auto* input = BuildInput(builder, "input", {1, 2, 3, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLayerNormalizationOptions::Create();
    auto* bias = BuildInput(builder, "bias", {3, 3, 4},
                            V8MLOperandDataType::Enum::kFloat32,
                            scope.GetExceptionState());
    options->setBias(bias);
    auto* output =
        builder->layerNormalization(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For bias operand: the shape doesn't match the axis dimensions "
              "of the input.");
  }
}

MLOperand* BuildLeakyRelu(V8TestingScope& scope,
                          MLGraphBuilder* builder,
                          const MLOperand* input,
                          const MLLeakyReluOptions* options) {
  auto* output = builder->leakyRelu(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* leaky_relu = output->Operator();
  EXPECT_THAT(leaky_relu, testing::NotNull());
  EXPECT_EQ(leaky_relu->Kind(),
            webnn::mojom::blink::Operation::Tag::kLeakyRelu);
  EXPECT_TRUE(leaky_relu->IsConnected());
  EXPECT_THAT(leaky_relu->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, LeakyReluTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building leaky_relu with float32 input.
    auto* input = BuildInput(builder, "input", {1, 2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLeakyReluOptions::Create();
    auto* output = BuildLeakyRelu(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3}));
  }
  {
    // Test building leaky_relu as an activation.
    MLActivation* leaky_relu = builder->leakyRelu(MLLeakyReluOptions::Create(),
                                                  scope.GetExceptionState());
    ASSERT_THAT(leaky_relu, testing::NotNull());
    ASSERT_THAT(leaky_relu->Operator(), testing::NotNull());
    EXPECT_EQ(leaky_relu->Kind(),
              webnn::mojom::blink::Activation::Tag::kLeakyRelu);
    EXPECT_FALSE(leaky_relu->Operator()->IsConnected());
    ASSERT_THAT(leaky_relu->Operator()->Options(), testing::NotNull());
  }
}

MLOperand* BuildLinear(V8TestingScope& scope,
                       MLGraphBuilder* builder,
                       const MLOperand* input,
                       const MLLinearOptions* options) {
  auto* output = builder->linear(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* linear = output->Operator();
  EXPECT_THAT(linear, testing::NotNull());
  EXPECT_EQ(linear->Kind(), webnn::mojom::blink::Operation::Tag::kLinear);
  EXPECT_TRUE(linear->IsConnected());
  EXPECT_THAT(linear->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, LinearTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building linear with float32 input.
    auto* input = BuildInput(builder, "input", {1, 2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLLinearOptions::Create();
    auto* output = BuildLinear(scope, builder, input, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3}));
  }
  {
    // Test throwing exception when the input data type is int32.
    auto* input = BuildInput(builder, "input", {1, 2, 3},
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* options = MLLinearOptions::Create();
    auto* output = builder->linear(input, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test building linear as an activation.
    MLActivation* linear =
        builder->linear(MLLinearOptions::Create(), scope.GetExceptionState());
    ASSERT_THAT(linear, testing::NotNull());
    ASSERT_THAT(linear->Operator(), testing::NotNull());
    EXPECT_EQ(linear->Kind(), webnn::mojom::blink::Activation::Tag::kLinear);
    EXPECT_FALSE(linear->Operator()->IsConnected());
    ASSERT_THAT(linear->Operator()->Options(), testing::NotNull());
  }
}

MLOperand* BuildPad(V8TestingScope& scope,
                    MLGraphBuilder* builder,
                    const MLOperand* input,
                    const Vector<uint32_t>& beginningPadding,
                    const Vector<uint32_t>& endingPadding,
                    const MLPadOptions* options) {
  auto* output = builder->pad(input, beginningPadding, endingPadding, options,
                              scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  auto* pad = output->Operator();
  EXPECT_THAT(pad, testing::NotNull());
  EXPECT_EQ(pad->Kind(), webnn::mojom::blink::Operation::Tag::kPad);
  EXPECT_TRUE(pad->IsConnected());
  EXPECT_THAT(pad->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, PadTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building pad with default options, beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    auto* input = BuildInput(builder, "input", {2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    EXPECT_TRUE(options->hasMode());
    EXPECT_EQ(options->mode(), V8MLPaddingMode::Enum::kConstant);
    EXPECT_TRUE(options->hasValue());
    EXPECT_EQ(options->value(), 0);
    auto* output = BuildPad(scope, builder, input, {1, 2}, {1, 2}, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 7}));
  }
  {
    // Test throwing error when building pad for scalar input.
    auto* input =
        BuildInput(builder, "input", {}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    auto* output =
        builder->pad(input, {}, {}, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input should not be a scalar.");
  }
  {
    // Test throwing error when the length of beginningPadding is not equal to
    // the input rank.
    auto* input = BuildInput(builder, "input", {2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    options->setMode(V8MLPaddingMode::Enum::kEdge);
    auto* output =
        builder->pad(input, {1}, {1, 2}, options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of beginningPadding must be equal to the rank of the "
              "input tensor.");
  }
  {
    // Test throwing error when the length of endingPadding is not equal to the
    // input rank.
    auto* input = BuildInput(builder, "input", {2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    options->setMode(V8MLPaddingMode::Enum::kReflection);
    auto* output = builder->pad(input, {1, 0}, {1, 2, 0}, options,
                                scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The length of endingPadding must be equal to the rank of the "
              "input tensor.");
  }
  {
    // Test throwing error when the padding of one dimension is too large.
    auto* input = BuildInput(builder, "input", {2, 3},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* options = MLPadOptions::Create();
    options->setMode(V8MLPaddingMode::Enum::kReflection);
    auto* output = builder->pad(input, {2294967295, 0}, {3294967295, 2},
                                options, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The padding of dimension (0) is too large.");
  }
}

TEST_F(MLGraphBuilderTest, Softmax) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building softmax with float32 input.
    auto* input = BuildInput(builder, "input", {2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->softmax(input, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
    auto* softmax = output->Operator();
    ASSERT_THAT(softmax, testing::NotNull());
    EXPECT_EQ(softmax->Kind(), webnn::mojom::blink::Operation::Tag::kSoftmax);
    EXPECT_TRUE(softmax->IsConnected());
    EXPECT_THAT(softmax->Options(), testing::IsNull());
  }
  {
    // Test throwing exception when building softmax with 4-D input.
    auto* input = BuildInput(builder, "input", {1, 1, 2, 4},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->softmax(input, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input must be a 2-D tensor.");
  }
  {
    // Test throwing exception when building softmax with int32 input.
    auto* input =
        BuildInput(builder, "input", {3, 4}, V8MLOperandDataType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* output = builder->softmax(input, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the floating point types.");
  }
}

MLOperand* BuildSoftplus(V8TestingScope& scope,
                         MLGraphBuilder* builder,
                         const MLOperand* input,
                         const MLSoftplusOptions* options) {
  auto* output = builder->softplus(input, options, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), input->DataType());
  EXPECT_EQ(output->Dimensions(), input->Dimensions());
  auto* softplus = output->Operator();
  EXPECT_THAT(softplus, testing::NotNull());
  EXPECT_EQ(softplus->Kind(), webnn::mojom::blink::Operation::Tag::kSoftplus);
  EXPECT_TRUE(softplus->IsConnected());
  EXPECT_THAT(softplus->Options(), testing::NotNull());
  return output;
}

TEST_F(MLGraphBuilderTest, SoftPlusTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building softplus with a scalar input and default options.
    auto* input =
        BuildInput(builder, "input", {}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    BuildSoftplus(scope, builder, input);
  }
  {
    // Test building softplus with a 2-D input and steepness = 2.0.
    auto* input = BuildInput(builder, "input", {3, 4},
                             V8MLOperandDataType::Enum::kFloat16,
                             scope.GetExceptionState());
    auto* options = MLSoftplusOptions::Create();
    options->setSteepness(2.0);
    BuildSoftplus(scope, builder, input, options);
  }
  {
    // Test building softplus activation.
    auto* output = builder->softplus(MLSoftplusOptions::Create(),
                                     scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    const MLOperator* softplus = output->Operator();
    ASSERT_THAT(softplus, testing::NotNull());
    EXPECT_EQ(softplus->Kind(), webnn::mojom::blink::Operation::Tag::kSoftplus);
    EXPECT_FALSE(softplus->IsConnected());
    ASSERT_THAT(softplus->Options(), testing::NotNull());
  }
  {
    // Test throwing exception when building softplus with int32 input.
    Vector<uint32_t> input_shape({3, 4});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* output = builder->softplus(input, MLSoftplusOptions::Create(),
                                     scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
}

TEST_F(MLGraphBuilderTest, SoftSignTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building softsign with float32 input.
    Vector<uint32_t> input_shape({3, 4});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->softsign(input, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* softsign = output->Operator();
    ASSERT_THAT(softsign, testing::NotNull());
    EXPECT_EQ(softsign->Kind(), webnn::mojom::blink::Operation::Tag::kSoftsign);
    EXPECT_TRUE(softsign->IsConnected());
    EXPECT_THAT(softsign->Options(), testing::IsNull());
  }
  {
    // Test throwing exception when building softsign with int32 input.
    Vector<uint32_t> input_shape({3, 4});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* output = builder->softsign(input, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test building softsign activation.
    MLActivation* softsign = builder->softsign(scope.GetExceptionState());
    ASSERT_THAT(softsign, testing::NotNull());
    ASSERT_THAT(softsign->Operator(), testing::NotNull());
    EXPECT_EQ(softsign->Kind(),
              webnn::mojom::blink::Activation::Tag::kSoftsign);
    EXPECT_FALSE(softsign->Operator()->IsConnected());
    EXPECT_THAT(softsign->Operator()->Options(), testing::IsNull());
  }
}

TEST_F(MLGraphBuilderTest, SigmoidTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building sigmoid with float32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->sigmoid(input, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* sigmoid = output->Operator();
    ASSERT_THAT(sigmoid, testing::NotNull());
    EXPECT_EQ(sigmoid->Kind(), webnn::mojom::blink::Operation::Tag::kSigmoid);
    EXPECT_TRUE(sigmoid->IsConnected());
    EXPECT_THAT(sigmoid->Options(), testing::IsNull());
  }
  {
    // Test throwing exception when building sigmoid with int32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* output = builder->sigmoid(input, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test building sigmoid activation.
    MLActivation* sigmoid = builder->sigmoid(scope.GetExceptionState());
    ASSERT_THAT(sigmoid, testing::NotNull());
    ASSERT_THAT(sigmoid->Operator(), testing::NotNull());
    EXPECT_EQ(sigmoid->Kind(), webnn::mojom::blink::Activation::Tag::kSigmoid);
    EXPECT_FALSE(sigmoid->Operator()->IsConnected());
    EXPECT_THAT(sigmoid->Operator()->Options(), testing::IsNull());
  }
}

TEST_F(MLGraphBuilderTest, SliceTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building slice with starts = {0, 1, 2} and sizes = {1, 2, 3}.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output =
        builder->slice(input, {0, 1, 2}, {1, 2, 3}, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3}));
    const MLOperator* slice = output->Operator();
    ASSERT_THAT(slice, testing::NotNull());
    EXPECT_EQ(slice->Kind(), webnn::mojom::blink::Operation::Tag::kSlice);
    EXPECT_TRUE(slice->IsConnected());
    EXPECT_THAT(slice->Options(), testing::IsNull());
  }
  {
    // Test building slice a scalar.
    Vector<uint32_t> input_shape({});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->slice(input, {0}, {1}, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input should not be a scalar.");
  }
  {
    // Test throwing error when the length of sizes is not equal to the rank of
    // the input tensor.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const Vector<uint32_t> starts = {1, 2, 3};
    const Vector<uint32_t> sizes = {1, 1};
    auto* output =
        builder->slice(input, starts, sizes, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The length of sizes must be equal to the rank of the input tensor.");
  }
  {
    // Test throwing error when the length of starts is not equal to the rank
    // of the input tensor.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const Vector<uint32_t> starts = {1, 2, 1, 3};
    const Vector<uint32_t> sizes = {1, 1, 1};
    auto* output =
        builder->slice(input, starts, sizes, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The length of starts must be equal to the rank of the input tensor.");
  }
  {
    // Test throwing error when the starting index is equal to or greater than
    // input size in the same dimension.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const Vector<uint32_t> starts = {0, 4, 4};
    const Vector<uint32_t> sizes = {1, 1, 1};
    auto* output =
        builder->slice(input, starts, sizes, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For dimension (1): the starting index to slice must be less "
              "than input size (4).");
  }
  {
    // Test throwing error when the number of elements to slice is equal to 0.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const Vector<uint32_t> starts = {1, 2, 3};
    const Vector<uint32_t> sizes = {1, 0, 1};
    auto* output =
        builder->slice(input, starts, sizes, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "For dimension (1): the number of elements to slice must not be 0.");
  }
  {
    // Test throwing error when the ending index to slice is greater than input
    // size in the same dimension.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const Vector<uint32_t> starts = {0, 1, 2};
    const Vector<uint32_t> sizes = {3, 4, 1};
    auto* output =
        builder->slice(input, starts, sizes, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "For dimension (1): the ending index to slice must not be "
              "greater than input "
              "size (4).");
  }
}

TEST_F(MLGraphBuilderTest, Split) {
  V8TestingScope scope;
  MLGraphBuilder* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building split with default option.
    auto* input = BuildInput(builder, "input", {2, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const uint32_t splits = 2;
    auto* options = MLSplitOptions::Create();
    auto outputs =
        builder->split(input, splits, options, scope.GetExceptionState());
    EXPECT_EQ(outputs.size(), splits);
    for (auto output : outputs) {
      ASSERT_THAT(output, testing::NotNull());
      EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
      EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 6}));
    }
    auto* split = static_cast<const MLSplitOperator*>(outputs[0]->Operator());
    ASSERT_THAT(split, testing::NotNull());
    EXPECT_TRUE(split->IsEvenSplit());
    EXPECT_EQ(split->SplitNumber(), splits);
    EXPECT_EQ(split->Kind(), webnn::mojom::blink::Operation::Tag::kSplit);
    EXPECT_TRUE(split->IsConnected());
  }
  {
    // Test building split with a sequence of unsigned long splits and with
    // options.axis = 1.
    auto* input = BuildInput(builder, "input", {2, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const Vector<uint32_t> splits = {1, 2, 3};
    auto* options = MLSplitOptions::Create();
    options->setAxis(1);
    auto outputs =
        builder->split(input, splits, options, scope.GetExceptionState());
    EXPECT_EQ(outputs.size(), splits.size());
    for (uint32_t i = 0; i < outputs.size(); ++i) {
      ASSERT_THAT(outputs[i], testing::NotNull());
      EXPECT_EQ(outputs[i]->Kind(),
                webnn::mojom::blink::Operand::Kind::kOutput);
      EXPECT_EQ(outputs[i]->DataType(), V8MLOperandDataType::Enum::kFloat32);
      EXPECT_EQ(outputs[i]->Dimensions(), Vector<uint32_t>({2, splits[i]}));
    }
    auto* split = static_cast<const MLSplitOperator*>(outputs[0]->Operator());
    ASSERT_THAT(split, testing::NotNull());
    EXPECT_FALSE(split->IsEvenSplit());
    EXPECT_EQ(split->SplitSizes(), splits);
    EXPECT_EQ(split->Kind(), webnn::mojom::blink::Operation::Tag::kSplit);
    EXPECT_TRUE(split->IsConnected());
  }
  {
    // Test throwing exception when splitting a scalar.
    auto* input =
        BuildInput(builder, "input", {}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    const uint32_t splits = 2;
    auto* options = MLSplitOptions::Create();
    auto outputs =
        builder->split(input, splits, options, scope.GetExceptionState());
    EXPECT_EQ(outputs.size(), 0u);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The axis must be in the range [0, N-1] where N is the rank of the "
        "input tensor.");
  }
  {
    // Test throwing exception when axis is larger than input rank.
    auto* input = BuildInput(builder, "input", {2, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const uint32_t splits = 2;
    auto* options = MLSplitOptions::Create();
    options->setAxis(2);
    auto outputs =
        builder->split(input, splits, options, scope.GetExceptionState());
    EXPECT_EQ(outputs.size(), 0u);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The axis must be in the range [0, N-1] where N is the rank of the "
        "input tensor.");
  }
  {
    // Test throwing exception when axis is larger than input rank when splits
    // parameter is a sequence of unsigned long.
    auto* input = BuildInput(builder, "input", {2, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const Vector<uint32_t> splits = {1, 2, 3};
    auto* options = MLSplitOptions::Create();
    options->setAxis(2);
    auto outputs =
        builder->split(input, splits, options, scope.GetExceptionState());
    EXPECT_EQ(outputs.size(), 0u);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The axis must be in the range [0, N-1] where N is the rank of the "
        "input tensor.");
  }
  {
    // Test throwing exception when splits is equal to 0.
    auto* input = BuildInput(builder, "input", {2, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const uint32_t splits = 0;
    auto* options = MLSplitOptions::Create();
    auto outputs =
        builder->split(input, splits, options, scope.GetExceptionState());
    EXPECT_EQ(outputs.size(), 0u);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The splits must be greater than zero.");
  }
  {
    // Test throwing exception when the splits (unsigned long) can not evenly
    // divide the dimension size of input along options.axis.
    auto* input = BuildInput(builder, "input", {2, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const uint32_t splits = 2;
    auto* options = MLSplitOptions::Create();
    options->setAxis(1);
    auto outputs =
        builder->split(input, splits, options, scope.GetExceptionState());
    EXPECT_EQ(outputs.size(), 0u);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The dimension size of the input tensor along "
              "options.axis must be divisible by splits.");
  }
  {
    // Test throwing exception when the sum of splits (sequence of unsigned
    // long) sizes not equal to the dimension size of input along options.axis.
    auto* input = BuildInput(builder, "input", {2, 6},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    const Vector<uint32_t> splits = {2, 2, 3};
    auto* options = MLSplitOptions::Create();
    options->setAxis(1);
    auto outputs =
        builder->split(input, splits, options, scope.GetExceptionState());
    EXPECT_EQ(outputs.size(), 0u);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The sum of all sizes in splits must be equal to the dimension size "
        "of the input tensor specified by options.axis.");
  }
}

TEST_F(MLGraphBuilderTest, TanhTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test building tanh with float32 input.
    Vector<uint32_t> input_shape({3, 4});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* output = builder->tanh(input, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
    EXPECT_EQ(output->DataType(), V8MLOperandDataType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* tanh = output->Operator();
    ASSERT_THAT(tanh, testing::NotNull());
    EXPECT_EQ(tanh->Kind(), webnn::mojom::blink::Operation::Tag::kTanh);
    EXPECT_TRUE(tanh->IsConnected());
    EXPECT_THAT(tanh->Options(), testing::IsNull());
  }
  {
    // Test throwing exception when building tanh with int32 input.
    Vector<uint32_t> input_shape({3, 4});
    auto* input = BuildInput(builder, "input", input_shape,
                             V8MLOperandDataType::Enum::kInt32,
                             scope.GetExceptionState());
    auto* output = builder->tanh(input, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input data type must be one of the float32,float16 types.");
  }
  {
    // Test building tanh activation.
    MLActivation* tanh = builder->tanh(scope.GetExceptionState());
    ASSERT_THAT(tanh, testing::NotNull());
    ASSERT_THAT(tanh->Operator(), testing::NotNull());
    EXPECT_EQ(tanh->Kind(), webnn::mojom::blink::Activation::Tag::kTanh);
    EXPECT_FALSE(tanh->Operator()->IsConnected());
    EXPECT_THAT(tanh->Operator()->Options(), testing::IsNull());
  }
}

MLOperand* BuildMatmul(V8TestingScope& scope,
                       MLGraphBuilder* builder,
                       const MLOperand* a,
                       const MLOperand* b) {
  auto* output = builder->matmul(a, b, scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), a->DataType());
  auto* matmul = output->Operator();
  EXPECT_THAT(matmul, testing::NotNull());
  EXPECT_EQ(matmul->Kind(), webnn::mojom::blink::Operation::Tag::kMatmul);
  EXPECT_TRUE(matmul->IsConnected());
  EXPECT_THAT(matmul->Options(), testing::IsNull());
  return output;
}

TEST_F(MLGraphBuilderTest, MatmulTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test throwing exception when the rank of input is smaller than 2.
    auto* a = BuildInput(builder, "a", {2}, V8MLOperandDataType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {2}, V8MLOperandDataType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* output = builder->matmul(a, b, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The rank of input must be larger than or equal to 2.");
  }
  {
    // Test building matmul with 2-D * 4-D inputs.
    auto* a =
        BuildInput(builder, "a", {1, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b = BuildInput(builder, "b", {2, 2, 4, 2},
                         V8MLOperandDataType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* output = BuildMatmul(scope, builder, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 2, 1, 2}));
  }
  {
    // Test building matmul with 2-D * 2-D inputs.
    auto* a =
        BuildInput(builder, "a", {4, 2}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = BuildMatmul(scope, builder, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 3}));
  }
  {
    // Test building matmul with 3-D * 3-D inputs using broadcast.
    auto* a =
        BuildInput(builder, "a", {2, 3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {1, 4, 1}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = BuildMatmul(scope, builder, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 3, 1}));
  }
  {
    // Test building matmul with 4-D * 3-D inputs using broadcast.
    auto* a = BuildInput(builder, "a", {2, 2, 3, 4},
                         V8MLOperandDataType::Enum::kFloat32,
                         scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {1, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = BuildMatmul(scope, builder, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 2, 3, 5}));
  }
  {
    // Test building matmul with 3-D * 3-D inputs.
    auto* a =
        BuildInput(builder, "a", {2, 3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {2, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = BuildMatmul(scope, builder, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 3, 5}));
  }
  {
    // Test throwing exception when the data types of first two inputs don't
    // match.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {3, 4}, V8MLOperandDataType::Enum::kInt32,
                   scope.GetExceptionState());
    auto* output = builder->matmul(a, b, scope.GetExceptionState());
    ;
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The data types of first two inputs don't match.");
  }
  {
    // Test throwing exception when the number of columns in first matrix
    // mismatches with the number of rows in second matrix.
    auto* a =
        BuildInput(builder, "a", {2, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {2, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->matmul(a, b, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The number of columns (3) in the first matrix isn't equal to "
              "the number of rows (2) in the second matrix.");
  }
  {
    // Test throwing exception when the input shapes are not broadcastable.
    auto* a =
        BuildInput(builder, "a", {3, 3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {2, 4, 1}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->matmul(a, b, scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The matmul input shapes are not broadcastable.");
  }
}

MLOperand* BuildWhere(V8TestingScope& scope,
                      MLGraphBuilder* builder,
                      const MLOperand* condition,
                      const MLOperand* true_value,
                      const MLOperand* false_value) {
  auto* output = builder->where(condition, true_value, false_value,
                                scope.GetExceptionState());
  EXPECT_THAT(output, testing::NotNull());
  EXPECT_EQ(output->Kind(), webnn::mojom::blink::Operand::Kind::kOutput);
  EXPECT_EQ(output->DataType(), true_value->DataType());
  auto* where = output->Operator();
  EXPECT_THAT(where, testing::NotNull());
  EXPECT_EQ(where->Kind(), webnn::mojom::blink::Operation::Tag::kWhere);
  EXPECT_TRUE(where->IsConnected());
  EXPECT_THAT(where->Options(), testing::IsNull());
  return output;
}

TEST_F(MLGraphBuilderTest, WhereTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test throwing exception when the condition data type is not uint8.
    auto* condition = BuildInput(builder, "condition", {2, 4},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* true_value = BuildInput(builder, "true_value", {2, 4},
                                  V8MLOperandDataType::Enum::kFloat32,
                                  scope.GetExceptionState());
    auto* false_value = BuildInput(builder, "false_value", {2, 4},
                                   V8MLOperandDataType::Enum::kFloat32,
                                   scope.GetExceptionState());
    auto* output = builder->where(condition, true_value, false_value,
                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The condition data type must be uint8.");
  }
  {
    // Test throwing exception when the the data types of true_value and
    // false_value don't match.
    auto* condition = BuildInput(builder, "condition", {2, 4},
                                 V8MLOperandDataType::Enum::kUint8,
                                 scope.GetExceptionState());
    auto* true_value = BuildInput(builder, "true_value", {2, 4},
                                  V8MLOperandDataType::Enum::kFloat16,
                                  scope.GetExceptionState());
    auto* false_value = BuildInput(builder, "false_value", {2, 4},
                                   V8MLOperandDataType::Enum::kFloat32,
                                   scope.GetExceptionState());
    auto* output = builder->where(condition, true_value, false_value,
                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The data types of true_value and false_value don't match.");
  }
  {
    // Test throwing exception when the shapes of true_value and false_value are
    // not broadcastable.
    auto* condition = BuildInput(builder, "condition", {2, 4},
                                 V8MLOperandDataType::Enum::kUint8,
                                 scope.GetExceptionState());
    auto* true_value = BuildInput(builder, "true_value", {2, 4},
                                  V8MLOperandDataType::Enum::kFloat32,
                                  scope.GetExceptionState());
    auto* false_value = BuildInput(builder, "false_value", {2, 3},
                                   V8MLOperandDataType::Enum::kFloat32,
                                   scope.GetExceptionState());
    auto* output = builder->where(condition, true_value, false_value,
                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The shapes of true_value and false_value are not broadcastable.");
  }
  {
    // Test throwing exception when the condition shape is not broadcastable.
    auto* condition = BuildInput(builder, "condition", {2, 4},
                                 V8MLOperandDataType::Enum::kUint8,
                                 scope.GetExceptionState());
    auto* true_value = BuildInput(builder, "true_value", {2, 3},
                                  V8MLOperandDataType::Enum::kFloat32,
                                  scope.GetExceptionState());
    auto* false_value = BuildInput(builder, "false_value", {2, 1},
                                   V8MLOperandDataType::Enum::kFloat32,
                                   scope.GetExceptionState());
    auto* output = builder->where(condition, true_value, false_value,
                                  scope.GetExceptionState());
    EXPECT_THAT(output, testing::IsNull());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The condition shape is not broadcastable to the shape "
              "broadcasted from true_value and false_value.");
  }
  {
    // Test building where with 2-D condition, 2-D true_value and 2-D
    // false_value using broadcast.
    auto* condition = BuildInput(builder, "condition", {2, 1},
                                 V8MLOperandDataType::Enum::kUint8,
                                 scope.GetExceptionState());
    auto* true_value = BuildInput(builder, "true_value", {2, 4},
                                  V8MLOperandDataType::Enum::kFloat32,
                                  scope.GetExceptionState());
    auto* false_value = BuildInput(builder, "false_value", {2, 4},
                                   V8MLOperandDataType::Enum::kFloat32,
                                   scope.GetExceptionState());
    auto* output =
        BuildWhere(scope, builder, condition, true_value, false_value);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building where with 2-D condition, 2-D true_value and 3-D
    // false_value using broadcast.
    auto* condition = BuildInput(builder, "condition", {1, 4},
                                 V8MLOperandDataType::Enum::kUint8,
                                 scope.GetExceptionState());
    auto* true_value = BuildInput(builder, "true_value", {3, 4},
                                  V8MLOperandDataType::Enum::kFloat32,
                                  scope.GetExceptionState());
    auto* false_value = BuildInput(builder, "false_value", {2, 3, 4},
                                   V8MLOperandDataType::Enum::kFloat32,
                                   scope.GetExceptionState());
    auto* output =
        BuildWhere(scope, builder, condition, true_value, false_value);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 3, 4}));
  }
  {
    // Test building where with 3-D condition, 3-D true_value and 2-D
    // false_value using broadcast.
    auto* condition = BuildInput(builder, "condition", {2, 1, 4},
                                 V8MLOperandDataType::Enum::kUint8,
                                 scope.GetExceptionState());
    auto* true_value = BuildInput(builder, "true_value", {2, 3, 4},
                                  V8MLOperandDataType::Enum::kFloat32,
                                  scope.GetExceptionState());
    auto* false_value = BuildInput(builder, "false_value", {1, 4},
                                   V8MLOperandDataType::Enum::kFloat32,
                                   scope.GetExceptionState());
    auto* output =
        BuildWhere(scope, builder, condition, true_value, false_value);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 3, 4}));
  }
  {
    // Test building where with 4-D condition, 3-D true_value and 2-D
    // false_value using broadcast.
    auto* condition = BuildInput(builder, "condition", {2, 3, 4, 5},
                                 V8MLOperandDataType::Enum::kUint8,
                                 scope.GetExceptionState());
    auto* true_value = BuildInput(builder, "true_value", {3, 4, 5},
                                  V8MLOperandDataType::Enum::kFloat32,
                                  scope.GetExceptionState());
    auto* false_value = BuildInput(builder, "false_value", {4, 5},
                                   V8MLOperandDataType::Enum::kFloat32,
                                   scope.GetExceptionState());
    auto* output =
        BuildWhere(scope, builder, condition, true_value, false_value);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 3, 4, 5}));
  }
}

class FakeMLGraphBackend final : public MLGraph {
 public:
  // Create and build a FakeMLGraphBackend object. Resolve the promise with
  // this concrete object if no errors.
  static void ValidateAndBuild(MLContext* context,
                               const MLNamedOperands& named_outputs,
                               ScriptPromiseResolverTyped<MLGraph>* resolver) {
    auto* graph = MakeGarbageCollected<FakeMLGraphBackend>(context);
    graph->Build(ScopedMLTrace("Build"), named_outputs, resolver);
  }

  // The constructor shouldn't be called directly. The callers should use
  // ValidateAndBuild() method instead.
  explicit FakeMLGraphBackend(MLContext* context) : MLGraph(context) {}

  ~FakeMLGraphBackend() override = default;

 private:
  // Resolve the promise with this FakeMLGraphBackend object for testing the
  // input and output resources info.
  void BuildImpl(ScopedMLTrace scoped_trace,
                 const MLNamedOperands& named_outputs,
                 ScriptPromiseResolverTyped<MLGraph>* resolver) override {
    resolver->Resolve(this);
  }

  // Resolve the promise for testing the validation of inputs and outputs in
  // MLGraph::Compute().
  void ComputeImpl(ScopedMLTrace scoped_trace,
                   const MLNamedArrayBufferViews& inputs,
                   const MLNamedArrayBufferViews& outputs,
                   ScriptPromiseResolverTyped<MLComputeResult>* resolver,
                   ExceptionState& exception_state) override {
    resolver->Resolve();
  }
};

FakeMLGraphBackend* ToFakeMLGraphBackend(V8TestingScope* scope,
                                         ScriptValue value) {
  return NativeValueTraits<FakeMLGraphBackend>::NativeValue(
      scope->GetIsolate(), value.V8Value(), scope->GetExceptionState());
}

namespace {

// Helper class to create the FakeMLGraphBackend that is intended to test
// the GraphBuilder validation steps.
class FakeMLGraphBuilderBackend : public MLGraphBuilder::BackendForTesting {
 public:
  void BuildGraphImpl(MLContext* context,
                      const MLNamedOperands& named_outputs,
                      ScriptPromiseResolverTyped<MLGraph>* resolver) override {
    FakeMLGraphBackend::ValidateAndBuild(context, named_outputs, resolver);
  }
};

}  // namespace

// Helper class to test FakeMLGraphBackend.
class FakeMLGraphTest : public MLGraphTestBase {
 public:
  void SetUp() override {
    // Ensure MLGraphBuilder builds a FakeMLGraphBackend.
    MLGraphBuilder::SetBackendForTesting(&backend_for_testing);
  }

  void TearDown() override { MLGraphBuilder::SetBackendForTesting(nullptr); }

 private:
  FakeMLGraphBuilderBackend backend_for_testing;
};

TEST_P(FakeMLGraphTest, BuildTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test throwing exception if the named outputs is empty.
    MLNamedOperands named_outputs;
    auto [graph, exception] = BuildGraph(scope, builder, named_outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "At least one output needs to be provided.");
  }
  {
    // Test throwing exception if the named output is an input operand.
    auto* input = BuildInput(builder, "input", {3, 4, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto [graph, exception] = BuildGraph(scope, builder, {{"output", input}});
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "The operand with name \"output\" is not an output operand.");
  }
  {
    // Test throwing exception if the named output is a constant operand.
    auto* constant =
        BuildConstant(builder, {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto [graph, exception] =
        BuildGraph(scope, builder, {{"output", constant}});
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "The operand with name \"output\" is not an output operand.");
  }
  {
    // Test throwing exception if the named outputs is a mix of input and
    // constant operands.
    auto* input = BuildInput(builder, "input", {3, 4, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* constant =
        BuildConstant(builder, {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto [graph, exception] =
        BuildGraph(scope, builder, {{"output1", input}, {"output2", constant}});
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "The operand with name \"output1\" is not an output operand.");
  }
  {
    // Test throwing exception if two inputs have the same name.
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* c = builder->add(a, b, scope.GetExceptionState());
    ASSERT_THAT(c, testing::NotNull());

    auto [graph, exception] = BuildGraph(scope, builder, {{"c", c}});
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(), "The input name \"a\" is duplicated.");
  }
  {
    // Test building a graph with an elementwise add operator that uses the same
    // input for both lhs and rhs:
    //   [a]
    //   / \
    //   \ /
    //   add
    //    |
    //   [b]
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->add(a, a, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());
    auto [graph, exception] = BuildGraph(scope, builder, {{"b", output}});
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputResourcesInfo();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(inputs.at("a").data_type, a->DataType());
    EXPECT_EQ(inputs.at("a").byte_length, a->ByteLength());
    const auto& outputs = graph->GetOutputResourcesInfo();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(outputs.at("b").data_type, output->DataType());
    EXPECT_EQ(outputs.at("b").byte_length, output->ByteLength());
  }
  {
    // Test building a graph with two operators sharing a same input:
    //      [a]
    //     /   \
    //  relu   sigmoid
    //    |      |
    //   [b]    [c]
    auto* a =
        BuildInput(builder, "a", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b = builder->relu(a, scope.GetExceptionState());
    ASSERT_THAT(b, testing::NotNull());
    auto* c = builder->sigmoid(a, scope.GetExceptionState());
    ASSERT_THAT(c, testing::NotNull());
    auto [graph, exception] = BuildGraph(scope, builder, {{"b", b}, {"c", c}});
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputResourcesInfo();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(inputs.at("a").data_type, a->DataType());
    EXPECT_EQ(inputs.at("a").byte_length, a->ByteLength());
    const auto& outputs = graph->GetOutputResourcesInfo();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(outputs.at("b").data_type, b->DataType());
    EXPECT_EQ(outputs.at("b").byte_length, b->ByteLength());
    EXPECT_EQ(outputs.at("c").data_type, b->DataType());
    EXPECT_EQ(outputs.at("c").byte_length, b->ByteLength());
  }
  {
    // Test building a fake graph with two inputs, one gemm operation and one
    // output.
    auto* a =
        BuildInput(builder, "a", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* b =
        BuildInput(builder, "b", {4, 3}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* c = BuildGemm(scope, builder, a, b);

    auto [graph, exception] = BuildGraph(scope, builder, {{"c", c}});
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputResourcesInfo();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(2));
    EXPECT_EQ(inputs.at("a").data_type, a->DataType());
    EXPECT_EQ(inputs.at("a").byte_length, a->ByteLength());
    EXPECT_EQ(inputs.at("b").data_type, b->DataType());
    EXPECT_EQ(inputs.at("b").byte_length, b->ByteLength());
    const auto& outputs = graph->GetOutputResourcesInfo();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(outputs.at("c").data_type, c->DataType());
    EXPECT_EQ(outputs.at("c").byte_length, c->ByteLength());
  }
  {
    // Test building a fake graph with conv2d, add and relu operations.
    auto* input = BuildInput(builder, "input", {1, 1, 5, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto* filter = BuildConstant(builder, {1, 1, 3, 3},
                                 V8MLOperandDataType::Enum::kFloat32,
                                 scope.GetExceptionState());
    auto* conv2d = BuildConv2d(scope, builder, input, filter);
    auto* bias =
        BuildConstant(builder, {1}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* add = builder->add(conv2d, bias, scope.GetExceptionState());
    ASSERT_THAT(add, testing::NotNull());
    auto* output = builder->relu(add, scope.GetExceptionState());
    ASSERT_THAT(output, testing::NotNull());

    auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
    ASSERT_THAT(graph, testing::NotNull());
    const auto& inputs = graph->GetInputResourcesInfo();
    EXPECT_EQ(inputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(inputs.at("input").data_type, input->DataType());
    EXPECT_EQ(inputs.at("input").byte_length, input->ByteLength());
    const auto& outputs = graph->GetOutputResourcesInfo();
    EXPECT_EQ(outputs.size(), static_cast<uint32_t>(1));
    EXPECT_EQ(outputs.at("output").data_type, output->DataType());
    EXPECT_EQ(outputs.at("output").byte_length, output->ByteLength());
  }
  {
    // Testing throwing exception if the ArrayBufferView of a constant operand
    // is detached.
    auto* a =
        BuildInput(builder, "a", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({4, 3});
    desc->setDataType(V8MLOperandDataType::Enum::kFloat32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(12, V8MLOperandDataType::Enum::kFloat32);
    auto* b = builder->constant(desc, buffer_view, scope.GetExceptionState());
    auto* c = BuildGemm(scope, builder, a, b);

    // Detach the ArrayBufferView of constant b for testing.
    buffer_view->DetachForTesting();

    auto [graph, exception] = BuildGraph(scope, builder, {{"c", c}});
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "The array buffer view of the constant operand is detached.");
  }
}

// Helper struct to create an ArrayBufferView for MLNamedArrayBufferViews test.
struct ArrayBufferViewHelper {
  size_t number_of_elements;
  V8MLOperandDataType::Enum data_type;

  NotShared<DOMArrayBufferView> ToArrayBufferView() {
    return CreateDOMArrayBufferView(number_of_elements, data_type);
  }
};

// Helper function to create an ArrayBufferView given an operand.
NotShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand) {
  return CreateDOMArrayBufferView(operand->NumberOfElements(),
                                  operand->DataType());
}

TEST_P(FakeMLGraphTest, CreateNamedArrayBufferViewsTest) {
  constexpr auto kOperandDataTypes =
      base::MakeFixedFlatSet<V8MLOperandDataType::Enum>(
          {V8MLOperandDataType::Enum::kFloat32,
           V8MLOperandDataType::Enum::kFloat16,
           V8MLOperandDataType::Enum::kInt32,
           V8MLOperandDataType::Enum::kUint32,
           V8MLOperandDataType::Enum::kInt64,
           V8MLOperandDataType::Enum::kUint64, V8MLOperandDataType::Enum::kInt8,
           V8MLOperandDataType::Enum::kUint8});
  static_assert(kOperandDataTypes.size() == V8MLOperandDataType::kEnumSize,
                "The number of operand data types declared here needs to match "
                "all possible enumeration values defined in the IDL.");

  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    for (auto operand_data_type : kOperandDataTypes) {
      SCOPED_TRACE(testing::Message()
                   << "Testing for MLOperandDataType: "
                   << V8MLOperandDataType(operand_data_type).AsString());
      auto* input = BuildInput(builder, "input", {3, 4}, operand_data_type,
                               scope.GetExceptionState());
      MLNamedArrayBufferViews inputs;
      inputs.emplace_back("input", CreateArrayBufferViewForOperand(input));
      auto inputs_info = TransferNamedArrayBufferViews(
          scope.GetIsolate(), inputs, scope.GetExceptionState());
      ASSERT_THAT(inputs_info, testing::NotNull());
      auto* input_views = CreateNamedArrayBufferViews(std::move(inputs_info));
      ASSERT_THAT(input_views, testing::NotNull());
      EXPECT_EQ((*input_views)[0].first, "input");
      auto input_data_type = (*input_views)[0].second->GetType();
      switch (operand_data_type) {
        case V8MLOperandDataType::Enum::kFloat32:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeFloat32);
          break;
        case V8MLOperandDataType::Enum::kFloat16:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeUint16);
          break;
        case V8MLOperandDataType::Enum::kInt32:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeInt32);
          break;
        case V8MLOperandDataType::Enum::kUint32:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeUint32);
          break;
        case V8MLOperandDataType::Enum::kInt64:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeBigInt64);
          break;
        case V8MLOperandDataType::Enum::kUint64:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeBigUint64);
          break;
        case V8MLOperandDataType::Enum::kInt8:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeInt8);
          break;
        case V8MLOperandDataType::Enum::kUint8:
          EXPECT_EQ(input_data_type, DOMArrayBufferView::kTypeUint8);
          break;
      }
    }
  }
}

TEST_P(FakeMLGraphTest, ComputeTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  // Build a fake graph represents computation 'c = a * b';
  auto* a =
      BuildInput(builder, "a", {3, 4}, V8MLOperandDataType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* b =
      BuildInput(builder, "b", {4, 3}, V8MLOperandDataType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* c = BuildGemm(scope, builder, a, b);
  auto [graph, build_exception] = BuildGraph(scope, builder, {{"c", c}});
  ASSERT_THAT(graph, testing::NotNull());
  ASSERT_THAT(build_exception, testing::IsNull());
  {
    // Test throwing exception if the inputs is empty.
    MLNamedArrayBufferViews inputs;
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The number (0) of the array buffer views "
              "doesn't match the expectation (2).");
  }
  {
    // Test throwing exception if the number of inputs doesn't match.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The number (1) of the array buffer views "
              "doesn't match the expectation (2).");
  }
  {
    // Test throwing exception if the outputs is empty.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The number (0) of the array buffer views "
              "doesn't match the expectation (1).");
  }
  {
    // Test throwing exception if the number of outputs doesn't match.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    outputs.emplace_back("d", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The number (2) of the array buffer views "
              "doesn't match the expectation (1).");
  }
  {
    // Test throwing exception if the input name is unknown.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("invalid-input-name",
                        CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The name \"invalid-input-name\" isn't part of "
              "the graph.");
  }
  {
    // Test throwing exception if the output name is unknown.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("invalid-output-name",
                         CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The name \"invalid-output-name\" isn't part of "
              "the graph.");
  }
  {
    // Test throwing exception if the input array buffer view type is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back(
        "a",
        ArrayBufferViewHelper{.number_of_elements = 12,
                              .data_type = V8MLOperandDataType::Enum::kInt32}
            .ToArrayBufferView());
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(
        exception->message(),
        "Invalid inputs: The type (Int32) of the array buffer view with "
        "name \"a\" doesn't match the expected operand data type (float32).");
  }
  {
    // Test throwing exception if the input array buffer view size is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back(
        "a",
        ArrayBufferViewHelper{.number_of_elements = 10,
                              .data_type = V8MLOperandDataType::Enum::kFloat32}
            .ToArrayBufferView());
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid inputs: The byte length (40) of the array buffer view "
              "with name \"a\" doesn't match the expected byte length (48).");
  }
  {
    // Test throwing exception if the output array buffer view type is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back(
        "c",
        ArrayBufferViewHelper{.number_of_elements = 9,
                              .data_type = V8MLOperandDataType::Enum::kInt32}
            .ToArrayBufferView());
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(
        exception->message(),
        "Invalid outputs: The type (Int32) of the array buffer view with "
        "name \"c\" doesn't match the expected operand data type (float32).");
  }
  {
    // Test throwing exception if the output array buffer view size is wrong.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back(
        "c",
        ArrayBufferViewHelper{.number_of_elements = 8,
                              .data_type = V8MLOperandDataType::Enum::kFloat32}
            .ToArrayBufferView());
    auto* exception = ComputeGraph(scope, graph, inputs, outputs);
    ASSERT_THAT(exception, testing::NotNull());
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The byte length (32) of the array buffer view "
              "with name \"c\" doesn't match the expected byte length (36).");
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         FakeMLGraphTest,
                         testing::Values(BackendType::kFake),
                         TestParamInfoToString);

}  // namespace blink
