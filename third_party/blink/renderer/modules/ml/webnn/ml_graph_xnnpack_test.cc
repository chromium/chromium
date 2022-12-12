// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"

#include "base/system/sys_info.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
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
  const auto OperandTypes = {V8MLOperandType::Enum::kFloat32,
                             V8MLOperandType::Enum::kFloat16};
  {
    // Test defining XNNPACK Values for operands in the following topology:
    //       [input0] [input1]
    //           \   /
    //            add
    //             |
    //          [output]
    for (const auto operand_type : OperandTypes) {
      auto* input0 = BuildInput(scope, builder, "input0", shape, operand_type);
      auto* input1 = BuildInput(scope, builder, "input1", shape, operand_type);
      auto* output = BuildElementWiseBinary(
          scope, builder, ElementWiseBinaryKind::kAdd, input0, input1);
      auto [graph, exception] =
          BuildGraph(scope, builder, {{"output", output}});
      EXPECT_NE(graph, nullptr);
      MLGraphXnnpack* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
      const auto& output_externals =
          xnnpack_graph->GetOutputExternalValueIdMap();
      EXPECT_EQ(output_externals.size(), 1u);
      EXPECT_EQ(output_externals.Contains("output"), true);
      // MLGraphXnnpack defines output external Values first.
      EXPECT_EQ(output_externals.at("output"), 0u);
      const auto& input_externals = xnnpack_graph->GetInputExternalValueIdMap();
      EXPECT_EQ(input_externals.size(), 2u);
      EXPECT_EQ(input_externals.Contains("input0"), true);
      EXPECT_EQ(input_externals.Contains("input1"), true);
      EXPECT_EQ(input_externals.at("input0"), 1u);
      EXPECT_EQ(input_externals.at("input1"), 2u);
    }
  }
  {
    // Test defining XNNPACK Values for the operands in the following topology:
    //       [input] [constant]
    //           \   /
    //            add
    //             |
    //          [output]
    for (const auto operand_type : OperandTypes) {
      auto* input = BuildInput(scope, builder, "input", shape, operand_type);
      auto* constant = BuildConstant(scope, builder, shape, operand_type);
      auto* output = BuildElementWiseBinary(
          scope, builder, ElementWiseBinaryKind::kAdd, input, constant);
      auto [graph, exception] =
          BuildGraph(scope, builder, {{"output", output}});
      EXPECT_NE(graph, nullptr);
      MLGraphXnnpack* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
      const auto& output_externals =
          xnnpack_graph->GetOutputExternalValueIdMap();
      EXPECT_EQ(output_externals.size(), 1u);
      EXPECT_EQ(output_externals.Contains("output"), true);
      // MLGraphXnnpack defines output external Values first.
      EXPECT_EQ(output_externals.at("output"), 0u);
      const auto& input_externals = xnnpack_graph->GetInputExternalValueIdMap();
      EXPECT_EQ(input_externals.size(), 1u);
      EXPECT_EQ(input_externals.Contains("input"), true);
      EXPECT_EQ(input_externals.at("input"), 1u);
    }
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
    for (const auto operand_type : OperandTypes) {
      auto* input = BuildInput(scope, builder, "input", shape, operand_type);
      auto* constant0 = BuildConstant(scope, builder, shape, operand_type);
      auto* intermediate = BuildElementWiseBinary(
          scope, builder, ElementWiseBinaryKind::kAdd, input, constant0);
      auto* constant1 = BuildConstant(scope, builder, shape, operand_type);
      auto* output = BuildElementWiseBinary(
          scope, builder, ElementWiseBinaryKind::kAdd, intermediate, constant1);
      auto [graph, exception] =
          BuildGraph(scope, builder, {{"output", output}});
      EXPECT_NE(graph, nullptr);
      MLGraphXnnpack* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
      const auto& output_externals =
          xnnpack_graph->GetOutputExternalValueIdMap();
      EXPECT_EQ(output_externals.size(), 1u);
      EXPECT_EQ(output_externals.Contains("output"), true);
      // MLGraphXnnpack defines output external Values first, so the external
      // Value's ID of the output operand should start from 0.
      EXPECT_EQ(output_externals.at("output"), 0u);
      const auto& input_externals = xnnpack_graph->GetInputExternalValueIdMap();
      EXPECT_EQ(input_externals.size(), 1u);
      EXPECT_EQ(input_externals.Contains("input"), true);
      EXPECT_EQ(input_externals.at("input"), 1u);
    }
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
    for (const auto operand_type : OperandTypes) {
      auto* input0 = BuildInput(scope, builder, "input0", shape, operand_type);
      auto* input1 = BuildInput(scope, builder, "input1", shape, operand_type);
      auto* intermediate = BuildElementWiseBinary(
          scope, builder, ElementWiseBinaryKind::kAdd, input0, input1);
      auto* output0 = builder->relu(intermediate, scope.GetExceptionState());
      auto* input2 = BuildInput(scope, builder, "input2", shape, operand_type);
      auto* output1 = BuildElementWiseBinary(
          scope, builder, ElementWiseBinaryKind::kAdd, intermediate, input2);
      auto [graph, exception] = BuildGraph(
          scope, builder, {{"output0", output0}, {"output1", output1}});
      EXPECT_NE(graph, nullptr);
      MLGraphXnnpack* xnnpack_graph = static_cast<MLGraphXnnpack*>(graph.Get());
      const auto& output_externals =
          xnnpack_graph->GetOutputExternalValueIdMap();
      EXPECT_EQ(output_externals.size(), 2u);
      EXPECT_EQ(output_externals.Contains("output0"), true);
      EXPECT_EQ(output_externals.Contains("output1"), true);
      // MLGraphXnnpack defines output external Values first, so the external
      // Value's ID of the output operand should start from 0.
      EXPECT_EQ(output_externals.at("output0"), 0u);
      EXPECT_EQ(output_externals.at("output1"), 1u);
      const auto& input_externals = xnnpack_graph->GetInputExternalValueIdMap();
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
}

INSTANTIATE_TEST_SUITE_P(All,
                         MLGraphXnnpackTest,
                         ::testing::Values(ExecutionMode::kAsync,
                                           ExecutionMode::kSync),
                         ExecutionModeParamToString);

}  // namespace blink
