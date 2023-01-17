// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class MLGraphBuilder;
class MLOperand;
class V8TestingScope;

// The utility methods for graph builder test.
MLGraphBuilder* CreateMLGraphBuilder(
    V8TestingScope& scope,
    MLContextOptions* options = MLContextOptions::Create());

MLOperand* BuildInput(V8TestingScope& scope,
                      MLGraphBuilder* builder,
                      const String& name,
                      const Vector<uint32_t>& dimensions,
                      V8MLOperandType::Enum type);

NotShared<DOMArrayBufferView> CreateDOMArrayBufferView(
    size_t size,
    V8MLOperandType::Enum type);

MLOperand* BuildConstant(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const Vector<uint32_t>& dimensions,
    V8MLOperandType::Enum type,
    absl::optional<NotShared<DOMArrayBufferView>> buffer_view = absl::nullopt);

NotShared<DOMArrayBufferView> CreateArrayBufferViewForOperand(
    const MLOperand* operand);

MLOperand* BuildClamp(V8TestingScope& scope,
                      MLGraphBuilder* builder,
                      const MLOperand* input,
                      const MLClampOptions* options = MLClampOptions::Create());

MLOperand* BuildConv2d(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLOperand* filter,
    const MLConv2dOptions* options = MLConv2dOptions::Create());

enum class ElementWiseBinaryKind { kAdd, kSub, kMul, kDiv, kMin, kMax };

MLOperand* BuildElementWiseBinary(V8TestingScope& scope,
                                  MLGraphBuilder* builder,
                                  ElementWiseBinaryKind kind,
                                  const MLOperand* a,
                                  const MLOperand* b);

enum class Pool2dKind { kAverage, kMax };

MLOperand* BuildPool2d(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    Pool2dKind kind,
    const MLOperand* input,
    const MLPool2dOptions* options = MLPool2dOptions::Create());

MLOperand* BuildGemm(V8TestingScope& scope,
                     MLGraphBuilder* builder,
                     const MLOperand* a,
                     const MLOperand* b,
                     const MLGemmOptions* options = MLGemmOptions::Create());

MLOperand* BuildResample2d(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLResample2dOptions* options = MLResample2dOptions::Create());

enum ExecutionMode { kAsync, kSync };

std::string ExecutionModeParamToString(
    const ::testing::TestParamInfo<ExecutionMode>& execution_mode);

class MLGraphTestBase : public ::testing::Test,
                        public ::testing::WithParamInterface<ExecutionMode> {
 public:
  // BuildResult is returned by Build() method. Only one member of BuildResult
  // is valid. If the graph building is successful, graph points to the MLGraph
  // and exception is a nullptr. Otherwise, exception points to the DOMException
  // and graph is a nullptr.
  struct BuildResult {
    Persistent<MLGraph> graph;
    Persistent<DOMException> exception;
  };

  // Helper method for testing both BuildAsyncImpl() and BuildSyncImpl() with
  // the same named operands and expected results.
  BuildResult BuildGraph(V8TestingScope& scope,
                         MLGraphBuilder* builder,
                         const MLNamedOperands& named_operands);

  // Helper method for testinh both ComputeAsync() and ComputeSync() with the
  // same input/output buffers and expected results. If the graph computes
  // successfully, it returns nullptr and the results are produced into the
  // output buffers. Otherwise, it returns the pointer to the DOMException
  // thrown by the graph computing.
  DOMException* ComputeGraph(V8TestingScope& scope,
                             MLGraph* graph,
                             const MLNamedArrayBufferViews& inputs,
                             const MLNamedArrayBufferViews& outputs);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_
