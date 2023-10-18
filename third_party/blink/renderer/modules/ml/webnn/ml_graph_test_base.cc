// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
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
  BackendType backend_type = std::get<0>(info.param);
  ExecutionMode execution_mode = std::get<1>(info.param);
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

  switch (execution_mode) {
    case ExecutionMode::kAsync:
      name += "Async";
      break;
    case ExecutionMode::kSync:
      name += "Sync";
      break;
  }
  return name;
}

ExecutionMode MLGraphTestBase::GetExecutionMode() {
  return std::get<1>(GetParam());
}

MLGraphTestBase::BuildResult MLGraphTestBase::BuildGraph(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLNamedOperands& named_operands) {
  switch (GetExecutionMode()) {
    case ExecutionMode::kAsync: {
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
    case ExecutionMode::kSync: {
      auto* graph =
          builder->buildSync(named_operands, scope.GetExceptionState());
      if (graph) {
        return BuildResult{.graph = static_cast<MLGraph*>(graph),
                           .exception = nullptr};
      } else {
        return BuildResult{
            .graph = nullptr,
            .exception = MakeGarbageCollected<DOMException>(
                scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
                scope.GetExceptionState().Message())};
      }
    }
    default:
      NOTREACHED();
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
  switch (GetExecutionMode()) {
    case ExecutionMode::kAsync: {
      auto* resolver =
          MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
      ScriptPromiseTester tester(scope.GetScriptState(), resolver->Promise());
      graph->ComputeAsync(inputs, outputs, resolver, scope.GetExceptionState());
      tester.WaitUntilSettled();
      if (tester.IsFulfilled()) {
        // For `MLGraph::ComputeAsync()`, the input and output ArrayBufferViews
        // are transferred. The new ArrayBufferViews are returned via the
        // MLComputeResult. Set the inputs and outputs to the returned ones, so
        // the user code could check the outputs in the same way as for
        // `MLGraph::ComputeSync()`.
        auto* results = ToMLComputeResult(&scope, tester.Value());
        inputs = results->inputs();
        outputs = results->outputs();
        return nullptr;
      } else {
        return V8DOMException::ToWrappable(scope.GetIsolate(),
                                           tester.Value().V8Value());
      }
    }
    case ExecutionMode::kSync: {
      graph->ComputeSync(inputs, outputs, scope.GetExceptionState());
      if (scope.GetExceptionState().HadException()) {
        return MakeGarbageCollected<DOMException>(
            scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            scope.GetExceptionState().Message());
      } else {
        return nullptr;
      }
    }
    default:
      NOTREACHED();
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

}  // namespace blink
