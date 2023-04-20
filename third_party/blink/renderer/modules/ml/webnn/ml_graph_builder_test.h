// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_utils.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class MLGraphBuilder;
class MLOperand;
class V8TestingScope;

// The utility methods for graph builder test.
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

MLOperand* BuildConvTranspose2d(V8TestingScope& scope,
                                MLGraphBuilder* builder,
                                const MLOperand* input,
                                const MLOperand* filter,
                                const MLConvTranspose2dOptions* options =
                                    MLConvTranspose2dOptions::Create());

MLOperand* BuildLeakyRelu(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLLeakyReluOptions* options = MLLeakyReluOptions::Create());

enum class ElementWiseBinaryKind { kAdd, kSub, kMul, kDiv, kMin, kMax };

MLOperand* BuildElementWiseBinary(V8TestingScope& scope,
                                  MLGraphBuilder* builder,
                                  ElementWiseBinaryKind kind,
                                  const MLOperand* a,
                                  const MLOperand* b);

MLOperand* BuildPad(V8TestingScope& scope,
                    MLGraphBuilder* builder,
                    const MLOperand* input,
                    const Vector<uint32_t>& beginningPadding,
                    const Vector<uint32_t>& endingPadding,
                    const MLPadOptions* options = MLPadOptions::Create());

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

MLOperand* BuildTranspose(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLTransposeOptions* options = MLTransposeOptions::Create());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_
