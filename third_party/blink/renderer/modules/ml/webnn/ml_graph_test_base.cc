// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

namespace blink {

MLGraph* ToMLGraph(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<MLGraph>::NativeValue(
      scope->GetIsolate(), value.V8Value(), scope->GetExceptionState());
}

std::string TestVarietyToString(
    const ::testing::TestParamInfo<TestVariety>& info) {
  BackendType backend_type = info.param.backend_type;
  std::string name;

  switch (backend_type) {
    case BackendType::kFake:
      // The name of Fake backend from test parameter doesn't output avoid
      // duplicating with the fixture name |FakeMLGraphTest|.
      name += "";
      break;
    case BackendType::kXnnpack:
      name += "Xnnpack_";
      break;
    case BackendType::kModelLoader:
      name += "ModelLoader_";
      break;
    case BackendType::kWebNNService:
      name += "WebNNService_";
      break;
  }

  // TODO: crbug.com/40283536 - Remove this.
  name += "Async";
  return name;
}

BackendType MLGraphTestBase::GetBackendType() {
  return GetParam().backend_type;
}

MLGraphTestBase::BuildResult MLGraphTestBase::BuildGraph(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLNamedOperands& named_operands) {
  ScriptPromiseTester tester(
      scope.GetScriptState(),
      builder->build(scope.GetScriptState(), named_operands,
                     scope.GetExceptionState()));
  tester.WaitUntilSettled();
  if (tester.IsFulfilled()) {
    return BuildResult{.graph = ToMLGraph(&scope, tester.Value()),
                       .exception = nullptr};
  } else {
    return BuildResult{.graph = nullptr,
                       .exception = V8DOMException::ToWrappable(
                           scope.GetIsolate(), tester.Value().V8Value())};
  }
}

MLComputeResult* ToMLComputeResult(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<MLComputeResult>::NativeValue(
      scope->GetIsolate(), value.V8Value(), scope->GetExceptionState());
}

DOMException* MLGraphTestBase::ComputeGraph(V8TestingScope& scope,
                                            MLGraph* graph,
                                            MLNamedArrayBufferViews& inputs,
                                            MLNamedArrayBufferViews& outputs) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  ScriptPromiseTester tester(scope.GetScriptState(), resolver->Promise());
  graph->Compute(ScopedMLTrace("Compute"), inputs, outputs, resolver,
                 scope.GetExceptionState());
  tester.WaitUntilSettled();
  if (tester.IsFulfilled()) {
    // For `MLGraph::Compute()`, the input and output ArrayBufferViews
    // are transferred. The new ArrayBufferViews are returned via the
    // MLComputeResult. Set the inputs and outputs to the returned ones.
    auto* results = ToMLComputeResult(&scope, tester.Value());
    inputs = results->inputs();
    outputs = results->outputs();
    return nullptr;
  } else {
    return V8DOMException::ToWrappable(scope.GetIsolate(),
                                       tester.Value().V8Value());
  }
}

ScriptPromise MLGraphTestBase::CreateContext(V8TestingScope& scope,
                                             MLContextOptions* options) {
  auto* ml = MakeGarbageCollected<ML>(scope.GetExecutionContext());
  return ml->createContext(scope.GetScriptState(), options,
                           scope.GetExceptionState());
}

// static
MLGraphBuilder* MLGraphTestBase::CreateGraphBuilder(V8TestingScope& scope,
                                                    MLContextOptions* options) {
  ScriptPromiseTester tester(scope.GetScriptState(),
                             CreateContext(scope, options));
  tester.WaitUntilSettled();
  CHECK(tester.IsFulfilled());

  auto* context = NativeValueTraits<MLContext>::NativeValue(
      scope.GetIsolate(), tester.Value().V8Value(), scope.GetExceptionState());
  return MLGraphBuilder::Create(context);
}

void ExpectFloatArrayEqual(const Vector<float>& data,
                           const Vector<float>& expected_data) {
  EXPECT_EQ(data.size(), expected_data.size());
  for (wtf_size_t i = 0; i < data.size(); ++i) {
    EXPECT_FLOAT_EQ(data[i], expected_data[i]);
  }
}

}  // namespace blink
