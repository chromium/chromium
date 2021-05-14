// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/offscreencanvas/offscreen_canvas_module.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_context_creation_attributes_module.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"

namespace blink {

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
V8OffscreenRenderingContext* OffscreenCanvasModule::getContext(
    ExecutionContext* execution_context,
    OffscreenCanvas& offscreen_canvas,
    const String& context_id,
    const CanvasContextCreationAttributesModule* attributes,
    ExceptionState& exception_state) {
  if (offscreen_canvas.IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "OffscreenCanvas object is detached");
    return nullptr;
  }
  CanvasContextCreationAttributesCore canvas_context_creation_attributes;
  if (!ToCanvasContextCreationAttributes(
          attributes, canvas_context_creation_attributes, exception_state)) {
    return nullptr;
  }

  // OffscreenCanvas cannot be transferred after getContext, so this execution
  // context will always be the right one from here on.
  CanvasRenderingContext* context = offscreen_canvas.GetCanvasRenderingContext(
      execution_context, context_id, canvas_context_creation_attributes);
  if (!context)
    return nullptr;
  return context->AsV8OffscreenRenderingContext();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
void OffscreenCanvasModule::getContext(
    ExecutionContext* execution_context,
    OffscreenCanvas& offscreen_canvas,
    const String& id,
    const CanvasContextCreationAttributesModule* attributes,
    OffscreenRenderingContext& result,
    ExceptionState& exception_state) {
  if (offscreen_canvas.IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "OffscreenCanvas object is detached");
    return;
  }
  CanvasContextCreationAttributesCore canvas_context_creation_attributes;
  if (!ToCanvasContextCreationAttributes(
          attributes, canvas_context_creation_attributes, exception_state)) {
    return;
  }

  // OffscreenCanvas cannot be transferred after getContext, so this execution
  // context will always be the right one from here on.
  CanvasRenderingContext* context = offscreen_canvas.GetCanvasRenderingContext(
      execution_context, id, canvas_context_creation_attributes);
  if (context)
    context->SetOffscreenCanvasGetContextResult(result);
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

}  // namespace blink
