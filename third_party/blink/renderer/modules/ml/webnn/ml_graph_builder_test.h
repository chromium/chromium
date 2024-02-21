// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_arg_min_max_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_hard_sigmoid_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_instance_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_layer_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_linear_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_softplus_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
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

enum class ArgMinMaxKind { kArgMin, kArgMax };

MLOperand* BuildArgMinMax(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    ArgMinMaxKind kind,
    const MLOperand* input,
    const MLArgMinMaxOptions* options = MLArgMinMaxOptions::Create());

MLOperand* BuildBatchNormalization(V8TestingScope& scope,
                                   MLGraphBuilder* builder,
                                   const MLOperand* input,
                                   const MLOperand* mean,
                                   const MLOperand* variance,
                                   const MLBatchNormalizationOptions* options =
                                       MLBatchNormalizationOptions::Create());

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

MLOperand* BuildGather(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLOperand* indices,
    const MLGatherOptions* options = MLGatherOptions::Create());

MLOperand* BuildLeakyRelu(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLLeakyReluOptions* options = MLLeakyReluOptions::Create());

enum class ElementWiseBinaryKind {
  kAdd,
  kSub,
  kMul,
  kDiv,
  kMin,
  kMax,
  kPow,
  kEqual,
  kGreater,
  kGreaterOrEqual,
  kLesser,
  kLesserOrEqual,
};

MLOperand* BuildElementWiseBinary(V8TestingScope& scope,
                                  MLGraphBuilder* builder,
                                  ElementWiseBinaryKind kind,
                                  const MLOperand* a,
                                  const MLOperand* b);

enum class ElementWiseUnaryKind {
  kAbs,
  kCeil,
  kCos,
  kExp,
  kFloor,
  kLog,
  kNeg,
  kSin,
  kTan,
  kErf,
  kIdentity,
  kLogicalNot,
  kReciprocal,
  kSqrt,
};

MLOperand* BuildPad(V8TestingScope& scope,
                    MLGraphBuilder* builder,
                    const MLOperand* input,
                    const Vector<uint32_t>& beginningPadding,
                    const Vector<uint32_t>& endingPadding,
                    const MLPadOptions* options = MLPadOptions::Create());

enum class Pool2dKind { kAverage, kL2, kMax };

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

MLOperand* BuildHardSigmoid(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLHardSigmoidOptions* options = MLHardSigmoidOptions::Create());

MLOperand* BuildInstanceNormalization(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLInstanceNormalizationOptions* options =
        MLInstanceNormalizationOptions::Create());

MLOperand* BuildLayerNormalization(V8TestingScope& scope,
                                   MLGraphBuilder* builder,
                                   const MLOperand* input,
                                   const MLLayerNormalizationOptions* options =
                                       MLLayerNormalizationOptions::Create());

enum class ReduceKind {
  kL1,
  kL2,
  kLogSum,
  kLogSumExp,
  kMax,
  kMean,
  kMin,
  kProduct,
  kSum,
  kSumSquare
};

MLOperand* BuildReduce(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    ReduceKind kind,
    const MLOperand* input,
    const MLReduceOptions* options = MLReduceOptions::Create());

MLOperand* BuildResample2d(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLResample2dOptions* options = MLResample2dOptions::Create());

MLOperand* BuildSoftplus(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLSoftplusOptions* options = MLSoftplusOptions::Create());

MLOperand* BuildTranspose(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLTransposeOptions* options = MLTransposeOptions::Create());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_
