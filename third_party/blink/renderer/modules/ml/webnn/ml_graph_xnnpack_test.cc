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

}  // namespace blink
