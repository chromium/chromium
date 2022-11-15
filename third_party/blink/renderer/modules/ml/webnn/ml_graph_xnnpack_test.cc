// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"

#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class MLGraphXnnpackTest : public testing::Test {
 public:
  MLGraphXnnpackTest() = default;
  ~MLGraphXnnpackTest() override = default;
};

MLGraphXnnpack* ToMLGraphXnnpack(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<MLGraphXnnpack>::NativeValue(
      scope->GetIsolate(), value.V8Value(), scope->GetExceptionState());
}

// Build a simple MLGraph asynchronously with only one relu operator.
ScriptPromise BuildSimpleGraph(
    V8TestingScope& scope,
    MLContextOptions* context_options = MLContextOptions::Create()) {
  auto* builder = CreateMLGraphBuilder(scope, context_options);
  auto* input = BuildInput(scope, builder, "input", {3, 4, 5},
                           V8MLOperandType::Enum::kFloat32);
  auto* output = builder->relu(input, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  return builder->buildAsync(scope.GetScriptState(), {{"output", output}},
                             scope.GetExceptionState());
}

TEST_F(MLGraphXnnpackTest, SharedXnnpackContextTest) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  {
    // Test building MLGraphXnnpack with default options. The promise should be
    // resoveld with an MLGraphXnnpack object. The XNNPACK library should be
    // initialized successfully.
    ScriptPromiseTester tester(script_state, BuildSimpleGraph(scope));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    auto* xnnpack_graph = ToMLGraphXnnpack(&scope, tester.Value());
    EXPECT_NE(xnnpack_graph, nullptr);
  }
  {
    // Test building MLGraphXnnpack with devicePreference = "cpu". The promise
    // should be resoveld with an MLGraphXnnpack object. The XNNPACK library
    // should be initialized successfully.
    auto* context_options = MLContextOptions::Create();
    context_options->setDevicePreference(V8MLDevicePreference::Enum::kCpu);
    ScriptPromiseTester tester(script_state,
                               BuildSimpleGraph(scope, context_options));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    auto* xnnpack_graph = ToMLGraphXnnpack(&scope, tester.Value());
    EXPECT_NE(xnnpack_graph, nullptr);
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

}  // namespace blink
