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
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"
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
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input =
        BuildInput(builder, "input", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
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
    auto* builder =
        CreateMLGraphBuilder(scope.GetExecutionContext(), context_options);
    auto* input =
        BuildInput(builder, "input", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* output = builder->relu(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    auto [graph, exception] = BuildGraph(scope, builder, {{"output", output}});
    EXPECT_NE(graph, nullptr);
  }
}

TEST_F(MLGraphXnnpackTest, TopoSortOperatorsTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  {
    // Test sorting a graph in the following topology:
    //   conv2d
    //     |
    //    add
    //     |
    //   relu
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* conv2d = BuildConv2d(scope, builder, input, filter);
    auto* bias = BuildConstant(builder, {1}, V8MLOperandType::Enum::kFloat32,
                               scope.GetExceptionState());
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
    auto* input =
        BuildInput(builder, "input", {1, 1, 5, 5},
                   V8MLOperandType::Enum::kFloat32, scope.GetExceptionState());
    auto* filter =
        BuildConstant(builder, {1, 1, 3, 3}, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
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
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
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
    auto* input0 =
        BuildInput(builder, "input0", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* input1 =
        BuildInput(builder, "input1", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
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
    auto* input =
        BuildInput(builder, "input", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* constant =
        BuildConstant(builder, shape, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
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
    auto* input =
        BuildInput(builder, "input", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* constant0 =
        BuildConstant(builder, shape, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
    auto* intermediate = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, input, constant0);
    auto* constant1 =
        BuildConstant(builder, shape, V8MLOperandType::Enum::kFloat32,
                      scope.GetExceptionState());
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
    auto* input0 =
        BuildInput(builder, "input0", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* input1 =
        BuildInput(builder, "input1", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* intermediate = BuildElementWiseBinary(
        scope, builder, ElementWiseBinaryKind::kAdd, input0, input1);
    auto* output0 = builder->relu(intermediate, scope.GetExceptionState());
    auto* input2 =
        BuildInput(builder, "input2", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
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

TEST_P(MLGraphXnnpackTest, ElementWiseBinaryTest) {
  V8TestingScope scope;
  TestElementWiseBinary(scope);
}

template <typename T>
struct ReluTester {
  MLGraphXnnpackTest* helper;
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

template <typename T>
struct Resample2dTester {
  MLGraphXnnpackTest* helper;
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

TEST_P(MLGraphXnnpackTest, Resample2dTest) {
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
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  Vector<uint32_t> shape({1, 2, 2, 1});
  // Create an MLGraphXnnpack with the following topology:
  //       [input0] [input1]
  //           \   /
  //            add
  //             |
  //          [output]
  auto* input0 =
      BuildInput(builder, "input0", shape, V8MLOperandType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* input1 =
      BuildInput(builder, "input1", shape, V8MLOperandType::Enum::kFloat32,
                 scope.GetExceptionState());
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
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    results = GetArrayBufferViewValues<float>(outputs[0].second);
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
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
    SetArrayBufferViewValues<float>(inputs[0].second, {2.0, 3.0, 4.0, 5.0});
    SetArrayBufferViewValues<float>(inputs[1].second, {6.0, 7.0, 8.0, 9.0});
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    results = GetArrayBufferViewValues<float>(outputs[0].second);
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
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
    auto new_input0_buffer =
        CreateArrayBufferViewForOperand<float>(input0, {2.0, 3.0, 4.0, 5.0});
    auto new_input1_buffer =
        CreateArrayBufferViewForOperand<float>(input1, {6.0, 7.0, 8.0, 9.0});
    inputs = {{"input0", new_input0_buffer}, {"input1", new_input1_buffer}};
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    results = GetArrayBufferViewValues<float>(outputs[0].second);
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
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
    auto new_output_buffer = CreateArrayBufferViewForOperand(output);
    outputs = {{"output", new_output_buffer}};
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    CheckExternalValues(xnnpack_graph, inputs, outputs);
    results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
  }
}

// Test invoking XNNPACK Runtimbe object when input and output use the same
// name.
TEST_P(MLGraphXnnpackTest, InputAndOutputUseSameNameTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  Vector<uint32_t> shape({1, 2, 2, 1});
  {
    // Create an MLGraphXnnpack with the following topology:
    //            [x]
    //             |
    //            relu
    //             |
    //            [x]
    auto* input =
        BuildInput(builder, "x", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
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
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
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
        BuildInput(builder, "x", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
    auto* input1 =
        BuildInput(builder, "y", shape, V8MLOperandType::Enum::kFloat32,
                   scope.GetExceptionState());
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
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);
    EXPECT_EQ(results, Vector<float>({6.0, 8.0, 10.0, 12.0}));
  }
}

TEST_F(MLGraphXnnpackTest, ComputeAsyncTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
  // Build an MLGraphXnnpack with the following topology:
  //        [a]     [b]
  //           \    /
  //            add
  //             |
  //          [output]
  auto* a_operand =
      BuildInput(builder, "a", {1, 2, 2, 1}, V8MLOperandType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* b_operand =
      BuildInput(builder, "b", {1, 2, 2, 1}, V8MLOperandType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* output_operand =
      builder->add(a_operand, b_operand, scope.GetExceptionState());
  ScriptPromiseTester graph_build_tester(
      scope.GetScriptState(),
      builder->build(scope.GetScriptState(), {{"output", output_operand}},
                     scope.GetExceptionState()));
  graph_build_tester.WaitUntilSettled();
  ASSERT_EQ(graph_build_tester.IsFulfilled(), true);
  auto* graph = NativeValueTraits<MLGraph>::NativeValue(
      scope.GetIsolate(), graph_build_tester.Value().V8Value(),
      scope.GetExceptionState());
  ASSERT_NE(graph, nullptr);
  {
    // Test throwing exception if the first input ArrayBufferView is detached.
    auto a_buffer_view = CreateArrayBufferViewForOperand(a_operand);
    a_buffer_view->DetachForTesting();
    ASSERT_EQ(a_buffer_view->IsDetached(), true);
    auto b_buffer_view = CreateArrayBufferViewForOperand(b_operand);
    auto output_buffer_view = CreateArrayBufferViewForOperand(output_operand);
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
    ScriptPromiseTester tester(scope.GetScriptState(), resolver->Promise());
    graph->ComputeAsync({{"a", a_buffer_view}, {"b", b_buffer_view}},
                        {{"output", output_buffer_view}}, resolver,
                        scope.GetExceptionState());
    tester.WaitUntilSettled();
    EXPECT_NE(tester.IsFulfilled(), true);
    auto* exception = V8DOMException::ToImplWithTypeCheck(
        scope.GetIsolate(), tester.Value().V8Value());
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(
        exception->message(),
        "Invalid inputs: The array buffer view with name \"a\" is detached.");
    // Other ArrayBufferViews should not be detached.
    EXPECT_EQ(b_buffer_view->IsDetached(), false);
    EXPECT_EQ(output_buffer_view->IsDetached(), false);
  }
  {
    // Test throwing exception if the second input ArrayBufferView is detached.
    auto a_buffer_view = CreateArrayBufferViewForOperand(a_operand);
    auto b_buffer_view = CreateArrayBufferViewForOperand(b_operand);
    b_buffer_view->DetachForTesting();
    ASSERT_EQ(b_buffer_view->IsDetached(), true);
    auto output_buffer_view = CreateArrayBufferViewForOperand(output_operand);
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
    ScriptPromiseTester tester(scope.GetScriptState(), resolver->Promise());
    graph->ComputeAsync({{"a", a_buffer_view}, {"b", b_buffer_view}},
                        {{"output", output_buffer_view}}, resolver,
                        scope.GetExceptionState());
    tester.WaitUntilSettled();
    EXPECT_NE(tester.IsFulfilled(), true);
    auto* exception = V8DOMException::ToImplWithTypeCheck(
        scope.GetIsolate(), tester.Value().V8Value());
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(
        exception->message(),
        "Invalid inputs: The array buffer view with name \"b\" is detached.");
    // Other ArrayBufferViews should not be detached.
    EXPECT_EQ(a_buffer_view->IsDetached(), false);
    EXPECT_EQ(output_buffer_view->IsDetached(), false);
  }
  {
    // Test throwing exception if the output ArrayBufferView is detached.
    auto a_buffer_view = CreateArrayBufferViewForOperand(a_operand);
    auto b_buffer_view = CreateArrayBufferViewForOperand(b_operand);
    auto output_buffer_view = CreateArrayBufferViewForOperand(output_operand);
    output_buffer_view->DetachForTesting();
    ASSERT_EQ(output_buffer_view->IsDetached(), true);
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
    ScriptPromiseTester tester(scope.GetScriptState(), resolver->Promise());
    graph->ComputeAsync({{"a", a_buffer_view}, {"b", b_buffer_view}},
                        {{"output", output_buffer_view}}, resolver,
                        scope.GetExceptionState());
    tester.WaitUntilSettled();
    EXPECT_NE(tester.IsFulfilled(), true);
    auto* exception = V8DOMException::ToImplWithTypeCheck(
        scope.GetIsolate(), tester.Value().V8Value());
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
    EXPECT_EQ(exception->message(),
              "Invalid outputs: The array buffer view with name \"output\" is "
              "detached.");
    // Other ArrayBufferViews should not be detached.
    EXPECT_EQ(a_buffer_view->IsDetached(), false);
    EXPECT_EQ(b_buffer_view->IsDetached(), false);
  }
  {
    // Test the input and output ArrayBufferViews are detached if
    // ComputeAsync() call succeeds.
    auto a_buffer_view = CreateArrayBufferViewForOperand(a_operand);
    auto b_buffer_view = CreateArrayBufferViewForOperand(b_operand);
    auto output_buffer_view = CreateArrayBufferViewForOperand(output_operand);
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
    ScriptPromiseTester tester(scope.GetScriptState(), resolver->Promise());
    graph->ComputeAsync({{"a", a_buffer_view}, {"b", b_buffer_view}},
                        {{"output", output_buffer_view}}, resolver,
                        scope.GetExceptionState());
    EXPECT_EQ(a_buffer_view->IsDetached(), true);
    EXPECT_EQ(b_buffer_view->IsDetached(), true);
    EXPECT_EQ(output_buffer_view->IsDetached(), true);
    tester.WaitUntilSettled();
    EXPECT_EQ(tester.IsFulfilled(), true);
  }
  {
    // Test the input and output ArrayBufferViews of MLComputeResult have the
    // same type, byte offset, byte length and base address of those passed to
    // ComputeAsync().
    auto a_buffer_view = CreateArrayBufferViewForOperand(a_operand);
    auto a_buffer_view_type = a_buffer_view->GetType();
    size_t a_buffer_view_byte_offset = a_buffer_view->byteOffset();
    size_t a_buffer_view_byte_length = a_buffer_view->byteLength();
    void* a_buffer_view_base_address = a_buffer_view->BaseAddress();
    auto b_buffer_view = CreateArrayBufferViewForOperand(b_operand);
    auto b_buffer_view_type = b_buffer_view->GetType();
    size_t b_buffer_view_byte_offset = b_buffer_view->byteOffset();
    size_t b_buffer_view_byte_length = b_buffer_view->byteLength();
    void* b_buffer_view_base_address = b_buffer_view->BaseAddress();
    // Create a double-sized ArrayBuffer for output operand.
    auto* array_buffer = DOMArrayBuffer::Create(
        output_operand->NumberOfElements() * 2, sizeof(float));
    // Create an ArrayBufferView that views the second half of the output
    // ArrayBuffer.
    const size_t expected_byte_offset =
        output_operand->NumberOfElements() * sizeof(float);
    auto output_buffer_view = NotShared<DOMArrayBufferView>(
        blink::DOMFloat32Array::Create(array_buffer, expected_byte_offset,
                                       output_operand->NumberOfElements()));
    auto output_buffer_view_type = output_buffer_view->GetType();
    size_t output_buffer_view_byte_offset = output_buffer_view->byteOffset();
    ASSERT_EQ(output_buffer_view_byte_offset, expected_byte_offset);
    size_t output_buffer_view_byte_length = output_buffer_view->byteLength();
    void* output_buffer_view_base_address = output_buffer_view->BaseAddress();
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
    ScriptPromiseTester tester(scope.GetScriptState(), resolver->Promise());
    graph->ComputeAsync({{"a", a_buffer_view}, {"b", b_buffer_view}},
                        {{"output", output_buffer_view}}, resolver,
                        scope.GetExceptionState());
    tester.WaitUntilSettled();
    EXPECT_EQ(tester.IsFulfilled(), true);
    auto* compute_result = NativeValueTraits<MLComputeResult>::NativeValue(
        scope.GetIsolate(), tester.Value().V8Value(),
        scope.GetExceptionState());
    EXPECT_EQ(compute_result->inputs().size(), 2u);
    EXPECT_EQ(compute_result->inputs()[0].first, "a");
    auto result_a_buffer_view = compute_result->inputs()[0].second;
    EXPECT_EQ(result_a_buffer_view->GetType(), a_buffer_view_type);
    EXPECT_EQ(result_a_buffer_view->byteOffset(), a_buffer_view_byte_offset);
    EXPECT_EQ(result_a_buffer_view->byteLength(), a_buffer_view_byte_length);
    EXPECT_EQ(result_a_buffer_view->BaseAddress(), a_buffer_view_base_address);
    EXPECT_EQ(compute_result->inputs()[1].first, "b");
    auto result_b_buffer_view = compute_result->inputs()[1].second;
    EXPECT_EQ(result_b_buffer_view->GetType(), b_buffer_view_type);
    EXPECT_EQ(result_b_buffer_view->byteOffset(), b_buffer_view_byte_offset);
    EXPECT_EQ(result_b_buffer_view->byteLength(), b_buffer_view_byte_length);
    EXPECT_EQ(result_b_buffer_view->BaseAddress(), b_buffer_view_base_address);
    EXPECT_EQ(compute_result->outputs()[0].first, "output");
    auto result_output_buffer_view = compute_result->outputs()[0].second;
    EXPECT_EQ(result_output_buffer_view->GetType(), output_buffer_view_type);
    EXPECT_EQ(result_output_buffer_view->byteOffset(),
              output_buffer_view_byte_offset);
    EXPECT_EQ(result_output_buffer_view->byteLength(),
              output_buffer_view_byte_length);
    EXPECT_EQ(result_output_buffer_view->BaseAddress(),
              output_buffer_view_base_address);
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
  MLGraphXnnpackTest* helper;
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

TEST_P(MLGraphXnnpackTest, Conv2dTest) {
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
  MLGraphXnnpackTest* helper;
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

TEST_P(MLGraphXnnpackTest, GemmTest) {
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
  MLGraphXnnpackTest* helper;
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

TEST_P(MLGraphXnnpackTest, HardSwishTest) {
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
  MLGraphXnnpackTest* helper;
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

// Because XNNPACK reshape Node runs copy operator, ReshapeTester just checks
// the output against the input. So there is no need to set expected results.
template <typename T>
struct ReshapeTester {
  MLGraphXnnpackTest* helper;
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

TEST_P(MLGraphXnnpackTest, ReshapeTest) {
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

// The outputs of softmax function,
// https://en.wikipedia.org/wiki/Softmax_function, are floating-point numbers
// with mantissa. The WPT WebNN conformance test cases of softmax operator,
// https://github.com/web-platform-tests/wpt/blob/master/webnn/resources/test_data/softmax.json,
// will test the accuracy loss of the results against the expected values with
// the WG-agreed tolerance setting.
//
// For MLGraphXnnpack unit testing, SoftmaxTester instead checks the compute
// results of an MLGraph containing a softmax MLOperator against the results of
// calling XNNPACK softmax operator API for the same input. With that, the
// expected values are not needed.
struct SoftmaxTester {
  MLGraphXnnpackTest* helper;
  OperandInfo<float> input;

  void Test(V8TestingScope& scope) {
    // Create and run XNNPACK softmax operator.
    ASSERT_EQ(xnn_status_success, xnn_initialize(/*allocator=*/nullptr));

    DCHECK_EQ(input.dimensions.size(), 2u);
    const uint32_t batch_size = input.dimensions[0];
    const uint32_t channels = input.dimensions[1];

    xnn_operator_t softmax_op = nullptr;
    const xnn_status status = xnn_create_softmax_nc_f32(
        channels, channels, channels, /*flags=*/0, &softmax_op);
    ASSERT_EQ(xnn_status_success, status);
    ASSERT_NE(nullptr, softmax_op);
    std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)> auto_op(
        softmax_op, xnn_delete_operator);

    // XNNPACK may access beyond array bounds. The caller must allocate at least
    // XNN_EXTRA_BYTES extra bytes after the tensor data passed to XNNPACK.
    Vector<float> xnnpack_input(input.values);
    xnnpack_input.Grow(input.values.size() + XNN_EXTRA_BYTES / sizeof(float));
    Vector<float> xnnpack_output(batch_size * channels +
                                 XNN_EXTRA_BYTES / sizeof(float));
    ASSERT_EQ(
        xnn_status_success,
        xnn_setup_softmax_nc_f32(softmax_op, batch_size, xnnpack_input.data(),
                                 xnnpack_output.data(),
                                 /*threadpool=*/nullptr));

    ASSERT_EQ(xnn_status_success,
              xnn_run_operator(softmax_op, /*threadpool=*/nullptr));
    // Remove the extra bytes of XNNPACK output.
    xnnpack_output.Shrink(batch_size * channels);

    // Build WebNN graph with softmax operator.
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->softmax(input_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute WebNN graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);

    // Compare the results of WebNN graph and XNNPACK operator.
    EXPECT_EQ(results, xnnpack_output);
  }
};

TEST_P(MLGraphXnnpackTest, SoftmaxTest) {
  V8TestingScope scope;
  {
    // Test softmax operator for input operand with [2, 2] dimensions.
    // There is no need to set expected results, because SoftmaxTester will
    // calculate the expected results by calling XNNPACK softmax operator
    // APIs.
    SoftmaxTester{.helper = this,
                  .input = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {2, 2},
                            .values = {-1.0, -0.5, 0.5, 1.0}}}
        .Test(scope);
  }
  {
    // Test softmax operator for input operand with [1, 4] dimensions.
    SoftmaxTester{.helper = this,
                  .input = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {1, 4},
                            .values = {-1.0, -0.5, 0.5, 1.0}}}
        .Test(scope);
  }
}

// The outputs of sigmoid function,
// https://en.wikipedia.org/wiki/Sigmoid_function, are floating-point numbers
// with mantissa follow the formula: y = 1 / (exp(-x) + 1). The WPT WebNN
// conformance test cases of sigmoid operator,
// https://github.com/web-platform-tests/wpt/blob/master/webnn/resources/test_data/sigmoid.json,
// will test the accuracy loss of the results against the expected values with
// the WG-agreed tolerance setting.
//
// For MLGraphXnnpack unit testing, SigmoidTester instead checks the compute
// results of an MLGraph containing a sigmoid MLOperator against the results of
// calling XNNPACK sigmoid operator API for the same input. With that, the
// expected values are not needed.
struct SigmoidTester {
  MLGraphXnnpackTest* helper;
  OperandInfo<float> input;

  void Test(V8TestingScope& scope) {
    // Create and run XNNPACK sigmoid operator.
    ASSERT_EQ(xnn_status_success, xnn_initialize(/*allocator=*/nullptr));

    const uint32_t batch_size = 1;
    uint32_t channels = input.dimensions[0];
    for (uint32_t i = 1; i < input.dimensions.size(); i++) {
      channels *= input.dimensions[i];
    }

    xnn_operator_t sigmoid_op = nullptr;
    const xnn_status status = xnn_create_sigmoid_nc_f32(
        channels, channels, channels, /*flags=*/0, &sigmoid_op);
    ASSERT_EQ(xnn_status_success, status);
    ASSERT_NE(nullptr, sigmoid_op);
    std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)> auto_op(
        sigmoid_op, xnn_delete_operator);

    // XNNPACK may access beyond array bounds. The caller must allocate at least
    // XNN_EXTRA_BYTES extra bytes after the tensor data passed to XNNPACK.
    Vector<float> xnnpack_input(input.values);
    xnnpack_input.Grow(input.values.size() + XNN_EXTRA_BYTES / sizeof(float));
    Vector<float> xnnpack_output(batch_size * channels +
                                 XNN_EXTRA_BYTES / sizeof(float));
    ASSERT_EQ(
        xnn_status_success,
        xnn_setup_sigmoid_nc_f32(sigmoid_op, batch_size, xnnpack_input.data(),
                                 xnnpack_output.data(),
                                 /*threadpool=*/nullptr));

    ASSERT_EQ(xnn_status_success,
              xnn_run_operator(sigmoid_op, /*threadpool=*/nullptr));
    // Remove the extra bytes of XNNPACK output.
    xnnpack_output.Shrink(batch_size * channels);

    // Build WebNN graph with sigmoid operator.
    auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext());
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->sigmoid(input_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper->BuildGraph(scope, builder, {{"output", output_operand}});
    EXPECT_NE(graph, nullptr);

    // Compute WebNN graph.
    MLNamedArrayBufferViews inputs(
        {{"input",
          CreateArrayBufferViewForOperand(input_operand, input.values)}});
    MLNamedArrayBufferViews outputs(
        {{"output", CreateArrayBufferViewForOperand(output_operand)}});
    auto* compute_exception =
        helper->ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<float>(outputs[0].second);

    // Compare the results of WebNN graph and XNNPACK operator.
    EXPECT_EQ(results, xnnpack_output);
  }
};

TEST_P(MLGraphXnnpackTest, SigmoidTest) {
  V8TestingScope scope;
  {
    // Test sigmoid operator for 1-D tensor.
    // There is no need to set expected results, because SigmoidTester will
    // calculate the expected results by calling XNNPACK sigmoid operator
    // APIs.
    SigmoidTester{.helper = this,
                  .input = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {2},
                            .values = {-1.0, 1.0}}}
        .Test(scope);
  }
  {
    // Test sigmoid operator for 2-D tensor.
    SigmoidTester{.helper = this,
                  .input = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {2, 2},
                            .values = {-10.0, -0.5, 0.5, 10.0}}}
        .Test(scope);
  }
  {
    // Test sigmoid operator for 3-D tensor.
    SigmoidTester{.helper = this,
                  .input = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {1, 2, 2},
                            .values = {-10.0, -0.5, 0.5, 10.0}}}
        .Test(scope);
  }
  {
    // Test sigmoid operator for 4-D tensor.
    SigmoidTester{.helper = this,
                  .input = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {1, 2, 2, 1},
                            .values = {-10.0, -0.5, 0.5, 10.0}}}
        .Test(scope);
  }
}

// TODO(crbug.com/1273291): Test the async execution mode once the
// MLGraphXnnpack implements it.
INSTANTIATE_TEST_SUITE_P(All,
                         MLGraphXnnpackTest,
                         ::testing::Values(ExecutionMode::kSync,
                                           ExecutionMode::kAsync),
                         ExecutionModeParamToString);

}  // namespace blink
