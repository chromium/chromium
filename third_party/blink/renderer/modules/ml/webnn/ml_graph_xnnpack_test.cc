// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"

#include <numeric>

#include "base/system/sys_info.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_xnnpack.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// Helper class to create MLGraphXnnpack by using the default MLGraphBuilder
// backend.
class MLGraphXnnpackTest : public MLGraphTestBase {};

TEST_P(MLGraphXnnpackTest, SharedXnnpackContextTest) {
  V8TestingScope scope;
  {
    // Test building MLGraphXnnpack with default options. The promise should be
    // resoveld with an MLGraphXnnpack object. The XNNPACK library should be
    // initialized successfully.
    auto* builder = CreateMLGraphBuilder(scope);
    auto* input = BuildInput(scope, builder, "input", {3, 4, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* output = builder->relu(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
    EXPECT_NE(graph, nullptr);
  }
  {
    // Test building MLGraphXnnpack with devicePreference = "cpu". The promise
    // should be resoveld with an MLGraphXnnpack object. The XNNPACK library
    // should be initialized successfully.
    auto* context_options = MLContextOptions::Create();
    context_options->setDevicePreference(V8MLDevicePreference::Enum::kCpu);
    auto* builder = CreateMLGraphBuilder(scope, context_options);
    auto* input = BuildInput(scope, builder, "input", {3, 4, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* output = builder->relu(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
    EXPECT_NE(graph, nullptr);
  }
}

TEST_F(MLGraphXnnpackTest, TopoSortOperatorsTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope);
  {
    // Test sorting a graph in the following topology:
    //   conv2d
    //     |
    //    add
    //     |
    //   relu
    auto* input = BuildInput(scope, builder, "input", {1, 1, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 1, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* conv2d = BuildConv2d(scope, builder, input, filter);
    auto* bias =
        BuildConstant(scope, builder, {1}, V8MLOperandType::Enum::kFloat32);
    auto* add = builder->add(conv2d, bias, scope.GetExceptionState());
    ASSERT_NE(add, nullptr);
    auto* relu = builder->relu(add, scope.GetExceptionState());
    ASSERT_NE(relu, nullptr);

    auto* toposorted_operators =
        MLGraphXnnpack::GetOperatorsInTopologicalOrder({{"output", relu}});
    EXPECT_EQ(toposorted_operators->size(), static_cast<wtf_size_t>(3));
    EXPECT_EQ(toposorted_operators->at(0), conv2d->Operator());
    EXPECT_EQ(toposorted_operators->at(1), add->Operator());
    EXPECT_EQ(toposorted_operators->at(2), relu->Operator());
  }
  {
    // Test sorting a graph in the following topology:
    //      conv2d
    //      /    \
    //  conv2d   conv2d
    //      \   /   \
    //       add    output
    auto* input = BuildInput(scope, builder, "input", {1, 1, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 1, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
    auto* conv2d_0 = BuildConv2d(scope, builder, input, filter, options);
    auto* conv2d_1 = BuildConv2d(scope, builder, conv2d_0, filter, options);
    auto* conv2d_2 = BuildConv2d(scope, builder, conv2d_0, filter, options);
    auto* add = builder->add(conv2d_1, conv2d_2, scope.GetExceptionState());
    auto* toposorted_operators = MLGraphXnnpack::GetOperatorsInTopologicalOrder(
        {{"add", add}, {"output", conv2d_2}});
    EXPECT_EQ(toposorted_operators->size(), static_cast<wtf_size_t>(4));
    EXPECT_EQ(toposorted_operators->at(0), conv2d_0->Operator());
    EXPECT_TRUE((toposorted_operators->at(1) == conv2d_1->Operator() &&
                 toposorted_operators->at(2) == conv2d_2->Operator()) ||
                (toposorted_operators->at(1) == conv2d_2->Operator() &&
                 toposorted_operators->at(2) == conv2d_1->Operator()));
    EXPECT_EQ(toposorted_operators->at(3), add->Operator());
  }
}

TEST_P(MLGraphXnnpackTest, DefineXnnpackValuesTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope);
  Vector<uint32_t> shape({1, 4, 4, 3});
  // TODO(crbug.com/1273291): Test float16 data type once the XNNPACK Subgraph
  // Add Node supports it.
  {
    // Test defining XNNPACK Values for operands in the following topology:
    //       [input0] [input1]
    //           \   /
    //            add
    //             |
    //          [output]
    auto* input0 = BuildInput(scope, builder, "input0", shape,
                              V8MLOperandType::Enum::kFloat32);
    auto* input1 = BuildInput(scope, builder, "input1", shape,
                              V8MLOperandType::Enum::kFloat32);
    auto* output = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, input0, input1);
    auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
    EXPECT_NE(graph, nullptr);
    MLGraphXnnpack* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
    const auto& output_externals =
        xnnpack_graph->GetOutputExternalValueIdMapForTesting();
    EXPECT_EQ(output_externals.size(), 1u);
    EXPECT_EQ(output_externals.Contains("output"), true);
    // MLGraphXnnpack defines output external Values first.
    EXPECT_EQ(output_externals.at("output"), 0u);
    const auto& input_externals =
        xnnpack_graph->GetInputExternalValueIdMapForTesting();
    EXPECT_EQ(input_externals.size(), 2u);
    EXPECT_EQ(input_externals.Contains("input0"), true);
    EXPECT_EQ(input_externals.Contains("input1"), true);
    EXPECT_EQ(input_externals.at("input0"), 1u);
    EXPECT_EQ(input_externals.at("input1"), 2u);
  }
  {
    // Test defining XNNPACK Values for the operands in the following topology:
    //       [input] [constant]
    //           \   /
    //            add
    //             |
    //          [output]
    auto* input = BuildInput(scope, builder, "input", shape,
                             V8MLOperandType::Enum::kFloat32);
    auto* constant =
        BuildConstant(scope, builder, shape, V8MLOperandType::Enum::kFloat32);
    auto* output = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, input, constant);
    auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
    EXPECT_NE(graph, nullptr);
    MLGraphXnnpack* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
    const auto& output_externals =
        xnnpack_graph->GetOutputExternalValueIdMapForTesting();
    EXPECT_EQ(output_externals.size(), 1u);
    EXPECT_EQ(output_externals.Contains("output"), true);
    // MLGraphXnnpack defines output external Values first.
    EXPECT_EQ(output_externals.at("output"), 0u);
    const auto& input_externals =
        xnnpack_graph->GetInputExternalValueIdMapForTesting();
    EXPECT_EQ(input_externals.size(), 1u);
    EXPECT_EQ(input_externals.Contains("input"), true);
    EXPECT_EQ(input_externals.at("input"), 1u);
  }
  {
    // Test defining XNNPACK Values for the operands in the following topology:
    //       [input] [constant0]
    //           \   /
    //            add
    //             |
    //      [intermediate]  [constant1]
    //                  \   /
    //                   add
    //                    |
    //                 [output]
    auto* input = BuildInput(scope, builder, "input", shape,
                             V8MLOperandType::Enum::kFloat32);
    auto* constant0 =
        BuildConstant(scope, builder, shape, V8MLOperandType::Enum::kFloat32);
    auto* intermediate = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, input, constant0);
    auto* constant1 =
        BuildConstant(scope, builder, shape, V8MLOperandType::Enum::kFloat32);
    auto* output = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, intermediate, constant1);
    auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
    EXPECT_NE(graph, nullptr);
    MLGraphXnnpack* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
    const auto& output_externals =
        xnnpack_graph->GetOutputExternalValueIdMapForTesting();
    EXPECT_EQ(output_externals.size(), 1u);
    EXPECT_EQ(output_externals.Contains("output"), true);
    // MLGraphXnnpack defines output external Values first, so the external
    // Value's ID of the output operand should start from 0.
    EXPECT_EQ(output_externals.at("output"), 0u);
    const auto& input_externals =
        xnnpack_graph->GetInputExternalValueIdMapForTesting();
    EXPECT_EQ(input_externals.size(), 1u);
    EXPECT_EQ(input_externals.Contains("input"), true);
    EXPECT_EQ(input_externals.at("input"), 1u);
  }
  {
    // Test defining XNNPACK Values for the operands in the following topology:
    //     [input0] [input1]
    //           \   /
    //            add
    //             |
    //      [intermediate]  [input2]
    //             |     \   /
    //            relu    add
    //             |       |
    //       [output0]   [output1]
    auto* input0 = BuildInput(scope, builder, "input0", shape,
                              V8MLOperandType::Enum::kFloat32);
    auto* input1 = BuildInput(scope, builder, "input1", shape,
                              V8MLOperandType::Enum::kFloat32);
    auto* intermediate = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, input0, input1);
    auto* output0 = builder->relu(intermediate, scope.GetExceptionState());
    auto* input2 = BuildInput(scope, builder, "input2", shape,
                              V8MLOperandType::Enum::kFloat32);
    auto* output1 = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, intermediate, input2);
    auto [graph, exception] = BuildGraph(
        scope, builder, {{"output0", output0}, {"output1", output1}});
    EXPECT_NE(graph, nullptr);
    MLGraphXnnpack* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
    const auto& output_externals =
        xnnpack_graph->GetOutputExternalValueIdMapForTesting();
    EXPECT_EQ(output_externals.size(), 2u);
    EXPECT_EQ(output_externals.Contains("output0"), true);
    EXPECT_EQ(output_externals.Contains("output1"), true);
    // MLGraphXnnpack defines output external Values first, so the external
    // Value's ID of the output operand should start from 0.
    EXPECT_EQ(output_externals.at("output0"), 0u);
    EXPECT_EQ(output_externals.at("output1"), 1u);
    const auto& input_externals =
        xnnpack_graph->GetInputExternalValueIdMapForTesting();
    EXPECT_EQ(input_externals.size(), 3u);
    EXPECT_EQ(input_externals.Contains("input0"), true);
    EXPECT_EQ(input_externals.Contains("input1"), true);
    EXPECT_EQ(input_externals.Contains("input2"), true);
    // MLGraphXnnpack defines input external Values in the topological order
    // of operators, so the Value ID of input2 should be greater than input0
    // and input1.
    EXPECT_EQ(input_externals.at("input0"), 2u);
    EXPECT_EQ(input_externals.at("input1"), 3u);
    EXPECT_EQ(input_externals.at("input2"), 4u);
  }
}

template <typename T>
struct OperandInfo {
  V8MLOperandType::Enum type;
  Vector<uint32_t> dimensions;
  Vector<T> values;
};

// Helper function to set the data of an ArrayBufferView from a vector.
template <typename T>
void SetArrayBufferViewValues(NotShared<DOMArrayBufferView> array_buffer_view,
                              const Vector<T>& values) {
  DCHECK_EQ(array_buffer_view->byteLength(), values.size() * sizeof(T));
  memcpy(array_buffer_view->BaseAddress(), values.data(),
         values.size() * sizeof(T));
}

// Overrode helper function to create an ArrayBufferView given an operand and
// set its data from a vector.
template <typename T>
NotShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand,
    const Vector<T>& values) {
  auto array_buffer_view = CreateArrayBufferViewForOperand(operand);
  SetArrayBufferViewValues(array_buffer_view, values);
  return array_buffer_view;
}

// Helper function to get the data of an ArrayBufferView into a vector.
template <typename T>
Vector<T> GetArrayBufferViewValues(
    NotShared<DOMArrayBufferView> array_buffer_view) {
  Vector<T> values(base::checked_cast<wtf_size_t>(
      array_buffer_view->byteLength() / array_buffer_view->TypeSize()));
  memcpy(values.data(), array_buffer_view->BaseAddress(),
         array_buffer_view->byteLength());
  return values;
}

template <typename T>
struct ElementWiseBinaryTester {
  MLGraphXnnpackTest* helper;
  ElementWiseBinaryKind kind;
  OperandInfo<T> lhs;
  OperandInfo<T> rhs;
  Vector<T> expected;

  void Test(V8TestingScope& scope) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope);
    auto* lhs_operand =
        BuildInput(scope, builder, "lhs", lhs.dimensions, lhs.type);
    auto* rhs_operand =
        BuildInput(scope, builder, "rhs", rhs.dimensions, rhs.type);
    auto* output_operand =
        BuildElementWiseBinary(scope, builder, kind, lhs_operand, rhs_operand);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    auto lhs_buffer = CreateArrayBufferViewForOperand(lhs_operand, lhs.values);
    auto rhs_buffer = CreateArrayBufferViewForOperand(rhs_operand, rhs.values);
    auto output_buffer = CreateArrayBufferViewForOperand(output_operand);
    auto* compute_exception = helper->ComputeGraph(
        scope, graph, {{"lhs", lhs_buffer}, {"rhs", rhs_buffer}},
        {{"output", output_buffer}});
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(output_buffer);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphXnnpackTest, ElementWiseBinaryTest) {
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
  MLGraphXnnpackTest* helper;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(V8TestingScope& scope) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope);
    auto* input_operand =
        BuildInput(scope, builder, "input", input.dimensions, input.type);
    auto* output_operand =
        builder->relu(input_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    auto input_buffer =
        CreateArrayBufferViewForOperand(input_operand, input.values);
    auto output_buffer = CreateArrayBufferViewForOperand(output_operand);
    auto* compute_exception = helper->ComputeGraph(
        scope, graph, {{"input", input_buffer}}, {{"output", output_buffer}});
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(output_buffer);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphXnnpackTest, ReluTest) {
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

void CheckExternalValues(const MLGraphXnnpack* xnnpack_graph,
                         const MLNamedArrayBufferViews& inputs,
                         const MLNamedArrayBufferViews& outputs) {
  const auto& external_values = xnnpack_graph->GetXnnExternalValuesTesting();
  for (const auto& [name, array_buffer_view] : inputs) {
    const auto& input_external_values =
        xnnpack_graph->GetInputExternalValueIdMapForTesting();
    DCHECK(input_external_values.Contains(name));
    const auto& value_id = input_external_values.at(name);
    const auto* it =
        std::find_if(external_values.begin(), external_values.end(),
                     [value_id](const auto& external_value) {
                       return external_value.id == value_id;
                     });
    DCHECK(it);
    EXPECT_EQ(it->data, array_buffer_view->BaseAddress());
  }
  for (const auto& [name, array_buffer_view] : outputs) {
    const auto& output_external_values =
        xnnpack_graph->GetOutputExternalValueIdMapForTesting();
    DCHECK(output_external_values.Contains(name));
    const auto& value_id = output_external_values.at(name);
    const auto* it =
        std::find_if(external_values.begin(), external_values.end(),
                     [value_id](const auto& external_value) {
                       return external_value.id == value_id;
                     });
    DCHECK(it);
    EXPECT_EQ(it->data, array_buffer_view->BaseAddress());
  }
}

TEST_P(MLGraphXnnpackTest, InvokeXnnpackRuntimeTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope);
  Vector<uint32_t> shape({1, 2, 2, 1});
  // Create an MLGraphXnnpack with the following topology:
  //       [input0] [input1]
  //           \   /
  //            add
  //             |
  //          [output]
  auto* input0 = BuildInput(scope, builder, "input0", shape,
                            V8MLOperandType::Enum::kFloat32);
  auto* input1 = BuildInput(scope, builder, "input1", shape,
                            V8MLOperandType::Enum::kFloat32);
  auto* output = BuildElementWiseBinary(
      scope, builder, ElementWiseBinaryKind::kAdd, input0, input1);
  auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
  ASSERT_NE(graph, nullptr);
  auto* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
  {
    // Test invoking XNNPACK Runtimbe object twice with the same buffers.
    auto input0_buffer =
        CreateArrayBufferViewForOperand<float>(input0, {1.0, 2.0, 3.0, 4.0});
    auto input1_buffer =
        CreateArrayBufferViewForOperand<float>(input1, {5.0, 6.0, 7.0, 8.0});
    auto output_buffer = CreateArrayBufferViewForOperand(output);
    MLNamedArrayBufferViews inputs = {{"input0", input0_buffer},
                                      {"input1", input1_buffer}};
    MLNamedArrayBufferViews outputs = {{"output", output_buffer}};
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    auto results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
  }
  {
    // Test invoking XNNPACK Runtimbe object twice with the same buffers, but
    // for each invocation the buffers are set to different values.
    auto input0_buffer =
        CreateArrayBufferViewForOperand<float>(input0, {1.0, 2.0, 3.0, 4.0});
    auto input1_buffer =
        CreateArrayBufferViewForOperand<float>(input1, {5.0, 6.0, 7.0, 8.0});
    auto output_buffer = CreateArrayBufferViewForOperand(output);
    MLNamedArrayBufferViews inputs = {{"input0", input0_buffer},
                                      {"input1", input1_buffer}};
    MLNamedArrayBufferViews outputs = {{"output", output_buffer}};
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    auto results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
    SetArrayBufferViewValues<float>(input0_buffer, {2.0, 3.0, 4.0, 5.0});
    SetArrayBufferViewValues<float>(input1_buffer, {6.0, 7.0, 8.0, 9.0});
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({8.0, 10.0, 12.0, 14.0}));
  }
  {
    // Test invoking XNNPACK Runtimbe object twice with different input buffers.
    auto input0_buffer =
        CreateArrayBufferViewForOperand<float>(input0, {1.0, 2.0, 3.0, 4.0});
    auto input1_buffer =
        CreateArrayBufferViewForOperand<float>(input1, {5.0, 6.0, 7.0, 8.0});
    auto output_buffer = CreateArrayBufferViewForOperand(output);
    MLNamedArrayBufferViews inputs = {{"input0", input0_buffer},
                                      {"input1", input1_buffer}};
    MLNamedArrayBufferViews outputs = {{"output", output_buffer}};
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    auto results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
    auto new_input0_buffer =
        CreateArrayBufferViewForOperand<float>(input0, {2.0, 3.0, 4.0, 5.0});
    auto new_input1_buffer =
        CreateArrayBufferViewForOperand<float>(input1, {6.0, 7.0, 8.0, 9.0});
    inputs = {{"input0", new_input0_buffer}, {"input1", new_input1_buffer}};
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({8.0, 10.0, 12.0, 14.0}));
  }
  {
    // Test invoking XNNPACK Runtimbe object twice with different output buffer.
    auto input0_buffer =
        CreateArrayBufferViewForOperand<float>(input0, {1.0, 2.0, 3.0, 4.0});
    auto input1_buffer =
        CreateArrayBufferViewForOperand<float>(input1, {5.0, 6.0, 7.0, 8.0});
    auto output_buffer = CreateArrayBufferViewForOperand(output);
    MLNamedArrayBufferViews inputs = {{"input0", input0_buffer},
                                      {"input1", input1_buffer}};
    MLNamedArrayBufferViews outputs = {{"output", output_buffer}};
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    auto results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
    auto new_output_buffer = CreateArrayBufferViewForOperand(output);
    outputs = {{"output", new_output_buffer}};
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    results = GetArrayBufferViewValues<float>(new_output_buffer);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
  }
}

// Test invoking XNNPACK Runtimbe object when input and output use the same
// name.
TEST_P(MLGraphXnnpackTest, InputAndOutputUseSameNameTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope);
  Vector<uint32_t> shape({1, 2, 2, 1});
  {
    // Create an MLGraphXnnpack with the following topology:
    //            [x]
    //             |
    //            relu
    //             |
    //            [x]
    auto* input =
        BuildInput(scope, builder, "x", shape, V8MLOperandType::Enum::kFloat32);
    auto* output = builder->relu(input, scope.GetExceptionState());
    auto [graph, exception] = BuildGraph(scope, builder, {{"x", output}});
    ASSERT_NE(graph, nullptr);
    auto* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
    auto input_buffer =
        CreateArrayBufferViewForOperand<float>(input, {-10.0, -0.5, 0.5, 10.0});
    auto output_buffer = CreateArrayBufferViewForOperand(output);
    MLNamedArrayBufferViews inputs = {{"x", input_buffer}};
    MLNamedArrayBufferViews outputs = {{"x", output_buffer}};
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    auto results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({0.0, 0.0, 0.5, 10.0}));
  }
  {
    // Create an MLGraphXnnpack with the following topology:
    //         [x]   [y]
    //           \   /
    //            add
    //             |
    //            [y]
    auto* input0 =
        BuildInput(scope, builder, "x", shape, V8MLOperandType::Enum::kFloat32);
    auto* input1 =
        BuildInput(scope, builder, "y", shape, V8MLOperandType::Enum::kFloat32);
    auto* output = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, input0, input1);
    auto [graph, exception] = BuildGraph(scope, builder, {{"y", output}});
    ASSERT_NE(graph, nullptr);
    auto* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
    auto input0_buffer =
        CreateArrayBufferViewForOperand<float>(input0, {1.0, 2.0, 3.0, 4.0});
    auto input1_buffer =
        CreateArrayBufferViewForOperand<float>(input1, {5.0, 6.0, 7.0, 8.0});
    auto output_buffer = CreateArrayBufferViewForOperand(output);
    MLNamedArrayBufferViews inputs = {{"x", input0_buffer},
                                      {"y", input1_buffer}};
    MLNamedArrayBufferViews outputs = {{"y", output_buffer}};
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    auto results = GetArrayBufferViewValues<float>(output_buffer);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
  }
}

template <typename T>
struct ClampTester {
  MLGraphXnnpackTest* helper;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(V8TestingScope& scope,
            MLClampOptions* options = MLClampOptions::Create()) {
    // Build the graph.
    auto* builder = CreateMLGraphBuilder(scope);
    auto* input_operand =
        BuildInput(scope, builder, "input", input.dimensions, input.type);
    auto* output_operand =
        builder->clamp(input_operand, options, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    auto input_buffer =
        CreateArrayBufferViewForOperand(input_operand, input.values);
    auto output_buffer = CreateArrayBufferViewForOperand(output_operand);
    auto* compute_exception = helper->ComputeGraph(
        scope, graph, {{"input", input_buffer}}, {{"output", output_buffer}});
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<T>(output_buffer);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphXnnpackTest, ClampTest) {
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
MLOperand* BuildConstant(V8TestingScope& scope,
                         MLGraphBuilder* builder,
                         const Vector<uint32_t>& dimensions,
                         V8MLOperandType::Enum type,
                         const Vector<T>& values) {
  size_t buffer_size = std::accumulate(dimensions.begin(), dimensions.end(),
                                       size_t(1), std::multiplies<uint32_t>());
  auto buffer = CreateDOMArrayBufferView(buffer_size, type);
  DCHECK_EQ(buffer->byteLength(), values.size() * sizeof(T));
  memcpy(buffer->BaseAddress(), values.data(), buffer->byteLength());
  return BuildConstant(scope, builder, dimensions, type, buffer);
}

template <typename T>
struct Conv2dTester {
  MLGraphXnnpackTest* helper;
  OperandInfo<T> input;
  OperandInfo<T> filter;
  absl::optional<OperandInfo<T>> bias = absl::nullopt;
  Vector<T> expected;

  void Test(V8TestingScope& scope,
            MLGraphBuilder* builder,
            MLConv2dOptions* options = MLConv2dOptions::Create()) {
    // Build the graph.
    auto* input_operand =
        BuildInput(scope, builder, "input", input.dimensions, input.type);
    auto* filter_operand = BuildConstant(scope, builder, filter.dimensions,
                                         filter.type, filter.values);
    if (bias) {
      options->setBias(BuildConstant(scope, builder, bias.value().dimensions,
                                     bias.value().type, bias.value().values));
    }
    auto* output_operand =
        BuildConv2d(scope, builder, input_operand, filter_operand, options);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute the graph.
    auto input_buffer =
        CreateArrayBufferViewForOperand(input_operand, input.values);
    auto output_buffer = CreateArrayBufferViewForOperand(output_operand);
    auto* compute_exception = helper->ComputeGraph(
        scope, graph, {{"input", input_buffer}}, {{"output", output_buffer}});
    EXPECT_EQ(compute_exception, nullptr);
    Vector<float> results = GetArrayBufferViewValues<T>(output_buffer);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphXnnpackTest, Conv2dTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope);
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
struct Pool2dTester {
  MLGraphXnnpackTest* helper;
  Pool2dKind kind;
  OperandInfo<T> input;
  Vector<T> expected;

  void Test(V8TestingScope& scope,
            MLPool2dOptions* options = MLPool2dOptions::Create()) {
    auto* builder = CreateMLGraphBuilder(scope);
    auto* input_operand =
        BuildInput(scope, builder, "input", input.dimensions, input.type);
    auto* output_operand =
        BuildPool2d(scope, builder, kind, input_operand, options);
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    auto input_buffer =
        CreateArrayBufferViewForOperand(input_operand, input.values);
    auto output_buffer = CreateArrayBufferViewForOperand(output_operand);
    auto* compute_exception = helper->ComputeGraph(
        scope, graph, {{"input", input_buffer}}, {{"output", output_buffer}});
    EXPECT_EQ(compute_exception, nullptr);
    Vector<float> results = GetArrayBufferViewValues<T>(output_buffer);
    EXPECT_EQ(results, expected);
  }
};

TEST_P(MLGraphXnnpackTest, Pool2dTest) {
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

// TODO(crbug.com/1273291): Test the async execution mode once the
// MLGraphXnnpack implements it.
INSTANTIATE_TEST_SUITE_P(All,
                         MLGraphXnnpackTest,
                         ::testing::Values(ExecutionMode::kSync),
                         ExecutionModeParamToString);

}  // namespace blink
