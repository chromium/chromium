// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_

#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_arg_min_max_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_hard_sigmoid_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_instance_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_layer_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_linear_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_softplus_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
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

MLOperand* BuildArgMinMax(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    webnn::mojom::blink::ArgMinMax::Kind kind,
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

MLOperand* BuildLeakyRelu(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLLeakyReluOptions* options = MLLeakyReluOptions::Create());

MLOperand* BuildElementWiseBinary(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    const MLOperand* a,
    const MLOperand* b);

MLOperand* BuildPad(V8TestingScope& scope,
                    MLGraphBuilder* builder,
                    const MLOperand* input,
                    const Vector<uint32_t>& beginningPadding,
                    const Vector<uint32_t>& endingPadding,
                    const MLPadOptions* options = MLPadOptions::Create());

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

MLOperand* BuildReduce(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    webnn::mojom::blink::Reduce::Kind kind,
    const MLOperand* input,
    const MLReduceOptions* options = MLReduceOptions::Create());

MLOperand* BuildSoftplus(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLSoftplusOptions* options = MLSoftplusOptions::Create());
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_TEST_H_
