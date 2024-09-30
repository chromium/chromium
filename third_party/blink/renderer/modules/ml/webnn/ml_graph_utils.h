// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_

#include <utility>
#include <vector>

#include "base/types/expected.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_filter_operand_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_input_operand_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class MLOperator;
class ScriptState;

// Return the operators in topological order by searching from the named
// output operands. It ensures operator 'j' appears before operator 'i' in the
// result, if 'i' depends on 'j'.
MODULES_EXPORT HeapVector<Member<const MLOperator>>*
GetOperatorsInTopologicalOrder(const MLNamedOperands& named_outputs);

// `TransferNamedArrayBufferViews()` and `CreateNamedArrayBufferViews()`
// implement the MLNamedArrayBufferViews transfer algorithm of WebNN spec:
// https://www.w3.org/TR/webnn/#mlnamedarraybufferviews-transfer
//
// The `NamedArrayBufferViewsInfo` returned by `TransferNamedArrayBufferViews()`
// doesn't contain any GC objects, so it is safe to be posted to a background
// thread. After that, the `NamedArrayBufferViewsInfo` should be posted back to
// the calling thread and call `CreateNamedArrayBufferViews()` to create
// `MLNamedArrayBufferViews` from the info.
//
// If it fails to transfer an `ArrayBufferView` of the
// `MLNamedArrayBufferViews`, the current implementation leaves the
// already-transferred views detached, the failing one and remaining others
// unchanged.
//
// TODO(crbug.com/1273291): Revisit the error handling once the WebNN spec issue
// is resolved: https://github.com/webmachinelearning/webnn/issues/351
//
// TODO(crbug.com/332782852): Accepts a `ScriptPromiseResolver` rather than
// `ExceptionState`. So the caller, i.e. `MLGraph::Compute()`, won't need to
// pass both ways of throwing an exception.
MODULES_EXPORT std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
TransferNamedArrayBufferViews(v8::Isolate* isolate,
                              const MLNamedArrayBufferViews& source_views,
                              ExceptionState& exception_state);

MODULES_EXPORT DOMArrayBufferView* CreateArrayBufferView(
    ArrayBufferViewInfo view_info);

MODULES_EXPORT MLNamedArrayBufferViews* CreateNamedArrayBufferViews(
    std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>> views_info);

MODULES_EXPORT DOMArrayBufferView::ViewType GetArrayBufferViewType(
    webnn::OperandDataType data_type);

// Create a default permutation vector [rank - 1, ..., 0].
Vector<uint32_t> CreateDefaultPermutation(const wtf_size_t rank);

// Create a axes vector [0, ..., rank - 1].
Vector<uint32_t> CreateAllAxes(const wtf_size_t rank);

// Create a default axes vector [1, ... , rank - 1] when rank > 1 and an empty
// vector when rank <= 1 for layer normalization specified in
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-layernorm.
Vector<uint32_t> CreateLayerNormalizationDefaultAxes(const wtf_size_t rank);

// Helper to validate filer layout for Nhwc input layout.
base::expected<void, String> ValidateFilterLayout(
    bool depthwise,
    V8MLInputOperandLayout input_layout,
    V8MLConv2dFilterOperandLayout filter_layout);

// Helper to get output sizes for convolution transpose 2d Node.
webnn::Size2d<uint32_t> CalculateConvTransposeOutputSize2D(
    const blink::MLConvTranspose2dOptions* options,
    uint32_t input_height,
    uint32_t input_width,
    uint32_t filter_height,
    uint32_t filter_width,
    uint32_t stride_height,
    uint32_t stride_width,
    uint32_t dilation_height,
    uint32_t dilation_width,
    uint32_t output_padding_height,
    uint32_t output_padding_width);

V8MLOperandDataType ToBlinkDataType(webnn::OperandDataType data_type);
webnn::OperandDataType FromBlinkDataType(V8MLOperandDataType::Enum data_type);

MODULES_EXPORT bool IsLogicalBinaryOperator(
    webnn::mojom::blink::ElementWiseBinary::Kind kind);

// Allows a tensor's shape to be specified through either the
// `MLOperandDescriptor`'s `shape` or `dimensions` fields. This code exists for
// now to give callers the opportunity to migrate their code to use `shape`.
//
// TODO(crbug.com/365813262): Remove this function after about a milestone.
MODULES_EXPORT base::expected<Vector<uint32_t>, std::string>
GetShapeFromDescriptor(ScriptState* script_state,
                       const MLOperandDescriptor& desc);

MODULES_EXPORT void LogConsoleWarning(
    ScriptState* script_state,
    const String& message,
    mojom::blink::ConsoleMessageSource message_source =
        mojom::blink::ConsoleMessageSource::kJavaScript);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_
