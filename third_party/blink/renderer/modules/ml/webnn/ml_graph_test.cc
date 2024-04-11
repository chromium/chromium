// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_triangular_options.h"
#include "third_party/blink/renderer/modules/ml/buildflags.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"

// TODO(https://crbug.com/1273291): Remove all uses of this macro.
#define SKIP_TEST_ON_UNSUPPORTED_BACKEND(backend_type)       \
  do {                                                       \
    if (GetParam() == backend_type)                          \
      GTEST_SKIP() << #backend_type << " is not supported."; \
  } while (0)

namespace blink {

namespace {

// kWebNNService is a valid parameter type, but ml_graph_test doesn't run
// against it.
const BackendType kGraphBackendType[] = {
#if BUILDFLAG(BUILD_WEBNN_WITH_XNNPACK)
    BackendType::kXnnpack,
#endif
};

}  // namespace

class MLGraphTest : public MLGraphTestBase {};

template <typename T>
struct ElementWiseBinaryTester {
  webnn::mojom::blink::ElementWiseBinary::Kind kind;
  OperandInfo<T> lhs;
  OperandInfo<T> rhs;
  Vector<T> expected;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* lhs_operand = BuildInput(builder, "lhs", lhs.dimensions,
                                   lhs.data_type, scope.GetExceptionState());
    auto* rhs_operand = BuildInput(builder, "rhs", rhs.dimensions,
                                   rhs.data_type, scope.GetExceptionState());
    auto* output_operand =
        BuildElementWiseBinary(scope, builder, kind, lhs_operand, rhs_operand);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"lhs", CreateArrayBufferViewForOperand(lhs_operand, lhs.values)},
         {"rhs", CreateArrayBufferViewForOperand(rhs_operand, rhs.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ElementWiseBinaryTest) {
  V8TestingScope scope;
  {
    // Test element-wise add operator for two 0-D scalars.
    // The expected results should be the sum of the values of the two input
    // scalars.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {},
                .values = {2.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {},
                .values = {3.0}},
        .expected = {5.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise add operator for two 1-D tensors.
    // The expected results should be the sum of the values of the two input
    // tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2},
                .values = {1.0, 2.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2},
                .values = {3.0, 4.0}},
        .expected = {4.0, 6.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise add operator for two 2-D tensors.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2, 2},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2, 2},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {6.0, 8.0, 10.0, 12.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise add operator for 0-D scalar broadcasting to 2-D
    // tensor.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2, 2},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {},
                .values = {5.0}},
        .expected = {6.0, 7.0, 8.0, 9.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise add operator for 1-D tensor broadcasting to 2-D
    // tensor.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2, 2},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2},
                .values = {5.0, 6.0}},
        .expected = {6.0, 8.0, 8.0, 10.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise add operator for 3-D tensor broadcasting to 3-D
    // tensor.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2, 1, 2},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {6.0, 8.0, 8.0, 10.0, 8.0, 10.0, 10.0, 12.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise add operator for two 4-D tensors
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kAdd,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {6.0, 8.0, 10.0, 12.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise sub operator for two 4-D tensors.
    // The expected results should be the difference of the values of the two
    // input tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kSub,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {-4.0, -4.0, -4.0, -4.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise mul operator for two 4-D tensors.
    // The expected results should be the product of the values of the two
    // input tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kMul,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {5.0, 12.0, 21.0, 32.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise div operator for two 4-D tensors.
    // The expected results should be the quotient of the values of the two
    // input tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kDiv,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {3.0, 4.0, 6.0, 8.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 2.0, 2.0, 2.0}},
        .expected = {3.0, 2.0, 3.0, 4.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise min operator for two 4-D tensors.
    // The expected results should be the lesser values of the two input
    // tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kMin,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 4.0, 5.0, 8.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {2.0, 3.0, 6.0, 7.0}},
        .expected = {1.0, 3.0, 5.0, 7.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise max operator for two 4-D tensors.
    // The expected results should be the greater values of the two input
    // tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseBinary::Kind::kMax,
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 4.0, 5.0, 8.0}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {2.0, 3.0, 6.0, 7.0}},
        .expected = {2.0, 4.0, 6.0, 8.0}}
        .Test(*this, scope);
  }
}

template <typename T>
struct PowTester {
  OperandInfo<T> lhs;
  OperandInfo<T> rhs;
  Vector<T> expected;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* lhs_operand = BuildInput(builder, "lhs", lhs.dimensions,
                                   lhs.data_type, scope.GetExceptionState());
    auto* rhs_operand = BuildConstant(builder, rhs.dimensions, rhs.data_type,
                                      rhs.values, scope.GetExceptionState());
    auto* output_operand = BuildElementWiseBinary(
        scope, builder, webnn::mojom::blink::ElementWiseBinary::Kind::kPow,
        lhs_operand, rhs_operand);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"lhs", CreateArrayBufferViewForOperand(lhs_operand, lhs.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, PowTest) {
  V8TestingScope scope;
  {
    // Test element-wise pow operator with exponent = 2.
    PowTester<float>{.lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                             .dimensions = {1, 2, 2, 1},
                             .values = {1.0, 2.0, 3.0, 4.0}},
                     .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                             .dimensions = {},
                             .values = {2.0}},
                     .expected = {1.0, 4.0, 9.0, 16.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise pow operator with exponent = 0.5.
    PowTester<float>{.lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                             .dimensions = {1, 2, 2, 1},
                             .values = {1.0, 4.0, 9.0, 16.0}},
                     .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                             .dimensions = {},
                             .values = {0.5}},
                     .expected = {1.0, 2.0, 3.0, 4.0}}
        .Test(*this, scope);
  }
}

template <typename T>
struct ElementWiseUnaryTester {
  webnn::mojom::blink::ElementWiseUnary::Kind kind;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLOperand* output_operand = nullptr;
    switch (kind) {
      case webnn::mojom::blink::ElementWiseUnary::Kind::kAbs:
        output_operand = builder->abs(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kCeil:
        output_operand =
            builder->ceil(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kCos:
        output_operand = builder->cos(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kExp:
        output_operand = builder->exp(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kFloor:
        output_operand =
            builder->floor(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kLog:
        output_operand = builder->log(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kNeg:
        output_operand = builder->neg(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kSin:
        output_operand = builder->sin(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kTan:
        output_operand = builder->tan(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kErf:
        output_operand = builder->erf(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kIdentity:
        output_operand =
            builder->identity(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kLogicalNot:
        output_operand =
            builder->logicalNot(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kReciprocal:
        output_operand =
            builder->reciprocal(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kSqrt:
        output_operand =
            builder->sqrt(input_operand, scope.GetExceptionState());
        break;
      case webnn::mojom::blink::ElementWiseUnary::Kind::kCast:
        // TODO: crbug.com/325598628 - Add tests for this case.
        break;
    }
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ElementWiseUnaryTest) {
  V8TestingScope scope;
  {
    // Test element-wise abs operator for a 0-D scalar.
    // The expected results should be the absolute value of the input scalar.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kAbs,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {},
                  .values = {-2.0}},
        .expected = {2.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise abs operator for a 1-D tensor.
    // The expected results should be the absolute value of the input tensor,
    // element-wise.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kAbs,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2},
                  .values = {-1.0, -2.0}},
        .expected = {1.0, 2.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise ceil operator for a 2-D tensor.
    // The expected results should be the ceiling of the input tensor,
    // element-wise.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kCeil,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2},
                  .values = {1.1, -2.2}},
        .expected = {2.0, -2.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise floor operator for a 3-D tensor.
    // The expected results should be the floor of the input tensor,
    // element-wise.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kFloor,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2},
                  .values = {1.1, -2.2, 3.3, -4.4}},
        .expected = {1.0, -3.0, 3.0, -5.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise neg operator for a 4-D tensor.
    // The expected results should be the numerical negative value of the input
    // tensor, element-wise.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kNeg,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1.0, -2.0, 3.0, -4.0}},
        .expected = {-1.0, 2.0, -3.0, 4.0}}
        .Test(*this, scope);
  }
  {
    // Test element-wise sqrt operator for a 4-D tensor.
    // The expected results should be the square root value of the input
    // tensor, element-wise.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kSqrt,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1.0, 4.0, 9.0, 16.0}},
        .expected = {1.0, 2.0, 3.0, 4.0}}
        .Test(*this, scope);
  }
  // Below operators are not implemented on XNNPACK backend.
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kXnnpack);
  {
    // Test element-wise Cos operator.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kCos,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1, -2, 3, -4}},
        .expected = {cos(1.f), cos(-2.f), cos(3.f), cos(-4.f)}}
        .Test(*this, scope);
  }
  {
    // Test element-wise Exp operator.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kExp,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1, -2, 3, -4}},
        .expected = {exp(1.f), exp(-2.f), exp(3.f), exp(-4.f)}}
        .Test(*this, scope);
  }
  {
    // Test element-wise Log operator.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kLog,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {3},
                  .values = {0, 3, 10}},
        .expected = {log(0.f), log(3.f), log(10.f)}}
        .Test(*this, scope);
  }
  {
    // Test element-wise Sin operator.
    ElementWiseUnaryTester<float>{
        .kind = webnn::mojom::blink::ElementWiseUnary::Kind::kSin,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1, -2, 3, -4}},
        .expected = {sin(1.f), sin(-2.f), sin(3.f), sin(-4.f)}}
        .Test(*this, scope);
  }
}

template <typename T>
struct PReluTester {
  OperandInfo<T> input;
  OperandInfo<T> slope;
  Vector<T> expected;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* slope_operand =
        BuildConstant(builder, slope.dimensions, slope.data_type, slope.values,
                      scope.GetExceptionState());
    auto* output_operand =
        builder->prelu(input_operand, slope_operand, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, PReluTest) {
  V8TestingScope scope;
  {
    // Test prelu operator with input_shape = {3} and slope_shape =
    // {3}.
    PReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {3},
                  .values = {1.0, -2.0, 3.0}},
        .slope = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {3},
                  .values = {1.0, 2.0, 3.0}},
        .expected = {1.0, -4.0, 3.0}}
        .Test(*this, scope);
  }
  {
    // Test prelu operator with input_shape = {1, 2, 3, 3} and slope_shape = {1,
    // 3}.
    PReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {-1.0, -2.0, -3.0, -4.0, -5.0, -6.0, -7.0, -8.0,
                             -9.0, -10.0, -11.0, -12.0, -13.0, -14.0, -15.0,
                             -16.0, -17.0, -18.0}},
        .slope = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3},
                  .values = {1.0, 2.0, 3.0}},
        .expected = {-1.0, -4.0, -9.0, -4.0, -10.0, -18.0, -7.0, -16.0, -27.0,
                     -10.0, -22.0, -36.0, -13.0, -28.0, -45.0, -16.0, -34.0,
                     -54.0}}
        .Test(*this, scope);
  }
}

template <typename T>
struct ReluTester {
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->relu(input_operand, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ReluTest) {
  V8TestingScope scope;
  {
    // Test relu operator for 1-D tensor.
    // The expected results should be the result of the rectified linear
    // function, y = max(0, x), applied to the input tensor, element-wise.
    ReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2},
                  .values = {-1.0, 1.0}},
        .expected = {0.0, 1.0}}
        .Test(*this, scope);
  }
  {
    // Test relu operator for 0-D scalar.
    ReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {},
                  .values = {-1.0}},
        .expected = {0.0}}
        .Test(*this, scope);
  }
  {
    // Test relu operator for 2-D tensor.
    ReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .expected = {0.0, 0.0, 0.5, 10.0}}
        .Test(*this, scope);
  }
  {
    // Test relu operator for 3-D tensor.
    ReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .expected = {0.0, 0.0, 0.5, 10.0}}
        .Test(*this, scope);
  }
  {
    // Test relu operator for 4-D tensor.
    ReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .expected = {0.0, 0.0, 0.5, 10.0}}
        .Test(*this, scope);
  }
}

template <typename T>
struct LeakyReluTester {
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLLeakyReluOptions* options = MLLeakyReluOptions::Create()) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        BuildLeakyRelu(scope, builder, input_operand, options);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, LeakyReluTest) {
  V8TestingScope scope;
  {
    // Test leakyRelu operator with default options.
    auto* options = MLLeakyReluOptions::Create();
    LeakyReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {10, 5, -100, 0}},
        .expected = {10, 5, -1, 0}}
        .Test(*this, scope, options);
  }
  {
    // Test leakyRelu operator with alpha = 0.2.
    auto* options = MLLeakyReluOptions::Create();
    options->setAlpha(0.2);
    LeakyReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {10, 5, -100, 0}},
        .expected = {10, 5, -20, 0}}
        .Test(*this, scope, options);
  }
  {
    // Test leakyRelu operator for scalar input.
    auto* options = MLLeakyReluOptions::Create();
    LeakyReluTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {},
                  .values = {-100}},
        .expected = {-1}}
        .Test(*this, scope, options);
  }
}

template <typename T>
struct ReduceTester {
  webnn::mojom::blink::Reduce::Kind kind;
  OperandInfo<T> input;
  bool keep_dimensions = false;
  Vector<T> expected;
  Vector<uint32_t> expected_output_shape;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLReduceOptions* options = MLReduceOptions::Create()) {
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    options->setKeepDimensions(keep_dimensions);
    auto* output_operand =
        BuildReduce(scope, builder, kind, input_operand, options);
    EXPECT_EQ(output_operand->Dimensions(), expected_output_shape);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ReduceTest) {
  V8TestingScope scope;
  {
    // Test reduceMean operator with default options.
    auto* options = MLReduceOptions::Create();
    ReduceTester<float>{
        .kind = webnn::mojom::blink::Reduce::Kind::kMean,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1.0, 2.0, 3.0, 4.0}},
        .expected = {2.5},
        .expected_output_shape = {}}
        .Test(*this, scope, options);
  }
  {
    // Test reduceMean operator with keep_dimensions = true.
    auto* options = MLReduceOptions::Create();
    ReduceTester<float>{
        .kind = webnn::mojom::blink::Reduce::Kind::kMean,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1.0, 2.0, 3.0, 4.0}},
        .keep_dimensions = true,
        .expected = {2.5},
        .expected_output_shape = {1, 1, 1, 1}}
        .Test(*this, scope, options);
  }
  {
    // Test reduceMean operator with axes = {1} and keep_dimensions = false.
    auto* options = MLReduceOptions::Create();
    options->setAxes({1});
    ReduceTester<float>{
        .kind = webnn::mojom::blink::Reduce::Kind::kMean,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1.0, 2.0, 3.0, 4.0}},
        .expected = {1.5, 3.5},
        .expected_output_shape = {2}}
        .Test(*this, scope, options);
  }
  {
    // Test reduceMean operator with axes = {1} and keep_dimensions = true.
    auto* options = MLReduceOptions::Create();
    options->setAxes({1});
    ReduceTester<float>{
        .kind = webnn::mojom::blink::Reduce::Kind::kMean,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1.0, 2.0, 3.0, 4.0}},
        .keep_dimensions = true,
        .expected = {1.5, 3.5},
        .expected_output_shape = {2, 1}}
        .Test(*this, scope, options);
  }
}

template <typename T>
struct ClampTester {
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLClampOptions* options = MLClampOptions::Create()) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->clamp(input_operand, options, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ClampTest) {
  V8TestingScope scope;
  {
    // Test clamp operator with the minimum value defined.
    MLClampOptions* options = MLClampOptions::Create();
    options->setMinValue(0.0);
    ClampTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .expected = {0.0, 0.0, 0.5, 10.0}}
        .Test(*this, scope, options);
  }
  {
    // Test clamp operator with the minimum = 0 and maximum = 6.
    MLClampOptions* options = MLClampOptions::Create();
    options->setMinValue(0.0);
    options->setMaxValue(6.0);
    ClampTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .expected = {0.0, 0.0, 0.5, 6.0}}
        .Test(*this, scope, options);
  }
  {
    // Test clamp operator with the minimum = -1 and maximum = 1.
    MLClampOptions* options = MLClampOptions::Create();
    options->setMinValue(-1.0);
    options->setMaxValue(1.0);
    ClampTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .expected = {-1, -0.5, 0.5, 1}}
        .Test(*this, scope, options);
  }
  {
    // Test clamp operator for scalar input.
    MLClampOptions* options = MLClampOptions::Create();
    options->setMinValue(0.0);
    options->setMaxValue(6.0);
    ClampTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {},
                  .values = {10.0}},
        .expected = {6.0}}
        .Test(*this, scope, options);
  }
  {
    // Test clamp operator with default options that no minimum and maximum
    // values are defined.
    ClampTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .expected = {-10.0, -0.5, 0.5, 10.0}}
        .Test(*this, scope);
  }
  {
    // Test clamp operator with the maximum value defined.
    MLClampOptions* options = MLClampOptions::Create();
    options->setMaxValue(6.0);
    ClampTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .expected = {-10.0, -0.5, 0.5, 6.0}}
        .Test(*this, scope, options);
  }
}

template <typename T>
struct Conv2dTester {
  OperandInfo<T> input;
  OperandInfo<T> filter;
  std::optional<OperandInfo<T>> bias = std::nullopt;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            MLConv2dOptions* options = MLConv2dOptions::Create()) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* filter_operand =
        BuildConstant(builder, filter.dimensions, filter.data_type,
                      filter.values, scope.GetExceptionState());
    if (bias) {
      options->setBias(BuildConstant(
          builder, bias.value().dimensions, bias.value().data_type,
          bias.value().values, scope.GetExceptionState()));
    }
    auto* output_operand =
        BuildConv2d(scope, builder, input_operand, filter_operand, options);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, Conv2dTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test conv2d operator for nhwc input layout and ohwi filter layout.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    Conv2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {3, 1, 1, 3},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0}},
        .expected = {30.0, 36.0, 42.0, 66.0, 81.0, 96.0, 102.0, 126.0, 150.0,
                     138.0, 171.0, 204.0, 174.0, 216.0, 258.0, 210.0, 261.0,
                     312.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test conv2d operator for explicit padding are not same as the calculated
    // padding with kSameUpper, input, filter size, stride and dilation that
    // are used by CalculateConv2dPadding function.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    // The paddings are {1, 1, 1, 1} with calculating by CalculateConv2dPadding
    // function.
    options->setPadding({2, 2, 1, 1});
    options->setStrides({2, 2});
    Conv2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 7, 5, 1},
                  .values = Vector<float>(35, 1.0)},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 3, 3, 1},
                   .values = Vector<float>(9, 1.0)},
        .expected = {2.0, 3.0, 2.0, 6.0, 9.0, 6.0, 6.0, 9.0, 6.0, 6.0, 9.0, 6.0,
                     2.0, 3.0, 2.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test fused conv2d operator for nhwc input layout and ohwi filter
    // layout, fusing with bias operand and relu activation.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    options->setActivation(builder->relu(scope.GetExceptionState()));
    Conv2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {3, 1, 1, 3},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0}},
        .bias =
            OperandInfo<float>{.data_type = V8MLOperandDataType::Enum::kFloat32,
                               .dimensions = {3},
                               .values = {-6000.0, -7000.0, 8000.0}},
        .expected = {0.0, 0.0, 8042.0, 0.0, 0.0, 8096.0, 0.0, 0.0, 8150.0, 0.0,
                     0.0, 8204.0, 0.0, 0.0, 8258.0, 0.0, 0.0, 8312.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test depthwise conv2d operator by setting groups to input channels,
    // nhwc input layout, ihwo filter layout.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    options->setGroups(4);
    Conv2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 4},
                  .values = {10.0, 21.0, 10.0, 0.0, 10.0, 22.0, 20.0, 0.0, 10.0,
                             23.0, 30.0, 0.0, 10.0, 24.0, 40.0, 0.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 2, 2, 4},
                   .values = {0.25, 0.0, 10.0, 50.0, 0.25, 1.0, 20.0, 50.0,
                              0.25, 0.0, 30.0, 50.0, 0.25, 1.0, 40.0, 50.0}},
        .expected = {10.0, 46.0, 3000.0, 0.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test fused depthwise conv2d operator by setting groups to input
    // channels, nhwc input layout, ihwo filter layout, fusing with bias
    // operand and relu activation.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    options->setGroups(4);
    options->setActivation(builder->relu(scope.GetExceptionState()));
    Conv2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 4},
                  .values = {10.0, 21.0, 10.0, 0.0, 10.0, 22.0, 20.0, 0.0, 10.0,
                             23.0, 30.0, 0.0, 10.0, 24.0, 40.0, 0.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 2, 2, 4},
                   .values = {0.25, 0.0, 10.0, 50.0, 0.25, 1.0, 20.0, 50.0,
                              0.25, 0.0, 30.0, 50.0, 0.25, 1.0, 40.0, 50.0}},
        .bias =
            OperandInfo<float>{.data_type = V8MLOperandDataType::Enum::kFloat32,
                               .dimensions = {4},
                               .values = {-6000.0, -7000.0, 8000.0, 9000.0}},
        .expected = {0.0, 0.0, 11000.0, 9000.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test fused depthwise conv2d operator by setting groups to input
    // channels, nhwc input layout, ihwo filter layout, fusing with bias
    // operand and clamp activation.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    options->setGroups(4);
    auto* clamp_options = MLClampOptions::Create();
    clamp_options->setMinValue(0.0);
    clamp_options->setMaxValue(6.0);
    options->setActivation(
        builder->clamp(clamp_options, scope.GetExceptionState()));
    Conv2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 4},
                  .values = {10.0, 21.0, 10.0, 0.0, 10.0, 22.0, 20.0, 0.0, 10.0,
                             23.0, 30.0, 0.0, 10.0, 24.0, 40.0, 0.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 2, 2, 4},
                   .values = {0.25, 0.0, 10.0, 50.0, 0.25, 1.0, 20.0, 50.0,
                              0.25, 0.0, 30.0, 50.0, 0.25, 1.0, 40.0, 50.0}},
        .bias =
            OperandInfo<float>{.data_type = V8MLOperandDataType::Enum::kFloat32,
                               .dimensions = {4},
                               .values = {-6000.0, -7000.0, 8000.0, 9000.0}},
        .expected = {0.0, 0.0, 6.0, 6.0}}
        .Test(*this, scope, builder, options);
  }
}

template <typename T>
struct ConvTranspose2dTester {
  OperandInfo<T> input;
  OperandInfo<T> filter;
  std::optional<OperandInfo<T>> bias = std::nullopt;
  Vector<T> expected;

  void Test(
      MLGraphTest& helper,
      V8TestingScope& scope,
      MLGraphBuilder* builder,
      MLConvTranspose2dOptions* options = MLConvTranspose2dOptions::Create()) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* filter_operand =
        BuildConstant(builder, filter.dimensions, filter.data_type,
                      filter.values, scope.GetExceptionState());
    if (bias) {
      options->setBias(BuildConstant(
          builder, bias.value().dimensions, bias.value().data_type,
          bias.value().values, scope.GetExceptionState()));
    }
    auto* output_operand = BuildConvTranspose2d(scope, builder, input_operand,
                                                filter_operand, options);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ConvTranspose2dTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test convTranspose2d operator for nhwc input layout and ohwi filter
    // layout.
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    ConvTranspose2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 3, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 3, 3, 1},
                   .values = {1.0, 3.0, 5.0, 7.0, 9.0, 2.0, 4.0, 6.0, 8.0}},
        .expected = {1.0,  5.0,   14.0,  19.0,  15.0,  11.0,  40.0,
                     82.0, 74.0,  36.0,  39.0,  114.0, 195.0, 165.0,
                     81.0, 65.0,  163.0, 235.0, 173.0, 66.0,  28.0,
                     74.0, 140.0, 118.0, 72.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test fused convTranspose2d operator for nhwc input layout and ohwi filter
    // layout, fusing with bias operand and relu activation.
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    options->setActivation(builder->relu(scope.GetExceptionState()));
    ConvTranspose2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 3, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {3, 3, 3, 1},
                   .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,
                              9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0,
                              1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0}},
        .bias =
            OperandInfo<float>{.data_type = V8MLOperandDataType::Enum::kFloat32,
                               .dimensions = {3},
                               .values = {-6000.0, -7000.0, 8000.0}},
        .expected = {0.0, 0.0, 8001.0, 0.0, 0.0, 8004.0, 0.0, 0.0, 8010.0,
                     0.0, 0.0, 8012.0, 0.0, 0.0, 8009.0, 0.0, 0.0, 8008.0,
                     0.0, 0.0, 8026.0, 0.0, 0.0, 8056.0, 0.0, 0.0, 8054.0,
                     0.0, 0.0, 8036.0, 0.0, 0.0, 8030.0, 0.0, 0.0, 8084.0,
                     0.0, 0.0, 8165.0, 0.0, 0.0, 8144.0, 0.0, 0.0, 8090.0,
                     0.0, 0.0, 8056.0, 0.0, 0.0, 8134.0, 0.0, 0.0, 8236.0,
                     0.0, 0.0, 8186.0, 0.0, 0.0, 8108.0, 0.0, 0.0, 8049.0,
                     0.0, 0.0, 8112.0, 0.0, 0.0, 8190.0, 0.0, 0.0, 8144.0,
                     0.0, 0.0, 8081.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test convTranspose2d operator by setting padding=1.
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    options->setPadding({1, 1, 1, 1});
    ConvTranspose2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 5, 5, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,
                             1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,
                             1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 3, 3, 1},
                   .values = {1.0, 3.0, 5.0, 7.0, 9.0, 2.0, 4.0, 6.0, 8.0}},
        .expected = {48.0,  100.0, 127.0, 145.0, 101.0, 126.0, 186.0,
                     231.0, 213.0, 132.0, 132.0, 249.0, 285.0, 267.0,
                     153.0, 156.0, 231.0, 213.0, 177.0, 147.0, 129.0,
                     217.0, 217.0, 199.0, 95.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test convTranspose2d operator by setting strides=2, padding=1.
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    options->setStrides({2, 2});
    options->setPadding({1, 1, 1, 1});
    ConvTranspose2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 3, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 3, 3, 1},
                   .values = {1.0, 3.0, 5.0, 7.0, 9.0, 2.0, 4.0, 6.0, 8.0}},
        .expected = {9.0,   16.0, 18.0, 25.0, 27.0, 18.0, 41.0, 27.0, 59.0,
                     36.0,  36.0, 43.0, 45.0, 52.0, 54.0, 45.0, 95.0, 54.0,
                     113.0, 63.0, 63.0, 70.0, 72.0, 79.0, 81.0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test convTranspose2d by setting outputSizes={1, 8, 8, 1}.
    auto* options = MLConvTranspose2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(
        V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
    options->setStrides({2, 2});
    options->setOutputSizes({8, 8});
    ConvTranspose2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 3, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 3, 3, 1},
                   .values = {1.0, 3.0, 5.0, 7.0, 9.0, 2.0, 4.0, 6.0, 8.0}},
        .expected = {1.0,  3.0,  7.0,  6.0,  13.0,  9.0,  15.0, 0.0,
                     7.0,  9.0,  16.0, 18.0, 25.0,  27.0, 6.0,  0.0,
                     8.0,  18.0, 41.0, 27.0, 59.0,  36.0, 54.0, 0.0,
                     28.0, 36.0, 43.0, 45.0, 52.0,  54.0, 12.0, 0.0,
                     23.0, 45.0, 95.0, 54.0, 113.0, 63.0, 93.0, 0.0,
                     49.0, 63.0, 70.0, 72.0, 79.0,  81.0, 18.0, 0.0,
                     28.0, 42.0, 88.0, 48.0, 100.0, 54.0, 72.0, 0.0,
                     0.0,  0.0,  0.0,  0.0,  0.0,   0.0,  0.0,  0.0}}
        .Test(*this, scope, builder, options);
  }
}

template <typename T>
struct GemmTester {
  OperandInfo<T> a;
  OperandInfo<T> b;
  std::optional<OperandInfo<T>> c = std::nullopt;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            MLGemmOptions* options = MLGemmOptions::Create()) {
    // Build the graph.
    auto* a_operand = BuildInput(builder, "input", a.dimensions, a.data_type,
                                 scope.GetExceptionState());
    auto* b_operand = BuildConstant(builder, b.dimensions, b.data_type,
                                    b.values, scope.GetExceptionState());
    if (c) {
      options->setC(BuildConstant(builder, c.value().dimensions,
                                  c.value().data_type, c.value().values,
                                  scope.GetExceptionState()));
    }
    auto* output_operand =
        BuildGemm(scope, builder, a_operand, b_operand, options);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input", CreateArrayBufferViewForOperand(a_operand, a.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, GemmTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test gemm operator without operand c.
    GemmTester<float>{.a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {2, 2},
                            .values = {1.0, 2.0, 2.0, 1.0}},
                      .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {2, 1},
                            .values = {2.0, 4.0}},
                      .expected = {10.0, 8.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test gemm operator with operand c.
    GemmTester<float>{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 2},
              .values = {1.0, 2.0, 2.0, 1.0}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 1},
              .values = {2.0, 4.0}},
        .c =
            OperandInfo<float>{.data_type = V8MLOperandDataType::Enum::kFloat32,
                               .dimensions = {1},
                               .values = {1.0}},
        .expected = {11.0, 9.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test gemm operator with bTranspose = true.
    auto* options = MLGemmOptions::Create();
    options->setBTranspose(true);
    GemmTester<float>{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 2},
              .values = {1.0, 2.0, 2.0, 1.0}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {1, 2},
              .values = {2.0, 4.0}},
        .c =
            OperandInfo<float>{.data_type = V8MLOperandDataType::Enum::kFloat32,
                               .dimensions = {1},
                               .values = {1.0}},
        .expected = {11.0, 9.0}}
        .Test(*this, scope, builder, options);
  }
}

struct HardSwishTester {
  OperandInfo<float> input;
  Vector<float> expected;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->hardSwish(input_operand, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    ExpectFloatArrayEqual(results, expected);
  }
};

TEST_P(MLGraphTest, HardSwishTest) {
  V8TestingScope scope;
  {
    // Test hardSwish operator for 1-D tensor.
    // The expected results should be the result of the nonlinear function, y
    // = x * max(0, min(6, (x + 3))) / 6, applied to the input tensor,
    // element-wise.
    HardSwishTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                              .dimensions = {2},
                              .values = {-0.6, 0.6}},
                    .expected = {-0.24, 0.36}}
        .Test(*this, scope);
  }
  {
    // Test hardSwish operator for 0-D scalar.
    HardSwishTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                              .dimensions = {},
                              .values = {0.6}},
                    .expected = {0.36}}
        .Test(*this, scope);
  }
  {
    // Test hardSwish operator for 2-D tensor.
    HardSwishTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                              .dimensions = {2, 2},
                              .values = {-1.2, -0.6, 0.6, 1.2}},
                    .expected = {-0.36, -0.24, 0.36, 0.84}}
        .Test(*this, scope);
  }
  {
    // Test hardSwish operator for 3-D tensor.
    HardSwishTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                              .dimensions = {1, 2, 2},
                              .values = {-1.2, -0.6, 0.6, 1.2}},
                    .expected = {-0.36, -0.24, 0.36, 0.84}}
        .Test(*this, scope);
  }
  {
    // Test hardSwish operator for 4-D tensor.
    HardSwishTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                              .dimensions = {1, 2, 2, 1},
                              .values = {-1.2, -0.6, 0.6, 1.2}},
                    .expected = {-0.36, -0.24, 0.36, 0.84}}
        .Test(*this, scope);
  }
}

// Because reshape Node runs copy operator, ReshapeTester just checks the
// output against the input. So there is no need to set expected results.
template <typename T>
struct ReshapeTester {
  OperandInfo<T> input;
  Vector<uint32_t> new_shape;
  Vector<uint32_t> expected_output_shape;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->reshape(input_operand, new_shape, scope.GetExceptionState());
    EXPECT_EQ(output_operand->Dimensions(), expected_output_shape);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, input.values);
  }
};

TEST_P(MLGraphTest, ReshapeTest) {
  V8TestingScope scope;
  {
    // Test reshaping 1-D 1-element tensor to 0-D scalar.
    ReshapeTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1},
                  .values = {1.0}},
        .new_shape = {},
        .expected_output_shape = {}}
        .Test(*this, scope);
  }
  {
    // Test reshaping 0-D scalar to 1-D 1-element tensor.
    ReshapeTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {},
                  .values = {1.0}},
        .new_shape = {1},
        .expected_output_shape = {1}}
        .Test(*this, scope);
  }
  {
    // Test reshaping 2-D tensor to 1-D tensor.
    ReshapeTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .new_shape = {4},
        .expected_output_shape = {4}}
        .Test(*this, scope);
  }
  {
    // Test reshaping from 4-D tensor to 2-D tensor.
    ReshapeTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {-10.0, -0.5, 0.5, 10.0}},
        .new_shape = {1, 4},
        .expected_output_shape = {1, 4}}
        .Test(*this, scope);
  }
}

template <typename T>
struct SigmoidTester {
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->sigmoid(input_operand, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    ASSERT_EQ(outputs.size(), 1u);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    ExpectFloatArrayEqual(results, expected);
  }
};

TEST_P(MLGraphTest, SigmoidTest) {
  V8TestingScope scope;
  {
    // Test sigmoid with a 0-D scalar input.
    SigmoidTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {},
                  .values = {0}},
        .expected = {0.5}}
        .Test(*this, scope);
  }
  {
    // Test sigmoid with a 1d input.
    SigmoidTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {3},
                  .values = {0, 0, 0}},
        .expected = {0.5, 0.5, 0.5}}
        .Test(*this, scope);
  }
  {
    // Test sigmoid with a 3d input.
    SigmoidTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3, 1},
                  .values = {0, 0, 0, 0, 0, 0}},
        .expected = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5}}
        .Test(*this, scope);
  }
}

template <typename T, typename S>
struct SplitTester {
  OperandInfo<T> input;
  S splits;
  Vector<Vector<T>> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            MLSplitOptions* options = MLSplitOptions::Create()) {
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto output_operands = builder->split(input_operand, splits, options,
                                          scope.GetExceptionState());
    MLNamedOperands named_operands;
    for (uint32_t i = 0; i < output_operands.size(); ++i) {
      named_operands.push_back(
          std::pair<WTF::String, blink::Member<blink::MLOperand>>{
              "output" + String::Number(i), output_operands[i]});
    }
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, named_operands);
    ASSERT_THAT(graph, testing::NotNull());

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs;
    for (uint32_t i = 0; i < output_operands.size(); ++i) {
      outputs.push_back(
          std::pair<WTF::String, blink::NotShared<blink::DOMArrayBufferView>>{
              "output" + String::Number(i),
              CreateArrayBufferViewForOperand(output_operands[i])});
    }
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    for (uint32_t i = 0; i < outputs.size(); ++i) {
      auto result = GetArrayBufferViewValues<T>(outputs[i].second);
      EXPECT_EQ(result, expected[i]);
    }
  }
};

TEST_P(MLGraphTest, SplitTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test split operator with default options.
    auto* options = MLSplitOptions::Create();
    SplitTester<float, uint32_t>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {0.0, 1.0, 2.0, 3.0}},
        .splits = 2u,
        .expected = {{0.0, 1.0}, {2.0, 3.0}}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test split operator with axis = 1 when splits is an unsigned long.
    auto* options = MLSplitOptions::Create();
    options->setAxis(1);
    SplitTester<float, uint32_t>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 4},
                  .values = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}},
        .splits = 2u,
        .expected = {{0.0, 1.0, 4.0, 5.0}, {2.0, 3.0, 6.0, 7.0}}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test split operator with default options when splits is a sequence of
    // unsigned long.
    auto* options = MLSplitOptions::Create();
    SplitTester<float, Vector<uint32_t>>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {0.0, 1.0, 2.0, 3.0}},
        .splits = {1, 1},
        .expected = {{0.0, 1.0}, {2.0, 3.0}}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test split operator with axis = 1 when splits is a sequence of unsigned
    // long.
    auto* options = MLSplitOptions::Create();
    options->setAxis(1);
    SplitTester<float, Vector<uint32_t>>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 4},
                  .values = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}},
        .splits = {1, 2, 1},
        .expected = {{0.0, 4.0}, {1.0, 2.0, 5.0, 6.0}, {3.0, 7.0}}}
        .Test(*this, scope, builder, options);
  }
}

template <typename T>
struct ConcatTester {
  Vector<OperandInfo<T>> inputs;
  uint32_t axis;
  Vector<uint32_t> expected_output_shape;
  Vector<T> expected_output_data;

  void Test(MLGraphTest& helper, V8TestingScope& scope) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    HeapVector<Member<MLOperand>> input_operands;
    String input_str = "input_";
    for (uint32_t i = 0; i < inputs.size(); ++i) {
      input_operands.push_back(BuildInput(
          builder, input_str + String::Number(i), inputs[i].dimensions,
          inputs[i].data_type, scope.GetExceptionState()));
    }
    auto* output_operand =
        builder->concat(input_operands, axis, scope.GetExceptionState());
    EXPECT_EQ(output_operand->Dimensions(), expected_output_shape);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews named_inputs;
    for (uint32_t i = 0; i < inputs.size(); ++i) {
      named_inputs.push_back(std::pair<String, NotShared<DOMArrayBufferView>>{
          input_str + String::Number(i),
          CreateArrayBufferViewForOperand(input_operands[i],
                                          inputs[i].values)});
    }
    MLNamedArrayBufferViews named_outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, named_inputs, named_outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(named_outputs[0].second);
    EXPECT_EQ(results, expected_output_data);
  }
};

TEST_P(MLGraphTest, ConcatTest) {
  V8TestingScope scope;
  {
    // Test concat operator with one input and axis = 0.
    ConcatTester<float>{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1., 2., 3., 4.}}},
        .axis = 0,
        .expected_output_shape = {2, 2},
        .expected_output_data = {1., 2., 3., 4.}}
        .Test(*this, scope);
  }
  {
    // Test concat operator with two inputs and axis = 0.
    ConcatTester<float>{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1., 2., 3., 4.}},
                   {.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1., 2., 3., 4.}}},
        .axis = 0,
        .expected_output_shape = {4, 2},
        .expected_output_data = {1., 2., 3., 4., 1., 2., 3., 4.}}
        .Test(*this, scope);
  }
  {
    // Test concat operator with two inputs and axis = 1;
    ConcatTester<float>{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1., 2., 3., 4.}},
                   {.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1., 2., 3., 4.}}},
        .axis = 1,
        .expected_output_shape = {2, 4},
        .expected_output_data = {1., 2., 1., 2., 3., 4., 3., 4.}}
        .Test(*this, scope);
  }
  {
    // Test concat operator with three inputs and axis = 0.
    ConcatTester<float>{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {1, 2},
                    .values = {1., 2.}},
                   {.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {2, 2},
                    .values = {1., 2., 3., 4.}},
                   {.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {3, 2},
                    .values = {1., 2., 3., 4., 5., 6.}}},
        .axis = 0,
        .expected_output_shape = {6, 2},
        .expected_output_data = {1., 2., 1., 2., 3., 4., 1., 2., 3., 4., 5.,
                                 6.}}
        .Test(*this, scope);
  }
  {
    // Test concat operator with four inputs and axis = 2.
    ConcatTester<float>{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {1, 2, 1},
                    .values = {1., 2.}},
                   {.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {1, 2, 2},
                    .values = {1., 2., 3., 4.}},
                   {.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {1, 2, 3},
                    .values = {1., 2., 3., 4., 5., 6.}},
                   {.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {1, 2, 4},
                    .values = {1., 2., 3., 4., 5., 6., 7., 8.}}},
        .axis = 2,
        .expected_output_shape = {1, 2, 10},
        .expected_output_data = {1.0, 1.0, 2.0, 1.0, 2.0, 3.0, 1.0,
                                 2.0, 3.0, 4.0, 2.0, 3.0, 4.0, 4.0,
                                 5.0, 6.0, 5.0, 6.0, 7.0, 8.0}}
        .Test(*this, scope);
  }
}

template <typename T>
struct PadTester {
  OperandInfo<T> input;
  Vector<uint32_t> beginning_padding;
  Vector<uint32_t> ending_padding;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            MLPadOptions* options = MLPadOptions::Create()) {
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand = BuildPad(scope, builder, input_operand,
                                    beginning_padding, ending_padding, options);
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, PadTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test pad operator with default options.
    auto* options = MLPadOptions::Create();
    PadTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3},
                  .values = {1, 2, 3, 4, 5, 6}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .expected = {0., 0., 0., 0., 0., 0., 0., 0., 0., 1., 2., 3., 0., 0.,
                     0., 0., 4., 5., 6., 0., 0., 0., 0., 0., 0., 0., 0., 0.}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test pad operator with options->value = 8.
    auto* options = MLPadOptions::Create();
    options->setValue(8);
    PadTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3},
                  .values = {1, 2, 3, 4, 5, 6}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .expected = {8., 8., 8., 8., 8., 8., 8., 8., 8., 1., 2., 3., 8., 8.,
                     8., 8., 4., 5., 6., 8., 8., 8., 8., 8., 8., 8., 8., 8.}}
        .Test(*this, scope, builder, options);
  }
  // Reflection and Symmetric padding mode are not implemented on XNNPACK.
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kXnnpack);
  {
    // Test pad with mode = "reflection".
    auto* options = MLPadOptions::Create();
    options->setMode("reflection");
    PadTester<float>{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                               .dimensions = {1, 1, 2, 3},
                               .values = {0, 1, 2, 3, 4, 5}},
                     .beginning_padding = {0, 0, 1, 2},
                     .ending_padding = {0, 0, 1, 2},
                     .expected = {5, 4, 3, 4, 5, 4, 3, 2, 1, 0, 1, 2, 1, 0,
                                  5, 4, 3, 4, 5, 4, 3, 2, 1, 0, 1, 2, 1, 0}}
        .Test(*this, scope, builder, options);
  }
  {
    // Test pad with mode = "symmetric".
    auto* options = MLPadOptions::Create();
    options->setMode("symmetric");
    PadTester<float>{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                               .dimensions = {1, 2, 3, 1},
                               .values = {0, 1, 2, 3, 4, 5}},
                     .beginning_padding = {0, 1, 2, 0},
                     .ending_padding = {0, 1, 2, 0},
                     .expected = {1, 0, 0, 1, 2, 2, 1, 1, 0, 0, 1, 2, 2, 1,
                                  4, 3, 3, 4, 5, 5, 4, 4, 3, 3, 4, 5, 5, 4}}
        .Test(*this, scope, builder, options);
  }
}

template <typename T>
struct SliceTester {
  OperandInfo<T> input;
  Vector<uint32_t> starts;
  Vector<uint32_t> sizes;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->slice(input_operand, starts, sizes, scope.GetExceptionState());
    auto [graph, error_name, error_message] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, SliceTest) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test slice with input_shape = {3, 4, 5}, starts = {0, 0, 1} and sizes =
    // {2, 3, 4}.
    SliceTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {3, 4, 5},
                  .values = {1,  4,  4,  -6, -3, -1, 7,  3,  1,  -8, 1,  -1,
                             -2, -3, 6,  7,  6,  1,  -5, -7, 1,  1,  5,  3,
                             3,  3,  -3, -8, 2,  -1, 8,  -1, -6, 1,  -7, 1,
                             4,  1,  -5, 1,  -8, 4,  1,  -1, 9,  -4, 1,  -5,
                             -4, -1, 4,  -1, -3, 7,  1,  9,  -4, -9, -8, -9}},
        .starts = {0, 0, 1},
        .sizes = {2, 3, 4},
        .expected = {4, 4, -6, -3, 7,  3,  1, -8, -1, -2, -3, 6,
                     1, 5, 3,  3,  -3, -8, 2, -1, -1, -6, 1,  -7}}
        .Test(*this, scope, builder);
  }
}

TEST_P(MLGraphTest, BuildAndComputeGraphWithOnlyConstants) {
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Build the graph whose relu operator has only constant operand as input.
    auto* constant_operand =
        BuildConstant<float>(builder, {3}, V8MLOperandDataType::Enum::kFloat32,
                             {-1, 0, 1}, scope.GetExceptionState());
    ASSERT_THAT(constant_operand, testing::NotNull());
    auto* output_operand =
        builder->relu(constant_operand, scope.GetExceptionState());
    ASSERT_THAT(output_operand, testing::NotNull());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs;
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({0, 0, 1}));
  }
  {
    // Build the graph whose add operator has only constant operands as input.
    auto* constant_a_operand = BuildConstant<float>(
        builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32, {1, 1, 1, 1},
        scope.GetExceptionState());
    ASSERT_THAT(constant_a_operand, testing::NotNull());
    auto* constant_b_operand = BuildConstant<float>(
        builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32, {2, 2, 2, 2},
        scope.GetExceptionState());
    ASSERT_THAT(constant_b_operand, testing::NotNull());
    auto* output_operand = builder->add(constant_a_operand, constant_b_operand,
                                        scope.GetExceptionState());
    ASSERT_THAT(output_operand, testing::NotNull());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs;
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({3, 3, 3, 3}));
  }
  {
    // Build the graph whose add and mul operators have only constant and
    // intermediate operands as input.
    auto* constant_a_operand = BuildConstant<float>(
        builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32, {1, 1, 1, 1},
        scope.GetExceptionState());
    ASSERT_THAT(constant_a_operand, testing::NotNull());
    auto* constant_b_operand = BuildConstant<float>(
        builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32, {2, 2, 2, 2},
        scope.GetExceptionState());
    ASSERT_THAT(constant_b_operand, testing::NotNull());
    auto* intermediate_operand = builder->add(
        constant_a_operand, constant_b_operand, scope.GetExceptionState());
    ASSERT_THAT(intermediate_operand, testing::NotNull());
    auto* constant_c_operand = BuildConstant<float>(
        builder, {2, 2}, V8MLOperandDataType::Enum::kFloat32, {3, 3, 3, 3},
        scope.GetExceptionState());
    ASSERT_THAT(constant_c_operand, testing::NotNull());
    auto* output_operand = builder->mul(
        intermediate_operand, constant_c_operand, scope.GetExceptionState());
    ASSERT_THAT(output_operand, testing::NotNull());
    auto [graph, error_name, error_message] =
        BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_THAT(graph, testing::NotNull());

    // Compute the graph.
    MLNamedArrayBufferViews inputs;
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    std::tie(error_name, error_message) =
        ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_TRUE(error_name.IsNull());
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({9, 9, 9, 9}));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         MLGraphTest,
                         testing::ValuesIn(kGraphBackendType),
                         TestParamInfoToString);

}  // namespace blink
