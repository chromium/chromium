// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/offscreencanvas/offscreen_canvas_module.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_context_creation_attributes_module.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_offscreen_rendering_context_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"

namespace blink {

V8OffscreenRenderingContext* OffscreenCanvasModule::getContext(
    ScriptState* script_state,
    OffscreenCanvas& offscreen_canvas,
    const V8OffscreenRenderingContextType& context_type,
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

  CanvasRenderingContext::CanvasRenderingAPI rendering_api;
  switch (context_type.AsEnum()) {
    case V8OffscreenRenderingContextType::Enum::k2D:
      rendering_api = CanvasRenderingContext::CanvasRenderingAPI::k2D;
      break;
    case V8OffscreenRenderingContextType::Enum::kWebGL:
      rendering_api = CanvasRenderingContext::CanvasRenderingAPI::kWebgl;
      break;
    case V8OffscreenRenderingContextType::Enum::kWebGL2:
      rendering_api = CanvasRenderingContext::CanvasRenderingAPI::kWebgl2;
      break;
    case V8OffscreenRenderingContextType::Enum::kBitmaprenderer:
      rendering_api =
          CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer;
      break;
    case V8OffscreenRenderingContextType::Enum::kWebGPU:
      rendering_api = CanvasRenderingContext::CanvasRenderingAPI::kWebgpu;
      break;
    default:
      NOTREACHED();
  }

  // OffscreenCanvas cannot be transferred after getContext, so this execution
  // context will always be the right one from here on.
  CanvasRenderingContext* context = offscreen_canvas.GetCanvasRenderingContext(
      ExecutionContext::From(script_state), rendering_api,
      canvas_context_creation_attributes);
  if (!context)
    return nullptr;
  return context->AsV8OffscreenRenderingContext();
}

}  // namespace blink
