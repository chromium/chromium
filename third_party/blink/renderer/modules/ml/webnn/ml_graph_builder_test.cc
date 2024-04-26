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

class FakeMLGraphBackend final : public MLGraph {
 public:
  // Create and build a FakeMLGraphBackend object. Resolve the promise with
  // this concrete object if no errors.
  static void ValidateAndBuild(MLContext* context,
                               const MLNamedOperands& named_outputs,
                               ScriptPromiseResolver<MLGraph>* resolver) {
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
                 ScriptPromiseResolver<MLGraph>* resolver) override {
    resolver->Resolve(this);
  }

  // Resolve the promise for testing the validation of inputs and outputs in
  // MLGraph::Compute().
  void ComputeImpl(ScopedMLTrace scoped_trace,
                   const MLNamedArrayBufferViews& inputs,
                   const MLNamedArrayBufferViews& outputs,
                   ScriptPromiseResolver<MLComputeResult>* resolver,
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
                      ScriptPromiseResolver<MLGraph>* resolver) override {
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
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, named_outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message, "At least one output needs to be provided.");
  }
  {
    // Test throwing exception if the named output is an input operand.
    auto* input = BuildInput(builder, "input", {3, 4, 5},
                             V8MLOperandDataType::Enum::kFloat32,
                             scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", input}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "The operand with name \"output\" is not an output operand.");
  }
  {
    // Test throwing exception if the named output is a constant operand.
    auto* constant =
        BuildConstant(builder, {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", constant}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
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
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output1", input}, {"output2", constant}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
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

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"c", c}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message, "The input name \"a\" is duplicated.");
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
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"b", output}});
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
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"b", b}, {"c", c}});
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

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"c", c}});
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

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", output}});
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

    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"c", c}});
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
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
  auto [graph, error_name, error_message] =
      BuildGraph(scope, builder, {{"c", c}});
  ASSERT_THAT(graph, testing::NotNull());
  {
    // Test throwing exception if the inputs is empty.
    MLNamedArrayBufferViews inputs;
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid inputs: The number (0) of the array buffer views "
              "doesn't match the expectation (2).");
  }
  {
    // Test throwing exception if the number of inputs doesn't match.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    MLNamedArrayBufferViews outputs;
    outputs.emplace_back("c", CreateArrayBufferViewForOperand(c));
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid inputs: The number (1) of the array buffer views "
              "doesn't match the expectation (2).");
  }
  {
    // Test throwing exception if the outputs is empty.
    MLNamedArrayBufferViews inputs;
    inputs.emplace_back("a", CreateArrayBufferViewForOperand(a));
    inputs.emplace_back("b", CreateArrayBufferViewForOperand(b));
    MLNamedArrayBufferViews outputs;
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
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
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
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
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
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
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
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
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(
        error_message,
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
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
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
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(
        error_message,
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
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(error_name, "TypeError");
    EXPECT_EQ(error_message,
              "Invalid outputs: The byte length (32) of the array buffer view "
              "with name \"c\" doesn't match the expected byte length (36).");
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         FakeMLGraphTest,
                         testing::Values(BackendType::kFake),
                         TestParamInfoToString);

}  // namespace blink
