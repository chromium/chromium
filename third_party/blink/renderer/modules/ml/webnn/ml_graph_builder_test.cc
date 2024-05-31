// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"

#include <algorithm>
#include <limits>
#include <memory>

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

}  // namespace blink
