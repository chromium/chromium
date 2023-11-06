// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

namespace blink {

MLGraphBuilder* CreateMLGraphBuilder(
    ExecutionContext* execution_context,
    ScriptState* script_state,
    ExceptionState& exception_state,
    MLContextOptions* options = MLContextOptions::Create());

MLOperand* BuildInput(MLGraphBuilder* builder,
                      const String& name,
                      const Vector<uint32_t>& dimensions,
                      V8MLOperandType::Enum type,
                      ExceptionState& exception_state);

NotShared<DOMArrayBufferView> CreateDOMArrayBufferView(
    size_t size,
    V8MLOperandType::Enum type);

MLOperand* BuildConstant(
    MLGraphBuilder* builder,
    const Vector<uint32_t>& dimensions,
    V8MLOperandType::Enum type,
    ExceptionState& exception_state,
    absl::optional<NotShared<DOMArrayBufferView>> buffer_view = absl::nullopt);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_UTILS_H_
