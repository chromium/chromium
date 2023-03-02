// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <numeric>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"

namespace blink {

class MLGraphTest : public MLGraphTestBase {};

template <typename T>
struct ElementWiseBinaryTester {
  MLGraphTest* helper;
  ElementWiseBinaryKind kind;
  OperandInfo<T> lhs;
  OperandInfo<T> rhs;
  Vector<T> expected;

  void Test(V8TestingScope& scope) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* lhs_operand = BuildInput(builder, "lhs", lhs.dimensions, lhs.type,
                                   scope.GetExceptionState());
    auto* rhs_operand = BuildInput(builder, "rhs", rhs.dimensions, rhs.type,
                                   scope.GetExceptionState());
    auto* output_operand =
        BuildElementWiseBinary(scope, builder, kind, lhs_operand, rhs_operand);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"lhs", CreateArrayBufferViewForOperand(lhs_operand, lhs.values)},
         {"rhs", CreateArrayBufferViewForOperand(rhs_operand, rhs.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ElementWiseBinaryTest) {
  V8TestingScope scope;
  {
    // Test element-wise add operator for two 1-D tensors.
    // The expected results should be the sum of the values of the two input
    // tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2},
                .values = {1.0, 2.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2},
                .values = {3.0, 4.0}},
        .expected = {4.0, 6.0}}
        .Test(scope);
  }
  {
    // Test element-wise add operator for two 2-D tensors.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2, 2},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2, 2},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {6.0, 8.0, 10.0, 12.0}}
        .Test(scope);
  }
  {
    // Test element-wise add operator for 1-D tensor broadcasting to 2-D
    // tensor.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2, 2},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2},
                .values = {5.0, 6.0}},
        .expected = {6.0, 8.0, 8.0, 10.0}}
        .Test(scope);
  }
  {
    // Test element-wise add operator for 3-D tensor broadcasting to 3-D
    // tensor.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {2, 1, 2},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {6.0, 8.0, 8.0, 10.0, 8.0, 10.0, 10.0, 12.0}}
        .Test(scope);
  }
  {
    // Test element-wise add operator for two 4-D tensors
    ElementWiseBinaryTester<float>{
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {6.0, 8.0, 10.0, 12.0}}
        .Test(scope);
  }
  {
    // Test element-wise sub operator for two 4-D tensors.
    // The expected results should be the difference of the values of the two
    // input tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kSub,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {-4.0, -4.0, -4.0, -4.0}}
        .Test(scope);
  }
  {
    // Test element-wise mul operator for two 4-D tensors.
    // The expected results should be the prdocut of the values of the two input
    // tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kMul,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 2.0, 3.0, 4.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {5.0, 6.0, 7.0, 8.0}},
        .expected = {5.0, 12.0, 21.0, 32.0}}
        .Test(scope);
  }
  {
    // Test element-wise div operator for two 4-D tensors.
    // The expected results should be the quotient of the values of the two
    // input tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kDiv,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {3.0, 4.0, 6.0, 8.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 2.0, 2.0, 2.0}},
        .expected = {3.0, 2.0, 3.0, 4.0}}
        .Test(scope);
  }
  {
    // Test element-wise min operator for two 4-D tensors.
    // The expected results should be the lesser values of the two input
    // tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kMin,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 4.0, 5.0, 8.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {2.0, 3.0, 6.0, 7.0}},
        .expected = {1.0, 3.0, 5.0, 7.0}}
        .Test(scope);
  }
  {
    // Test element-wise max operator for two 4-D tensors.
    // The expected results should be the greater values of the two input
    // tensors, element-wise.
    ElementWiseBinaryTester<float>{
        .helper = this,
        .kind = ElementWiseBinaryKind::kMax,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {1.0, 4.0, 5.0, 8.0}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32,
                .dimensions = {1, 2, 2, 1},
                .values = {2.0, 3.0, 6.0, 7.0}},
        .expected = {2.0, 4.0, 6.0, 8.0}}
        .Test(scope);
  }
}

template <typename T>
struct ReluTester {
  MLGraphTestBase* helper;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(V8TestingScope& scope) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->relu(input_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
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
    ReluTester<float>{.helper = this,
                      .input = {.type = V8MLOperandType::Enum::kFloat32,
                                .dimensions = {2},
                                .values = {-1.0, 1.0}},
                      .expected = {0.0, 1.0}}
        .Test(scope);
  }
  {
    // Test relu operator for 2-D tensor.
    ReluTester<float>{.helper = this,
                      .input = {.type = V8MLOperandType::Enum::kFloat32,
                                .dimensions = {2, 2},
                                .values = {-10.0, -0.5, 0.5, 10.0}},
                      .expected = {0.0, 0.0, 0.5, 10.0}}
        .Test(scope);
  }
  {
    // Test relu operator for 3-D tensor.
    ReluTester<float>{.helper = this,
                      .input = {.type = V8MLOperandType::Enum::kFloat32,
                                .dimensions = {1, 2, 2},
                                .values = {-10.0, -0.5, 0.5, 10.0}},
                      .expected = {0.0, 0.0, 0.5, 10.0}}
        .Test(scope);
  }
  {
    // Test relu operator for 4-D tensor.
    ReluTester<float>{.helper = this,
                      .input = {.type = V8MLOperandType::Enum::kFloat32,
                                .dimensions = {1, 2, 2, 1},
                                .values = {-10.0, -0.5, 0.5, 10.0}},
                      .expected = {0.0, 0.0, 0.5, 10.0}}
        .Test(scope);
  }
}

template <typename T>
struct Resample2dTester {
  MLGraphTestBase* helper;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(V8TestingScope& scope,
            MLResample2dOptions* options = MLResample2dOptions::Create()) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        BuildResample2d(scope, builder, input_operand, options);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, Resample2dTest) {
  V8TestingScope scope;
  {
    // Test resample2d operator with axes = {1, 2}, sizes = {4, 4}.
    auto* options = MLResample2dOptions::Create();
    options->setSizes({4, 4});
    options->setAxes({1, 2});
    options->setMode(V8MLInterpolationMode::Enum::kLinear);
    Resample2dTester<float>{
        .helper = this,
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1, 2, 3, 4}},
        .expected = {1., 1.25, 1.75, 2., 1.5, 1.75, 2.25, 2.5, 2.5, 2.75, 3.25,
                     3.5, 3., 3.25, 3.75, 4.}}
        .Test(scope, options);
  }
  {
    // Test resample2d operator with axes = {1, 2}, scales = {2.0, 2.0}.
    auto* options = MLResample2dOptions::Create();
    options->setScales({2.0, 2.0});
    options->setAxes({1, 2});
    options->setMode(V8MLInterpolationMode::Enum::kLinear);
    Resample2dTester<float>{
        .helper = this,
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1},
                  .values = {1, 2, 3, 4}},
        .expected = {1., 1.25, 1.75, 2., 1.5, 1.75, 2.25, 2.5, 2.5, 2.75, 3.25,
                     3.5, 3., 3.25, 3.75, 4.}}
        .Test(scope, options);
  }
}

template <typename T>
struct ClampTester {
  MLGraphTestBase* helper;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(V8TestingScope& scope,
            MLClampOptions* options = MLClampOptions::Create()) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->clamp(input_operand, options, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, ClampTest) {
  V8TestingScope scope;
  {
    // Test clamp operator with default options that no minimum and maximum
    // values are defined.
    ClampTester<float>{.helper = this,
                       .input = {.type = V8MLOperandType::Enum::kFloat32,
                                 .dimensions = {1, 2, 2, 1},
                                 .values = {-10.0, -0.5, 0.5, 10.0}},
                       .expected = {-10.0, -0.5, 0.5, 10.0}}
        .Test(scope);
  }
  {
    // Test clamp operator with the minimum value defined.
    MLClampOptions* options = MLClampOptions::Create();
    options->setMinValue(0.0);
    ClampTester<float>{.helper = this,
                       .input = {.type = V8MLOperandType::Enum::kFloat32,
                                 .dimensions = {1, 2, 2, 1},
                                 .values = {-10.0, -0.5, 0.5, 10.0}},
                       .expected = {0.0, 0.0, 0.5, 10.0}}
        .Test(scope, options);
  }
  {
    // Test clamp operator with the maximum value defined.
    MLClampOptions* options = MLClampOptions::Create();
    options->setMaxValue(6.0);
    ClampTester<float>{.helper = this,
                       .input = {.type = V8MLOperandType::Enum::kFloat32,
                                 .dimensions = {1, 2, 2, 1},
                                 .values = {-10.0, -0.5, 0.5, 10.0}},
                       .expected = {-10.0, -0.5, 0.5, 6.0}}
        .Test(scope, options);
  }
  {
    // Test clamp operator with both the minimum and maximum values defined.
    MLClampOptions* options = MLClampOptions::Create();
    options->setMinValue(0.0);
    options->setMaxValue(6.0);
    ClampTester<float>{.helper = this,
                       .input = {.type = V8MLOperandType::Enum::kFloat32,
                                 .dimensions = {1, 2, 2, 1},
                                 .values = {-10.0, -0.5, 0.5, 10.0}},
                       .expected = {0.0, 0.0, 0.5, 6.0}}
        .Test(scope, options);
  }
}

template <typename T>
MLOperand* BuildConstant(MLGraphBuilder* builder,
                         const Vector<uint32_t>& dimensions,
                         V8MLOperandType::Enum type,
                         const Vector<T>& values,
                         ExceptionState& exception_state) {
  size_t buffer_size = std::accumulate(dimensions.begin(), dimensions.end(),
                                       size_t(1), std::multiplies<uint32_t>());
  auto buffer = CreateDOMArrayBufferView(buffer_size, type);
  DCHECK_EQ(buffer->byteLength(), values.size() * sizeof(T));
  memcpy(buffer->BaseAddress(), values.data(), buffer->byteLength());
  return BuildConstant(builder, dimensions, type, exception_state, buffer);
}

template <typename T>
struct Conv2dTester {
  MLGraphTestBase* helper;
  OperandInfo<T> input;
  OperandInfo<T> filter;
  absl::optional<OperandInfo<T>> bias = absl::nullopt;
  Vector<T> expected;

  void Test(V8TestingScope& scope,
            MLGraphBuilder* builder,
            MLConv2dOptions* options = MLConv2dOptions::Create()) {
    // Build the graph.
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* filter_operand =
        BuildConstant(builder, filter.dimensions, filter.type, filter.values,
                      scope.GetExceptionState());
    if (bias) {
      options->setBias(BuildConstant(builder, bias.value().dimensions,
                                     bias.value().type, bias.value().values,
                                     scope.GetExceptionState()));
    }
    auto* output_operand =
        BuildConv2d(scope, builder, input_operand, filter_operand, options);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, Conv2dTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test conv2d operator for nhwc input layout and ohwi filter layout.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    Conv2dTester<float>{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0}},
        .filter = {.type = V8MLOperandType::Enum::kFloat32,
                   .dimensions = {3, 1, 1, 3},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0}},
        .expected = {30.0, 36.0, 42.0, 66.0, 81.0, 96.0, 102.0, 126.0, 150.0,
                     138.0, 171.0, 204.0, 174.0, 216.0, 258.0, 210.0, 261.0,
                     312.0}}
        .Test(scope, builder, options);
  }
  {
    // Test fused conv2d operator for nhwc input layout and ohwi filter layout,
    // fusing with bias operand and relu activation.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    options->setActivation(builder->relu(scope.GetExceptionState()));
    Conv2dTester<float>{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 3},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0}},
        .filter = {.type = V8MLOperandType::Enum::kFloat32,
                   .dimensions = {3, 1, 1, 3},
                   .values = {1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0}},
        .bias = OperandInfo<float>{.type = V8MLOperandType::Enum::kFloat32,
                                   .dimensions = {3},
                                   .values = {-6000.0, -7000.0, 8000.0}},
        .expected = {0.0, 0.0, 8042.0, 0.0, 0.0, 8096.0, 0.0, 0.0, 8150.0, 0.0,
                     0.0, 8204.0, 0.0, 0.0, 8258.0, 0.0, 0.0, 8312.0}}
        .Test(scope, builder, options);
  }
  {
    // Test depthwise conv2d operator by setting groups to input channels,
    // nhwc input layout, ihwo filter layout.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    options->setGroups(4);
    Conv2dTester<float>{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 4},
                  .values = {10.0, 21.0, 10.0, 0.0, 10.0, 22.0, 20.0, 0.0, 10.0,
                             23.0, 30.0, 0.0, 10.0, 24.0, 40.0, 0.0}},
        .filter = {.type = V8MLOperandType::Enum::kFloat32,
                   .dimensions = {1, 2, 2, 4},
                   .values = {0.25, 0.0, 10.0, 50.0, 0.25, 1.0, 20.0, 50.0,
                              0.25, 0.0, 30.0, 50.0, 0.25, 1.0, 40.0, 50.0}},
        .expected = {10.0, 46.0, 3000.0, 0.0}}
        .Test(scope, builder, options);
  }
  {
    // Test fused depthwise conv2d operator by setting groups to input channels,
    // nhwc input layout, ihwo filter layout, fusing with bias operand and relu
    // activation.
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    options->setGroups(4);
    options->setActivation(builder->relu(scope.GetExceptionState()));
    Conv2dTester<float>{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 4},
                  .values = {10.0, 21.0, 10.0, 0.0, 10.0, 22.0, 20.0, 0.0, 10.0,
                             23.0, 30.0, 0.0, 10.0, 24.0, 40.0, 0.0}},
        .filter = {.type = V8MLOperandType::Enum::kFloat32,
                   .dimensions = {1, 2, 2, 4},
                   .values = {0.25, 0.0, 10.0, 50.0, 0.25, 1.0, 20.0, 50.0,
                              0.25, 0.0, 30.0, 50.0, 0.25, 1.0, 40.0, 50.0}},
        .bias =
            OperandInfo<float>{.type = V8MLOperandType::Enum::kFloat32,
                               .dimensions = {4},
                               .values = {-6000.0, -7000.0, 8000.0, 9000.0}},
        .expected = {0.0, 0.0, 11000.0, 9000.0}}
        .Test(scope, builder, options);
  }
  {
    // Test fused depthwise conv2d operator by setting groups to input channels,
    // nhwc input layout, ihwo filter layout, fusing with bias operand and clamp
    // activation.
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
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 4},
                  .values = {10.0, 21.0, 10.0, 0.0, 10.0, 22.0, 20.0, 0.0, 10.0,
                             23.0, 30.0, 0.0, 10.0, 24.0, 40.0, 0.0}},
        .filter = {.type = V8MLOperandType::Enum::kFloat32,
                   .dimensions = {1, 2, 2, 4},
                   .values = {0.25, 0.0, 10.0, 50.0, 0.25, 1.0, 20.0, 50.0,
                              0.25, 0.0, 30.0, 50.0, 0.25, 1.0, 40.0, 50.0}},
        .bias =
            OperandInfo<float>{.type = V8MLOperandType::Enum::kFloat32,
                               .dimensions = {4},
                               .values = {-6000.0, -7000.0, 8000.0, 9000.0}},
        .expected = {0.0, 0.0, 6.0, 6.0}}
        .Test(scope, builder, options);
  }
}

template <typename T>
struct GemmTester {
  MLGraphTestBase* helper;
  OperandInfo<T> a;
  OperandInfo<T> b;
  absl::optional<OperandInfo<T>> c = absl::nullopt;
  Vector<T> expected;

  void Test(V8TestingScope& scope,
            MLGraphBuilder* builder,
            MLGemmOptions* options = MLGemmOptions::Create()) {
    // Build the graph.
    auto* a_operand = BuildInput(builder, "input", a.dimensions, a.type,
                                 scope.GetExceptionState());
    auto* b_operand = BuildConstant(builder, b.dimensions, b.type, b.values,
                                    scope.GetExceptionState());
    if (c) {
      options->setC(BuildConstant(builder, c.value().dimensions, c.value().type,
                                  c.value().values, scope.GetExceptionState()));
    }
    auto* output_operand =
        BuildGemm(scope, builder, a_operand, b_operand, options);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input", CreateArrayBufferViewForOperand(a_operand, a.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
  }
};

TEST_P(MLGraphTest, GemmTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test gemm operator without operand c.
    GemmTester<float>{.a = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {2, 2},
                            .values = {1.0, 2.0, 2.0, 1.0}},
                      .b = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {2, 1},
                            .values = {2.0, 4.0}},
                      .expected = {10.0, 8.0}}
        .Test(scope, builder);
  }
  {
    // Test gemm operator with operand c.
    GemmTester<float>{
        .a = {.type = V8MLOperandType::Enum::kFloat32,
              .dimensions = {2, 2},
              .values = {1.0, 2.0, 2.0, 1.0}},
        .b = {.type = V8MLOperandType::Enum::kFloat32,
              .dimensions = {2, 1},
              .values = {2.0, 4.0}},
        .c = OperandInfo<float>{.type = V8MLOperandType::Enum::kFloat32,
                                .dimensions = {1},
                                .values = {1.0}},
        .expected = {11.0, 9.0}}
        .Test(scope, builder);
  }
  {
    // Test gemm operator with bTranspose = true.
    auto* options = MLGemmOptions::Create();
    options->setBTranspose(true);
    GemmTester<float>{
        .a = {.type = V8MLOperandType::Enum::kFloat32,
              .dimensions = {2, 2},
              .values = {1.0, 2.0, 2.0, 1.0}},
        .b = {.type = V8MLOperandType::Enum::kFloat32,
              .dimensions = {1, 2},
              .values = {2.0, 4.0}},
        .c = OperandInfo<float>{.type = V8MLOperandType::Enum::kFloat32,
                                .dimensions = {1},
                                .values = {1.0}},
        .expected = {11.0, 9.0}}
        .Test(scope, builder, options);
  }
}

struct HardSwishTester {
  MLGraphTestBase* helper;
  OperandInfo<float> input;
  Vector<float> expected;

  void Test(V8TestingScope& scope) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->hardSwish(input_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results.size(), expected.size());
    for (wtf_size_t i = 0; i < expected.size(); ++i) {
      EXPECT_FLOAT_EQ(results[i], expected[i]);
    }
  }
};

TEST_P(MLGraphTest, HardSwishTest) {
  V8TestingScope scope;
  {
    // Test hardSwish operator for 1-D tensor.
    // The expected results should be the result of the nonlinear function, y =
    // x * max(0, min(6, (x + 3))) / 6, applied to the input tensor,
    // element-wise.
    HardSwishTester{.helper = this,
                    .input = {.type = V8MLOperandType::Enum::kFloat32,
                              .dimensions = {2},
                              .values = {-0.6, 0.6}},
                    .expected = {-0.24, 0.36}}
        .Test(scope);
  }
  {
    // Test hardSwish operator for 2-D tensor.
    HardSwishTester{.helper = this,
                    .input = {.type = V8MLOperandType::Enum::kFloat32,
                              .dimensions = {2, 2},
                              .values = {-1.2, -0.6, 0.6, 1.2}},
                    .expected = {-0.36, -0.24, 0.36, 0.84}}
        .Test(scope);
  }
  {
    // Test hardSwish operator for 3-D tensor.
    HardSwishTester{.helper = this,
                    .input = {.type = V8MLOperandType::Enum::kFloat32,
                              .dimensions = {1, 2, 2},
                              .values = {-1.2, -0.6, 0.6, 1.2}},
                    .expected = {-0.36, -0.24, 0.36, 0.84}}
        .Test(scope);
  }
  {
    // Test hardSwish operator for 4-D tensor.
    HardSwishTester{.helper = this,
                    .input = {.type = V8MLOperandType::Enum::kFloat32,
                              .dimensions = {1, 2, 2, 1},
                              .values = {-1.2, -0.6, 0.6, 1.2}},
                    .expected = {-0.36, -0.24, 0.36, 0.84}}
        .Test(scope);
  }
}

template <typename T>
struct Pool2dTester {
  MLGraphTestBase* helper;
  Pool2dKind kind;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(V8TestingScope& scope,
            MLPool2dOptions* options = MLPool2dOptions::Create()) {
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        BuildPool2d(scope, builder, kind, input_operand, options);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphTest, Pool2dTest) {
  V8TestingScope scope;
  {
    // Test averagePool2d operator for nhwc input layout.
    auto* options = MLPool2dOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setWindowDimensions({3, 3});
    Pool2dTester<float>{
        .kind = Pool2dKind::kAverage,
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 4, 4, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0}},
        .expected = {6.0, 7.0, 10.0, 11.0}}
        .Test(scope, options);
  }
  {
    // Test global averagePool2d operator for nhwc input layout.
    auto* options = MLPool2dOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    Pool2dTester<float>{
        .kind = Pool2dKind::kAverage,
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 4, 4, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0}},
        .expected = {8.5}}
        .Test(scope, options);
  }
  {
    // Test maxPool2d operator for nhwc input layout.
    auto* options = MLPool2dOptions::Create();
    options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setWindowDimensions({3, 3});
    Pool2dTester<float>{
        .kind = Pool2dKind::kMax,
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 4, 4, 1},
                  .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
                             11.0, 12.0, 13.0, 14.0, 15.0, 16.0}},
        .expected = {11.0, 12.0, 15.0, 16.0}}
        .Test(scope, options);
  }
}

// Because reshape Node runs copy operator, ReshapeTester just checks the output
// against the input. So there is no need to set expected results.
template <typename T>
struct ReshapeTester {
  MLGraphTestBase* helper;
  OperandInfo<T> input;
  Vector<absl::optional<uint32_t>> new_shape;
  Vector<uint32_t> expected_output_shape;

  void Test(V8TestingScope& scope) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->reshape(input_operand, new_shape, scope.GetExceptionState());
    EXPECT_EQ(output_operand->Dimensions(), expected_output_shape);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(outputs[0].second);
    EXPECT_EQ(results, input.values);
  }
};

TEST_P(MLGraphTest, ReshapeTest) {
  V8TestingScope scope;
  {
    // Test reshaping 2-D tensor to 1-D tensor.
    ReshapeTester<float>{.helper = this,
                         .input = {.type = V8MLOperandType::Enum::kFloat32,
                                   .dimensions = {2, 2},
                                   .values = {-10.0, -0.5, 0.5, 10.0}},
                         .new_shape = {4},
                         .expected_output_shape = {4}}
        .Test(scope);
  }
  {
    // Test reshaping from 2-D tensor to 1-D tensor with calculated dimension.
    ReshapeTester<float>{.helper = this,
                         .input = {.type = V8MLOperandType::Enum::kFloat32,
                                   .dimensions = {2, 2},
                                   .values = {-10.0, -0.5, 0.5, 10.0}},
                         .new_shape = {absl::nullopt},
                         .expected_output_shape = {4}}
        .Test(scope);
  }
  {
    // Test reshaping from 4-D tensor to 2-D tensor.
    ReshapeTester<float>{.helper = this,
                         .input = {.type = V8MLOperandType::Enum::kFloat32,
                                   .dimensions = {1, 2, 2, 1},
                                   .values = {-10.0, -0.5, 0.5, 10.0}},
                         .new_shape = {1, 4},
                         .expected_output_shape = {1, 4}}
        .Test(scope);
  }
  {
    // Test reshaping from 4-D tensor to 2-D tensor with calculated dimension.
    ReshapeTester<float>{.helper = this,
                         .input = {.type = V8MLOperandType::Enum::kFloat32,
                                   .dimensions = {1, 2, 2, 1},
                                   .values = {-10.0, -0.5, 0.5, 10.0}},
                         .new_shape = {1, absl::nullopt},
                         .expected_output_shape = {1, 4}}
        .Test(scope);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MLGraphTest,
    testing::Combine(::testing::Values(BackendType::kXnnpack),
                     ::testing::Values(ExecutionMode::kAsync,
                                       ExecutionMode::kSync)),
    TestVarietyToString);

}  // namespace blink
