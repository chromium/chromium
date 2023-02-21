// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

namespace blink {

MLGraph* ToMLGraph(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<MLGraph>::NativeValue(
      scope->GetIsolate(), value.V8Value(), scope->GetExceptionState());
}

std::string ExecutionModeParamToString(
    const ::testing::TestParamInfo<ExecutionMode>& execution_mode) {
  switch (execution_mode.param) {
    case ExecutionMode::kAsync:
      return "Async";
    case ExecutionMode::kSync:
      return "Sync";
  }
}

MLGraphTestBase::BuildResult MLGraphTestBase::BuildGraph(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLNamedOperands& named_operands) {
  switch (GetParam()) {
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
                           .exception = V8DOMException::ToImplWithTypeCheck(
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
  switch (GetParam()) {
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
        return V8DOMException::ToImplWithTypeCheck(scope.GetIsolate(),
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

template <typename T>
struct ElementWiseBinaryTester {
  MLGraphTestBase* helper;
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

void MLGraphTestBase::TestElementWiseBinary(V8TestingScope& scope) {
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

}  // namespace blink
