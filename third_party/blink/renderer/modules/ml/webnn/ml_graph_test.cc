// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/modules/ml/buildflags.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"

#define SKIP_TEST_ON_UNSUPPORTED_BACKEND(backend_type)       \
  do {                                                       \
    if (GetBackendType() == backend_type)                    \
      GTEST_SKIP() << #backend_type << " is not supported."; \
  } while (0)

namespace blink {

namespace {

const TestVariety kGraphTestVariety[] = {
#if BUILDFLAG(BUILD_WEBNN_WITH_XNNPACK)
    {BackendType::kXnnpack, ExecutionMode::kAsync},
    {BackendType::kXnnpack, ExecutionMode::kSync},
#endif

#if BUILDFLAG(BUILD_WEBNN_ON_CROS)
    {BackendType::kModelLoader, ExecutionMode::kAsync},
#endif
};

}  // namespace

class MLGraphTest : public MLGraphTestBase {};

template <typename T>
struct ElementWiseBinaryTester {
  ElementWiseBinaryKind kind;
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"lhs", CreateArrayBufferViewForOperand(lhs_operand, lhs.values)},
         {"rhs", CreateArrayBufferViewForOperand(rhs_operand, rhs.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ElementWiseBinaryTest) {
  MLGraphV8TestingScope scope;
  {
    // Test element-wise add operator for two 0-D scalars.
    // The expected results should be the sum of the values of the two input
    // scalars.
    ElementWiseBinaryTester<float>{
        .kind = ElementWiseBinaryKind::kAdd,
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
        .kind = ElementWiseBinaryKind::kAdd,
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
        .kind = ElementWiseBinaryKind::kAdd,
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
        .kind = ElementWiseBinaryKind::kAdd,
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
        .kind = ElementWiseBinaryKind::kAdd,
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
        .kind = ElementWiseBinaryKind::kAdd,
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
        .kind = ElementWiseBinaryKind::kAdd,
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
        .kind = ElementWiseBinaryKind::kSub,
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
        .kind = ElementWiseBinaryKind::kMul,
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
        .kind = ElementWiseBinaryKind::kDiv,
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
        .kind = ElementWiseBinaryKind::kMin,
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
        .kind = ElementWiseBinaryKind::kMax,
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
        scope, builder, ElementWiseBinaryKind::kPow, lhs_operand, rhs_operand);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"lhs", CreateArrayBufferViewForOperand(lhs_operand, lhs.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, PowTest) {
  MLGraphV8TestingScope scope;
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
  ElementWiseUnaryKind kind;
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
      case ElementWiseUnaryKind::kAbs:
        output_operand = builder->abs(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kCeil:
        output_operand =
            builder->ceil(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kCos:
        output_operand = builder->cos(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kExp:
        output_operand = builder->exp(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kFloor:
        output_operand =
            builder->floor(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kLog:
        output_operand = builder->log(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kNeg:
        output_operand = builder->neg(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kSin:
        output_operand = builder->sin(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kTan:
        output_operand = builder->tan(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kErf:
        output_operand = builder->erf(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kIdentity:
        output_operand =
            builder->identity(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kLogicalNot:
        output_operand =
            builder->logicalNot(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kReciprocal:
        output_operand =
            builder->reciprocal(input_operand, scope.GetExceptionState());
        break;
      case ElementWiseUnaryKind::kSqrt:
        output_operand =
            builder->sqrt(input_operand, scope.GetExceptionState());
        break;
    }
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ElementWiseUnaryTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
  V8TestingScope scope;
  {
    // Test element-wise abs operator for a 0-D scalar.
    // The expected results should be the absolute value of the input scalar.
    ElementWiseUnaryTester<float>{
        .kind = ElementWiseUnaryKind::kAbs,
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
        .kind = ElementWiseUnaryKind::kAbs,
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
        .kind = ElementWiseUnaryKind::kCeil,
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
        .kind = ElementWiseUnaryKind::kFloor,
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
        .kind = ElementWiseUnaryKind::kNeg,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1.0, -2.0, 3.0, -4.0}},
        .expected = {-1.0, 2.0, -3.0, 4.0}}
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, PReluTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ReluTest) {
  MLGraphV8TestingScope scope;
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, LeakyReluTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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
  ReduceKind kind;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLReduceOptions* options = MLReduceOptions::Create()) {
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        BuildReduce(scope, builder, kind, input_operand, options);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ReduceTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
  V8TestingScope scope;
  {
    // Test reduceMean operator with default options.
    auto* options = MLReduceOptions::Create();
    ReduceTester<float>{
        .kind = ReduceKind::kMean,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1.0, 2.0, 3.0, 4.0}},
        .expected = {2.5}}
        .Test(*this, scope, options);
  }
  {
    // Test reduceMean operator with axes = {1}.
    auto* options = MLReduceOptions::Create();
    options->setAxes({1});
    ReduceTester<float>{
        .kind = ReduceKind::kMean,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2},
                  .values = {1.0, 2.0, 3.0, 4.0}},
        .expected = {1.5, 3.5}}
        .Test(*this, scope, options);
  }
}

template <typename T>
struct Resample2dTester {
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLResample2dOptions* options = MLResample2dOptions::Create()) {
    // Build the graph.
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        BuildResample2d(scope, builder, input_operand, options);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, Resample2dTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
  V8TestingScope scope;
  {
    // Test resample2d operator with axes = {1, 2}, sizes = {4, 4}.
    auto* options = MLResample2dOptions::Create();
    options->setSizes({4, 4});
    options->setAxes({1, 2});
    options->setMode(V8MLInterpolationMode::Enum::kLinear);
    Resample2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1, 2, 3, 4}},
        .expected = {1., 1.25, 1.75, 2., 1.5, 1.75, 2.25, 2.5, 2.5, 2.75, 3.25,
                     3.5, 3., 3.25, 3.75, 4.}}
        .Test(*this, scope, options);
  }
  {
    // Test resample2d operator with axes = {1, 2}, scales = {2.0, 2.0}.
    auto* options = MLResample2dOptions::Create();
    options->setScales({2.0, 2.0});
    options->setAxes({1, 2});
    options->setMode(V8MLInterpolationMode::Enum::kLinear);
    Resample2dTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1, 2, 3, 4}},
        .expected = {1., 1.25, 1.75, 2., 1.5, 1.75, 2.25, 2.5, 2.5, 2.75, 3.25,
                     3.5, 3., 3.25, 3.75, 4.}}
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ClampTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
  V8TestingScope scope;
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
  {
    // Test clamp operator with both the minimum and maximum values defined.
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
}

template <typename T>
struct Conv2dTester {
  OperandInfo<T> input;
  OperandInfo<T> filter;
  absl::optional<OperandInfo<T>> bias = absl::nullopt;
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, Conv2dTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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
  absl::optional<OperandInfo<T>> bias = absl::nullopt;
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ConvTranspose2dTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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
  absl::optional<OperandInfo<T>> c = absl::nullopt;
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input", CreateArrayBufferViewForOperand(a_operand, a.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
  }
};

TEST_P(MLGraphTest, GemmTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results.size(), expected.size());
    for (wtf_size_t i = 0; i < expected.size(); ++i) {
      EXPECT_FLOAT_EQ(results[i], expected[i]);
    }
  }
};

TEST_P(MLGraphTest, HardSwishTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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

template <typename T>
struct Pool2dTester {
  Pool2dKind kind;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLPool2dOptions* options = MLPool2dOptions::Create()) {
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(),
                             scope.GetScriptState(), scope.GetExceptionState());
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        BuildPool2d(scope, builder, kind, input_operand, options);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, Pool2dTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
  V8TestingScope scope;
  {
    // Test averagePool2d operator for nhwc input layout.
    auto* options = MLPool2dOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setWindowDimensions({3, 3});
    Pool2dTester<float>{
        .kind = Pool2dKind::kAverage,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 4, 4, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0}},
        .expected = {6.0, 7.0, 10.0, 11.0}}
        .Test(*this, scope, options);
  }
  {
    // Test global averagePool2d operator for nhwc input layout.
    auto* options = MLPool2dOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    Pool2dTester<float>{
        .kind = Pool2dKind::kAverage,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 4, 4, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0}},
        .expected = {8.5}}
        .Test(*this, scope, options);
  }
  {
    // Test maxPool2d operator for nhwc input layout.
    auto* options = MLPool2dOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setWindowDimensions({3, 3});
    Pool2dTester<float>{
        .kind = Pool2dKind::kMax,
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 4, 4, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0}},
        .expected = {11.0, 12.0, 15.0, 16.0}}
        .Test(*this, scope, options);
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, input.values);
  }
};

TEST_P(MLGraphTest, ReshapeTest) {
  MLGraphV8TestingScope scope;
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, named_operands);
    ASSERT_NE(graph, nullptr);

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
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    for (uint32_t i = 0; i < outputs.size(); ++i) {
      auto result = GetArrayBufferViewValues<T>(outputs[i].second);
      EXPECT_EQ(result, expected[i]);
    }
  }
};

TEST_P(MLGraphTest, SplitTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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
struct TransposeTester {
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(MLGraphTest& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            MLTransposeOptions* options = MLTransposeOptions::Create()) {
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        BuildTranspose(scope, builder, input_operand, options);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, TransposeTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
  V8TestingScope scope;
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), scope.GetScriptState(),
                           scope.GetExceptionState());
  {
    // Test transpose operator with default options.
    auto* options = MLTransposeOptions::Create();
    TransposeTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3, 4},
                  .values =
                      {
                          0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                          12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                      }},
        .expected =
            {
                0, 12, 4, 16, 8,  20, 1, 13, 5, 17, 9,  21,
                2, 14, 6, 18, 10, 22, 3, 15, 7, 19, 11, 23,
            }}
        .Test(*this, scope, builder, options);
  }
  {
    // Test transpose with permutation = {0, 2, 1}.
    auto* options = MLTransposeOptions::Create();
    options->setPermutation({{0, 2, 1}});
    TransposeTester<float>{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3, 4},
                  .values =
                      {
                          0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                          12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                      }},
        .expected =
            {
                0,  4,  8,  1,  5,  9,  2,  6,  10, 3,  7,  11,
                12, 16, 20, 13, 17, 21, 14, 18, 22, 15, 19, 23,
            }}
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

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
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, named_inputs, named_outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(named_outputs[0].second);
    EXPECT_EQ(results, expected_output_data);
  }
};

TEST_P(MLGraphTest, ConcatTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, PadTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper.ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, SliceTest) {
  SKIP_TEST_ON_UNSUPPORTED_BACKEND(BackendType::kModelLoader);
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

INSTANTIATE_TEST_SUITE_P(All,
                         MLGraphTest,
                         testing::ValuesIn(kGraphTestVariety),
                         TestVarietyToString);

}  // namespace blink
